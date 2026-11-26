$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    $taskCommon = @('-std=c11', '-Wall', '-Wextra', '-Werror')
    $taskIncludes = @('-Itests/ax650_stubs', '-Icomponents/ax650', '-Iports/stm32f4', '-Iframework/kernel', '-Iconfig/infantry_2026')
    foreach ($taskOptimization in @('-O2', '-Ofast')) {
        $taskTag = $taskOptimization.TrimStart('-')
        & gcc @taskCommon $taskOptimization -Icomponents/ax650 `
            tests/ax650_pose_test.c components/ax650/ax650_pose.c -lm `
            -o "tests/build/ax650_pose_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 pose compilation failed ($taskTag)." }
        & "./tests/build/ax650_pose_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 pose tests failed ($taskTag)." }
        foreach ($taskRxEnable in @(1, 0)) {
            $taskUartTag = "${taskTag}_rx${taskRxEnable}"
            & gcc @taskCommon $taskOptimization "-DAX650_UART1_RX_ENABLE=$taskRxEnable" @taskIncludes `
                tests/ax650_uart1_test.c components/ax650/ax650_pose.c `
                components/ax650/ax650_uart1.c ports/stm32f4/usart_port.c -lm `
                -o "tests/build/ax650_uart1_$taskUartTag.exe"
            if ($LASTEXITCODE -ne 0) { throw "AX650 UART1 compilation failed ($taskUartTag)." }
            & "./tests/build/ax650_uart1_$taskUartTag.exe"
            if ($LASTEXITCODE -ne 0) { throw "AX650 UART1 tests failed ($taskUartTag)." }
        }
    }
    $taskRefereeSource = Get-Content -Raw -Encoding utf8 -LiteralPath 'components/referee/ui/referee_task.c'
    if ($taskRefereeSource -match 'extern\s+UART_HandleTypeDef\s+huart1|HAL_UART_Transmit\s*\(\s*&huart1') {
        throw 'UART routing contract failed: referee component references USART1.'
    }
    $taskChassisSource = Get-Content -Raw -Encoding utf8 -LiteralPath 'apps/infantry/chassis/chassis.c'
    if ($taskChassisSource -notmatch 'referee_ui_task_init\(&huart6,') {
        throw 'UART routing contract failed: referee system is not bound to USART6.'
    }
    $taskGimbalSource = Get-Content -Raw -Encoding utf8 -LiteralPath 'apps/infantry/gimbal/gimbal.c'
    if ($taskGimbalSource -notmatch 'YAW_AUTO_VOFA_USART6_ENABLE' -or
        $taskGimbalSource -notmatch 'gimbal_send_auto_vofa') {
        throw 'UART routing contract failed: USART6 auto-aim debug path is missing.'
    }
    $taskRobotTaskSource = Get-Content -Raw -Encoding utf8 -LiteralPath 'apps/common/app/robot_task.h'
    if ($taskRobotTaskSource -notmatch 'YAW_AUTO_VOFA_USART6_ENABLE' -or
        $taskRobotTaskSource -notmatch 'ROBOT_TASK_UI_ENABLE 0') {
        throw 'UART routing contract failed: USART6 debug/referee UI arbitration is missing.'
    }
    $taskAssemblySource = Get-Content -Raw -Encoding utf8 -LiteralPath 'apps/infantry/app/assembly.c'
    if ($taskAssemblySource -notmatch 'ShootData\.bullet_speed' -or
        $taskAssemblySource -notmatch 'ax650_uart1_set_projectile_speed' -or
        $taskAssemblySource -notmatch 'GameRobotState\.robot_id' -or
        $taskAssemblySource -notmatch 'ax650_uart1_set_robot_id') {
        throw 'Referee forwarding contract failed: speed or robot_id is not forwarded to AX650.'
    }
    Write-Output 'PASS: UART routing contract (owners, arbitration, referee speed and robot_id forwarding)'
} finally {
    Pop-Location
}
