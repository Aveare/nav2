/**
 * @file    cmd_autoaim.c
 * @brief   哨兵自瞄输入整形：NUC 0x0503 世界 yaw 目标的卡尔曼与二阶平滑
 * @layer   APP
 * @author  小猫大王
 * @note    只处理目标指令,不进入 IMU 反馈链;目标断帧时冻结,不按旧速度外推。
 */
#include "robot_def.h"
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "general_def.h"
#include "can_port.h"
#include "motor_def.h"
#include "dwt_port.h"
#include "log_port.h"
#include "vision_link.h"
#include <math.h>

/* 运行期可写(ELF/DWARF):世界 yaw 偏置;默认=config,不改变上电行为。 */
volatile float sentry_rt_auto_aim_yaw_offset_deg = SENTRY_AUTO_AIM_YAW_OFFSET_DEG;

/* ---------------- 自瞄目标解算(左中挡 / 键鼠 Shift 共用) ---------------- */

static float auto_aim_last_valid_ms;
static float auto_aim_hold_yaw;
static float auto_aim_hold_pitch;
static uint8_t auto_aim_have_target;

/**
 * @brief 读取 0x0503 并更新 gimbal_cmd 的世界目标
 * @return 1=本拍由自瞄控制云台(含目标丢失后的保持窗);0=退回手动
 * @note  只做目标整形,不写 gimbal mode;调用方决定 AUTO_AIM/GYRO。
 *        目标断帧后保持 SENTRY_AUTO_AIM_HOLD_MS,超时才交还手动。
 */
int cmd_auto_aim_set(void)
{
#if SENTRY_AUTO_AIM_ENABLE
    const float now_ms = dwt_get_timeline_ms();
    vision_state_t vision;
    vision_link_get_state(&vision);
    const uint8_t right_up =
        REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right);
    const uint8_t rc_ok =
        (uint8_t)(remote_control_is_online() ? !right_up : !SENTRY_AUTH_RC_LOST_STOP);
    const uint8_t valid =
        (uint8_t)(robot_state == ROBOT_STATUS_READY && rc_ok &&
                  vision.valid && vision.command.target_id != 0u &&
                  isfinite(vision.command.yaw) && isfinite(vision.command.pitch));

    if (valid)
    {
        nuc_raw_yaw_target = vision.command.yaw;
        const float yaw_offset = isfinite(sentry_rt_auto_aim_yaw_offset_deg)
                                     ? sentry_rt_auto_aim_yaw_offset_deg
                                     : SENTRY_AUTO_AIM_YAW_OFFSET_DEG;
        float yaw = vision.command.yaw + yaw_offset;
#if YAW_AUTO_KF_ENABLE
        yaw = cmd_yaw_auto_kalman_update(yaw, vision.command.yaw_speed, vision.command.target_id,
                                         vision.command_count, now_ms);
#endif
#if YAW_AUTO_SMOOTH_ENABLE
        yaw = cmd_yaw_auto_smooth_update(yaw, gimbal_fb_recv.big_yaw_world_deg, now_ms);
#endif
        float pitch = vision.command.pitch;
        MOTOR_LIMIT_MIN_MAX(pitch, SENTRY_AUTO_AIM_PITCH_MIN_DEG, SENTRY_AUTO_AIM_PITCH_MAX_DEG);

        gimbal_cmd.big_yaw = yaw;
        gimbal_cmd.pitch_abs = pitch;
        gimbal_cmd.aim_valid = 1u;
        auto_aim_fire_request = (vision.command.shoot != 0u) ? 1u : 0u;
        auto_aim_hold_yaw = yaw;
        auto_aim_hold_pitch = pitch;
        auto_aim_last_valid_ms = now_ms;
        auto_aim_have_target = 1u;
        return 1;
    }

    auto_aim_fire_request = 0u;
#if YAW_AUTO_KF_ENABLE
    cmd_yaw_auto_kalman_reset();
#endif
#if YAW_AUTO_SMOOTH_ENABLE
    cmd_yaw_auto_smooth_reset();
#endif
    if (auto_aim_have_target &&
        (now_ms - auto_aim_last_valid_ms) <= SENTRY_AUTO_AIM_HOLD_MS)
    {
        gimbal_cmd.big_yaw = auto_aim_hold_yaw;
        gimbal_cmd.pitch_abs = auto_aim_hold_pitch;
        gimbal_cmd.aim_valid = 1u;
        return 1;
    }
    gimbal_cmd.aim_valid = 0u;
    auto_aim_have_target = 0u;
    return 0;
