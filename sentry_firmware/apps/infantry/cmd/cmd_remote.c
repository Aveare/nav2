/**
 * @file    cmd_remote.c
 * @brief   遥控输入:DBUS 挡位/杆量到三路指令映射
 * @layer   APP
 * @author  小猫大王
* @note    从 robot_cmd.c 拆出;函数内 static 状态随函数整段搬迁。
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

/**
 * @brief 遥控器输入设置:左中 AX650 自瞄，左下摇杆云台，右拨杆选择底盘模式
 * @note  自瞄时右中挡为底盘无跟随，右下挡为可平移小陀螺；手动右中挡仍跟随云台。
 */
void cmd_remote_set()
{
    /* 自瞄状态统一由 cmd_task 刷新(左中挡 || 键鼠 Shift),本地不再读挡位判定(DEV-20261124-03)。
     * 右上挡为急停/手动接管:此时即使 auto_aim_active 也不进自瞄云台模式。 */
    const uint8_t auto_aim = auto_aim_active;
    const uint8_t right_up = REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right);
    if (REMOTE_RC_SWITCH_IS_DOWN(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = CHASSIS_ROTATE;
        gimbal_cmd.gimbal_mode = GIMBAL_GYRO_MODE;
    }
    else if (REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_right))
    {
        chassis_cmd.chassis_mode = auto_aim ? CHASSIS_NO_FOLLOW : CHASSIS_FOLLOW_GIMBAL_YAW;
        gimbal_cmd.gimbal_mode = GIMBAL_GYRO_MODE;
    }

    /* 自瞄目标由 AX650 0x0503 世界系角度给定(统一解算见 cmd_auto_aim_set);
     * 非自瞄时保留摇杆增量控制,退出自瞄后立即由操作手接管。 */
    static uint8_t yaw_stick_active = 0;
    if (auto_aim)
    {
        yaw_stick_active = 0;
        if (!right_up)
            gimbal_cmd.gimbal_mode = GIMBAL_AUTO_AIM_MODE;
        cmd_auto_aim_set();
    }
    else
    {
#if YAW_AUTO_KF_ENABLE
        cmd_yaw_auto_kalman_reset();
#endif
#if YAW_AUTO_SMOOTH_ENABLE
        cmd_yaw_auto_smooth_reset();
#endif
        /* 松杆重锚定:左杆回中沿把 yaw 目标设为当前 IMU 角，消除残留误差。 */
        if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) > RC_STICK_DEADBAND)
            yaw_stick_active = 1;
        else if (yaw_stick_active)
        {
            yaw_stick_active = 0;
            if (robot_state == ROBOT_STATUS_READY && remote_control_is_online())
                gimbal_cmd.yaw = gimbal_fb_recv.gimbal_imu_data.YawTotalAngle;
        }

        // 角目标增量:200Hz下左横杆最大180deg/s,左纵杆60deg/s
        if (robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
            !REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right))
        {
            if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l_) > RC_STICK_DEADBAND)
                gimbal_cmd.yaw -= RC_YAW_MAX_RATE * KBM_CMD_DT_S / RC_STICK_FULL_SCALE *
                                       rc_data[RC_SLOT_CURRENT].rc.rocker_l_;
            if (MATH_ABS(rc_data[RC_SLOT_CURRENT].rc.rocker_l1) > 10)
                gimbal_cmd.pitch += RC_PITCH_MAX_RATE * KBM_CMD_DT_S / RC_STICK_FULL_SCALE *
                                         rc_data[RC_SLOT_CURRENT].rc.rocker_l1;
        }
    }
    MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, PITCH_MIN_ANGLE, PITCH_MAX_ANGLE);

    /* 平移:右杆上推前进/右推右移,合速度限运行期最大速度(ELF 可写);
     * 死区加迟滞(进入30/退出20),滤静止噪声防指令打摆 */
    const float max_speed = chassis_max_speed_mm_s;
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
        chassis_cmd.vx = max_speed * forward / norm;
        chassis_cmd.vy = max_speed * right / norm;
    }
    else
        chassis_cmd.vx = chassis_cmd.vy = 0;

    // 发射:AX650 自瞄挡由拨轮压入触发区开启摩擦轮并请求一发，必须回中后重装。
    // 手动挡仍为拨轮下压启动摩擦轮；短拨松手=单发，按住≥SHOOT_DIAL_SINGLE_HOLD_MS=连发。
    // 进/出阈值构成回中迟滞，避免拨轮边界抖动导致重复触发。
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.friction_mode = FRICTION_OFF;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate(); // 请求射速由战术档位决定(DEV-20261121-11)
    shoot_cmd.fire_tier = cmd_fire_tier_get();   // 档位显示与日志用
    {
        static uint8_t dial_in_trigger = 0; // 手动挡拨轮处于扳机区
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
        static uint8_t auto_dial_armed = 0; // 自瞄挡必须先看到拨轮回中，才允许下一次单发
#endif
        static uint8_t friction_keep_active = 0;
        static float dial_press_ms = 0.0f; // 本次压入触发区的起始时刻
        static float friction_last_trigger_ms = 0.0f;
        const int16_t dial = rc_data[RC_SLOT_CURRENT].rc.dial;
        const float now_ms = dwt_get_timeline_ms();

        if (robot_state != ROBOT_STATUS_READY)
        {
            dial_in_trigger = 0;
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
            auto_dial_armed = 0;
#endif
            friction_keep_active = 0;
            shoot_cmd.load_mode = LOADER_STOP;
        }
        else if (auto_aim)
        {
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
            /* 自瞄只允许遥控拨轮触发，不采用 AX650 0x0503 的 shoot 字段。
             * 单发用一个控制周期的模式脉冲，shoot 任务按 LOADER_STOP->LOADER_SINGLE
             * 的切换沿登记一发；保持拨轮不会重复登记。 */
            dial_in_trigger = 0;
            shoot_cmd.load_mode = LOADER_STOP;
            if (dial > SHOOT_DIAL_TRIGGER_RELEASE)
                auto_dial_armed = 1;
            else if (dial <= SHOOT_DIAL_TRIGGER_ZONE)
            {
                friction_keep_active = 1;
                friction_last_trigger_ms = now_ms;
                if (auto_dial_armed)
                {
                    auto_dial_armed = 0;
                    shoot_cmd.load_mode = LOADER_SINGLE;
                }
            }
            if (friction_keep_active && now_ms - friction_last_trigger_ms <= SHOOT_DIAL_FRICTION_KEEP_MS)
                shoot_cmd.friction_mode = FRICTION_ON;
            else
                friction_keep_active = 0;
#else
            /* 功能关闭时恢复安全默认值：视觉只接管云台，发射链保持关闭。 */
            dial_in_trigger = 0;
            friction_keep_active = 0;
            shoot_cmd.shoot_mode = SHOOT_OFF;
            shoot_cmd.friction_mode = FRICTION_OFF;
            shoot_cmd.load_mode = LOADER_STOP;
#endif
        }
        else
        {
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
            auto_dial_armed = 0;
#endif
            if (dial <= SHOOT_DIAL_TRIGGER_ZONE)
            {
                friction_keep_active = 1;
                friction_last_trigger_ms = now_ms;
            }
            if (friction_keep_active && now_ms - friction_last_trigger_ms <= SHOOT_DIAL_FRICTION_KEEP_MS)
                shoot_cmd.friction_mode = FRICTION_ON;
            else
                friction_keep_active = 0;

            if (shoot_cmd.friction_mode == FRICTION_OFF)
            {
                dial_in_trigger = 0; // 摩擦轮关:复位扳机,拨盘无动作
                shoot_cmd.load_mode = LOADER_STOP;
            }
            else if (!dial_in_trigger)
            {
                if (dial <= SHOOT_DIAL_TRIGGER_ZONE) // 进入扳机区,开始计时
                {
                    dial_in_trigger = 1;
                    dial_press_ms = now_ms;
                }
                shoot_cmd.load_mode = LOADER_STOP;
            }
            else if (dial > SHOOT_DIAL_TRIGGER_RELEASE) // 已出迟滞带=松手沿:短拨登记单发
            {
                dial_in_trigger = 0;
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms < SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_SINGLE : LOADER_STOP;
            }
            else // 区内按住:未达时长前保持停止,达到后连发直至松手
                shoot_cmd.load_mode =
                    (now_ms - dial_press_ms >= SHOOT_DIAL_SINGLE_HOLD_MS) ? LOADER_BURST : LOADER_STOP;
        }
    }
}
