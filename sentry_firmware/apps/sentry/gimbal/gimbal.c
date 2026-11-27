/**
 * @file    gimbal.c
 * @brief   哨兵三轴云台:大 yaw 回正/稳像、小 yaw 位置保持、pitch 相对行程
 * @layer   APP
 * @author  modified by 小猫大王
 * @note    [SENTRY] 控制结构与参数来自本车 RM_shaobin2；保留云端 USART1 VOFA 输出。
 * @note    普通挡大 yaw 锁定机械正前方；小陀螺挡不保存世界航向，只把世界角速度闭环到 0。
 */
#include "gimbal.h"
#include "gimbal_control.h"
#include "sentry_def.h"
#include "robot_def.h"
#include "dm_motor.h"
#include "dji_motor.h"
#include "ins_task.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "general_def.h"
#include "dwt_port.h"
#include "kernel.h"
#include "log_port.h"
#if GIMBAL_VOFA_UART1_ENABLE
#include "fault_port.h"
#include "usart_port.h"
#include "usart.h"
#include <string.h>
#endif
#include <math.h>

static attitude_t *gimbal_imu_data;
static dm_motor_t *big_yaw_motor;
static dm_motor_t *pitch_motor;
static dji_motor_t *small_yaw_motor;

static gimbal_axis_controller_t big_yaw_controller;
static gimbal_axis_controller_t pitch_controller;

static msg_pub_t gimbal_pub;
static msg_sub_t gimbal_sub;
static msg_sub_t chassis_sub;
static sentry_gimbal_cmd_t gimbal_cmd_recv;
static chassis_ctrl_cmd_t chassis_cmd_recv;
static sentry_gimbal_feed_t gimbal_fb;
static cmd_watchdog_t gimbal_cmd_wd;
static cmd_watchdog_t chassis_cmd_wd;

static float big_yaw_torque_nm;
static float big_yaw_torque_lpf;
static float big_yaw_last_ms;
static uint8_t big_yaw_world_mode;
static uint8_t big_yaw_band_latched;
static uint32_t big_yaw_imu_update_count;
static float big_yaw_imu_last_ms;
static uint8_t big_yaw_imu_seen_update;

static uint8_t pitch_reference_initialized;
static float pitch_start_rad;
static float pitch_command_start_deg;
static float pitch_target_rad;
static float pitch_last_ms;
static float pitch_torque_nm;

static uint8_t small_yaw_target_initialized;
static float small_yaw_target_deg;
static float small_yaw_last_ms;
static float small_yaw_joint_deg;

#if GIMBAL_VOFA_UART1_ENABLE
static usart_t *gimbal_vofa_usart;
#endif

static uint8_t gimbal_dm_feedback_fresh(const dm_motor_t *motor)
{
    return motor != NULL && motor->rx_init_flag &&
           motor->rx_refresh_dt < (int)(SENTRY_GIMBAL_DM_LOST_MS / (float)DM_TASK_CYCLE);
}

static uint8_t gimbal_run_requested(void)
{
    return gimbal_cmd_recv.mode == GIMBAL_GYRO_MODE ||
           gimbal_cmd_recv.mode == GIMBAL_FREE_MODE ||
           gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE;
}

static float gimbal_big_yaw_position_rad(void)
{
    if (big_yaw_motor == NULL)
        return 0.0f;
    return SENTRY_BIG_YAW_DIRECTION * big_yaw_motor->measure.total_position /
           SENTRY_BIG_YAW_TRANSMISSION_RATIO;
}

static float gimbal_big_yaw_align_rad(void)
{
    return SENTRY_BIG_YAW_DIRECTION * SENTRY_BIG_YAW_FRONT_ALIGN_RAD /
           SENTRY_BIG_YAW_TRANSMISSION_RATIO;
}

static float gimbal_small_yaw_joint_deg(void)
{
    if (small_yaw_motor == NULL)
        return 0.0f;
    const float centered = (float)small_yaw_motor->measure.ecd -
                           (float)SENTRY_SMALL_YAW_CENTER_ECD;
    const float wrapped = centered > 4096.0f   ? centered - 8192.0f
                          : centered < -4096.0f ? centered + 8192.0f
                                                : centered;
    return wrapped * (360.0f / 8192.0f);
}