#else
    gimbal_cmd.aim_valid = 0u;
    auto_aim_fire_request = 0u;
    return 0;
#endif
}

#if YAW_AUTO_KF_ENABLE
typedef struct
{
    float angle;
    float rate;
    float p00, p01, p10, p11;
    float last_ms;
    uint32_t command_count;
    uint8_t target_id;
    uint8_t initialized;
} cmd_yaw_auto_kalman_t;

static cmd_yaw_auto_kalman_t yaw_auto_kf;

void cmd_yaw_auto_kalman_reset(void)
{
    yaw_auto_kf.initialized = 0u;
}

static void cmd_yaw_auto_kalman_initialize(float angle, float rate, uint8_t target_id,
                                           uint32_t command_count, float now_ms)
{
    const float angle_var = YAW_AUTO_KF_ANGLE_NOISE_STD * YAW_AUTO_KF_ANGLE_NOISE_STD;
    const float rate_var = YAW_AUTO_KF_RATE_NOISE_STD * YAW_AUTO_KF_RATE_NOISE_STD;
    MOTOR_LIMIT_MIN_MAX(rate, -YAW_AUTO_KF_RATE_LIMIT, YAW_AUTO_KF_RATE_LIMIT);
    yaw_auto_kf.angle = angle;
    yaw_auto_kf.rate = rate;
    yaw_auto_kf.p00 = angle_var;
    yaw_auto_kf.p01 = 0.0f;
    yaw_auto_kf.p10 = 0.0f;
    yaw_auto_kf.p11 = rate_var;
    yaw_auto_kf.last_ms = now_ms;
    yaw_auto_kf.command_count = command_count;
    yaw_auto_kf.target_id = target_id;
    yaw_auto_kf.initialized = 1u;
}

float cmd_yaw_auto_kalman_update(float measured_angle, float measured_rate, uint8_t target_id,
                                 uint32_t command_count, float now_ms)
{
    measured_rate *= FW_RAD_TO_DEG; // 0x0503 yaw_speed: rad/s -> deg/s
    MOTOR_LIMIT_MIN_MAX(measured_rate, -YAW_AUTO_KF_RATE_LIMIT, YAW_AUTO_KF_RATE_LIMIT);
    if (!yaw_auto_kf.initialized || yaw_auto_kf.target_id != target_id)
    {
        cmd_yaw_auto_kalman_initialize(measured_angle, measured_rate, target_id, command_count, now_ms);
        return measured_angle;
    }
    if (command_count == yaw_auto_kf.command_count)
        return yaw_auto_kf.angle;

    float dt = (now_ms - yaw_auto_kf.last_ms) * 0.001f;
    if (dt <= 0.0f || dt > 0.05f)
        dt = 0.005f;
    yaw_auto_kf.last_ms = now_ms;

    yaw_auto_kf.angle += yaw_auto_kf.rate * dt;
    const float accel_var = YAW_AUTO_KF_ACCEL_NOISE_STD * YAW_AUTO_KF_ACCEL_NOISE_STD;
    const float dt2 = dt * dt;
    const float dt3 = dt2 * dt;
    const float dt4 = dt2 * dt2;
    const float q00 = 0.25f * dt4 * accel_var;
    const float q01 = 0.5f * dt3 * accel_var;
    const float q11 = dt2 * accel_var;
    const float old_p00 = yaw_auto_kf.p00;
    const float old_p01 = yaw_auto_kf.p01;
    const float old_p10 = yaw_auto_kf.p10;
    const float old_p11 = yaw_auto_kf.p11;
    yaw_auto_kf.p00 = old_p00 + dt * (old_p01 + old_p10) + dt2 * old_p11 + q00;
    yaw_auto_kf.p01 = old_p01 + dt * old_p11 + q01;
    yaw_auto_kf.p10 = old_p10 + dt * old_p11 + q01;
    yaw_auto_kf.p11 = old_p11 + q11;

    while (measured_angle - yaw_auto_kf.angle > 180.0f)
        measured_angle -= 360.0f;
    while (measured_angle - yaw_auto_kf.angle < -180.0f)
        measured_angle += 360.0f;
    const float innovation = measured_angle - yaw_auto_kf.angle;
    if (fabsf(innovation) > YAW_AUTO_KF_RESET_ANGLE)
    {
        cmd_yaw_auto_kalman_initialize(measured_angle, measured_rate, target_id, command_count, now_ms);
        return measured_angle;
    }

    if (command_count != yaw_auto_kf.command_count)
    {
        yaw_auto_kf.command_count = command_count;
        const float angle_var = YAW_AUTO_KF_ANGLE_NOISE_STD * YAW_AUTO_KF_ANGLE_NOISE_STD;
        float s = yaw_auto_kf.p00 + angle_var;
        float k0 = yaw_auto_kf.p00 / s;
        float k1 = yaw_auto_kf.p10 / s;
        yaw_auto_kf.angle += k0 * innovation;
        yaw_auto_kf.rate += k1 * innovation;
        float p00 = yaw_auto_kf.p00;
        float p01 = yaw_auto_kf.p01;
        float p10 = yaw_auto_kf.p10;
        float p11 = yaw_auto_kf.p11;
        yaw_auto_kf.p00 = (1.0f - k0) * p00;
        yaw_auto_kf.p01 = (1.0f - k0) * p01;
        yaw_auto_kf.p10 = p10 - k1 * p00;
        yaw_auto_kf.p11 = p11 - k1 * p01;

        const float rate_var = YAW_AUTO_KF_RATE_NOISE_STD * YAW_AUTO_KF_RATE_NOISE_STD;
        s = yaw_auto_kf.p11 + rate_var;
        k0 = yaw_auto_kf.p01 / s;
        k1 = yaw_auto_kf.p11 / s;
        const float rate_innovation = measured_rate - yaw_auto_kf.rate;
        yaw_auto_kf.angle += k0 * rate_innovation;
        yaw_auto_kf.rate += k1 * rate_innovation;
        p00 = yaw_auto_kf.p00;
        p01 = yaw_auto_kf.p01;
        p10 = yaw_auto_kf.p10;
        p11 = yaw_auto_kf.p11;
        yaw_auto_kf.p00 = p00 - k0 * p10;
        yaw_auto_kf.p01 = p01 - k0 * p11;
        yaw_auto_kf.p10 = (1.0f - k1) * p10;
        yaw_auto_kf.p11 = (1.0f - k1) * p11;

        const float cross = 0.5f * (yaw_auto_kf.p01 + yaw_auto_kf.p10);
        yaw_auto_kf.p01 = cross;
        yaw_auto_kf.p10 = cross;
    }
    return yaw_auto_kf.angle;
}
#endif

