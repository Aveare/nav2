/**
 * @file    chassis_kinematics.c
 * @brief   哨兵麦克纳姆轮正逆解纯函数(无状态,可主机单测)
 * @layer   APP
 * @author  小猫大王
 * @note    45° 麦轮公式沿用步兵实车标定;哨兵若更换轮系/辊子方向,
 *          先改 config 的 CHASSIS_WHEEL_DIR_* 做方向标定,再考虑改公式。
 */
#include "robot_def.h"
#include "chassis.h"
#include "chassis_internal.h"
#include "dji_motor.h"
#include "general_def.h"
#include <math.h>

float chassis_ramp_to(float cur, float target, float accel, float dt)
{
    float step = accel * dt;
    float d = target - cur;
    if (d > step)
        d = step;
    if (d < -step)
        d = -step;
    return cur + d;
}

void chassis_mecanum_calculate(void)
{
    vt_lf = (-chassis_vx - chassis_vy - chassis_cmd_recv.wz * SENTRY_CHASSIS_LF_CENTER) * 0.70710678f *
            SENTRY_CHASSIS_WHEEL_RATIO;
    vt_rf = (chassis_vx - chassis_vy - chassis_cmd_recv.wz * SENTRY_CHASSIS_RF_CENTER) * 0.70710678f *
            SENTRY_CHASSIS_WHEEL_RATIO;
    vt_lb = (-chassis_vx + chassis_vy - chassis_cmd_recv.wz * SENTRY_CHASSIS_LB_CENTER) * 0.70710678f *
            SENTRY_CHASSIS_WHEEL_RATIO;
    vt_rb = (chassis_vx + chassis_vy - chassis_cmd_recv.wz * SENTRY_CHASSIS_RB_CENTER) * 0.70710678f *
            SENTRY_CHASSIS_WHEEL_RATIO;
}

void chassis_limit_output(float power_scale, float integral_scale)
{
    if (!(power_scale > 0.0f) || power_scale > 1.0f)
        power_scale = 1.0f;
    if (!(integral_scale >= 0.0f))
        integral_scale = 1.0f;
    if (integral_scale > 1.0f)
        integral_scale = 1.0f;

    /* 制动轮不参与功率限幅(与 chassis_power.c 的电动轮求和配套):
     * i 指令与转速反号 = 回馈制动,电功率为负,不占电池功率预算;
     * 旧实现四轮统一乘 scale,满刹车时模型误判大功率、scale≈下限,
     * 刹车力被削掉(步兵 DEV-20261127-200745 实车定性)。近零速(<30 deg/s)
     * 按电动处理,避免换向瞬间符号抖动导致 scale 跳变。 */
    dji_motor_t *motors[4] = {motor_lf, motor_rf, motor_lb, motor_rb};
    for (uint8_t k = 0u; k < 4u; ++k)
    {
        dji_motor_t *m = motors[k];
        const uint8_t braking = (fabsf(m->measure.speed_aps) >= 30.0f) &&
                                (m->current_cmd * m->measure.speed_aps < 0.0f);
        const float scale = braking ? 1.0f : power_scale;
        m->output_scale = scale;
        m->motor_controller.speed_PID.IntegralLimit =
            (float)CHASSIS_SPEED_PID_IL * scale * integral_scale;
    }

    float max_speed = fmaxf(fmaxf(fabsf(vt_lf), fabsf(vt_rf)), fmaxf(fabsf(vt_lb), fabsf(vt_rb)));
    float limit = CHASSIS_MAX_MOTOR_RPM * FW_RPM_TO_DEG_S;
    if (max_speed > limit)
    {
        float scale = limit / max_speed;
        vt_lf *= scale;
        vt_rf *= scale;
        vt_lb *= scale;
        vt_rb *= scale;
    }

    dji_motor_set_ref(motor_lf, vt_lf * CHASSIS_WHEEL_DIR_LF);
    dji_motor_set_ref(motor_rf, vt_rf * CHASSIS_WHEEL_DIR_RF);
    dji_motor_set_ref(motor_lb, vt_lb * CHASSIS_WHEEL_DIR_LB);
    dji_motor_set_ref(motor_rb, vt_rb * CHASSIS_WHEEL_DIR_RB);
}

void chassis_estimate_speed(void)
{
    /* 反馈是原始方向(输出方向由 WHEEL_DIR 在 set_ref 侧施加),逆解先乘回同一符号 */
    float u_lf = motor_lf->measure.speed_aps * CHASSIS_WHEEL_DIR_LF /
                 (SENTRY_CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_rf = motor_rf->measure.speed_aps * CHASSIS_WHEEL_DIR_RF /
                 (SENTRY_CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_lb = motor_lb->measure.speed_aps * CHASSIS_WHEEL_DIR_LB /
                 (SENTRY_CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_rb = motor_rb->measure.speed_aps * CHASSIS_WHEEL_DIR_RB /
                 (SENTRY_CHASSIS_WHEEL_RATIO * 0.70710678f);

    est_vx = ((u_rf + u_rb) - (u_lf + u_lb)) * 0.25f;
    est_vy = ((u_lb + u_rb) - (u_lf + u_rf)) * 0.25f;
    est_wz = -(u_lf + u_rf + u_lb + u_rb) / (4.0f * SENTRY_CHASSIS_LF_CENTER);
}
