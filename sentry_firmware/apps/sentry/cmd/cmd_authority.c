/**
 * @file    cmd_authority.c
 * @brief   哨兵控制权仲裁:失联/急停/拨轮/运行四级与安全输出
 * @layer   APP
 * @author  小猫大王
 * @note    优先级:遥控失联 > 急停挡 > 拨轮急停/锁存 > 正常运行。
 *          自主输入接入后,在 AUTH_RUN 内再按"自主 > 视觉 > 遥控"细分。
 */
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "ins_task.h"
#include "general_def.h"
#include "log_port.h"

typedef enum
{
    AUTH_RC_LOST = 0,
    AUTH_KILL_SWITCH,
    AUTH_STOP_LATCHED,
    AUTH_RUN,
} cmd_authority_source_e;

static cmd_authority_source_e authority_source;
static cmd_authority_source_e authority_source_last;
static uint8_t unlock_logged;

static void cmd_apply_safe_outputs(void)
{
    gimbal_cmd.mode = GIMBAL_ZERO_FORCE;
    gimbal_cmd.small_yaw_rate_deg_s = 0.0f;
    gimbal_cmd.aim_valid = 0u;
    auto_aim_fire_request = 0u;
    chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
    chassis_cmd.vx = 0.0f;
    chassis_cmd.vy = 0.0f;
    chassis_cmd.wz = 0.0f;
    shoot_cmd.shoot_mode = SHOOT_OFF;
    shoot_cmd.friction_mode = FRICTION_OFF;
    shoot_cmd.load_mode = LOADER_STOP;
}

void cmd_authority_arbitrate(void)
{
    // 1. 裁决
#if SENTRY_AUTH_RC_LOST_STOP
    if (!remote_control_is_online())
        authority_source = AUTH_RC_LOST;
    else
#endif
        if (REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_left) ||
            REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right))
        authority_source = AUTH_KILL_SWITCH;
    else if (rc_data[RC_SLOT_CURRENT].rc.dial > SHOOT_DIAL_STOP_THRESHOLD ||
             robot_state == ROBOT_STATUS_STOP)
        authority_source = AUTH_STOP_LATCHED;
    else
        authority_source = AUTH_RUN;

    // 2. 非 RUN 一律锁存 STOP
    if (authority_source != AUTH_RUN)
        robot_state = ROBOT_STATUS_STOP;

    // 3. 边沿日志
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

    // 4. 统一安全输出
    if (authority_source != AUTH_RUN)
        cmd_apply_safe_outputs();

    // 5. 解锁:右上 + 左中 + 全回中;解锁沿只重锚定世界 yaw / pitch
    if (remote_control_is_online() && REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right) &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left) &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.dial) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r_) <= PROTECT_AUTH_NEUTRAL_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r1) <= PROTECT_AUTH_NEUTRAL_DEADBAND)
    {
        robot_state = ROBOT_STATUS_READY;
        /* 停机期间手转/随动会积累世界角误差,解锁瞬间重锚定,避免角度环满输出猛甩 */
        gimbal_cmd.big_yaw = gimbal_fb_recv.big_yaw_world_deg;
        /* pitch 上电不回中:只把目标重锚定到当前关节角,避免解锁瞬间大幅动作。
         * (2026-11-23 教训:上电回水平在连杆未确认行程/符号时会把机构顶断) */
        gimbal_cmd.pitch = 0.0f; // gimbal captures the motor-shaft unlock position
        /* 解锁不请求底盘回中,右拨杆切到中挡后直接自由平移(DEV-20261126-05) */
        if (!unlock_logged)
        {
            LOG_INFO("[CMD] reinstate, robot ready");
            unlock_logged = 1u;
        }
    }
    else
        unlock_logged = 0u;
}
