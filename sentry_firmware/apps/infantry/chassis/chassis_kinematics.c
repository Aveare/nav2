/**
 * @file    chassis_kinematics.c
 * @brief   麦克纳姆轮正逆解纯函数(无状态,可主机单测)
 * @layer   APP
 * @author  小猫大王
* @note    从 chassis.c 整段搬出(函数体未改);chassis_task 仍在 chassis.c,经 chassis_internal.h
* 调用。
 */
// app
#include "robot_def.h"
#include "chassis.h"
#include "chassis_internal.h"
// module
#include "dji_motor.h"
#include "ins_task.h"
#include "telemetry.h"
#include "general_def.h"
// bsp
#include "dwt_port.h"
#include "log_port.h"

#ifdef ONE_BOARD
/**
 * @brief 斜坡限速:单周期最大变化量=限加速度x dt
 * @note  消除阶跃指令在纯P速度环下的电流阶跃(起步冲击/急停)
 */
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

/**
 * @brief S 曲线斜坡:jerk 限幅的加速度,平滑起步/刹车
 * @param cur         当前值
 * @param target      目标值
 * @param accel_max   加速度上限
 * @param jerk_max    加速度变化率上限;<=0 退回线性斜坡
 * @param dt          周期,s
 * @param accel_state 加速度状态(调用方保存,首次置 0)
 * @return 下一拍值
 * @note  线性斜坡在起步/刹车时加速度阶跃,牵引峰值高;这里让加速度按 jerk
 *        平滑爬升/卸掉,并用"当前加速度卸到 0 还会走多远"预测刹车点防过冲。
 */
float chassis_ramp_to_jerk(float cur, float target, float accel_max, float jerk_max, float dt,
                           float *accel_state)
{
    if (!(dt > 0.0f) || accel_state == 0)
        return cur;
    if (!(jerk_max > 0.0f))
        return chassis_ramp_to(cur, target, accel_max, dt);

    const float err = target - cur;
    float a_des;
    if (fabsf(err) < 2.0f)
    {
        a_des = 0.0f; // 目标附近不再给加速度,避免正负反复换向
    }
    else
    {
        const float dv_stop = (*accel_state * *accel_state) / (2.0f * jerk_max);
        const float v_pred = cur + ((*accel_state > 0.0f) ? dv_stop : -dv_stop);
        a_des = ((target - v_pred) * err > 0.0f) ? ((err > 0.0f) ? accel_max : -accel_max) : 0.0f;
    }

    float da = a_des - *accel_state;
    const float da_max = jerk_max * dt;
    if (da > da_max)
        da = da_max;
    if (da < -da_max)
        da = -da_max;
    *accel_state += da;

    float next = cur + (*accel_state) * dt;
    if ((err > 0.0f && next > target) || (err < 0.0f && next < target))
        next = target;
    return next;
}
#endif

/**
 * @brief 45°全向轮正运动学解算:输入mm/s与deg/s,输出速度环所需转子deg/s
 * @note  vx/vy系数整体取反是实车标定结果(本车轮布局/电调拨码下的正方向);
 *        改动轮布局或拨码后须重新走方向标定流程
 */
void chassis_mecanum_calculate()
{
    vt_lf =
        (-chassis_vx - chassis_vy - chassis_cmd_recv.wz * CHASSIS_LF_CENTER) * 0.70710678f * CHASSIS_WHEEL_RATIO;
    vt_rf = (chassis_vx - chassis_vy - chassis_cmd_recv.wz * CHASSIS_RF_CENTER) * 0.70710678f * CHASSIS_WHEEL_RATIO;
    vt_lb =
        (-chassis_vx + chassis_vy - chassis_cmd_recv.wz * CHASSIS_LB_CENTER) * 0.70710678f * CHASSIS_WHEEL_RATIO;
    vt_rb = (chassis_vx + chassis_vy - chassis_cmd_recv.wz * CHASSIS_RB_CENTER) * 0.70710678f * CHASSIS_WHEEL_RATIO;
}

