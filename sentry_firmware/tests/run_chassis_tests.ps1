$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    # CMD控制权仲裁测试
    $taskIncludes = @('tests/native_stubs', 'config', 'application', 'application/autonomy', 'application/cmd', 'application/chassis', 'application/gimbal',
                      'modules', 'modules/navigation', 'modules/remote', 'modules/motor', 'modules/motor/DJImotor',
                      'modules/message_center', 'modules/algorithm', 'modules/daemon', 'modules/telemetry',
                      'bsp/can', 'bsp/usart') |
                    ForEach-Object { '-I' + $_ }
    $taskSources = @('tests/chassis_remote_test.c', 'application/cmd/robot_cmd.c',
                     'modules/remote/remote_control.c', 'modules/message_center/message_center.c',
                     'modules/daemon/daemon.c', 'modules/telemetry/telemetry.c')
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable @taskIncludes @taskSources -lm -o tests/build/chassis_remote_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed.' }
    & './tests/build/chassis_remote_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Native arbiter tests failed.' }

    # CMD命令断流看门狗边界测试(纯逻辑,无依赖)
    & gcc -std=gnu11 -Wall -Wextra -Iapplication/cmd tests/cmd_watchdog_test.c -o tests/build/cmd_watchdog_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Cmd watchdog test compilation failed.' }
    & './tests/build/cmd_watchdog_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Cmd watchdog tests failed.' }

    # 哨兵串级PID：使用真实PID实现，检查Nm/速度/积分限幅和复位。
    & gcc -std=gnu11 -Wall -Wextra -Werror @taskIncludes tests/sentry_control_test.c application/gimbal/sentry_control.c modules/algorithm/controller.c -lm -o tests/build/sentry_control_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Sentry control test compilation failed.' }
    & './tests/build/sentry_control_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Sentry control tests failed.' }
    # 执行从真实大yaw任务抽取的模式切换与世界稳像回归。
    & python tests/run_sentry_world_tests.py
    if ($LASTEXITCODE -ne 0) { throw 'Big yaw world hold tests failed.' }
} finally {
    Pop-Location
}
