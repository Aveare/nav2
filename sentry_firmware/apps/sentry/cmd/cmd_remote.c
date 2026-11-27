/**
 * @file    cmd_remote.c
 * @brief   哨兵遥控输入:DBUS 挡位/杆量到三路指令映射
 * @layer   APP
 * @author  小猫大王
 * @note    左中横杆控制底盘转向，左下横杆控制小 yaw；左纵杆控制 pitch，右杆控制平移。
 * @note    NUC 自瞄无有效目标时左中挡退回手动；右中挡底盘不跟随大 yaw；左上/右上是急停挡。
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
#include <math.h>

void cmd_remote_set(void)
{
    const uint8_t auto_aim = auto_aim_active;
    const uint8_t right_up =
        REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right);
    const int aim_ctrl = auto_aim ? cmd_auto_aim_set() : 0;

    /* 挡位:右中=不跟随,右下=小陀螺;自瞄时仍允许小陀螺(大 yaw 世界环自行补偿底盘转速)。 */
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_ROTATE;
    }
    else if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_NO_FOLLOW;
    }
    if (!right_up)
        gimbal_cmd.mode = aim_ctrl ? GIMBAL_AUTO_AIM_MODE : GIMBAL_GYRO_MODE;

    // 角目标增量:200Hz 下左横杆最大 180deg/s;左纵杆满量程 SENTRY_PITCH_STICK_RATE_DEG_S。
    // pitch 目标 = 相对上电零位的角度,夹在 [SENTRY_PITCH_MIN_ANGLE, SENTRY_PITCH_MAX_ANGLE]。
    // 自瞄时 pitch 由视觉绝对目标接管,杆量不再叠加。
    if (!aim_ctrl && robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
        !right_up)
    {
        const int16_t pitch_stick = rc_data[RC_SLOT_CURRENT].rc.rocker_l1;
        if (MATH_ABS(pitch_stick) > SENTRY_PITCH_STICK_DEADBAND)
        {
            /* 死区外按(杆量-死区)线性积分:越过死区时从 0 速起步,不在阈值边缘跳变。 */
            const float effort =
                (float)(pitch_stick - ((pitch_stick > 0) ? SENTRY_PITCH_STICK_DEADBAND
                                                         : -SENTRY_PITCH_STICK_DEADBAND)) /
                (660.0f - SENTRY_PITCH_STICK_DEADBAND);
            gimbal_cmd.pitch += SENTRY_PITCH_STICK_RATE_DEG_S * 0.005f * effort;
        }
    }
    MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, SENTRY_PITCH_MIN_ANGLE, SENTRY_PITCH_MAX_ANGLE);

    /* [用户指定] 以已标定的大yaw正前方为前进参考，右杆前后、左右均反向；合速度限CHASSIS_MAX_SPEED;
     * 死区加迟滞(进入30/退出20),滤静止噪声防指令打摆 */
    float forward = rc_data[RC_SLOT_CURRENT].rc.rocker_r1;
    float right = rc_data[RC_SLOT_CURRENT].rc.rocker_r_;
    static uint8_t rc_moving = 0;
    if (fabsf(forward) > CHASSIS_RC_DEADBAND || fabsf(right) > CHASSIS_RC_DEADBAND)
        rc_moving = 1;
    else if (fabsf(forward) < CHASSIS_RC_RELEASE && fabsf(right) < CHASSIS_RC_RELEASE)
        rc_moving = 0;
    if (rc_moving)
    {
        float norm = sqrtf(forward * forward + right * right);
        chassis_cmd.vx = CHASSIS_MAX_SPEED * forward / norm;
        chassis_cmd.vy = CHASSIS_MAX_SPEED * right / norm;
    }
    else
        chassis_cmd.vx = chassis_cmd.vy = 0;

    /* 左下挡:左横杆改控**小 yaw**(见 config 的小 yaw 段),此时底盘转向不再由左横杆驱动;
     * 其余挡位左横杆仍作底盘转向。两者互斥,避免一个杆同时驱动两个轴。 */
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_left))
    {
        float stick = -rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
        gimbal_cmd.small_yaw_rate_deg_s =
            (MATH_ABS((int)stick) < CHASSIS_RC_DEADBAND)
                ? 0.0f
                : SENTRY_SMALL_YAW_STICK_DIR * SENTRY_SMALL_YAW_STICK_RATE_DEG_S * stick / 660.0f;
        chassis_cmd.wz = 0.0f;
    }
    else
    {
        gimbal_cmd.small_yaw_rate_deg_s = 0.0f;
        float turn = -rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
        if (MATH_ABS((int)turn) < CHASSIS_RC_DEADBAND)
            chassis_cmd.wz = 0.0f;
        else
            chassis_cmd.wz = CHASSIS_ROTATE_SPEED * turn / 660.0f;
    }

    /* 发射:自瞄时视觉 shoot 驱动连发(热量守卫裁决节奏);手动时拨轮扳机驱动。 */
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.fire_tier = cmd_fire_tier_get();
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    {
        static uint8_t dial_in_trigger = 0; // 拨轮处于扳机区内(切回遥控/重启后按0起步,安全)
        static float dial_press_ms = 0.0f;  // 本次压入触发区的起始时刻
        if (aim_ctrl)
        {
            dial_in_trigger = 0;
            const uint8_t fire =
                (uint8_t)(SENTRY_AUTO_AIM_FIRE_ENABLE && auto_aim_fire_request);
            shoot_cmd.friction_mode = fire ? FRICTION_ON : FRICTION_OFF;
            shoot_cmd.load_mode = fire ? LOADER_BURST : LOADER_STOP;
        }
        else
        {
            const int16_t dial = rc_data[RC_SLOT_CURRENT].rc.dial;
            const float now_ms = dwt_get_timeline_ms();
            const uint8_t in_zone = (dial <= SHOOT_DIAL_TRIGGER_ZONE) ? 1u : 0u;
            const uint8_t released = (dial > SHOOT_DIAL_TRIGGER_RELEASE) ? 1u : 0u;

            shoot_cmd.friction_mode = in_zone ? FRICTION_ON : FRICTION_OFF;

            if (in_zone && !dial_in_trigger) // 进入扳机区,开始计时
            {
                dial_in_trigger = 1;
                dial_press_ms = now_ms;
            }

            if (dial_in_trigger && released) // 松手沿:短拨登记单发,长按已连发则不补发
            {
                dial_in_trigger = 0;
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms < SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_SINGLE : LOADER_STOP;
            }
            else if (dial_in_trigger && in_zone) // 区内按住:达到时长后连发直至松手
            {
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms >= SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_BURST : LOADER_STOP;
            }
            else
            {
                shoot_cmd.load_mode = LOADER_STOP;
            }
        }
    }
}