static void gimbal_controller_init(gimbal_axis_controller_t *axis, uint8_t yaw)
{
    const gimbal_axis_config_t config = {
        .angle =
            {
                .Kp = yaw ? SENTRY_BIG_YAW_ANGLE_KP : SENTRY_PITCH_ANGLE_KP,
                .MaxOut = (yaw ? SENTRY_BIG_YAW_MAX_SPEED_DEG_S
                               : SENTRY_PITCH_MAX_SPEED_DEG_S) *
                          FW_DEG_TO_RAD,
                .DeadBand = (yaw ? SENTRY_BIG_YAW_ANGLE_DEADBAND_DEG
                                 : SENTRY_PITCH_DEADBAND_DEG) *
                            FW_DEG_TO_RAD,
                .Improve = PID_Derivative_On_Measurement,
                .DerivativeLimit = SENTRY_AXIS_D_LIMIT_DEG_S * FW_DEG_TO_RAD,
            },
        .speed =
            {
                .Kp = yaw ? SENTRY_BIG_YAW_SPEED_KP : SENTRY_PITCH_SPEED_KP,
                .Ki = yaw ? SENTRY_BIG_YAW_SPEED_KI : SENTRY_PITCH_SPEED_KI,
                .MaxOut = yaw ? SENTRY_BIG_YAW_TORQUE_NM : SENTRY_PITCH_TORQUE_NM,
                .IntegralLimit = yaw ? SENTRY_BIG_YAW_INTEGRAL_NM
                                     : SENTRY_PITCH_INTEGRAL_NM,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_OutputFilter,
                .Output_LPF_RC = SENTRY_AXIS_OUTPUT_RC_S,
            },
        .kd_near = yaw ? SENTRY_BIG_YAW_KD_NEAR : 0.0f,
        .kd_far = yaw ? SENTRY_BIG_YAW_KD_FAR : 0.0f,
        .blend_lo_rad = SENTRY_BIG_YAW_KD_BLEND_LO_DEG * FW_DEG_TO_RAD,
        .blend_hi_rad = SENTRY_BIG_YAW_KD_BLEND_HI_DEG * FW_DEG_TO_RAD,
        .feedforward_gain = SENTRY_AXIS_FF_GAIN,
        .feedforward_lpf = SENTRY_AXIS_FF_LPF,
    };
    gimbal_axis_init(axis, &config);
}

void gimbal_init(void)
{
    gimbal_imu_data = ins_init();

    dm_motor_mit_init_config_t big_yaw_config = {
        .controller_param_init_config = {.Kp = 0.0f, .Kd = 0.0f},
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .can_init_config =
            {
                .can_handle = &hcan1,
                .tx_id = SENTRY_BIG_YAW_DM_ID,
                .rx_id = SENTRY_BIG_YAW_DM_MASTER_ID,
            },
    };
    big_yaw_motor = dm_motor_mit_init(&big_yaw_config);
    if (big_yaw_motor != NULL)
        dm_motor_stop(big_yaw_motor);

    dm_motor_mit_init_config_t pitch_config = {
        .controller_param_init_config = {.Kp = 0.0f, .Kd = 0.0f},
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .can_init_config =
            {
                .can_handle = &hcan2,
                .tx_id = SENTRY_PITCH_DM_ID,
                .rx_id = SENTRY_PITCH_DM_MASTER_ID,
            },
    };
    pitch_motor = dm_motor_mit_init(&pitch_config);
    if (pitch_motor != NULL)
        dm_motor_stop(pitch_motor);

    motor_init_config_t small_yaw_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
                .tx_id = SENTRY_SMALL_YAW_ESC_ID,
            },
        .controller_param_init_config =
            {
                .speed_PID =
                    {
                        .Kp = SENTRY_SMALL_YAW_SPEED_KP,
                        .Ki = SENTRY_SMALL_YAW_SPEED_KI,
                        .Kd = 0.0f,
                        .DeadBand = SENTRY_SMALL_YAW_DEADBAND_DEG_S,
                        .Improve = PID_Integral_Limit | PID_OutputFilter,
                        .IntegralLimit = SENTRY_SMALL_YAW_INTEGRAL_LIMIT,
                        .Output_LPF_RC = SENTRY_SMALL_YAW_OUTPUT_RC_S,
                        .MaxOut = SENTRY_SMALL_YAW_MAX_OUT,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .control_mode =
                    SENTRY_SMALL_YAW_CURRENT_MODE ? MOTOR_CONTROL_CURRENT : MOTOR_CONTROL_VOLTAGE,
                .outer_loop_type = SPEED_LOOP,
                .close_loop_type = SPEED_LOOP,
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = GM6020,
    };
    small_yaw_motor = dji_motor_init(&small_yaw_config);
    if (small_yaw_motor != NULL)
        dji_motor_stop(small_yaw_motor);

    gimbal_controller_init(&big_yaw_controller, 1u);
    gimbal_controller_init(&pitch_controller, 0u);
    gimbal_pub = msg_pub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(sentry_gimbal_cmd_t));
    chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
    chassis_cmd_recv.chassis_mode = CHASSIS_ZERO_FORCE;

