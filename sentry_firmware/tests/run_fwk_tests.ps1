$ErrorActionPreference = 'Stop'
# B1 框架/主机测试聚合(G3 判据之一;kernel/msg/err 主机用例落地后在此追加)
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    & python tools/check_golden.py --selftest
    if ($LASTEXITCODE -ne 0) { throw 'golden comparator selftest failed.' }

    & python tools/check_symbol_contract.py
    if ($LASTEXITCODE -ne 0) { throw 'tool symbol contract failed.' }

    # B12c: 宏重复/冲突/单字母宏(T14/附D;豁免见 .Doc/gate/macro-dups-allow.txt)
    & python tools/check_macro_dups.py
    if ($LASTEXITCODE -ne 0) { throw 'macro duplicate/conflict check failed.' }

    # G1b: 宏守恒探针(-E -dM 快照;新增允许,删除/改值 FAIL)
    & python tools/check_macro_golden.py
    if ($LASTEXITCODE -ne 0) { throw 'macro golden probe failed.' }

    # T11: 协议合同 ABI 快照(sizeof/offsetof 逐条守恒)
    & python tools/check_abi_golden.py
    if ($LASTEXITCODE -ne 0) { throw 'abi golden check failed.' }

    # T16: 注释风格(文件头/占位符/TODO/禁用标签)
    & python tools/check_comment_style.py
    if ($LASTEXITCODE -ne 0) { throw 'comment style check failed.' }

    # G10: 双构建 parity(Makefile 与 CMake 的 -D 集合)
    & python tools/check_build_parity.py
    if ($LASTEXITCODE -ne 0) { throw 'build parity check failed.' }

    # 命名规范(T15):文件/函数/类型/宏/变量
    & python tools/check_naming.py
    if ($LASTEXITCODE -ne 0) { throw 'naming check failed.' }

    # B11 三骨架编译矩阵(方案 G-B;infantry/heavy/sentry 各 1 个 sim TU)
    & powershell -ExecutionPolicy Bypass -File tests/run_profile_tests.ps1
    if ($LASTEXITCODE -ne 0) { throw 'profile compile matrix failed.' }

    & powershell -ExecutionPolicy Bypass -File tests/run_can_golden.ps1
    if ($LASTEXITCODE -ne 0) { throw 'can filter golden failed.' }

    # 整链 harness 重放:确定性回归(重生成金标准必须逐字节一致)
    Copy-Item .Doc/gate/arch-baseline/g8b-fullchain.csv tests/build/g8b-prev.csv -Force
    & powershell -ExecutionPolicy Bypass -File tests/run_fullchain_harness.ps1
    if ($LASTEXITCODE -ne 0) { throw 'fullchain harness failed.' }
    $d = Compare-Object (Get-Content tests/build/g8b-prev.csv) (Get-Content .Doc/gate/arch-baseline/g8b-fullchain.csv)
    if ($d) { throw 'fullchain golden DETERMINISM BROKEN: rerun differs.' }
    Write-Host 'PASS: fullchain golden determinism'

    # kernel posix 主机用例(B1 ②:framework kernel/err/mem + pthread 双线程互斥)
    & gcc -std=gnu11 -Wall -Wextra -Iframework -Iframework/kernel -Iframework/err -Iframework/mem -Itests/fwk_posix `
        tests/fwk_kernel_test.c framework/kernel/kernel_version.c framework/err/err.c `
        framework/mem/mem.c tests/fwk_posix/kernel_posix.c `
        -o tests/build/fwk_kernel_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'fwk_kernel_test compilation failed.' }
    & './tests/build/fwk_kernel_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'kernel host tests failed.' }

    # msg 主机用例(B2:对照表 12 行 + 双线程只保最新)
    & gcc -std=gnu11 -Wall -Wextra -Iframework/kernel -Iframework/err -Iframework -Iframework/msg -Itests/fwk_posix `
        tests/fwk_msg_test.c framework/kernel/kernel_version.c framework/err/err.c `
        framework/msg/msg.c tests/fwk_posix/kernel_posix.c `
        -o tests/build/fwk_msg_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'fwk_msg_test compilation failed.' }
    & './tests/build/fwk_msg_test.exe'
    if ($LASTEXITCODE -ne 0) { throw 'msg host tests failed.' }
} finally {
    Pop-Location
}
