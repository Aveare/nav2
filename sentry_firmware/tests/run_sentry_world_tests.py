"""Extract the actual big-yaw task and run it with real cascade PID and host IO stubs."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "application/gimbal/gimbal.c").read_text(encoding="utf-8")

def function(name):
    start = source.index("static void " + name + "(")
    brace = source.index("{", start)
    level = 1
    end = brace + 1
    while level:
        level += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

state_start = source.index("static DMMotorInstance *sentry_big_yaw_motor;")
state = source[state_start:source.index("#endif", state_start)]
prefix = r'''#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "robot_def.h"
#include "sentry_control.h"
#include "cmd_watchdog.h"
#include "ins_task.h"
#define PI 3.14159265358979323846f
#define PI2 (2.0f * PI)
#define DEGREE_2_RAD (PI / 180.0f)
#define DM_TASK_CYCLE 1
#define DM_STATE_ENABLE 1
#define CMD_WATCHDOG_TIMEOUT_MS 200.0f
/* Only peripheral IO is stubbed; task selection, guards and real PID are unmodified. */
typedef int Subscriber_t;
typedef struct {
 struct { float total_position, velocity; uint8_t state; } measure;
 uint8_t rx_init_flag; int rx_refresh_dt; float torque; bool enabled;
} DMMotorInstance;
static DMMotorInstance motor;
static attitude_t imu;
static attitude_t *gimba_IMU_data = &imu;
static Gimbal_Ctrl_Cmd_s gimbal_cmd_recv;
static struct { uint64_t UpdateCount; uint8_t Initialized; } QEKF_INS;
static uint32_t test_us = 1000000u;
float DWT_GetDeltaT(uint32_t *last) {
 float dt = (test_us - *last) * 0.000001f; *last = test_us; return dt;
}
uint32_t DWT_GetTimeline_ms(void) { return test_us / 1000u; }
static void DMMotorStop(DMMotorInstance *m) { m->enabled = false; }
static void DMMotorEnable(DMMotorInstance *m) { m->enabled = true; }
static void DMMotorSetMITTorque(DMMotorInstance *m, float t) { m->torque = t; }
'''
tests = r'''
static void Tick(bool update_imu, bool update_chassis) {
 test_us += 5000u;
 float now = test_us * 0.001f;
 if (update_imu) ++QEKF_INS.UpdateCount;
 if (update_chassis) CmdWatchdogFeed(&sentry_yaw_chassis_wd, now);
 SentryBigYawTask(now);
 assert(isfinite(motor.torque));
 assert(fabsf(motor.torque) <= SENTRY_BIG_YAW_TORQUE_NM + 0.001f);
}
static void Start(bool spin, float world_deg) {
 gimbal_cmd_recv.gimbal_mode = GIMBAL_ZERO_FORCE;
 Tick(true, true);
 memset(&imu, 0, sizeof(imu));
 imu.YawTotalAngle = world_deg;
 motor.measure.total_position = SENTRY_BIG_YAW_FRONT_ALIGN_RAD;
 motor.measure.velocity = 0;
 motor.measure.state = DM_STATE_ENABLE;
 motor.rx_init_flag = 1;
 motor.rx_refresh_dt = 0;
 QEKF_INS.Initialized = 1;
 sentry_yaw_chassis_cmd.chassis_mode = spin ? CHASSIS_ROTATE : CHASSIS_RC_CONTROL;
 gimbal_cmd_recv.gimbal_mode = GIMBAL_GYRO_MODE;
 Tick(true, true);
}
int main(void) {
 sentry_big_yaw_motor = &motor;
 assert(sentry_yaw_chassis_sub == NULL);
 InitSentryController(&sentry_yaw_controller, true);
 /* Initialized alone with no update must not enable world hold. */
 QEKF_INS.Initialized = 1;
 sentry_yaw_chassis_cmd.chassis_mode = CHASSIS_ROTATE;
 gimbal_cmd_recv.gimbal_mode = GIMBAL_GYRO_MODE;
 motor.measure.state = DM_STATE_ENABLE;
 motor.rx_init_flag = 1;
 Tick(false, true);
 assert(!sentry_big_yaw_imu_online && !motor.enabled);
 /* Spin has zero world-speed target; relative encoder motion must not create torque. */
 Start(true, 37.0f);
 assert(sentry_big_yaw_world_hold_active && motor.enabled && motor.torque == 0);
 float anchor = sentry_big_yaw_world_target_rad;
 for (int n=0; n<1500; ++n) {
  motor.measure.total_position -= 300.0f * DEGREE_2_RAD * 0.005f;
  motor.measure.velocity = -300.0f * DEGREE_2_RAD;
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
  assert(fabsf(sentry_big_yaw_world_target_rad-headings[i]*DEGREE_2_RAD)<0.0001f);
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
 /* A real speed pulse may leave PI integral; it must not create a heading target. */
 Start(true, 0);
 imu.Gyro[2] = 0.4f;
 for (int n=0; n<120; ++n) {
  imu.YawTotalAngle += 0.4f * 0.005f / DEGREE_2_RAD;
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
 assert(fabsf(sentry_big_yaw_world_target_rad-imu.YawTotalAngle*DEGREE_2_RAD)<0.0001f);
 /* Exit switches back to shortest-path mechanical front, no stale world integral. */
 motor.measure.total_position = SENTRY_BIG_YAW_FRONT_ALIGN_RAD + PI2 + 0.2f;
 sentry_yaw_chassis_cmd.chassis_mode = CHASSIS_RC_CONTROL;
 imu.Gyro[2] = 0;
 Tick(true, true);
 assert(!sentry_big_yaw_world_hold_active && motor.torque == 0);
 for (int n=0; n<5; ++n) Tick(true, true);
 assert(motor.torque < 0);
 assert(fabsf(sentry_big_yaw_target_rad - (SENTRY_BIG_YAW_FRONT_ALIGN_RAD+PI2)) < 0.0001f);
 /* Frozen INS while spinning stops and latches; fresh samples alone cannot restart. */
 Start(true, 10);
 for (int n=0; n<6; ++n) Tick(false, true);
 assert(!motor.enabled && motor.torque == 0 && sentry_big_yaw_rearm_required);
 Tick(true, true);
 assert(!motor.enabled);
 Start(true, 91);
 assert(motor.enabled && fabsf(sentry_big_yaw_world_target_rad-91*DEGREE_2_RAD)<0.0001f);
 /* Low-32 counter rollover is a fresh sample, never a false startup/fault. */
 QEKF_INS.UpdateCount = UINT32_MAX;
 Tick(false, true);
 assert(sentry_big_yaw_imu_online && motor.enabled);
 Tick(true, true);
 assert((uint32_t)QEKF_INS.UpdateCount == 0 && sentry_big_yaw_imu_online && motor.enabled);
 Tick(true, true);
 assert(sentry_big_yaw_imu_online && motor.enabled);
 /* Re-entry reports current heading; no historic anchor enters the speed loop. */
 Start(true, -42);
 assert(fabsf(sentry_big_yaw_world_target_rad + 42*DEGREE_2_RAD)<0.0001f);
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
 gimbal_cmd_recv.gimbal_mode = GIMBAL_ZERO_FORCE;
 Tick(true, true);
 assert(!motor.enabled && motor.torque == 0);
 puts("PASS: actual big-yaw task speed-only mode, no heading chase, synthetic 300deg/s encoder motion, mode edges, direction, stale/invalid INS, feedback loss and stop paths");
}
'''
out = root / "tests/build/sentry_world_task_test.c"
out.parent.mkdir(exist_ok=True)
out.write_text(prefix + state + function("InitSentryController") + function("SentryBigYawTask") + tests, encoding="utf-8")
include_dirs = ["tests/native_stubs", "config", "application", "application/cmd", "application/gimbal", "modules/algorithm"]
exe = out.with_suffix(".exe")
subprocess.run(["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", *["-I"+d for d in include_dirs], str(out), "application/gimbal/sentry_control.c", "modules/algorithm/controller.c", "-lm", "-o", str(exe)], cwd=root, check=True)
subprocess.run([str(exe)], cwd=root, check=True)
