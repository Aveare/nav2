"""Extract the actual pitch task and run it with the real cascade PID and host IO stubs.

Covers the 2026-11-28 pitch findings: loop-stability sign on an encoder loop (positive MIT torque
increases total_position, same as the bench-verified big-yaw DM4310), boot-zero latch, hold at the
boot zero, barrel-degree limits mapped through DIRECTION / transmission ratio, out-of-range cut.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/sentry/gimbal/gimbal.c").read_text(encoding="utf-8")

def function(name):
    start = -1
    for sig in ("static void ", "static float ", "static bool ", "static int "):
        try:
            start = source.index(sig + name + "(")
            break
        except ValueError:
            continue
    if start < 0:
        raise KeyError(name)
    brace = source.index("{", start)
    level = 1
    end = brace + 1
    while level:
        level += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

state_start = source.index("static dm_motor_t *sentry_pitch_motor;")
state = source[state_start:source.index("static dji_motor_t *sentry_small_yaw_motor;", state_start)]
prefix = r'''#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sentry_def.h"
#include "general_def.h"
#include "sentry_control.h"

#define DM_TASK_CYCLE 2
#define DM_STATE_ENABLE 1
/* 同 motor_def.h 的夹限宏;不直接 include 该头,避免拉入 HAL/CAN 类型。 */
#define MOTOR_LIMIT_MIN_MAX(x, min, max) (x) = (((x) <= (min)) ? (min) : (((x) >= (max)) ? (max) : (x)))

/* Only peripheral IO is stubbed; task guards, boot-zero latch, limits and real PID are unmodified. */

typedef struct {
 struct { float total_position, velocity, T_Mos, T_Rotor; uint8_t state; uint16_t position_raw; } measure;
 uint8_t rx_init_flag; int rx_refresh_dt; float torque; bool enabled;
} dm_motor_t;
static dm_motor_t motor;
static attitude_t imu;
volatile float sentry_rt_pitch_zero_rad = SENTRY_PITCH_ZERO_RAD;
volatile float sentry_rt_pitch_min_deg = SENTRY_PITCH_MIN_DEG;
volatile float sentry_rt_pitch_max_deg = SENTRY_PITCH_MAX_DEG;
volatile float sentry_rt_pitch_gravity_a = SENTRY_PITCH_GRAVITY_A;
volatile float sentry_rt_pitch_gravity_b = SENTRY_PITCH_GRAVITY_B;
volatile float sentry_rt_pitch_gravity_phase_deg = SENTRY_PITCH_GRAVITY_PHASE_DEG;
volatile float sentry_rt_pitch_ratio = SENTRY_PITCH_TRANSMISSION_RATIO;
volatile float sentry_rt_pitch_torque_sign = SENTRY_PITCH_TORQUE_SIGN;
static attitude_t *gimbal_imu_data = &imu;
static sentry_gimbal_cmd_t gimbal_cmd_recv;

static uint32_t test_us = 1000000u;
float dwt_get_delta_t(uint32_t *last) {
 float dt = (test_us - *last) * 0.000001f; *last = test_us; return dt;
}
static void dm_motor_stop(dm_motor_t *m) { m->enabled = false; m->torque = 0.0f; }
static void dm_motor_enable(dm_motor_t *m) { m->enabled = true; }
static void dm_motor_set_mit_torque(dm_motor_t *m, float t) { m->torque = t; }
'''
tests = r'''
/* 简化被控对象 = 正常达妙电机 + 轻枪管:MIT 正力矩 → 位置增大(与大 yaw DM4310 台架一致)。
 * v' = K_ACC·τ − b·v;J≈0.05 kg·m²,粘性摩擦 b/J=10。无重力,保持不需要力矩。 */
#define PLANT_ACC_PER_NM 20.0f
#define PLANT_FRICTION 10.0f
static float plant_v;
static void PlantReset(float position) {
 motor.measure.total_position = position;
 motor.measure.velocity = 0;
 plant_v = 0;
}
static void PlantStep(float dt) {
 plant_v += (PLANT_ACC_PER_NM * motor.torque - PLANT_FRICTION * plant_v) * dt;
 motor.measure.total_position += plant_v * dt;
 motor.measure.velocity = plant_v;
}
static void Idle(void) { plant_v = 0; motor.measure.velocity = 0; }

static void Tick(int plant) {
 test_us += 5000u; /* gimbal_task 200 Hz */
 gimbal_pitch_task(test_us * 0.001f);
 assert(isfinite(motor.torque));
 assert(fabsf(sentry_pitch_torque_nm) <= SENTRY_PITCH_MAX_TORQUE_NM + 0.001f);
 if (plant) PlantStep(0.005f); else Idle();
}
static void Run(int ticks, int plant) { for (int n = 0; n < ticks; ++n) Tick(plant); }

int main(void) {
 /* 零位:ZERO_AT_BOOT=1 时为首个有效反馈锁存的位置,ZERO_AT_BOOT=0 时为固定常量 SENTRY_PITCH_ZERO_RAD。
  * 枪管度经 DIR 换算到电机轴。 */
#if SENTRY_PITCH_ZERO_AT_BOOT
 const float boot = SENTRY_PITCH_ZERO_RAD + 0.3f; /* 任意上电位置 = 0 位 */
#else
 const float boot = SENTRY_PITCH_ZERO_RAD;        /* 固定零位 */
#endif
 const float dir = SENTRY_PITCH_DIRECTION;
 const float lim_up = SENTRY_PITCH_MAX_DEG * FW_DEG_TO_RAD;   /* 上抬可用角 */
 const float lim_dn = -SENTRY_PITCH_MIN_DEG * FW_DEG_TO_RAD;  /* 下压可用角(上下限不对称) */
 const float margin = SENTRY_PITCH_LIMIT_MARGIN_DEG * FW_DEG_TO_RAD;
 sentry_pitch_motor = &motor;
 gimbal_init_controller(&sentry_pitch_controller, false);
 motor.measure.state = DM_STATE_ENABLE;
 motor.rx_init_flag = 1;
 motor.rx_refresh_dt = 0;
 PlantReset(boot);
 /* 未解锁:零力矩;首个有效反馈锁存为 0 位。 */
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(0);
 assert(!motor.enabled && motor.torque == 0.0f);
#if SENTRY_PITCH_ZERO_AT_BOOT
 assert(sentry_pitch_boot_ref_valid && sentry_pitch_boot_ref_rad == boot);
 assert(fabsf(sentry_pitch_boot_ref_deg - boot * FW_RAD_TO_DEG) < 0.001f);
#else
 assert(!sentry_pitch_boot_ref_valid); /* 固定零位:不锁存 */
#endif
 /* 解锁前枪管下垂 3°(电机无出力);解锁后目标回到上电 0 位,而不是下垂后的位置。 */
 motor.measure.total_position = boot - 3.0f * FW_DEG_TO_RAD;
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 gimbal_cmd_recv.pitch = 0.0f;
 Tick(0);
 assert(motor.enabled && sentry_pitch_control_active && motor.torque == 0.0f); /* 首拍零输出 */
 Run(200, 0);
 assert(fabsf(sentry_pitch_target_rad - boot) < 0.0001f);
 assert(sentry_pitch_torque_nm > 0.0f); /* 位置低于目标 → 朝增大位置方向出力(PID 符号) */
 assert(fabsf(motor.torque - sentry_pitch_torque_nm * SENTRY_PITCH_TORQUE_SIGN) < 0.0001f);
 /* 软限位以枪管俯仰角定义(±30° 量级),经 DIR 落到电机轴;越界命令再夹一次。
 * 目标爬升 60°/s = 0.3°/拍,全行程 67° 需 ~230 拍,这里统一给 400 拍留余量。 */
 PlantReset(boot);
 gimbal_cmd_recv.pitch = SENTRY_PITCH_MAX_DEG;
 Run(400, 0);
 assert(fabsf(sentry_pitch_target_rad - (boot + dir * lim_up)) < 0.0001f);
 gimbal_cmd_recv.pitch = SENTRY_PITCH_MAX_DEG + 10.0f;
 Run(200, 0);
 assert(fabsf(sentry_pitch_target_rad - (boot + dir * lim_up)) < 0.0001f);
 gimbal_cmd_recv.pitch = SENTRY_PITCH_MIN_DEG;
 Run(400, 0);
 assert(fabsf(sentry_pitch_target_rad - (boot - dir * lim_dn)) < 0.0001f);
 gimbal_cmd_recv.pitch = SENTRY_PITCH_MIN_DEG - 10.0f;
 Run(200, 0);
 assert(fabsf(sentry_pitch_target_rad - (boot - dir * lim_dn)) < 0.0001f);
 gimbal_cmd_recv.pitch = 0.0f;
 Run(400, 0);
 assert(fabsf(sentry_pitch_target_rad - boot) < 0.0001f && motor.enabled);
 /* 传动比 R=2(电机转 2° 枪管抬 1°):枪管限位角 → 电机目标减半;非法值回退 config。 */
 sentry_rt_pitch_ratio = 2.0f;
 gimbal_cmd_recv.pitch = SENTRY_PITCH_MAX_DEG;
 Run(100, 0);
 assert(fabsf(sentry_pitch_target_rad - (boot + dir * lim_up / 2.0f)) < 0.0001f);
 sentry_rt_pitch_ratio = NAN;
 gimbal_cmd_recv.pitch = 0.0f;
 Run(100, 0);
 assert(fabsf(sentry_pitch_target_rad - boot) < 0.0001f);
#if !SENTRY_AUTO_AIM_PITCH_ENABLE
 /* 视觉不驱动 pitch:AUTO_AIM 模式 + aim_valid 仍按手动命令。 */
 gimbal_cmd_recv.mode = GIMBAL_AUTO_AIM_MODE;
 gimbal_cmd_recv.aim_valid = 1u;
 gimbal_cmd_recv.pitch_abs = 25.0f;
 Run(20, 0);
 assert(fabsf(sentry_pitch_target_rad - boot) < 0.0001f && motor.enabled);
 gimbal_cmd_recv.aim_valid = 0u;
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
#endif
 /* 实测角越过软限位 + 裕量:立即断力并要求重锚;退出使能态后清除;回行程内恢复。 */
 motor.measure.total_position = boot + dir * (lim_up + margin + 1.0f * FW_DEG_TO_RAD);
 Tick(0);
 assert(!motor.enabled && motor.torque == 0.0f && sentry_pitch_rearm_required);
 PlantReset(boot);
 Run(5, 0);
 assert(!motor.enabled);
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(0);
 assert(!motor.enabled && !sentry_pitch_rearm_required);
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 Run(2, 0);
 assert(motor.enabled && sentry_pitch_control_active);
 /* 反馈超时 / 非有限反馈:断力。 */
 motor.rx_refresh_dt = (int)(DM_ENABLE_RETRY_MS / DM_TASK_CYCLE);
 Tick(0);
 assert(!motor.enabled && motor.torque == 0.0f);
 motor.rx_refresh_dt = 0;
 motor.measure.total_position = NAN;
 Run(2, 0);
 assert(!motor.enabled);

 /* ===== 闭环符号验证(核心):正常达妙被控对象下,杆推 10° 必须收敛保持 ===== */
 PlantReset(boot);
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(0);
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 gimbal_cmd_recv.pitch = 10.0f; /* 枪管上抬 10° */
 const float expect = boot + dir * 10.0f * FW_DEG_TO_RAD;
 float peak = 0.0f;
 for (int n = 0; n < 400; ++n) {
 Tick(1);
 peak = fmaxf(peak, fabsf(motor.measure.total_position - boot));
 }
 assert(motor.enabled); /* 没有冲到 ±(15+2)° 的断力边界 */
 assert(peak < lim_up + margin);
 assert(dir * (motor.measure.total_position - boot) > 0.0f); /* 枪管方向 = 上抬 */
 assert(fabsf(motor.measure.total_position - expect) < 0.5f * FW_DEG_TO_RAD); /* 2 s 内收敛到 0.5° 内 */
 /* 松杆(命令不变)持续保持。 */
 Run(200, 1);
 assert(motor.enabled && fabsf(motor.measure.total_position - expect) < 0.5f * FW_DEG_TO_RAD);
 /* 反向:杆下压 10°,穿过上电位到另一侧。 */
 gimbal_cmd_recv.pitch = -10.0f;
 Run(400, 1);
 assert(motor.enabled && fabsf(motor.measure.total_position - (boot - dir * 10.0f * FW_DEG_TO_RAD)) < 0.5f * FW_DEG_TO_RAD);
 /* 对照:把稳定性符号取反(= 11-23 遗留的 -1 口径),同一命令必须发散到断力边界。
  * 这正是"动一下摇杆直接冲限位"的复现;同时验证运行期符号覆盖生效。 */
 PlantReset(boot);
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(0);
 sentry_rt_pitch_torque_sign = -SENTRY_PITCH_TORQUE_SIGN;
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 gimbal_cmd_recv.pitch = 10.0f;
 peak = 0.0f;
 for (int n = 0; n < 400; ++n) {
 Tick(1);
 peak = fmaxf(peak, fabsf(motor.measure.total_position - boot));
 if (!motor.enabled) break;
 }
 assert(!motor.enabled || peak >= lim_dn + margin); /* 发散:被越界断力拦下,或已冲出边界 */
 sentry_rt_pitch_torque_sign = SENTRY_PITCH_TORQUE_SIGN;
 puts("PASS: actual pitch task converges on a normal DM plant (boot-zero latch, hold-at-zero, barrel-degree limits via DIR/ratio, out-of-range cut) and diverges with the legacy -1 torque sign");
 return 0;
}
'''
out = root / "tests/build/sentry_pitch_task_test.c"
out.parent.mkdir(exist_ok=True)
out.write_text(prefix + state + function("gimbal_init_controller") + function("gimbal_pitch_task") + tests, encoding="utf-8")
include_dirs = ["tests/native_stubs", "config/sentry_2026", "apps/common/app", "apps/sentry/app", "apps/sentry/gimbal", "framework", "framework/msg", "apps/sentry/cmd", "framework/kernel", "components/control", "components/math", "components/motor/core", "ports/stm32f4"]
exe = out.with_suffix(".exe")
subprocess.run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", *["-I"+d for d in include_dirs], str(out), "apps/sentry/gimbal/sentry_control.c", "components/control/controller.c", "-lm", "-o", str(exe)], cwd=root, check=True)
subprocess.run([str(exe)], cwd=root, check=True)
