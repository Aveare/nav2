/**
 * @file    cmd_remote.c
 * @brief   哨兵遥控输入:本车 RM_shaobin2 挡位/杆量映射
 * @layer   APP
 * @author  modified by 小猫大王
 * @note    右杆控制底盘平移;左纵杆控制 pitch;左横杆在左下挡控制小 yaw,
 *          其余挡位控制底盘转向。右下挡为小陀螺,右中挡为人工自由移动。
 */
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "dwt_port.h"
#include <math.h>

void cmd_remote_set(void)
{
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_ROTATE;
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }
    else if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_RC_CONTROL;
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }

    if (robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
        !REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right) &&
        MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) > RC_STICK_DEADBAND)
    {
        gimbal_cmd.pitch += RC_PITCH_MAX_RATE * KBM_CMD_DT_S / RC_STICK_FULL_SCALE *
                            rc_data[RC_SLOT_CURRENT].rc.rocker_l1;
    }
    MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, -SENTRY_PITCH_TRAVEL_DEG,
                        SENTRY_PITCH_TRAVEL_DEG);

    /* 右杆平移:合速度限幅 + 进入/释放迟滞。 */
    const float forward = rc_data[RC_SLOT_CURRENT].rc.rocker_r1;
    const float right = rc_data[RC_SLOT_CURRENT].rc.rocker_r_;
    static uint8_t rc_moving;
    if (fabsf(forward) > CHASSIS_RC_DEADBAND || fabsf(right) > CHASSIS_RC_DEADBAND)
        rc_moving = 1u;
    else if (fabsf(forward) < CHASSIS_RC_RELEASE && fabsf(right) < CHASSIS_RC_RELEASE)
        rc_moving = 0u;
    if (rc_moving)
    {
        const float norm = sqrtf(forward * forward + right * right);
        chassis_cmd.vx = CHASSIS_MAX_SPEED * forward / norm;
        chassis_cmd.vy = CHASSIS_MAX_SPEED * right / norm;
    }
    else
    {
        chassis_cmd.vx = 0.0f;
        chassis_cmd.vy = 0.0f;
    }

    /* 左下挡单独控制小 yaw;其余挡位左横杆直接控制底盘转向。 */
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_left))
    {
        const float stick = -(float)rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
        gimbal_cmd.small_yaw_rate_deg_s =
            (fabsf(stick) < CHASSIS_RC_DEADBAND)
                ? 0.0f
                : SENTRY_SMALL_YAW_STICK_DIR * SENTRY_SMALL_YAW_STICK_RATE_DEG_S * stick /
                      RC_STICK_FULL_SCALE;
        chassis_cmd.wz = 0.0f;
    }
    else
    {
        const float turn = -(float)rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
        gimbal_cmd.small_yaw_rate_deg_s = 0.0f;
        chassis_cmd.wz = (fabsf(turn) < CHASSIS_RC_DEADBAND)
                             ? 0.0f
                             : CHASSIS_ROTATE_SPEED * turn / RC_STICK_FULL_SCALE;
    }

    /* 拨轮扳机:进入触发区开摩擦轮;短拨单发,长按连发。 */
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    shoot_cmd.fire_tier = cmd_fire_tier_get();
    {
        static uint8_t dial_in_trigger;
        static float dial_press_ms;
        const int16_t dial = rc_data[RC_SLOT_CURRENT].rc.dial;
        const float now_ms = dwt_get_timeline_ms();
        const uint8_t in_zone = dial <= SHOOT_DIAL_TRIGGER_ZONE;
        const uint8_t released = dial > SHOOT_DIAL_TRIGGER_RELEASE;

        shoot_cmd.friction_mode = in_zone ? FRICTION_ON : FRICTION_OFF;
        if (in_zone && !dial_in_trigger)
        {
            dial_in_trigger = 1u;
            dial_press_ms = now_ms;
        }
        if (dial_in_trigger && released)
        {
            dial_in_trigger = 0u;
            shoot_cmd.load_mode =
                (now_ms - dial_press_ms < SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_SINGLE
                                                                     : LOADER_STOP;
        }
        else if (dial_in_trigger && in_zone)
        {
            shoot_cmd.load_mode =
                (now_ms - dial_press_ms >= SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_BURST
                                                                      : LOADER_STOP;
        }
        else
        {
            shoot_cmd.load_mode = LOADER_STOP;
        }
    }
}
