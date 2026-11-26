/**
 * @file    robot_cmd.c
 * @brief   哨兵 cmd 应用:输入汇聚、仲裁调用与三话题发布
 * @layer   APP
 * @author  小猫大王
 * @note    职责:读遥控器 → 按固定优先级裁决控制权(失联 > 急停挡 > 拨轮急停/锁存 > 运行)
 *          → 生成底盘/三轴云台/发射命令 → 发布给 chassis / gimbal / shoot。
 * @note    三轴分配:cmd 只维护**世界 yaw 目标**;gimbal 内部把小 yaw 能消化的误差
 *          交给小 yaw,剩余误差由大 yaw 世界环承担。
 * @note    自主输入接口:autonomy 模块预留;启用后在本任务内按"自主 > 视觉 > 遥控"仲裁。
 */
// app
#include "robot_def.h"

robot_status_e robot_state; // 机器人整体工作状态
#include "robot_cmd.h"
#include "autonomy.h"
#include "robot_cmd_internal.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "ax650_usb.h"
#include "msg.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "telemetry.h"
#include "dwt_port.h"
#include "log_port.h"
#include <math.h>

/* ---------------- cmd 模块实例与消息 ---------------- */

static msg_pub_t chassis_cmd_pub;
static msg_pub_t gimbal_cmd_pub;
static msg_pub_t shoot_cmd_pub;
static msg_sub_t chassis_feed_sub;
static msg_sub_t gimbal_feed_sub;
static msg_sub_t shoot_feed_sub;

chassis_ctrl_cmd_t chassis_cmd;      // 发给底盘
sentry_gimbal_cmd_t gimbal_cmd;      // 发给三轴云台
shoot_ctrl_cmd_t shoot_cmd;          // 发给发射
sentry_chassis_feed_t chassis_fb_recv;
sentry_gimbal_feed_t gimbal_fb_recv;
sentry_shoot_feed_t shoot_fb_recv;

remote_rc_ctrl_t *rc_data;

float ax650_raw_yaw_target;
uint8_t yaw_recentering = 0;
uint16_t recenter_cnt = 0;
kbm_input_source_e input_source = INPUT_RC;

static fire_tier_e fire_tier = FIRE_TIER_NORMAL;

void cmd_fire_tier_cycle(void)
{
    fire_tier = (fire_tier_e)(((uint8_t)fire_tier + 1u) % 3u);
    const char *name = (fire_tier == FIRE_TIER_ECO)     ? "ECO"
                       : (fire_tier == FIRE_TIER_BURST) ? "BURST"
                                                        : "NORMAL";
    LOG_INFO("[CMD] fire tier -> %s", name);
}

fire_tier_e cmd_fire_tier_get(void)
{
    return fire_tier;
}

float cmd_fire_tier_rate(void)
{
    switch (fire_tier)
    {
    case FIRE_TIER_ECO:
        return SHOOT_RATE_ECO;
    case FIRE_TIER_BURST:
        return SHOOT_RATE_BURST;
    default:
        return SHOOT_RATE_DEFAULT;
    }
}

float cmd_get_ax650_raw_yaw(void)
{
    return ax650_raw_yaw_target;
}

uint8_t cmd_get_fire_tier(void)
{
    return (uint8_t)fire_tier; // 0=ECO 1=NORMAL 2=BURST
}

void cmd_init(void)
{
    rc_data = remote_control_init(&huart3); // C 板 DBUS 在 USART3(带反相器)

    gimbal_cmd_pub = msg_pub("gimbal_cmd", sizeof(sentry_gimbal_cmd_t));
    gimbal_feed_sub = msg_sub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
    shoot_cmd_pub = msg_pub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));
    shoot_feed_sub = msg_sub("shoot_feed", sizeof(sentry_shoot_feed_t));
    chassis_cmd_pub = msg_pub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    chassis_feed_sub = msg_sub("chassis_feed", sizeof(sentry_chassis_feed_t));

    /* 上电安全态:全部零力,右上挡回中解锁 */
    chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
    chassis_cmd.vx = 0.0f;
    chassis_cmd.vy = 0.0f;
    chassis_cmd.wz = 0.0f;
    chassis_cmd.offset_angle = 0.0f;
    chassis_cmd.yaw_rate_cmd = 0.0f;

    gimbal_cmd.big_yaw = 0.0f;
    gimbal_cmd.pitch = 0.0f;
    gimbal_cmd.small_yaw_rel = 0.0f;
    gimbal_cmd.mode = GIMBAL_ZERO_FORCE;

    shoot_cmd.shoot_mode = SHOOT_OFF;
    shoot_cmd.load_mode = LOADER_STOP;
    shoot_cmd.friction_mode = FRICTION_OFF;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    shoot_cmd.fire_tier = fire_tier;

    robot_state = ROBOT_STATUS_STOP;
}

/**
 * @brief 由云台回传的大 yaw 相对角更新底盘跟随 offset,折回 ±180
 */
