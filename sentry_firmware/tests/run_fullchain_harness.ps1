$ErrorActionPreference = 'Stop'
# B0-⑥b 整链 harness 编译与金标准生成(迁移前代码;B5/B6/B8 重放后用 check_golden.py 比对)
$taskProject = Split-Path -Parent $PSScriptRoot
Push-Location $taskProject
try {
    $taskIncludeDirs = @('tests/native_stubs', 'config/infantry_2026', 'apps/common/app', 'apps/infantry/app', 'apps/infantry/cmd', 'apps/infantry/gimbal',
                         'apps/infantry/chassis', 'apps/infantry/shoot',
                         'components/remote', 'components/motor/core', 'components/motor/dji', 'components/motor/dm', 'components/motor/ht', 'components/motor/servo',
                         'framework', 'framework/msg', 'ports/stm32f4', 'framework/kernel', 'framework/err', 'components/daemon', 'framework/diag',
                         'components/control', 'components/crc', 'components/math', 'components/alarm', 'components/ax650', 'components/calib', 'components/referee/protocol', 'components/referee/ui', 'components/referee/link', 'components/super_cap')
    $taskIncludes = $taskIncludeDirs | ForEach-Object { '-I' + $_ }
    $taskSources = @('tests/fullchain_harness.c',
                     'apps/infantry/cmd/robot_cmd.c', 'apps/infantry/cmd/cmd_authority.c',
                     'apps/infantry/cmd/cmd_autoaim.c', 'apps/infantry/cmd/cmd_kbm.c',
                     'apps/infantry/cmd/cmd_remote.c',
                     'apps/infantry/gimbal/gimbal.c', 'apps/infantry/gimbal/gimbal_recenter.c',
                     'apps/infantry/shoot/shoot.c', 'apps/infantry/shoot/heat_guard.c',
                     'apps/infantry/chassis/chassis.c', 'apps/infantry/chassis/chassis_kinematics.c',
                     'apps/infantry/chassis/chassis_power.c', 'apps/infantry/chassis/chassis_spin.c',
                     'components/remote/remote_control.c', 'framework/msg/msg.c',
                     'components/daemon/daemon.c', 'framework/diag/telemetry.c', 'ports/stm32f4/telemetry_arena.c',
                     'components/calib/calib.c', 'components/control/controller.c',
                     'components/math/user_lib.c', 'components/motor/dji/dji_motor.c')
    New-Item -ItemType Directory -Force -Path 'tests/build', '.Doc/gate/arch-baseline' | Out-Null
    & gcc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -include stddef.h @taskIncludes @taskSources -lm -o tests/build/fullchain_harness.exe
    if ($LASTEXITCODE -ne 0) { throw 'fullchain harness compilation failed.' }
    & './tests/build/fullchain_harness.exe'
    if ($LASTEXITCODE -ne 0) { throw 'fullchain harness run failed.' }
    & python tools/check_golden.py --selftest
    if ($LASTEXITCODE -ne 0) { throw 'golden comparator selftest failed.' }
} finally {
    Pop-Location
}
