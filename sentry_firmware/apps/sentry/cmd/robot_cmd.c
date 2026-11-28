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
#include "robot_cmd_internal.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "msg.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "telemetry.h"
#include "dwt_port.h"
#include "log_port.h"
#include "vision_link.h"
#include "autonomy.h"
#include <math.h>

#define CMD_SCAN_DT_S 0.005f
#define CMD_TWO_PI 6.28318530718f

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

float nuc_raw_yaw_target;
uint8_t yaw_recentering = 0;
uint16_t recenter_cnt = 0;
kbm_input_source_e input_source = INPUT_RC;
uint8_t auto_aim_active = 0;       // 左中挡或键鼠 Shift 选中自瞄
uint8_t auto_aim_fire_request = 0; // 视觉 shoot 请求(仍需热量守卫放行)

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

float cmd_get_nuc_raw_yaw(void)
{
    return nuc_raw_yaw_target;
}

uint8_t cmd_get_fire_tier(void)
{
    return (uint8_t)fire_tier; // 0=ECO 1=NORMAL 2=BURST
}

uint8_t cmd_get_auto_aim_active(void)
{
    return auto_aim_active;
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
    gimbal_cmd.pitch_abs = 0.0f;
    gimbal_cmd.small_yaw_rate_deg_s = 0.0f;
    gimbal_cmd.small_yaw_rel = 0.0f;
    gimbal_cmd.mode = GIMBAL_ZERO_FORCE;
    gimbal_cmd.aim_valid = 0u;

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
    chassis_cmd.offset_angle = 0.0f; // body-fixed translation, independent chassis steering
}

static void cmd_match_auto_set(void)
{
    const int aim_ctrl = auto_aim_active ? cmd_auto_aim_set() : 0;
    chassis_cmd.chassis_mode = SENTRY_MATCH_SPIN_ENABLE ? CHASSIS_ROTATE : CHASSIS_NO_FOLLOW;
    chassis_cmd.vx = 0.0f;
    chassis_cmd.vy = 0.0f;
    chassis_cmd.wz = 0.0f;
    gimbal_cmd.mode = aim_ctrl ? GIMBAL_AUTO_AIM_MODE : GIMBAL_GYRO_MODE;
    gimbal_cmd.small_yaw_rate_deg_s = 0.0f;
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.fire_tier = cmd_fire_tier_get();
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    if (aim_ctrl)
    {
        const uint8_t fire = (uint8_t)(SENTRY_AUTO_AIM_FIRE_ENABLE && auto_aim_fire_request);
        shoot_cmd.friction_mode = fire ? FRICTION_ON : FRICTION_OFF;
        shoot_cmd.load_mode = fire ? LOADER_BURST : LOADER_STOP;
    }
    else
    {
        shoot_cmd.friction_mode = FRICTION_OFF;
        shoot_cmd.load_mode = LOADER_STOP;
    }
}

static void cmd_match_small_yaw_scan_update(uint8_t match_active)
{
    static uint8_t scan_initialized;
    static float scan_phase;
    static float scan_dir = 1.0f;

    const uint8_t manual_small_yaw =
        (uint8_t)(remote_control_is_online() && input_source == INPUT_RC &&
                  REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_left) &&
                  MATH_ABS((int)rc_data[RC_SLOT_CURRENT].rc.rocker_l_) >= CHASSIS_RC_DEADBAND);
    const uint8_t scan_active =
        (uint8_t)(match_active && robot_state == ROBOT_STATUS_READY &&
                  gimbal_cmd.mode == GIMBAL_GYRO_MODE && !manual_small_yaw);
    if (!scan_active)
    {
        scan_initialized = 0u;
        return;
    }

    const float amp = SENTRY_MATCH_SCAN_AMP_DEG;
    const float max_rate = SENTRY_MATCH_SCAN_MAX_RATE_DEG_S;
    if (amp <= 1.0f || max_rate <= 1.0f)
        return;
    const float omega = max_rate / amp;

    if (!scan_initialized)
    {
        float ratio = gimbal_fb_recv.small_yaw_rel_deg / amp;
        MOTOR_LIMIT_MIN_MAX(ratio, -1.0f, 1.0f);
        scan_phase = asinf(ratio);
        if (scan_dir < 0.0f)
            scan_phase = (0.5f * CMD_TWO_PI) - scan_phase;
        scan_initialized = 1u;
    }

    scan_phase += omega * CMD_SCAN_DT_S;
    if (scan_phase >= CMD_TWO_PI)
        scan_phase -= CMD_TWO_PI;
    scan_dir = (cosf(scan_phase) >= 0.0f) ? 1.0f : -1.0f;

    const float desired_deg = amp * sinf(scan_phase);
    const float ff_rate = amp * omega * cosf(scan_phase);
    float rate = ff_rate + SENTRY_MATCH_SCAN_TRACK_KP * (desired_deg - gimbal_fb_recv.small_yaw_target_deg);
    MOTOR_LIMIT_MIN_MAX(rate, -max_rate, max_rate);
    gimbal_cmd.small_yaw_rate_deg_s = rate;
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

    /* 4. 输入源与自瞄选择(与步兵一致:左中挡 / 键鼠 Shift) */
    cmd_update_input_source();
    const uint8_t match_active = autonomy_is_match_started();
    {
        const uint8_t dial_auto_aim =
            REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left);
        const uint8_t kbm_auto_aim = cmd_kbm_auto_aim_held();
        auto_aim_active =
            (uint8_t)(SENTRY_AUTO_AIM_ENABLE &&
                      (match_active || dial_auto_aim ||
                       (input_source == INPUT_KBM && kbm_auto_aim)));
        vision_link_set_mode(auto_aim_active ? 1u : 0u);
    }
    if (input_source == INPUT_KBM)
        cmd_keyboard_mouse_set();
    else if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left) ||
             REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_left))
        cmd_remote_set();
    else if (match_active && !SENTRY_AUTH_RC_LOST_STOP && !remote_control_is_online())
        cmd_match_auto_set();

    /* 5. 手动回中:键鼠 F 或小陀螺切回跟随触发;解锁不再触发(DEV-20261126-05) */
#if SENTRY_GIMBAL_RECENTER_ENABLE
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

    /* 导航只接管底盘;遥控安全仲裁仍在最后执行。右杆推动可立即回手动。
     * 只有收到有效导航命令才覆盖模式:无效时保持手动(避免 NUC 掉线把底盘锁成零力)。 */
    if (SENTRY_AUTONOMY_ENABLE &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left) &&
        REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right) &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) <= CHASSIS_RC_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) <= CHASSIS_RC_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r_) <= CHASSIS_RC_DEADBAND &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_r1) <= CHASSIS_RC_DEADBAND)
    {
        sentry_auto_cmd_t navigation;
        if (autonomy_get_command(&navigation))
        {
            chassis_cmd.chassis_mode = CHASSIS_NAVIGATION;
            chassis_cmd.vx = navigation.vx;
            chassis_cmd.vy = navigation.vy;
            chassis_cmd.wz = navigation.wz;
            yaw_recentering = 0u;
        }
    }

    cmd_match_small_yaw_scan_update(match_active);

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
