$ErrorActionPreference = 'Stop'
# B0-⑥c CAN 过滤器金标准(真实 bsp_can.c 注册路径;B4a 重放后 diff 必须为空)
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    $taskIncludeDirs = @('tests/native_stubs', 'config/infantry_2026', 'ports/stm32f4', 'framework', 'framework/err')
    $taskIncludes = $taskIncludeDirs | ForEach-Object { '-I' + $_ }
    $taskSources = @('tests/can_filter_golden.c', 'ports/stm32f4/can_port.c', 'framework/err/err.c')
    New-Item -ItemType Directory -Force -Path 'tests/build', '.Doc/gate/arch-baseline' | Out-Null
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter @taskIncludes @taskSources -o tests/build/can_filter_golden.exe
    if ($LASTEXITCODE -ne 0) { throw 'can filter golden compilation failed.' }
    & './tests/build/can_filter_golden.exe'
    if ($LASTEXITCODE -ne 0) { throw 'can filter golden run failed.' }
} finally {
    Pop-Location
}
