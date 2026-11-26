$ErrorActionPreference = 'Stop'
# B11 skeleton profile compile matrix (plan G-B; sim target is enough).
# infantry profile.h is already compiled by the ARM build (robot.c includes it),
# so this script covers the anchor-only heavy/sentry skeletons as minimal host TUs.
# rationale: RM_DEVLOG DEV-20261122-22.
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    $taskCommon = @('-std=gnu11', '-Wall', '-Wextra', '-Werror')
    foreach ($taskProfile in @('heavy', 'sentry')) {
        $taskProfileInclude = @("-Iapps/${taskProfile}/app")
        if ($taskProfile -eq 'sentry') {
            $taskProfileInclude += @(
                '-Iapps/sentry/cmd',
                '-Iapps/sentry/chassis',
                '-Iapps/sentry/gimbal',
                '-Iapps/sentry/shoot',
                '-Iapps/sentry/autonomy'
            )
        }
        $taskArgs = @('-Iapps/common/app') + $taskProfileInclude + @("-Iconfig/${taskProfile}_2026")
        & gcc @taskCommon @taskArgs "tests/profile_compile_${taskProfile}.c" -o "tests/build/profile_${taskProfile}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "profile ${taskProfile} compilation failed." }
        & gcc @taskCommon @taskArgs -c "apps/${taskProfile}/app/assembly.c" -o "tests/build/assembly_${taskProfile}.o"
        if ($LASTEXITCODE -ne 0) { throw "profile ${taskProfile} assembly compilation failed." }
        & "./tests/build/profile_${taskProfile}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "profile ${taskProfile} anchor test failed." }
    }

    # sentry pure logic: dual-yaw allocation (no hardware / RTOS dependencies)
    & gcc @taskCommon -Iapps/sentry/gimbal tests/sentry_gimbal_alloc_test.c -lm -o tests/build/sentry_gimbal_alloc_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal alloc test compilation failed.' }
    & './tests/build/sentry_gimbal_alloc_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal alloc test failed.' }

    # sentry pure control: SI cascade PID and spin speed damping
    $taskControlIncludes = @('-Itests/native_stubs', '-Iapps/sentry/gimbal',
                             '-Icomponents/control', '-Iframework', '-Iports/stm32f4')
    & gcc @taskCommon @taskControlIncludes tests/sentry_gimbal_control_test.c `
        apps/sentry/gimbal/gimbal_control.c components/control/controller.c -lm `
        -o tests/build/sentry_gimbal_control_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal control test compilation failed.' }
    & './tests/build/sentry_gimbal_control_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal control test failed.' }

    # sentry RC mapping: local robot stick/switch contract
    $taskRemoteIncludes = @('-Itests/native_stubs', '-Iapps/common/app', '-Iapps/sentry/app',
                            '-Iapps/sentry/cmd', '-Iconfig/sentry_2026', '-Icomponents/remote',
                            '-Icomponents/motor/core', '-Icomponents/control', '-Iframework',
                            '-Iports/stm32f4')
    & gcc @taskCommon @taskRemoteIncludes tests/sentry_remote_mapping_test.c `
        apps/sentry/cmd/cmd_remote.c -lm -o tests/build/sentry_remote_mapping_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry remote mapping test compilation failed.' }
    & './tests/build/sentry_remote_mapping_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry remote mapping test failed.' }
} finally {
    Pop-Location
}
