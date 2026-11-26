/**
 * @file    cmd_authority.c
 * @brief   控制权仲裁:失联/急停/拨轮/运行四级与安全输出
 * @layer   APP
 * @author  小猫大王
* @note    从 robot_cmd.c 拆出;authority_source 只在本文件使用。
 */
// app
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "ax650_uart1.h"
#include "general_def.h"
#include "dji_motor.h"
#include "gimbal.h"
#include "telemetry.h"
// bsp
#include "dwt_port.h"
#include "log_port.h"

/* -------------------------控制权仲裁-------------------------
 * 每周期按固定优先级裁决唯一控制来源,输出动作集中处理,
 * 避免多处if各自为政导致安全状态遗漏。
 */
typedef enum
{
    AUTH_RC_LOST = 0,  // 遥控失联:最高优先级,失联即零力,恢复后仍需原解锁流程
    AUTH_KILL_SWITCH,  // 急停挡位:左上/右上挡
    AUTH_STOP_LATCHED, // 拨轮急停或此前急停尚未解锁
    AUTH_RUN,          // 正常运行
} cmd_authority_source_e;

static cmd_authority_source_e authority_source;      // 本周期裁决结果
static cmd_authority_source_e authority_source_last; // 上周期结果,日志边沿触发用

/**
 * @brief 三路控制消息的统一安全态
 */
static void cmd_apply_safe_outputs(void)
{
    gimbal_cmd.gimbal_mode = GIMBAL_ZERO_FORCE;
    chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
    shoot_cmd.shoot_mode = SHOOT_OFF;
    shoot_cmd.friction_mode = FRICTION_OFF;
    shoot_cmd.load_mode = LOADER_STOP;
}

/**
 * @brief 控制权仲裁,随cmd_task以200Hz执行
 *        优先级: 遥控失联 > 急停挡位 > 拨轮急停/锁存 > 正常运行
 * @note  锁存语义:任一停止来源都进入ROBOT_STATUS_STOP,须右上+左中+全回中才解除;
 *        解除当周期仍保持零力。来源变化时打印一次(停机期间不刷屏)。
 */
void cmd_authority_arbitrate()
{
    // 1.按优先级裁决本周期控制来源
    if (!remote_control_is_online())
        authority_source = AUTH_RC_LOST;
    else if (REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_left) ||
             REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right))
        authority_source = AUTH_KILL_SWITCH;
    else if (rc_data[RC_SLOT_CURRENT].rc.dial > SHOOT_DIAL_STOP_THRESHOLD || robot_state == ROBOT_STATUS_STOP)
        authority_source = AUTH_STOP_LATCHED; // 拨轮急停,或此前急停未解锁
    else
        authority_source = AUTH_RUN;

    // 2.停止来源一律进入ROBOT_STATUS_STOP等待解锁
    if (authority_source != AUTH_RUN)
        robot_state = ROBOT_STATUS_STOP;

    // 3.来源变化时打印一次
    if (authority_source != authority_source_last)
    {
        switch (authority_source)
        {
        case AUTH_RC_LOST:
            LOG_ERROR("[CMD] authority: RC lost, zero force");
            break;
        case AUTH_KILL_SWITCH:
            LOG_ERROR("[CMD] authority: kill switch, zero force");
            break;
        case AUTH_STOP_LATCHED:
            LOG_ERROR("[CMD] authority: dial stop or latched, zero force");
            break;
        default:
            LOG_INFO("[CMD] authority: normal control");
            break;
        }
        authority_source_last = authority_source;
    }

    // 4.统一输出动作:非RUN一律安全态
    if (authority_source != AUTH_RUN)
        cmd_apply_safe_outputs();

    // 5.解锁流程:右上+左中+全回中解除锁存(右上挡本周期仍零力)
    if (remote_control_is_online() && REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right) &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left) &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.dial) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r_) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r1) <= PROTECT_AUTH_NEUTRAL_DEADBAND)
    {
        robot_state = ROBOT_STATUS_READY;
        /* 解锁沿先把yaw目标重锚定到当前IMU世界角:停机期间手转/随动会积累巨大
         * 世界角误差,解锁瞬间角度环会满输出slam;重锚定后由回中以90deg/s渐进回正 */
        gimbal_cmd.yaw = gimbal_fb_recv.gimbal_imu_data.YawTotalAngle;
        /* pitch 回正目标:2026-11-16 起改由 Flash 标定的机械水平零位(ECD)决定,
         * IMU 仅辅助判圈(见 components/calib)。未标定时返回 0,
         * 即仍保持"解锁回 IMU 水平"的旧行为(2026-10-30 实测:改成锚定当前角后
         * "回水平"消失,已恢复原版)。 */
        gimbal_cmd.pitch = gimbal_pitch_recenter_target();
#if GIMBAL_RECENTER_ENABLE
        yaw_recentering = 1; // 每次解锁触发一次回正前方
        recenter_cnt = 0;
#endif
        LOG_INFO("[CMD] reinstate, robot ready");
    }
}
