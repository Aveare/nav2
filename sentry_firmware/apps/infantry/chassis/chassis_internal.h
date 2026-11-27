/**
 * @file    chassis_internal.h
 * @brief   底盘内部几何/限幅/运动学接口(仅本模块)
 * @layer   APP
 * @author  小猫大王
* @note    2026-11-20 拆分时建立。几何宏与共享状态集中在这里 / chassis.c,
*          正逆解两处必须共用同一份,否则几何一改就会静默走偏。
 */
#ifndef CHASSIS_INTERNAL_H
#define CHASSIS_INTERNAL_H

#include "robot_def.h"
#include "dji_motor.h"
#include "general_def.h"
#include "chassis.h"

/* ---------------- 轮系几何(来自 config 的物理量派生) ---------------- */
#define CHASSIS_HALF_WHEEL_BASE  (CHASSIS_WHEEL_BASE / 2.0f)     // 半轴距
#define CHASSIS_HALF_TRACK_WIDTH (CHASSIS_TRACK_WIDTH / 2.0f)    // 半轮距
#define CHASSIS_WHEEL_PERIMETER  (CHASSIS_WHEEL_RADIUS * 2 * FW_PI) // 轮子周长
#define CHASSIS_WHEEL_RATIO      (360.0f * CHASSIS_WHEEL_REDUCTION / CHASSIS_WHEEL_PERIMETER)

#define CHASSIS_LF_CENTER                                                                                  \
    ((CHASSIS_HALF_TRACK_WIDTH + CHASSIS_GIMBAL_OFFSET_X + CHASSIS_HALF_WHEEL_BASE - CHASSIS_GIMBAL_OFFSET_Y) *      \
     FW_DEG_TO_RAD)
#define CHASSIS_RF_CENTER                                                                                  \
    ((CHASSIS_HALF_TRACK_WIDTH - CHASSIS_GIMBAL_OFFSET_X + CHASSIS_HALF_WHEEL_BASE - CHASSIS_GIMBAL_OFFSET_Y) *      \
     FW_DEG_TO_RAD)
#define CHASSIS_LB_CENTER                                                                                  \
    ((CHASSIS_HALF_TRACK_WIDTH + CHASSIS_GIMBAL_OFFSET_X + CHASSIS_HALF_WHEEL_BASE + CHASSIS_GIMBAL_OFFSET_Y) *      \
     FW_DEG_TO_RAD)
#define CHASSIS_RB_CENTER                                                                                  \
    ((CHASSIS_HALF_TRACK_WIDTH - CHASSIS_GIMBAL_OFFSET_X + CHASSIS_HALF_WHEEL_BASE + CHASSIS_GIMBAL_OFFSET_Y) *      \
     FW_DEG_TO_RAD)

/* ---------------- 共享状态(定义在 chassis.c) ---------------- */
extern dji_motor_t *motor_lf, *motor_rf, *motor_lb, *motor_rb;
extern chassis_ctrl_cmd_t chassis_cmd_recv;
extern float chassis_vx, chassis_vy;     // 云台系投影到底盘系后的速度
extern float vt_lf, vt_rf, vt_lb, vt_rb; // 解算后的轮速,待限幅
extern float est_vx, est_vy, est_wz;     // 逆解估计(mm/s, mm/s, deg/s)

float chassis_ramp_to(float cur, float target, float accel, float dt);
float chassis_ramp_to_jerk(float cur, float target, float accel_max, float jerk_max, float dt,
                           float *accel_state);
void chassis_mecanum_calculate(void);
void chassis_limit_output(float power_scale, float integral_scale);
void chassis_estimate_speed(void);

#endif // CHASSIS_INTERNAL_H