#if YAW_AUTO_SMOOTH_ENABLE
typedef struct
{
    float angle;
    float rate;
    float last_ms;
    uint8_t initialized;
} cmd_yaw_auto_smooth_t;

static cmd_yaw_auto_smooth_t yaw_auto_smooth;

void cmd_yaw_auto_smooth_reset(void)
{
    yaw_auto_smooth.initialized = 0u;
}

float cmd_yaw_auto_smooth_update(float target, float initial_angle, float now_ms)
{
    if (!yaw_auto_smooth.initialized)
    {
        yaw_auto_smooth.angle = initial_angle;
        yaw_auto_smooth.rate = 0.0f;
        yaw_auto_smooth.last_ms = now_ms;
        yaw_auto_smooth.initialized = 1u;
        return initial_angle;
    }

    float dt = (now_ms - yaw_auto_smooth.last_ms) * 0.001f;
    if (dt <= 0.0f || dt > 0.05f)
        dt = 0.005f;
    yaw_auto_smooth.last_ms = now_ms;

    float error = target - yaw_auto_smooth.angle;
    while (error > 180.0f)
        error -= 360.0f;
    while (error < -180.0f)
        error += 360.0f;
    const float continuous_target = yaw_auto_smooth.angle + error;

    float desired_rate = YAW_AUTO_SMOOTH_POSITION_GAIN * error;
    MOTOR_LIMIT_MIN_MAX(desired_rate, -YAW_AUTO_SMOOTH_MAX_RATE, YAW_AUTO_SMOOTH_MAX_RATE);
    float delta_rate = desired_rate - yaw_auto_smooth.rate;
    const float max_delta_rate = YAW_AUTO_SMOOTH_MAX_ACCEL * dt;
    MOTOR_LIMIT_MIN_MAX(delta_rate, -max_delta_rate, max_delta_rate);
    yaw_auto_smooth.rate += delta_rate;

    const float previous_error = error;
    yaw_auto_smooth.angle += yaw_auto_smooth.rate * dt;
    error = continuous_target - yaw_auto_smooth.angle;
    if (previous_error * error <= 0.0f)
    {
        yaw_auto_smooth.angle = continuous_target;
        yaw_auto_smooth.rate = 0.0f;
    }
    return yaw_auto_smooth.angle;
}
#endif
