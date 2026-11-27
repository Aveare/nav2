"""Extract the actual big-yaw task and run it with real cascade PID and host IO stubs."""
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

state_start = source.index("static dm_motor_t *sentry_big_yaw_motor;")
state = source[state_start:source.index("static dm_motor_t *sentry_pitch_motor;", state_start)]
prefix = r'''#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sentry_def.h"
#include "general_def.h"
#include "msg.h"
#undef SENTRY_BIG_YAW_OUTPUT_ENABLE
#define SENTRY_BIG_YAW_OUTPUT_ENABLE 1
#include "sentry_control.h"
#include "gimbal_alloc.h"
#include "cmd_watchdog.h"




#define DM_TASK_CYCLE 2
#define DM_STATE_ENABLE 1

/* Only peripheral IO is stubbed; task selection, guards and real PID are unmodified. */

typedef struct {
 struct { float total_position, velocity; float T_Mos, T_Rotor; uint8_t state; } measure;
 uint8_t rx_init_flag; int rx_refresh_dt; float torque; bool enabled;
} dm_motor_t;
static dm_motor_t motor;
static attitude_t imu;
volatile float sentry_small_yaw_joint_deg;
volatile float sentry_rt_big_yaw_angle_kp = SENTRY_BIG_YAW_ANGLE_KP;
volatile float sentry_rt_big_yaw_speed_kp = SENTRY_BIG_YAW_SPEED_KP;
volatile float sentry_rt_big_yaw_speed_ki = SENTRY_BIG_YAW_SPEED_KI;
volatile float sentry_rt_big_yaw_torque_limit_nm = SENTRY_BIG_YAW_TORQUE_NM;
volatile float sentry_rt_small_yaw_max_out = SENTRY_SMALL_YAW_MAX_OUT;
volatile float sentry_rt_small_yaw_limit_deg = SENTRY_SMALL_YAW_LIMIT_DEG;
volatile float sentry_rt_pitch_zero_rad = SENTRY_PITCH_ZERO_RAD;
volatile float sentry_rt_pitch_gravity_a = SENTRY_PITCH_GRAVITY_A;
volatile float sentry_rt_pitch_gravity_b = SENTRY_PITCH_GRAVITY_B;
volatile float sentry_rt_pitch_gravity_phase_deg = SENTRY_PITCH_GRAVITY_PHASE_DEG;
static attitude_t *gimbal_imu_data = &imu;
static sentry_gimbal_cmd_t gimbal_cmd_recv;

static struct { uint64_t UpdateCount; uint8_t Initialized; } ekf_ins;
static uint32_t test_us = 1000000u;
float dwt_get_delta_t(uint32_t *last) {
 float dt = (test_us - *last) * 0.000001f; *last = test_us; return dt;
}
uint32_t dwt_get_timeline_ms(void) { return test_us / 1000u; }
static void dm_motor_stop(dm_motor_t *m) { m->enabled = false; }
static void dm_motor_enable(dm_motor_t *m) { m->enabled = true; }
static void dm_motor_set_mit_torque(dm_motor_t *m, float t) { m->torque = t; }
'''
tests = r'''
static void Tick(bool update_imu, bool update_chassis) {
 test_us += 5000u;
 float now = test_us * 0.001f;
 if (update_imu) ++ekf_ins.UpdateCount;
 if (update_chassis) cmd_watchdog_feed(&sentry_yaw_chassis_wd, now);
 gimbal_big_yaw_task(now);
 assert(isfinite(motor.torque));
 assert(fabsf(motor.torque) <= SENTRY_BIG_YAW_TORQUE_NM + 0.001f);
}
static void Start(bool spin, float world_deg) {
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(true, true);
 memset(&imu, 0, sizeof(imu));
 imu.YawTotalAngle = world_deg;
 motor.measure.total_position = SENTRY_BIG_YAW_FRONT_ALIGN_RAD;
 motor.measure.velocity = 0;
 motor.measure.state = DM_STATE_ENABLE;
 motor.rx_init_flag = 1;
 motor.rx_refresh_dt = 0;
 ekf_ins.Initialized = 1;
 sentry_yaw_chassis_cmd.chassis_mode = spin ? CHASSIS_ROTATE : CHASSIS_NO_FOLLOW;
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 Tick(true, true);
}
int main(void) {
 sentry_big_yaw_motor = &motor;
 assert(sentry_yaw_chassis_sub == 0);
 gimbal_init_controller(&sentry_yaw_controller, true);
 /* Initialized alone with no update must not enable world hold. */
 ekf_ins.Initialized = 1;
 sentry_yaw_chassis_cmd.chassis_mode = CHASSIS_ROTATE;
 gimbal_cmd_recv.mode = GIMBAL_GYRO_MODE;
 motor.measure.state = DM_STATE_ENABLE;
 motor.rx_init_flag = 1;
 /* 上电校准语义:首个有效反馈即车头参考,这里把初始位置摆在车头。 */
 motor.measure.total_position = SENTRY_BIG_YAW_FRONT_ALIGN_RAD;
 Tick(false, true);
 assert(!sentry_big_yaw_imu_online && !motor.enabled);
 /* Spin has zero world-speed target; relative encoder motion must not create torque. */
 Start(true, 37.0f);
 assert(sentry_big_yaw_world_hold_active && motor.enabled && motor.torque == 0);
 float anchor = sentry_big_yaw_world_target_rad;
 for (int n=0; n<1500; ++n) {
  motor.measure.total_position -= 300.0f * FW_DEG_TO_RAD * 0.005f;
  motor.measure.velocity = -300.0f * FW_DEG_TO_RAD;
  Tick(true, true);
  assert(sentry_big_yaw_world_target_rad == anchor);
  assert(fabsf(motor.torque) < 0.0001f);
  assert(!sentry_big_yaw_in_deadband);
 }
 /* A displaced or multi-turn heading alone must NEVER cause return-to-anchor torque. */
 const float headings[] = {117.0f, -43.0f, 757.0f, -1043.0f};
 for (unsigned i=0; i<sizeof(headings)/sizeof(headings[0]); ++i) {
  imu.YawTotalAngle = headings[i];
  imu.Gyro[2] = 0;
  for (int n=0; n<10; ++n) Tick(true, true);
  assert(fabsf(motor.torque) < 0.0001f);
  assert(sentry_big_yaw_speed_target_rad_s == 0);
  assert(fabsf(sentry_big_yaw_world_target_rad-headings[i]*FW_DEG_TO_RAD)<0.0001f);
 }
 /* Positive/negative angular velocities get opposing torque, independent of heading. */
 imu.YawTotalAngle = 38.0f;
 imu.Gyro[2] = 0.2f;
 for (int n=0; n<5; ++n) Tick(true, true);
 assert(motor.torque < 0);
 Start(true, 37.0f);
 imu.YawTotalAngle = 36.0f;
 imu.Gyro[2] = -0.2f;
 for (int n=0; n<5; ++n) Tick(true, true);
 assert(motor.torque > 0);
 Start(true, 37.0f);
 imu.Gyro[2] = 0.3f;
 for (int n=0; n<5; ++n) Tick(true, true);
 assert(motor.torque < 0 && !sentry_big_yaw_in_deadband);
 /* A real speed pulse may leave FW_PI integral; it must not create a heading target. */
 Start(true, 0);
 imu.Gyro[2] = 0.4f;
 for (int n=0; n<120; ++n) {
  imu.YawTotalAngle += 0.4f * 0.005f / FW_DEG_TO_RAD;
  Tick(true, true);
  assert(sentry_big_yaw_speed_target_rad_s == 0);
 }
 const float retained_i = sentry_yaw_controller.speed.Iout;
 assert(retained_i < 0);
 imu.Gyro[2] = 0;
 for (int n=0; n<200; ++n) Tick(true, true);
 assert(sentry_big_yaw_speed_target_rad_s == 0);
 assert(fabsf(sentry_yaw_controller.speed.Iout-retained_i)<0.00001f);
 assert(fabsf(motor.torque)<0.0001f); // 现有PID零误差规则及输出低通衰减，不代表Iout被清除
 imu.Gyro[2] = -0.001f;
 Tick(true, true);
 assert(sentry_big_yaw_speed_target_rad_s == 0);
 assert(fabsf(sentry_big_yaw_world_target_rad-imu.YawTotalAngle*FW_DEG_TO_RAD)<0.0001f);
 /* Exit switches back to shortest-path mechanical front, no stale world integral. */
 motor.measure.total_position = SENTRY_BIG_YAW_FRONT_ALIGN_RAD + (FW_TWO_PI + 0.2f) * SENTRY_BIG_YAW_TRANSMISSION_RATIO;
 sentry_yaw_chassis_cmd.chassis_mode = CHASSIS_NO_FOLLOW;
 imu.Gyro[2] = 0;
 Tick(true, true);
 assert(!sentry_big_yaw_world_hold_active && motor.torque == 0);
 for (int n=0; n<5; ++n) Tick(true, true);
 assert(motor.torque < 0);
 assert(fabsf(sentry_big_yaw_target_rad - (SENTRY_BIG_YAW_FRONT_ALIGN_RAD/SENTRY_BIG_YAW_TRANSMISSION_RATIO+FW_TWO_PI)) < 0.0001f);
 /* Frozen INS while spinning stops and latches; fresh samples alone cannot restart. */
 Start(true, 10);
 for (int n=0; n<6; ++n) Tick(false, true);
 assert(!motor.enabled && motor.torque == 0 && sentry_big_yaw_rearm_required);
 Tick(true, true);
 assert(!motor.enabled);
 Start(true, 91);
 assert(motor.enabled && fabsf(sentry_big_yaw_world_target_rad-91*FW_DEG_TO_RAD)<0.0001f);
 /* Low-32 counter rollover is a fresh sample, never a false startup/fault. */
 ekf_ins.UpdateCount = UINT32_MAX;
 Tick(false, true);
 assert(sentry_big_yaw_imu_online && motor.enabled);
 Tick(true, true);
 assert((uint32_t)ekf_ins.UpdateCount == 0 && sentry_big_yaw_imu_online && motor.enabled);
 Tick(true, true);
 assert(sentry_big_yaw_imu_online && motor.enabled);
 /* Re-entry reports current heading; no historic anchor enters the speed loop. */
 Start(true, -42);
 assert(fabsf(sentry_big_yaw_world_target_rad + 42*FW_DEG_TO_RAD)<0.0001f);
 /* Invalid INS / encoder, dropped feedback, both command stop paths. */
 imu.YawTotalAngle = NAN;
 Tick(true, true);
 assert(!motor.enabled && motor.torque == 0);
 Start(true, 20);
 imu.Gyro[2] = INFINITY;
 Tick(true, true);
 assert(!motor.enabled && motor.torque == 0);
 Start(false, 20);
 motor.measure.total_position = INFINITY;
 Tick(true, true);
 assert(!motor.enabled && motor.torque == 0);
 Start(true, 20);
 motor.rx_refresh_dt = SENTRY_BIG_YAW_MAX_FEEDBACK_AGE_CYCLES;
 Tick(true, true);
 assert(!motor.enabled);
 Start(true, 20);
 for (int n=0; n<42; ++n) Tick(true, false);
 assert(!motor.enabled && motor.torque == 0);
 Start(true, 20);
 sentry_yaw_chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
 Tick(true, true);
 assert(!motor.enabled && !sentry_big_yaw_world_hold_active);
 Start(true, 20);
 gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
 Tick(true, true);
 assert(!motor.enabled && motor.torque == 0);
 puts("PASS: actual big-yaw task speed-only mode, no heading chase, synthetic 300deg/s encoder motion, mode edges, direction, stale/invalid INS, feedback loss and stop paths");
}
'''
out = root / "tests/build/sentry_world_task_test.c"
out.parent.mkdir(exist_ok=True)
out.write_text(prefix + state + function("gimbal_init_controller") + function("gimbal_front_align_rad") + function("gimbal_big_yaw_task") + tests, encoding="utf-8")
include_dirs = ["tests/native_stubs", "config/sentry_2026", "apps/common/app", "apps/sentry/app", "apps/sentry/gimbal", "framework", "framework/msg", "apps/sentry/cmd", "framework/kernel", "components/control", "components/math", "ports/stm32f4"]
exe = out.with_suffix(".exe")
subprocess.run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", *["-I"+d for d in include_dirs], str(out), "apps/sentry/gimbal/sentry_control.c", "components/control/controller.c", "-lm", "-o", str(exe)], cwd=root, check=True)
subprocess.run([str(exe)], cwd=root, check=True)