/**
 * @brief 四轮等比例限幅(保持平移与旋转比例)并设置电机参考值
 * @note  功率限制接口见 chassis_power 模块
 */
void chassis_limit_output(float power_scale, float integral_scale)
{
    /* 功率限幅直接压电流指令(304 口径,DEV-20261126-12):
     * 速度环输出 → output_scale → 发 CAN。只压 MaxOut 在稳态不饱和时无效
     * (稳态电流远低于 MaxOut);压电流指令才能让底盘真正降功率。
     * 同时按比例压低速度环积分限幅,防止电流被压后积分继续累积(windup)。
     * integral_scale 独立于功率:小陀螺平移→纯自转交接段压积分(DEV-20261126-28),
     * 削掉松杆瞬态里由平移段积累的 I 项电流,P 项保留。
     * 轮速目标与 MaxOut 不动:速度环仍按操作手目标算误差,由电流限幅自然减速。 */
    if (!(power_scale > 0.0f) || power_scale > 1.0f)
        power_scale = 1.0f;
    if (!(integral_scale >= 0.0f))
        integral_scale = 1.0f;
    if (integral_scale > 1.0f)
        integral_scale = 1.0f;

    /* 制动轮不参与功率限幅(DEV-20261127-200745):
     * i 指令与转速反号 = 回馈制动,电功率为负,不占电池功率预算。
     * 旧实现四轮统一乘 scale,2000 mm/s 满刹车时模型误判约 450W、scale≈0.20,
     * 刹车力被削掉 80%,实车表现就是松杆滑行、BRAKE_RAMP 加大也无效。
     * 近零速(<30 deg/s)按电动处理,避免换向瞬间符号抖动导致 scale 跳变。 */
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

    dji_motor_set_ref(motor_lf, vt_lf);
    dji_motor_set_ref(motor_rf, vt_rf);
    dji_motor_set_ref(motor_lb, vt_lb);
    dji_motor_set_ref(motor_rb, vt_rb);
}

/**
 * @brief 45°全向轮**逆**运动学:由四轮实测转速解算底盘真实速度(mm/s 与 deg/s)
 * @note  与 chassis_mecanum_calculate() 共用同一组几何宏(LF/RF/LB/CHASSIS_RB_CENTER)与 CHASSIS_WHEEL_RATIO,
 *        两处若各写一份,几何一改就会静默走偏。
 *        推导:正解里 `vt = (…- wz*L) * 0.7071 * CHASSIS_WHEEL_RATIO`,反解时先除掉 CHASSIS_WHEEL_RATIO*0.7071 得到
 *        四轮等效线速度 u,再用对称组合消元。**该消元假设云台居中**(四个 L 相等),
 *        设了 CENTER_GIMBAL_OFFSET 就必须重推,故下方有静态断言兜住。
 */
void chassis_estimate_speed()
{
    /* 实测轮速(转子 deg/s)→ 四轮等效线速度 mm/s */
    float u_lf = motor_lf->measure.speed_aps / (CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_rf = motor_rf->measure.speed_aps / (CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_lb = motor_lb->measure.speed_aps / (CHASSIS_WHEEL_RATIO * 0.70710678f);
    float u_rb = motor_rb->measure.speed_aps / (CHASSIS_WHEEL_RATIO * 0.70710678f);

    est_vx = ((u_rf + u_rb) - (u_lf + u_lb)) * 0.25f;
    est_vy = ((u_lb + u_rb) - (u_lf + u_rf)) * 0.25f;
    /* wz 单位 deg/s:CHASSIS_LF_CENTER 已含 FW_DEG_TO_RAD,与正解量纲一致 */
    est_wz = -(u_lf + u_rf + u_lb + u_rb) / (4.0f * CHASSIS_LF_CENTER);
}
