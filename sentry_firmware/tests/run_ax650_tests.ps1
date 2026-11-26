$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    New-Item -ItemType Directory -Force -Path 'tests/build' | Out-Null
    $taskCommon = @('-std=c11', '-Wall', '-Wextra', '-Werror')
    $taskIncludes = @('-Itests/ax650_stubs', '-Imodules/master_machine', '-Ibsp/usart', '-Iconfig')
    foreach ($taskOptimization in @('-O2', '-Ofast')) {
        $taskTag = $taskOptimization.TrimStart('-')
        & gcc @taskCommon $taskOptimization -Imodules/master_machine `
            tests/ax650_pose_test.c modules/master_machine/ax650_pose.c -lm `
            -o "tests/build/ax650_pose_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 pose compilation failed ($taskTag)." }
        & "./tests/build/ax650_pose_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 pose tests failed ($taskTag)." }
        & gcc @taskCommon $taskOptimization @taskIncludes `
            tests/ax650_uart1_test.c modules/master_machine/ax650_pose.c `
            modules/master_machine/ax650_uart1.c bsp/usart/bsp_usart.c -lm `
            -o "tests/build/ax650_uart1_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 UART1 compilation failed ($taskTag)." }
        & "./tests/build/ax650_uart1_$taskTag.exe"
        if ($LASTEXITCODE -ne 0) { throw "AX650 UART1 tests failed ($taskTag)." }
    }
} finally {
    Pop-Location
}