#if GIMBAL_VOFA_UART1_ENABLE
    usart_init_config_t gimbal_vofa_config = {
        .recv_buff_size = 1u,
        .usart_handle = &huart1, /* REDFLAG-EXEMPT: Init 句柄注入。 */
        .module_callback = 0,
        .tx_only = 1u,
    };
    gimbal_vofa_usart = usart_register(&gimbal_vofa_config);
#endif
}

static void gimbal_apply_zero_force(void)
{
    if (big_yaw_motor != NULL)
        dm_motor_stop(big_yaw_motor);
    if (pitch_motor != NULL)
        dm_motor_stop(pitch_motor);
    if (small_yaw_motor != NULL)
    {
        dji_motor_stop(small_yaw_motor);
        dji_motor_set_ref(small_yaw_motor, 0.0f);
    }
    gimbal_axis_reset(&big_yaw_controller);
    gimbal_axis_reset(&pitch_controller);
    big_yaw_torque_nm = 0.0f;
    big_yaw_torque_lpf = 0.0f;
    big_yaw_band_latched = 0u;
    pitch_reference_initialized = 0u;
    pitch_torque_nm = 0.0f;
    small_yaw_target_initialized = 0u;
}

static void gimbal_big_yaw_step(float now_ms)
{
    if (big_yaw_motor == NULL || !SENTRY_BIG_YAW_OUTPUT_ENABLE)
        return;

    const uint8_t chassis_fresh =
        !cmd_watchdog_expired(&chassis_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS);
    const uint8_t command_active = gimbal_run_requested() && chassis_fresh &&
                                   chassis_cmd_recv.chassis_mode != CHASSIS_ZERO_FORCE;
    const uint8_t world_requested = chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE;
    const float position = gimbal_big_yaw_position_rad();
    const float speed = SENTRY_BIG_YAW_DIRECTION * big_yaw_motor->measure.velocity /
                        SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    const float world_speed =
        (gimbal_imu_data != NULL)
            ? SENTRY_BIG_YAW_IMU_DIRECTION * gimbal_imu_data->Gyro[IMU_AXIS_Z]
            : 0.0f;

    const uint64_t imu_snapshot = *(const volatile uint64_t *)&ekf_ins.UpdateCount;
    const uint32_t imu_updates = (uint32_t)imu_snapshot;
    if (imu_updates != big_yaw_imu_update_count)
    {
        big_yaw_imu_seen_update = 1u;
        big_yaw_imu_update_count = imu_updates;
        big_yaw_imu_last_ms = now_ms;
    }
    const float imu_age_ms = now_ms - big_yaw_imu_last_ms;
    const uint8_t imu_online = ekf_ins.Initialized && big_yaw_imu_seen_update &&
                               imu_age_ms >= 0.0f &&
                               imu_age_ms <= SENTRY_BIG_YAW_IMU_TIMEOUT_MS &&
                               isfinite(world_speed);

    if (!command_active)
    {
        dm_motor_stop(big_yaw_motor);
        gimbal_axis_reset(&big_yaw_controller);
        big_yaw_torque_nm = 0.0f;
        big_yaw_torque_lpf = 0.0f;
        big_yaw_band_latched = 0u;
        big_yaw_world_mode = 0u;
        return;
    }

    dm_motor_enable(big_yaw_motor);
    const uint8_t healthy = gimbal_dm_feedback_fresh(big_yaw_motor) &&
                            big_yaw_motor->measure.state == DM_STATE_ENABLE &&
                            isfinite(position) && isfinite(speed) &&
                            (!world_requested || imu_online);
    if (!healthy)
    {
        dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
        gimbal_axis_reset(&big_yaw_controller);
        big_yaw_torque_nm = 0.0f;
        big_yaw_torque_lpf = 0.0f;
        return;
    }

    if (world_requested != big_yaw_world_mode)
    {
        gimbal_axis_reset(&big_yaw_controller);
        big_yaw_torque_lpf = 0.0f;
        big_yaw_band_latched = 0u;
        big_yaw_last_ms = now_ms;
        big_yaw_world_mode = world_requested;
    }

    const float error = remainderf(gimbal_big_yaw_align_rad() - position, 2.0f * FW_PI);
    if (!world_requested)
    {
        if (fabsf(error) <= SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG * FW_DEG_TO_RAD)
            big_yaw_band_latched = 1u;
        else if (fabsf(error) >= SENTRY_BIG_YAW_TARGET_DEADBAND_DEG * FW_DEG_TO_RAD)
            big_yaw_band_latched = 0u;
    }

    const uint8_t updated =
        world_requested
            ? gimbal_axis_update_speed(&big_yaw_controller, world_speed, 0.0f, now_ms)
            : gimbal_axis_update(&big_yaw_controller, position, speed,
                                 big_yaw_band_latched ? position : position + error, now_ms);
    if (!updated)
    {
        dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
        big_yaw_torque_nm = 0.0f;
        big_yaw_torque_lpf = 0.0f;
        return;
    }

    float torque_cmd = big_yaw_controller.torque_nm;
    if (!world_requested && big_yaw_band_latched)
    {
        gimbal_axis_reset(&big_yaw_controller);
        torque_cmd = 0.0f;
    }
    const float dt_s = (now_ms - big_yaw_last_ms) * 0.001f;
    big_yaw_last_ms = now_ms;
    if (dt_s > 0.0f && dt_s < 0.1f)
    {
        const float alpha = dt_s / (SENTRY_BIG_YAW_TORQUE_LPF_RC_S + dt_s);
        big_yaw_torque_lpf += alpha * (torque_cmd - big_yaw_torque_lpf);
    }
    big_yaw_torque_nm = big_yaw_torque_lpf;
    dm_motor_set_mit_torque(big_yaw_motor,
                            big_yaw_torque_nm * SENTRY_BIG_YAW_DIRECTION /
                                SENTRY_BIG_YAW_TRANSMISSION_RATIO);
}

