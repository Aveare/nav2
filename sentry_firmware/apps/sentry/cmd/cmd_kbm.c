/**
 * @file    cmd_kbm.c
 * @brief   哨兵键鼠输入:WASD/鼠标/按键映射与灵敏度调档
 * @layer   APP
 * @author  小猫大王
 * @note    鼠标 x/y 直接增量修改世界 yaw / pitch 目标;小 yaw 由 gimbal 内部分配。
 */
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "ins_task.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "dwt_port.h"
#include "log_port.h"

static float kbm_last_active_ms = 0.0f;
static uint8_t spin_enabled = 0;
static uint8_t last_q_count = 0;
static uint8_t last_r_count = 0;
static uint8_t last_e_count = 0;
static uint8_t last_f_count = 0;
static uint8_t last_v_count = 0;
static uint8_t last_b_count = 0;
static uint8_t last_g_count = 0;
static uint8_t last_x_count = 0;
static float kbm_sens_scale = 1.0f;
static float kbm_yaw_filt = 0.0f;
static float kbm_pitch_filt = 0.0f;

void cmd_update_input_source(void)
{
    const remote_rc_ctrl_t *rc = &rc_data[RC_SLOT_CURRENT];
    uint8_t mouse_active =
        (MATH_ABS(rc->mouse.x) > KBM_MOUSE_DEADZONE || MATH_ABS(rc->mouse.y) > KBM_MOUSE_DEADZONE ||
         rc->mouse.press_l || rc->mouse.press_r);
    uint8_t key_active = (rc->key[REMOTE_KEY_PRESS].keys != 0);
    const float now_ms = dwt_get_timeline_ms();

    if (mouse_active || key_active)
    {
        if (input_source != INPUT_KBM)
        {
            LOG_INFO("[CMD] input source: keyboard-mouse");
            last_q_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q];
            last_r_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R];
            last_e_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E];
            last_f_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F];
            last_v_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V];
            last_b_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B];
            last_g_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G];
            last_x_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X];
        }
        input_source = INPUT_KBM;
        kbm_last_active_ms = now_ms;
    }
    else if (input_source == INPUT_KBM && !spin_enabled &&
             now_ms - kbm_last_active_ms > KBM_TIMEOUT_MS)
    {
        input_source = INPUT_RC;
        LOG_INFO("[CMD] input source: remote control");
    }
}

/**
 * @brief 键鼠自瞄:Shift 按住且处于键鼠输入源、已解锁
 * @note  Shift 走位域而非 key_count[],直接读电平;松开立即退回手动。
 */
uint8_t cmd_kbm_auto_aim_held(void)
{
    const uint8_t shift_now =
        (rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].shift) ? 1u : 0u;
    return (shift_now && input_source == INPUT_KBM && robot_state == ROBOT_STATUS_READY) ? 1u : 0u;
}

