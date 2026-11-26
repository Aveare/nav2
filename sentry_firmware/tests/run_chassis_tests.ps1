$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    # CMD authority arbitration test
    $taskIncludeDirs = @('tests/native_stubs', 'config/infantry_2026', 'apps/common/app', 'apps/infantry/app', 'apps/infantry/cmd', 'apps/infantry/chassis',
                         'apps/infantry/gimbal', 'components/remote', 'components/motor/core', 'components/motor/dji', 'components/motor/dm', 'components/motor/ht', 'components/motor/servo',
                         'framework', 'framework/msg', 'ports/stm32f4', 'framework/kernel', 'framework/err', 'components/control', 'components/crc', 'components/math', 'components/alarm', 'components/daemon', 'framework/diag')
    $taskIncludes = $taskIncludeDirs | ForEach-Object { '-I' + $_ }
    $taskSources = @('tests/chassis_remote_test.c', 'tests/native_stubs/gimbal_stub.c',
                     'apps/infantry/cmd/robot_cmd.c', 'apps/infantry/cmd/cmd_authority.c',
                     'apps/infantry/cmd/cmd_autoaim.c', 'apps/infantry/cmd/cmd_kbm.c',
                     'apps/infantry/cmd/cmd_remote.c',
                     'components/remote/remote_control.c', 'framework/msg/msg.c',
                     'components/daemon/daemon.c', 'framework/diag/telemetry.c',
                     'ports/stm32f4/telemetry_arena.c')
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable @taskIncludes @taskSources -lm -o tests/build/chassis_remote_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed.' }
    & './tests/build/chassis_remote_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Native arbiter tests failed.' }

    # 同一控制逻辑再以底盘安全锁关闭编译，确认恢复开关不会破坏原模式。
    $taskEnabledDefines = @('-DCHASSIS_M3508_FORCE_DISABLE=0')
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable @taskEnabledDefines @taskIncludes @taskSources -lm -o tests/build/chassis_remote_enabled_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Enabled chassis native test compilation failed.' }
    & './tests/build/chassis_remote_enabled_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Enabled chassis native tests failed.' }

    # CMD command-timeout boundary test
    & gcc -std=gnu11 -Wall -Wextra -Iapps/infantry/cmd -Iapps/common/app -Iapps/infantry/app -Iconfig/infantry_2026 tests/cmd_watchdog_test.c -o tests/build/cmd_watchdog_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cmd watchdog test compilation failed.' }
    & './tests/build/cmd_watchdog_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Cmd watchdog tests failed.' }

} finally {
    Pop-Location
}