static void gimbal_pitch_step(float now_ms)
{
    if (pitch_motor == NULL)
        return;
    if (!gimbal_run_requested())
    {
        dm_motor_stop(pitch_motor);
        gimbal_axis_reset(&pitch_controller);
        pitch_reference_initialized = 0u;
        pitch_torque_nm = 0.0f;
        return;
    }

    dm_motor_enable(pitch_motor);
    const float position = pitch_motor->measure.total_position;
    const float speed = pitch_motor->measure.velocity;
    const uint8_t healthy = gimbal_dm_feedback_fresh(pitch_motor) &&
                            pitch_motor->measure.state == DM_STATE_ENABLE && isfinite(position) &&
                            isfinite(speed) && isfinite(gimbal_cmd_recv.pitch);
    if (!healthy)
    {
        dm_motor_set_mit_torque(pitch_motor, 0.0f);
        gimbal_axis_reset(&pitch_controller);
        pitch_reference_initialized = 0u;
        pitch_torque_nm = 0.0f;
        return;
    }

    if (!pitch_reference_initialized)
    {
        pitch_start_rad = position;
        pitch_command_start_deg = gimbal_cmd_recv.pitch;
        pitch_target_rad = position;
        pitch_last_ms = now_ms;
        pitch_reference_initialized = 1u;
    }
    const float travel = SENTRY_PITCH_TRAVEL_DEG * FW_DEG_TO_RAD;
    const float delta = SENTRY_PITCH_DIRECTION *
                        (gimbal_cmd_recv.pitch - pitch_command_start_deg) * FW_DEG_TO_RAD;
    const float requested = pitch_start_rad + fminf(travel, fmaxf(-travel, delta));
    const float dt_s = fminf(0.02f, fmaxf(0.0f, (now_ms - pitch_last_ms) * 0.001f));
    pitch_last_ms = now_ms;
    const float step = SENTRY_PITCH_RATE_DEG_S * FW_DEG_TO_RAD * dt_s;
    pitch_target_rad += fminf(step, fmaxf(-step, requested - pitch_target_rad));

    if (gimbal_axis_update(&pitch_controller, position, speed, pitch_target_rad, now_ms))
    {
        pitch_torque_nm = pitch_controller.torque_nm;
        dm_motor_set_mit_torque(pitch_motor, pitch_torque_nm);
    }
    else
    {
        pitch_torque_nm = 0.0f;
        dm_motor_set_mit_torque(pitch_motor, 0.0f);
    }
}