static void cmd_calc_offset_angle(void)
{
    float angle = gimbal_fb_recv.big_yaw_rel_deg;
    chassis_cmd.offset_angle = angle;
    if (chassis_cmd.offset_angle > 180.0f)
        chassis_cmd.offset_angle -= 360.0f;
    else if (chassis_cmd.offset_angle < -180.0f)
        chassis_cmd.offset_angle += 360.0f;
}

void cmd_task(void)
{
    /* 1. 接收三路反馈 */
    msg_recv(chassis_feed_sub, (void *)&chassis_fb_recv);
    msg_recv(gimbal_feed_sub, (void *)&gimbal_fb_recv);
    msg_recv(shoot_feed_sub, (void *)&shoot_fb_recv);

    /* 2. 跟随 offset */
    cmd_calc_offset_angle();

    /* 3. 操作手意图角速度:世界 yaw 目标差分,给底盘做提前刹车。
     *    来源是目标角差分;自瞄/回正跳变时限幅并低通。 */
    {
        static float yaw_last = 0.0f;
        static float rate_lp = 0.0f;
        float raw = (gimbal_cmd.big_yaw - yaw_last) / 0.005f; // 200Hz
        yaw_last = gimbal_cmd.big_yaw;
        MOTOR_LIMIT_MIN_MAX(raw, -600.0f, 600.0f);
        rate_lp += 0.35f * (raw - rate_lp);
        chassis_cmd.yaw_rate_cmd = rate_lp;
    }

    /* 4. 输入源与 AX650 模式 */
    cmd_update_input_source();
#if AX650_USB_ENABLE
    ax650_usb_set_mode(REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left)
                           ? AX650_AUTO_AIM_MODE
                           : AX650_TX_MODE);
#endif
    if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left))
        cmd_remote_set();
    else if (input_source == INPUT_KBM)
        cmd_keyboard_mouse_set();
    else if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_left))
        cmd_remote_set();

    /* 5. 解锁回中:只动底盘,云台世界目标保持不变(与步兵同语义) */
#if SENTRY_GIMBAL_RECENTER_ENABLE
    const uint8_t auto_aim_active = REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left);
    if (auto_aim_active)
    {
        yaw_recentering = 0;
        recenter_cnt = 0;
    }
    {
        static uint8_t last_chassis_mode_cmd = CHASSIS_FOLLOW_GIMBAL_YAW;
        if (!auto_aim_active && last_chassis_mode_cmd == CHASSIS_ROTATE &&
            chassis_cmd.chassis_mode == CHASSIS_FOLLOW_GIMBAL_YAW)
        {
            yaw_recentering = 1;
            recenter_cnt = 0;
            LOG_INFO("[CMD] spin->follow, big-yaw align");
        }
        last_chassis_mode_cmd = chassis_cmd.chassis_mode;
    }
    if (!auto_aim_active && robot_state == ROBOT_STATUS_READY && yaw_recentering)
    {
        if (fabsf(chassis_cmd.offset_angle) < CHASSIS_FOLLOW_DEADBAND)
            yaw_recentering = 0;
        else if (++recenter_cnt > 600u)
            yaw_recentering = 0;
        else
            chassis_cmd.chassis_mode = CHASSIS_ALIGN;
    }
#endif

    /* 导航只接管底盘;遥控安全仲裁仍在最后执行。右杆推动可回手动。 */
    if (SENTRY_AUTONOMY_ENABLE &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left) &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right) &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r_) <= CHASSIS_RC_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r1) <= CHASSIS_RC_DEADBAND)
    {
        sentry_auto_cmd_t navigation;
        int valid = autonomy_get_command(&navigation);
        chassis_cmd.chassis_mode = valid ? CHASSIS_NAVIGATION : CHASSIS_ZERO_FORCE;
        chassis_cmd.vx = valid ? navigation.vx : 0.0f;
        chassis_cmd.vy = valid ? navigation.vy : 0.0f;
        chassis_cmd.wz = valid ? navigation.wz : 0.0f;
        yaw_recentering = 0u;
    }

    /* 6. 仲裁 */
    cmd_authority_arbitrate();

#if SENTRY_CHASSIS_FORCE_DISABLE
    /* 台架安全锁:只锁底盘,云台/发射保持可用 */
    chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
    chassis_cmd.vx = 0.0f;
    chassis_cmd.vy = 0.0f;
    chassis_cmd.wz = 0.0f;
#endif

    /* 7. 发布 */
    msg_publish(chassis_cmd_pub, (void *)&chassis_cmd);
    msg_publish(gimbal_cmd_pub, (void *)&gimbal_cmd);
    msg_publish(shoot_cmd_pub, (void *)&shoot_cmd);

    /* 8. 底盘命令遥测 */
    telemetry_push(&telemetry_chassis_cmd, chassis_cmd.vx, 0.0f, chassis_cmd.wz,
                   chassis_cmd.offset_angle, (uint8_t)chassis_cmd.chassis_mode);
}