void cmd_keyboard_mouse_set(void)
{
    const remote_rc_ctrl_t *rc = &rc_data[RC_SLOT_CURRENT];
    const kbm_key_t *key = &rc->key[REMOTE_KEY_PRESS];

    /* 底盘平移 */
    float speed = CHASSIS_MAX_SPEED * (key->shift ? KBM_SLOW_RATIO : 1.0f);
    float vx = 0.0f, vy = 0.0f;
    if (key->w)
        vx += speed;
    if (key->s)
        vx -= speed;
    if (key->a)
        vy += speed * KBM_STRAFE_DIR;
    if (key->d)
        vy -= speed * KBM_STRAFE_DIR;
    /* 斜向归一化:W+D 同时按不能超过 CHASSIS_MAX_SPEED。 */
    {
        const float norm = sqrtf(vx * vx + vy * vy);
        if (norm > CHASSIS_MAX_SPEED)
        {
            const float scale = CHASSIS_MAX_SPEED / norm;
            vx *= scale;
            vy *= scale;
        }
    }
    chassis_cmd.vx = vx;
    chassis_cmd.vy = vy;

    /* Q 小陀螺 */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q] != last_q_count)
    {
        spin_enabled = !spin_enabled;
        last_q_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q];
        LOG_INFO("[CMD] kb spin %s", spin_enabled ? "on" : "off");
    }
    chassis_cmd.chassis_mode = spin_enabled ? CHASSIS_ROTATE : CHASSIS_NO_FOLLOW;

    chassis_cmd.wz = 0.0f;
    gimbal_cmd.small_yaw_rate_deg_s = 0.0f;

    /* 灵敏度 V/B/G */
    uint8_t sens_changed = 0;
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V] != last_v_count)
    {
        last_v_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V];
        kbm_sens_scale *= KBM_SENS_SCALE_STEP;
        sens_changed = 1;
    }
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B] != last_b_count)
    {
        last_b_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B];
        kbm_sens_scale /= KBM_SENS_SCALE_STEP;
        sens_changed = 1;
    }
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G] != last_g_count)
    {
        last_g_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G];
        kbm_sens_scale = 1.0f;
        sens_changed = 1;
    }
    if (sens_changed)
    {
        MOTOR_LIMIT_MIN_MAX(kbm_sens_scale, KBM_SENS_SCALE_MIN, KBM_SENS_SCALE_MAX);
        char sens_str[16];
        log_float_to_str(sens_str, kbm_sens_scale);
        LOG_INFO("[CMD] kb sens x%s", sens_str);
    }

    /* X 射速档位 */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X] != last_x_count)
    {
        last_x_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X];
        cmd_fire_tier_cycle();
    }

    /* 云台:自瞄选中时由视觉统一解算;否则鼠标帧计数 → 世界 yaw/pitch 增量 */
    int aim_ctrl = 0;
    if (auto_aim_active)
    {
        kbm_yaw_filt = 0.0f;
        kbm_pitch_filt = 0.0f;
        aim_ctrl = cmd_auto_aim_set();
    }
    if (aim_ctrl)
    {
        gimbal_cmd.mode = GIMBAL_AUTO_AIM_MODE;
    }
    else if (robot_state == ROBOT_STATUS_READY && remote_control_is_online())
    {
        float mx = (MATH_ABS(rc->mouse.x) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.x : 0.0f;
        float my = (MATH_ABS(rc->mouse.y) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.y : 0.0f;
        kbm_yaw_filt += KBM_INPUT_LP * (mx - kbm_yaw_filt);
        kbm_pitch_filt += KBM_INPUT_LP * (my - kbm_pitch_filt);
        float yaw_rate = -kbm_yaw_filt * KBM_YAW_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        float pitch_rate = kbm_pitch_filt * KBM_PITCH_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        MOTOR_LIMIT_MIN_MAX(yaw_rate, -KBM_YAW_MAX_RATE, KBM_YAW_MAX_RATE);
        MOTOR_LIMIT_MIN_MAX(pitch_rate, -KBM_PITCH_MAX_RATE, KBM_PITCH_MAX_RATE);
        MOTOR_LIMIT_MIN_MAX(yaw_rate, -SENTRY_CHASSIS_MOUSE_RATE_DEG_S, SENTRY_CHASSIS_MOUSE_RATE_DEG_S);
        if (!spin_enabled) chassis_cmd.wz = yaw_rate;
        gimbal_cmd.pitch += pitch_rate * KBM_CMD_DT_S * KBM_PITCH_DIR;
        MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, SENTRY_PITCH_MIN_ANGLE, SENTRY_PITCH_MAX_ANGLE);
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }
    else
    {
        kbm_yaw_filt = 0.0f;
        kbm_pitch_filt = 0.0f;
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }

    /* 发射:自瞄时视觉 shoot 或左键都请求连发;手动时保持 R/E 模式与左键语义 */
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    shoot_cmd.fire_tier = cmd_fire_tier_get();
    if (aim_ctrl)
    {
        last_r_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R];
        last_e_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E];
        const uint8_t fire = (uint8_t)(SENTRY_AUTO_AIM_FIRE_ENABLE &&
                                       (auto_aim_fire_request || rc->mouse.press_l));
        shoot_cmd.friction_mode = fire ? FRICTION_ON : FRICTION_OFF;
        shoot_cmd.load_mode = fire ? LOADER_BURST : LOADER_STOP;
    }
    else
    {
        shoot_cmd.friction_mode = rc->mouse.press_r ? FRICTION_ON : FRICTION_OFF;
        if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R] != last_r_count)
        {
            last_r_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R];
            shoot_cmd.load_mode = LOADER_SINGLE;
        }
        else if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E] != last_e_count)
        {
            last_e_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E];
            shoot_cmd.load_mode = LOADER_BURST3;
        }
        else
            shoot_cmd.load_mode =
                (shoot_cmd.friction_mode == FRICTION_ON && rc->mouse.press_l) ? LOADER_BURST
                                                                              : LOADER_STOP;
    }

    /* F: 底盘回中(底盘追云台) */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F] != last_f_count)
    {
        last_f_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F];
#if SENTRY_GIMBAL_RECENTER_ENABLE
        yaw_recentering = 1;
        recenter_cnt = 0;
#endif
    }
}