static void gimbal_small_yaw_step(float now_ms)
{
    if (small_yaw_motor == NULL || !SENTRY_SMALL_YAW_ENABLE)
        return;

    const uint8_t command_active = gimbal_run_requested();
    const uint8_t online = daemon_is_online(small_yaw_motor->daemon);
    const float dt_ms = now_ms - small_yaw_last_ms;
    small_yaw_last_ms = now_ms;
    const float dt_s = (dt_ms > 0.0f && dt_ms <= 50.0f) ? dt_ms * 0.001f : 0.0f;
    small_yaw_joint_deg = gimbal_small_yaw_joint_deg();

    if (!(command_active && online))
    {
        small_yaw_target_initialized = 0u;
        dji_motor_stop(small_yaw_motor);
        dji_motor_set_ref(small_yaw_motor, 0.0f);
        return;
    }
    if (!small_yaw_target_initialized)
    {
        small_yaw_target_deg = 0.0f;
        small_yaw_target_initialized = 1u;
    }

    const float target_limit =
        SENTRY_SMALL_YAW_LIMIT_DEG - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
    small_yaw_target_deg += gimbal_cmd_recv.small_yaw_rate_deg_s * dt_s;
    MOTOR_LIMIT_MIN_MAX(small_yaw_target_deg, -target_limit, target_limit);

    const float error = small_yaw_target_deg - small_yaw_joint_deg;
    const float error_eff = fabsf(error) <= SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG
                                ? 0.0f
                                : error - copysignf(SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG, error);
    float speed_ref = SENTRY_SMALL_YAW_ANGLE_KP * error_eff;
    MOTOR_LIMIT_MIN_MAX(speed_ref, -SENTRY_SMALL_YAW_MAX_SPEED_DEG_S,
                        SENTRY_SMALL_YAW_MAX_SPEED_DEG_S);
#if SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE
    if (fabsf(small_yaw_joint_deg) >= SENTRY_SMALL_YAW_LIMIT_DEG)
        speed_ref = copysignf(SENTRY_SMALL_YAW_RETURN_DEG_S, -small_yaw_joint_deg);
#endif
    dji_motor_set_ref(small_yaw_motor, speed_ref);
    dji_motor_enable(small_yaw_motor);
}

