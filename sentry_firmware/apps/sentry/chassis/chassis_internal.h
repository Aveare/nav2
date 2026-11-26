/**
 * @file    chassis_internal.h
 * @brief   哨兵底盘内部几何/运动学接口(仅本模块)
 * @layer   APP
 * @author  小猫大王
 * @note    正逆解必须共用同一组几何宏,否则几何一改就会静默走偏。
 */
#ifndef SENTRY_CHASSIS_INTERNAL_H
#define SENTRY_CHASSIS_INTERNAL_H

#include "robot_def.h"
#include "dji_motor.h"
#include "general_def.h"

/* ---------------- 轮系几何(来自 config 的物理量派生) ---------------- */
#define SENTRY_CHASSIS_HALF_WHEEL_BASE  (CHASSIS_WHEEL_BASE / 2.0f)
#define SENTRY_CHASSIS_HALF_TRACK_WIDTH (CHASSIS_TRACK_WIDTH / 2.0f)
#define SENTRY_CHASSIS_WHEEL_PERIMETER  (CHASSIS_WHEEL_RADIUS * 2 * FW_PI)
#define SENTRY_CHASSIS_WHEEL_RATIO      (360.0f * CHASSIS_WHEEL_REDUCTION / SENTRY_CHASSIS_WHEEL_PERIMETER)

#define SENTRY_CHASSIS_LF_CENTER                                                                              \
    ((SENTRY_CHASSIS_HALF_TRACK_WIDTH + CHASSIS_GIMBAL_OFFSET_X + SENTRY_CHASSIS_HALF_WHEEL_BASE -                   \
      CHASSIS_GIMBAL_OFFSET_Y) *                                                                       \
     FW_DEG_TO_RAD)
#define SENTRY_CHASSIS_RF_CENTER                                                                              \
    ((SENTRY_CHASSIS_HALF_TRACK_WIDTH - CHASSIS_GIMBAL_OFFSET_X + SENTRY_CHASSIS_HALF_WHEEL_BASE -                   \
      CHASSIS_GIMBAL_OFFSET_Y) *                                                                       \
     FW_DEG_TO_RAD)
#define SENTRY_CHASSIS_LB_CENTER                                                                              \
    ((SENTRY_CHASSIS_HALF_TRACK_WIDTH + CHASSIS_GIMBAL_OFFSET_X + SENTRY_CHASSIS_HALF_WHEEL_BASE +                   \
      CHASSIS_GIMBAL_OFFSET_Y) *                                                                       \
     FW_DEG_TO_RAD)
#define SENTRY_CHASSIS_RB_CENTER                                                                              \
    ((SENTRY_CHASSIS_HALF_TRACK_WIDTH - CHASSIS_GIMBAL_OFFSET_X + SENTRY_CHASSIS_HALF_WHEEL_BASE +                   \
      CHASSIS_GIMBAL_OFFSET_Y) *                                                                       \
     FW_DEG_TO_RAD)

/* ---------------- 共享状态(定义在 chassis.c) ---------------- */
extern dji_motor_t *motor_lf, *motor_rf, *motor_lb, *motor_rb;
extern chassis_ctrl_cmd_t chassis_cmd_recv;
extern float chassis_vx, chassis_vy;
extern float vt_lf, vt_rf, vt_lb, vt_rb;
extern float est_vx, est_vy, est_wz;

/* ---------------- 运动学纯函数 ---------------- */
float chassis_ramp_to(float cur, float target, float accel, float dt);
void chassis_mecanum_calculate(void);
void chassis_limit_output(void);
void chassis_estimate_speed(void);

#endif // SENTRY_CHASSIS_INTERNAL_H
