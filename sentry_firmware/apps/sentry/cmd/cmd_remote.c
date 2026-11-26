/**
 * @file    cmd_remote.c
 * @brief   哨兵遥控输入:DBUS 挡位/杆量到三路指令映射
 * @layer   APP
 * @author  小猫大王
 * @note    左横杆=世界 yaw 目标增量,左纵杆=pitch 目标增量,右杆=底盘平移;
 *          小 yaw 不需要单独通道:gimbal 内部按"小 yaw 先吃误差"自动分配。
 * @note    左中挡=AX650 自瞄;右挡选择底盘模式;左上/右上是急停(stop)挡。
 */
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "ins_task.h"
#include "ax650_uart1.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "dwt_port.h"
#include "log_port.h"
#include <math.h>

void cmd_remote_set(void)
{
    const uint8_t auto_aim = REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left);

    /* 底盘模式:右挡决定 */
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_ROTATE;
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }
    else if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = auto_aim ? CHASSIS_NO_FOLLOW : CHASSIS_FOLLOW_GIMBAL_YAW;
        gimbal_cmd.mode = GIMBAL_GYRO_MODE;
    }

    /* 云台 yaw/pitch:左中挡自瞄,否则手动增量 */
    static uint8_t yaw_stick_active = 0;
    if (auto_aim)
    {
        yaw_stick_active = 0;
        const uint8_t auto_enabled = !REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right);
        ax650_uart1_state_t vision;
        ax650_uart1_get_state(&vision);
        const uint8_t vision_valid = robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
                                     auto_enabled && vision.command_valid &&
                                     vision.command.target_id != 0u;
        if (vision_valid)
        {
            ax650_raw_yaw_target = vision.command.yaw;
            const float now_ms = dwt_get_timeline_ms();
#if YAW_AUTO_KF_ENABLE
            float filtered_yaw = cmd_yaw_auto_kalman_update(
                vision.command.yaw, vision.command.yaw_speed, vision.command.target_id,
                vision.command_count, now_ms);
#else
            float filtered_yaw = vision.command.yaw;
#endif
#if YAW_AUTO_SMOOTH_ENABLE
            gimbal_cmd.big_yaw =
                cmd_yaw_auto_smooth_update(filtered_yaw, gimbal_fb_recv.big_yaw_world_deg, now_ms);
#else
            gimbal_cmd.big_yaw = filtered_yaw;
#endif
            /* 视觉给的是世界俯仰;pitch 环已改关节域,减去底座倾角换到关节目标 */
            gimbal_cmd.pitch = vision.command.pitch - gimbal_fb_recv.imu.Pitch;
            gimbal_cmd.mode = GIMBAL_AUTO_AIM_MODE;
        }
#if YAW_AUTO_KF_ENABLE
        else
            cmd_yaw_auto_kalman_reset();
#endif
#if YAW_AUTO_SMOOTH_ENABLE
        if (!vision_valid)
            cmd_yaw_auto_smooth_reset();
#endif
    }
    else
    {
#if YAW_AUTO_KF_ENABLE
        cmd_yaw_auto_kalman_reset();
#endif
#if YAW_AUTO_SMOOTH_ENABLE
        cmd_yaw_auto_smooth_reset();
#endif
        /* 松杆重锚定:消除增量控制残留,避免解锁后目标角长距离追赶 */
        if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) > RC_STICK_DEADBAND)
            yaw_stick_active = 1;
        else if (yaw_stick_active)
        {
            yaw_stick_active = 0;
            if (robot_state == ROBOT_STATUS_READY && remote_control_is_online())
                gimbal_cmd.big_yaw = gimbal_fb_recv.big_yaw_world_deg;
        }

        if (robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
            !REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right))
        {
            if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) > RC_STICK_DEADBAND)
                gimbal_cmd.big_yaw -= RC_YAW_MAX_RATE * KBM_CMD_DT_S / RC_STICK_FULL_SCALE *
                                      rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
            if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) > RC_STICK_DEADBAND)
                gimbal_cmd.pitch += RC_PITCH_MAX_RATE * KBM_CMD_DT_S / RC_STICK_FULL_SCALE *
                                    rc_data[RC_SLOT_CURRENT].rc.rocker_l1;
        }
    }
    MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, SENTRY_PITCH_MIN_ANGLE, SENTRY_PITCH_MAX_ANGLE);

    /* 底盘平移:右杆,合速度限幅 + 死区迟滞 */
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
        chassis_cmd.vx = chassis_cmd.vy = 0.0f;

    /* 发射:手动挡拨轮下压启动摩擦轮;短拨单发、按住连发;自瞄挡拨轮单发。 */
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.friction_mode = FRICTION_OFF;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate();
    shoot_cmd.fire_tier = cmd_fire_tier_get();
    {
        static uint8_t dial_in_trigger = 0;
        static uint8_t friction_keep_active = 0;
        static float dial_press_ms = 0.0f;
        static float friction_last_trigger_ms = 0.0f;
        const int16_t dial = rc_data[RC_SLOT_CURRENT].rc.dial;
        const float now_ms = dwt_get_timeline_ms();

        if (robot_state != ROBOT_STATUS_READY)
        {
            dial_in_trigger = 0;
            friction_keep_active = 0;
            shoot_cmd.load_mode = LOADER_STOP;
        }
        else if (auto_aim && !SENTRY_SHOOT_AUTO_AIM_REMOTE_ENABLE)
        {
            /* 宏置 0:自瞄挡完全禁射;宏=1 时自瞄挡与手动挡共用下方拨轮触发逻辑
             * (2026-11-23 用户裁决:自瞄挡不再常开摩擦轮) */
            dial_in_trigger = 0;
            friction_keep_active = 0;
            shoot_cmd.shoot_mode = SHOOT_OFF;
            shoot_cmd.friction_mode = FRICTION_OFF;
            shoot_cmd.load_mode = LOADER_STOP;
        }
        else
        {
            /* 手动挡 / 自瞄挡(宏=1):拨轮下压启动摩擦轮并保持,短拨单发、按住连发 */
            if (dial <= SHOOT_DIAL_TRIGGER_ZONE)
            {
                friction_keep_active = 1;
                friction_last_trigger_ms = now_ms;
            }
            if (friction_keep_active &&
                now_ms - friction_last_trigger_ms <= SHOOT_DIAL_FRICTION_KEEP_MS)
                shoot_cmd.friction_mode = FRICTION_ON;
            else
                friction_keep_active = 0;

            if (shoot_cmd.friction_mode == FRICTION_OFF)
            {
                dial_in_trigger = 0;
                shoot_cmd.load_mode = LOADER_STOP;
            }
            else if (!dial_in_trigger)
            {
                if (dial <= SHOOT_DIAL_TRIGGER_ZONE)
                {
                    dial_in_trigger = 1;
                    dial_press_ms = now_ms;
                }
                shoot_cmd.load_mode = LOADER_STOP;
            }
            else if (dial > SHOOT_DIAL_TRIGGER_RELEASE)
            {
                dial_in_trigger = 0;
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms < SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_SINGLE : LOADER_STOP;
            }
            else
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms >= SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_BURST : LOADER_STOP;
        }
    }
}