void gimbal_task(void)
{
    const float now_ms = dwt_get_timeline_ms();
    if (msg_recv(gimbal_sub, &gimbal_cmd_recv))
        cmd_watchdog_feed(&gimbal_cmd_wd, now_ms);
    if (msg_recv(chassis_sub, &chassis_cmd_recv))
        cmd_watchdog_feed(&chassis_cmd_wd, now_ms);

    const uint8_t zero_force = gimbal_cmd_recv.mode == GIMBAL_ZERO_FORCE ||
                               cmd_watchdog_expired(&gimbal_cmd_wd, now_ms,
                                                    CMD_WATCHDOG_TIMEOUT_MS);
    if (zero_force)
        gimbal_apply_zero_force();
    else
    {
        gimbal_big_yaw_step(now_ms);
        gimbal_pitch_step(now_ms);
        gimbal_small_yaw_step(now_ms);
    }

    const uint32_t sample_saved = kernel_enter_critical();
    const float big_position = gimbal_big_yaw_position_rad();
    const uint64_t big_yaw_sample_us =
        (big_yaw_motor != NULL) ? big_yaw_motor->rx_sample_us : 0u;
    const uint64_t pitch_sample_us =
        (pitch_motor != NULL) ? pitch_motor->rx_sample_us : 0u;
    kernel_exit_critical(sample_saved);
    if (gimbal_imu_data != NULL)
        gimbal_fb.imu = *gimbal_imu_data;
    gimbal_fb.big_yaw_world_deg =
        (gimbal_imu_data != NULL) ? gimbal_imu_data->YawTotalAngle : 0.0f;
    gimbal_fb.big_yaw_rel_deg =
        remainderf(big_position - gimbal_big_yaw_align_rad(), 2.0f * FW_PI) * FW_RAD_TO_DEG;
    gimbal_fb.small_yaw_rel_deg = small_yaw_joint_deg;
    gimbal_fb.pitch_deg =
        pitch_reference_initialized
            ? (pitch_motor->measure.total_position - pitch_start_rad) * SENTRY_PITCH_DIRECTION *
                  FW_RAD_TO_DEG
            : 0.0f;
    gimbal_fb.big_yaw_torque_nm = big_yaw_torque_nm;
    gimbal_fb.pitch_state = (pitch_motor != NULL) ? pitch_motor->measure.state : 0u;
    gimbal_fb.big_yaw_state = (big_yaw_motor != NULL) ? big_yaw_motor->measure.state : 0u;
    gimbal_fb.big_yaw_online = gimbal_dm_feedback_fresh(big_yaw_motor);
    gimbal_fb.small_yaw_online =
        (small_yaw_motor != NULL) && daemon_is_online(small_yaw_motor->daemon);
    gimbal_fb.pitch_online = gimbal_dm_feedback_fresh(pitch_motor);
    gimbal_fb.big_yaw_sample_us = big_yaw_sample_us;
    /* DJI 电机未保存 CAN 回调时刻；在线时用本拍快照时间，离线由有效位拒绝。 */
    gimbal_fb.small_yaw_sample_us = (uint64_t)kernel_tick_ms() * 1000u;
    gimbal_fb.pitch_sample_us = pitch_sample_us;
    msg_publish(gimbal_pub, &gimbal_fb);

#if GIMBAL_VOFA_UART1_ENABLE
    enum
    {
        GIMBAL_VOFA_CHANNEL_COUNT = 3,
        GIMBAL_VOFA_FRAME_SIZE = (GIMBAL_VOFA_CHANNEL_COUNT + 1) * sizeof(float)
    };
    static uint8_t gimbal_vofa_frame[GIMBAL_VOFA_FRAME_SIZE];
    static uint8_t gimbal_vofa_divider;
    if (!fault_hold_active() && ++gimbal_vofa_divider >= GIMBAL_VOFA_SEND_DIVIDER)
    {
        gimbal_vofa_divider = 0u;
        if (usart_is_ready(gimbal_vofa_usart))
        {
            const float channels[GIMBAL_VOFA_CHANNEL_COUNT] = {
                (big_yaw_motor != NULL) ? big_yaw_motor->measure.total_position : 0.0f,
                (small_yaw_motor != NULL) ? (float)small_yaw_motor->measure.ecd : 0.0f,
                (pitch_motor != NULL) ? pitch_motor->measure.total_position : 0.0f,
            };
            memcpy(gimbal_vofa_frame, channels, sizeof(channels));
            gimbal_vofa_frame[sizeof(channels) + 0u] = 0x00u;
            gimbal_vofa_frame[sizeof(channels) + 1u] = 0x00u;
            gimbal_vofa_frame[sizeof(channels) + 2u] = 0x80u;
            gimbal_vofa_frame[sizeof(channels) + 3u] = 0x7fu;
            (void)usart_try_send_it(gimbal_vofa_usart, gimbal_vofa_frame,
                                    sizeof(gimbal_vofa_frame));
        }
    }
#endif
}

float gimbal_get_pitch_deg(void)
{
    return gimbal_fb.pitch_deg;
}
