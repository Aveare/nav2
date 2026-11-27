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

    # sentry pure logic: chassis power model (no hardware / RTOS dependencies)
    & gcc @taskCommon -Iconfig/sentry_2026 -Iapps/sentry/chassis tests/sentry_chassis_power_test.c apps/sentry/chassis/chassis_power.c -lm -o tests/build/sentry_chassis_power_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry chassis power test compilation failed.' }
    & './tests/build/sentry_chassis_power_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry chassis power test failed.' }

    # sentry vision link: real 0x0503/0x1021 codec wrapper (host stubs only)
    & gcc @taskCommon -Itests/vision_stubs -Iapps/sentry/autonomy -Icomponents/ax650 -Iconfig/sentry_2026 `
        tests/sentry_vision_link_test.c apps/sentry/autonomy/vision_link.c components/ax650/ax650_pose.c -lm `
        -o tests/build/sentry_vision_link_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry vision link test compilation failed.' }
    & './tests/build/sentry_vision_link_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry vision link test failed.' }

    # sentry USB pipeline: NV COMMAND + 0x0503 on one stream, 0x1021 uplink
    & gcc -std=gnu11 -Wall -Wextra -Werror -Itests/usb_pipeline_stubs -Iapps/sentry/autonomy `
        -Icomponents/navigation -Icomponents/ax650 -Iconfig/sentry_2026 `
        tests/sentry_usb_pipeline_test.c apps/sentry/autonomy/autonomy.c `
        apps/sentry/autonomy/vision_link.c components/navigation/nav_protocol.c `
        components/ax650/ax650_pose.c -lm -o tests/build/sentry_usb_pipeline_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry USB pipeline test compilation failed.' }
    & './tests/build/sentry_usb_pipeline_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry USB pipeline test failed.' }

    # sentry navigation: NV command -> ramp -> mecanum inverse -> wheel RPM
    & python tests/simulation/sentry_nav_sim.py
    if ($LASTEXITCODE -ne 0) { throw 'sentry navigation simulation failed.' }

    # sentry 3-axis + chassis simulation (lumped plant; prints envelope metrics)
    & python tests/simulation/sentry_axis_sim.py
    if ($LASTEXITCODE -ne 0) { throw 'sentry axis simulation failed.' }

    # sentry pure logic: dual-yaw allocation (no hardware / RTOS dependencies)
    & gcc @taskCommon -Iapps/sentry/gimbal tests/sentry_gimbal_alloc_test.c -lm -o tests/build/sentry_gimbal_alloc_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal alloc test compilation failed.' }
    & './tests/build/sentry_gimbal_alloc_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry gimbal alloc test failed.' }

    # Migrated SI control, real big-yaw task, DM protocol, and real DBUS command chain.
    $taskSentryDirs = @('tests/native_stubs', 'config/sentry_2026', 'apps/common/app', 'apps/sentry/app',
        'apps/sentry/cmd', 'apps/sentry/gimbal', 'apps/sentry/autonomy', 'components/remote',
        'components/motor/core', 'components/motor/dji', 'components/motor/dm', 'components/navigation', 'components/ax650',
        'framework', 'framework/msg', 'ports/stm32f4', 'framework/kernel', 'framework/err',
        'components/control', 'components/crc', 'components/math', 'components/alarm',
        'components/daemon', 'framework/diag')
    $taskSentryIncludes = $taskSentryDirs | ForEach-Object { '-I' + $_ }

    # sentry autoaim target pipeline: validity/hold/pitch clamp
    & gcc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter @taskSentryIncludes `
        tests/sentry_autoaim_test.c apps/sentry/cmd/cmd_autoaim.c -lm `
        -o tests/build/sentry_autoaim_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'sentry autoaim test compilation failed.' }
    & './tests/build/sentry_autoaim_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'sentry autoaim test failed.' }

    & gcc @taskCommon @taskSentryIncludes tests/sentry_axis_control_test.c apps/sentry/gimbal/sentry_control.c components/control/controller.c -lm -o tests/build/sentry_axis_control_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'SI compilation failed' }
    & ./tests/build/sentry_axis_control_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'SI regression failed' }
    & python tests/run_sentry_axis_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Big yaw task regression failed' }
    & gcc @taskCommon @taskSentryIncludes tests/sentry_dm_port_test.c -lm -o tests/build/sentry_dm_port_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'DM compilation failed' }
    & ./tests/build/sentry_dm_port_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'DM regression failed' }
    $taskSentryCmdSources = @('tests/sentry_remote_test.c', 'apps/sentry/cmd/robot_cmd.c',
        'apps/sentry/cmd/cmd_authority.c', 'apps/sentry/cmd/cmd_autoaim.c', 'apps/sentry/cmd/cmd_kbm.c',
        'apps/sentry/cmd/cmd_remote.c', 'components/remote/remote_control.c', 'framework/msg/msg.c',
        'components/daemon/daemon.c', 'framework/diag/telemetry.c', 'ports/stm32f4/telemetry_arena.c')
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable @taskSentryIncludes @taskSentryCmdSources -lm -o tests/build/sentry_remote_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'DBUS compilation failed' }
    & ./tests/build/sentry_remote_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'DBUS regression failed' }

    # sentry navigation: NV v1 frame bytes C/Python parity plus autonomy USB ring
    & python tests/navigation/run_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'navigation protocol regression failed' }
} finally {
    Pop-Location
}
