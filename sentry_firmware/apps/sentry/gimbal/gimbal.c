/**
 * @file    gimbal.c
 * @brief   哨兵三轴云台:世界目标分配、大 yaw/pitch 达妙力矩环、小 yaw GM6020 相对角环
 * @layer   APP
 * @author  小猫大王
 * @note    **控制分配(核心)**:
 *            世界目标 T,大 yaw 世界角 W_big,小 yaw 机械相对角 s,IMU 世界 yaw A.
 *            s_target = clamp(wrap180(T - W_big), ±small_limit);
 *            big_target = T - s_target;
 *          即小 yaw 先吃掉它能吃掉的误差,大 yaw 只承担小 yaw 饱和后的剩余部分。
 *          这样小幅瞄准只动小 yaw(快、轻),大角度转火才动大 yaw(慢、大力矩)。
 * @note    IMU 安装位置由 SENTRY_IMU_AFTER_SMALL_YAW 选择:
 *          1 = IMU 在枪体上,A = W_big + s;0 = IMU 在大 yaw 上,A = W_big。
 * @note    大 yaw/pitch 用 DM MIT 模式但 Kp=Kd=0,只下发力矩;
 *          角度/速度/重力前馈全部在本文件计算,便于独立调度与限幅。
 */
#include "gimbal.h"
#include "gimbal_alloc.h"
#include "sentry_def.h"
#include "robot_def.h"
#include "kernel.h"
#include "dm_motor.h"
#include "dji_motor.h"
#include "controller.h"
#include "ins_task.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "general_def.h"
#include "dwt_port.h"
#include "log_port.h"
#include <math.h>

/* ---------------- 模块状态 ---------------- */

static attitude_t *gimbal_imu_data;
static dm_motor_t *big_yaw_motor;
static dm_motor_t *pitch_motor;
static dji_motor_t *small_yaw_motor;

static pid_instance_t big_yaw_angle_pid;
static pid_instance_t big_yaw_speed_pid;
static pid_instance_t pitch_angle_pid;
static pid_instance_t pitch_speed_pid;

static msg_pub_t gimbal_pub;
static msg_sub_t gimbal_sub;
static sentry_gimbal_cmd_t gimbal_cmd_recv;
static sentry_gimbal_feed_t gimbal_fb;
static cmd_watchdog_t gimbal_cmd_wd;

static float gimbal_last_ms;
static float big_yaw_torque_nm;
static float pitch_torque_nm;

/* 台架标定开关:写 1 → 大 yaw 三角波慢扫(仅零力态生效),扫完自动清 0。
 * 由 EmberProbe 变量写入触发,不参与正常运行。 */
volatile uint8_t gimbal_cal_sweep = 0u;
/* 台架动作测试:1=大 yaw 匀速旋转(模拟雷达扫描),2=小 yaw 正弦往返扫掠。
 * 写非 0 即接管云台目标,pitch 断电保持,动作结束自动清 0。 */
volatile uint8_t gimbal_test_mode = 0u;

/* ---------------- 小工具 ---------------- */

static uint8_t gimbal_dm_online(const dm_motor_t *motor)
{
    if (motor == NULL)
        return 0;
    return (motor->measure.state == DM_STATE_ENABLE) &&
           (motor->rx_refresh_dt <= (int)(SENTRY_GIMBAL_DM_LOST_MS / (float)DM_TASK_CYCLE));
}

static float gimbal_small_yaw_zero_deg(void)
{
    return (float)SENTRY_SMALL_YAW_ZERO_ECD * DJI_ECD_TO_DEG;
}

/* 小 yaw 机械相对角,deg:方向修正后,±180 */
static float gimbal_small_yaw_relative_deg(void)
{
    if (small_yaw_motor == NULL)
        return 0.0f;
    float raw = small_yaw_motor->measure.total_angle - gimbal_small_yaw_zero_deg();
    return SENTRY_SMALL_YAW_DIRECTION * sentry_gimbal_wrap180(raw);
}

/* 大 yaw 关节机械角(相对底盘零位),deg */
static float gimbal_big_yaw_joint_deg(void)
{
    if (big_yaw_motor == NULL)
        return 0.0f;
    float joint_rad = (big_yaw_motor->measure.total_position - SENTRY_BIG_YAW_ZERO_RAD) *
                      SENTRY_BIG_YAW_DIRECTION / SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    return joint_rad * FW_RAD_TO_DEG;
}

/* pitch 关节角(相对机械水平零位),deg。枪管上没有 IMU,IMU 在大 yaw 座上只测底座倾角,
 * 因此 pitch 环的反馈与目标统一用关节域(电机编码器),零位 = 机械水平。 */
static float gimbal_pitch_joint_deg(void)
{
    if (pitch_motor == NULL)
        return 0.0f;
    const float joint_rad = (pitch_motor->measure.total_position - SENTRY_PITCH_ZERO_RAD) *
                            SENTRY_PITCH_DIRECTION / SENTRY_PITCH_TRANSMISSION_RATIO;
    return joint_rad * FW_RAD_TO_DEG;
}

/**
 * @brief 使能大 yaw 的 DM 反馈，同时强制 MIT 控制量为零
 * @note 安全:反馈-only 模式不得给位置、速度或力矩控制量。
 */
static void gimbal_enable_big_yaw_feedback_only(void)
{
    if (big_yaw_motor == NULL)
        return;
    dm_motor_enable(big_yaw_motor);
    dm_motor_set_mit_angle(big_yaw_motor, 0.0f);
    dm_motor_set_mit_speed(big_yaw_motor, 0.0f);
    dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
    big_yaw_torque_nm = 0.0f;
}

/* 世界 yaw(W_big)与角速度:按 IMU 安装位置扣除小 yaw 分量 */
static void gimbal_big_yaw_world(float *world_deg, float *world_rate_deg_s, float imu_yaw_deg,
                                 float imu_yaw_rate_deg_s, float small_rel_deg, float small_rate_deg_s)
{
#if SENTRY_IMU_AFTER_SMALL_YAW
    *world_deg = imu_yaw_deg - small_rel_deg;
    *world_rate_deg_s = imu_yaw_rate_deg_s - small_rate_deg_s;
#else
    (void)small_rel_deg;
    (void)small_rate_deg_s;
    *world_deg = imu_yaw_deg;
    *world_rate_deg_s = imu_yaw_rate_deg_s;
#endif
}

void gimbal_init(void)
{
    gimbal_imu_data = ins_init(); // 幂等

    /* 大 yaw:DM-J4310,CAN1,控制 ID 5 / Master 0,MIT 力矩 */
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
    {
#if SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE
        gimbal_enable_big_yaw_feedback_only();
#else
        dm_motor_stop(big_yaw_motor);
#endif
    }

    /* pitch:DM-J4310,CAN2,控制 ID 5 / Master 0,MIT 力矩 */
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

    /* 小 yaw:GM6020,CAN2,ESC 1,相对角/速度串级 */
    motor_init_config_t small_yaw_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
                .tx_id = SENTRY_SMALL_YAW_ESC_ID,
            },
        .controller_param_init_config =
            {
                .angle_PID =
                    {
                        .Kp = SENTRY_SMALL_YAW_ANGLE_KP,
                        .Ki = 0,
                        .Kd = 0,
                        .DeadBand = SENTRY_SMALL_YAW_ANGLE_DEADBAND,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .IntegralLimit = SENTRY_SMALL_YAW_ANGLE_IL,
                        .MaxOut = SENTRY_SMALL_YAW_MAX_SPEED_DEG_S,
                    },
                .speed_PID =
                    {
                        .Kp = SENTRY_SMALL_YAW_SPEED_KP,
                        .Ki = SENTRY_SMALL_YAW_SPEED_KI,
                        .Kd = 0,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement | PID_OutputFilter,
                        .IntegralLimit = SENTRY_SMALL_YAW_SPEED_IL,
                        .MaxOut = SENTRY_SMALL_YAW_SPEED_MAXOUT,
                        .Output_LPF_RC = SENTRY_SMALL_YAW_OUTPUT_RC,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .feedforward_flag = FEEDFORWARD_NONE,
                .control_mode =
                    SENTRY_SMALL_YAW_CURRENT_MODE ? MOTOR_CONTROL_CURRENT : MOTOR_CONTROL_VOLTAGE,
                .outer_loop_type = ANGLE_LOOP,
                .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = GM6020,
    };
    small_yaw_motor = dji_motor_init(&small_yaw_config);
    if (small_yaw_motor != NULL)
        dji_motor_stop(small_yaw_motor);

    /* 大 yaw 角度/速度环 */
    pid_init(&big_yaw_angle_pid, &(pid_init_config_t){
        .Kp = SENTRY_BIG_YAW_ANGLE_KP,
        .Ki = 0,
        .Kd = 0,
        .MaxOut = SENTRY_BIG_YAW_MAX_SPEED_DEG_S,
        .DeadBand = SENTRY_BIG_YAW_ANGLE_DEADBAND,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit,
        .IntegralLimit = SENTRY_BIG_YAW_ANGLE_IL,
    });
    pid_init(&big_yaw_speed_pid, &(pid_init_config_t){
        .Kp = SENTRY_BIG_YAW_SPEED_KP,
        .Ki = SENTRY_BIG_YAW_SPEED_KI,
        .Kd = 0,
        .MaxOut = SENTRY_BIG_YAW_SPEED_MAXOUT,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                   PID_Derivative_On_Measurement | PID_OutputFilter,
        .IntegralLimit = SENTRY_BIG_YAW_SPEED_IL,
        .Output_LPF_RC = SENTRY_BIG_YAW_OUTPUT_LPF_RC,
    });

    /* pitch 角度/速度环 */
    pid_init(&pitch_angle_pid, &(pid_init_config_t){
        .Kp = SENTRY_PITCH_ANGLE_KP,
        .Ki = 0,
        .Kd = 0,
        .MaxOut = SENTRY_PITCH_MAX_SPEED_DEG_S,
        .DeadBand = SENTRY_PITCH_ANGLE_DEADBAND,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit,
        .IntegralLimit = SENTRY_PITCH_ANGLE_IL,
    });
    pid_init(&pitch_speed_pid, &(pid_init_config_t){
        .Kp = SENTRY_PITCH_SPEED_KP,
        .Ki = SENTRY_PITCH_SPEED_KI,
        .Kd = 0,
        .MaxOut = SENTRY_PITCH_SPEED_MAXOUT,
        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                   PID_Derivative_On_Measurement | PID_OutputFilter,
        .IntegralLimit = SENTRY_PITCH_SPEED_IL,
        .Output_LPF_RC = SENTRY_PITCH_OUTPUT_LPF_RC,
    });

    gimbal_pub = msg_pub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(sentry_gimbal_cmd_t));

    gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
    gimbal_last_ms = 0.0f;
}

/**
 * @brief 停止三轴输出并清 PID
 */
static void gimbal_apply_zero_force(void)
{
    if (big_yaw_motor != NULL)
    {
#if SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE
        gimbal_enable_big_yaw_feedback_only();
#else
        dm_motor_stop(big_yaw_motor);
#endif
    }
    if (pitch_motor != NULL)
        dm_motor_stop(pitch_motor);
    if (small_yaw_motor != NULL)
    {
        dji_motor_stop(small_yaw_motor);
        dji_motor_outer_loop(small_yaw_motor, ANGLE_LOOP);
    }
    pid_clear(&big_yaw_angle_pid);
    pid_clear(&big_yaw_speed_pid);
    pid_clear(&pitch_angle_pid);
    pid_clear(&pitch_speed_pid);
    big_yaw_torque_nm = 0.0f;
    pitch_torque_nm = 0.0f;
}

/* 台架标定:大 yaw 三角波慢扫。电机位置已知、IMU 给出关节真实转角 →
 * 比例 = Δ电机位置 / ΔIMU;三角波换向处的死区即同步带虚位。
 * 只在零力态被调用,±20°、5°/s、1.5 N·m 限幅,超范围/超时自动中止。 */
static void gimbal_cal_sweep_step(float now_ms, float imu_yaw_deg)
{
#if SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE
    (void)now_ms;
    (void)imu_yaw_deg;
    gimbal_cal_sweep = 0u;
    gimbal_enable_big_yaw_feedback_only();
#else
    static float t_start = -1.0f;
    static float pos_start = 0.0f;
    static float imu_start = 0.0f;
    static float log_ms = 0.0f;

    if (gimbal_cal_sweep == 0u || big_yaw_motor == NULL)
    {
        t_start = -1.0f;
        return;
    }
    if (!gimbal_dm_online(big_yaw_motor))
    {
        gimbal_cal_sweep = 0u;
        LOG_ERROR("[sweep] abort: big yaw offline");
        return;
    }

    if (t_start < 0.0f)
    {
        t_start = now_ms;
        pos_start = big_yaw_motor->measure.total_position;
        imu_start = imu_yaw_deg;
        LOG_INFO("[sweep] start pos_mrad=%d imu_cdeg=%d", (int)(pos_start * 1000.0f),
                 (int)(imu_yaw_deg * 100.0f));
    }

    /* 1 = 标准档(±20°,5°/s,1.5 N·m);2 = 强档(±14°,3°/s,3.5 N·m),用于区分“力矩不足”与“带子打滑”。 */
    const float amp = (gimbal_cal_sweep == 2u) ? 0.25f : 0.35f;
    const float speed = (gimbal_cal_sweep == 2u) ? 0.052f : 0.087f;
    const float torque_lim = (gimbal_cal_sweep == 2u) ? 3.5f : 1.5f;
    const float angle_kp = 15.0f;
    const float angle_kd = 0.3f;
    const float half = amp / speed;
    const float total = 4.0f * half; /* 两个来回 */
    const float el = (now_ms - t_start) * 0.001f;

    if (el >= total)
    {
        dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
        LOG_INFO("[sweep] done");
        gimbal_cal_sweep = 0u;
        t_start = -1.0f;
        return;
    }

    const float phase = fmodf(el, 2.0f * half);
    const float target = (phase < half) ? (pos_start - amp + speed * phase)
                                       : (pos_start + amp - speed * (phase - half));
    float torque = angle_kp * (target - big_yaw_motor->measure.total_position) -
                   angle_kd * big_yaw_motor->measure.velocity;
    MOTOR_LIMIT_MIN_MAX(torque, -torque_lim, torque_lim);

    if (fabsf(imu_yaw_deg - imu_start) > 90.0f ||
        fabsf(big_yaw_motor->measure.total_position - pos_start) > amp * 1.5f)
    {
        dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
        gimbal_cal_sweep = 0u;
        t_start = -1.0f;
        LOG_ERROR("[sweep] abort: range guard");
        return;
    }

    dm_motor_enable(big_yaw_motor);
    dm_motor_set_mit_torque(big_yaw_motor, torque);

    if (now_ms - log_ms >= 50.0f)
    {
        log_ms = now_ms;
        LOG_INFO("[sweep] pos_mrad=%d tgt_mrad=%d imu_cdeg=%d vel_mrad=%d",
                 (int)(big_yaw_motor->measure.total_position * 1000.0f),
                 (int)(target * 1000.0f), (int)(imu_yaw_deg * 100.0f),
                 (int)(big_yaw_motor->measure.velocity * 1000.0f));
    }
#endif
}

void gimbal_task(void)
{
    const float now_ms = dwt_get_timeline_ms();
    float dt_s = (now_ms - gimbal_last_ms) * 0.001f;
    gimbal_last_ms = now_ms;
    if (dt_s <= 0.0f || dt_s > 0.1f)
        dt_s = 0.005f;

    if (msg_recv(gimbal_sub, &gimbal_cmd_recv))
        cmd_watchdog_feed(&gimbal_cmd_wd, now_ms);

    /* 1. 反馈汇总 */
    const float imu_yaw_deg = (gimbal_imu_data != NULL) ? gimbal_imu_data->YawTotalAngle : 0.0f;
    const float imu_yaw_rate_deg_s = (gimbal_imu_data != NULL) ? gimbal_imu_data->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG : 0.0f;
    const float imu_pitch_deg = (gimbal_imu_data != NULL) ? gimbal_imu_data->Pitch : 0.0f;
    const float imu_pitch_rate_deg_s = (gimbal_imu_data != NULL) ? gimbal_imu_data->Gyro[IMU_AXIS_X] * FW_RAD_TO_DEG : 0.0f;
    /* pitch 关节角/角速度:枪管上没有 IMU,直接用电机编码器(零位=机械水平) */
    const float pitch_joint_deg = gimbal_pitch_joint_deg();
    const float pitch_joint_rate_deg_s =
        (pitch_motor != NULL)
            ? pitch_motor->measure.velocity * SENTRY_PITCH_DIRECTION / SENTRY_PITCH_TRANSMISSION_RATIO *
                  FW_RAD_TO_DEG
            : 0.0f;

    const float small_rel_deg = gimbal_small_yaw_relative_deg();
    const float small_rate_deg_s =
        (small_yaw_motor != NULL) ? small_yaw_motor->measure.speed_aps * SENTRY_SMALL_YAW_DIRECTION : 0.0f;
    float big_yaw_world_deg, big_yaw_world_rate_deg_s;
    gimbal_big_yaw_world(&big_yaw_world_deg, &big_yaw_world_rate_deg_s, imu_yaw_deg,
                         imu_yaw_rate_deg_s, small_rel_deg, small_rate_deg_s);

    /* 2. 断流 / 零力 */
    const uint8_t zero_force = (gimbal_test_mode == 0u) &&
                               ((gimbal_cmd_recv.mode == GIMBAL_ZERO_FORCE) ||
                                cmd_watchdog_expired(&gimbal_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS));
    if (zero_force)
    {
        gimbal_apply_zero_force();
        gimbal_cal_sweep_step(now_ms, imu_yaw_deg);
    }
    else
    {
        /* 3. 世界目标分配:粗精分离。小 yaw 只吃 ±FINE_DEG 的精细误差,
         *    其余全部交给大 yaw。与旧实现(小 yaw 吃满 ±125°)相比,小 yaw 始终居中、
         *    余量充足,大 yaw 才真正承担主跟踪。 */
        float target_world = gimbal_cmd_recv.big_yaw;
        float small_limit = SENTRY_SMALL_YAW_FINE_DEG;

        /* 台架动作测试:接管目标,便于验证扫描/限位/同步带。
         * 模式 1/2 pitch 保持断电;模式 3 单独驱动 pitch 做行程演示。 */
        float pitch_target_override = 0.0f;
        uint8_t pitch_override_active = 0u;
        if ((gimbal_test_mode == 1u || gimbal_test_mode == 2u) && pitch_motor != NULL)
            dm_motor_stop(pitch_motor);
        if (gimbal_test_mode != 0u)
        {
            static float test_ms = -1.0f;
            static float test_start_world = 0.0f;
            if (test_ms < 0.0f)
            {
                test_ms = now_ms;
                test_start_world = big_yaw_world_deg;
                LOG_INFO("[test] mode %u start", (unsigned)gimbal_test_mode);
            }
            const float test_el = (now_ms - test_ms) * 0.001f;
            if (gimbal_test_mode == 1u) /* 大 yaw 30°/s 速度指令式,2 s 加减速斜坡,14 s 约一圈 */
            {
                static float acc_deg = 0.0f;
                if (test_el <= 0.001f)
                    acc_deg = 0.0f;
                const float ramp_in = test_el / 2.0f;
                const float ramp_out = (14.0f - test_el) / 2.0f;
                float scale = (ramp_in < ramp_out) ? ramp_in : ramp_out;
                if (scale > 1.0f)
                    scale = 1.0f;
                if (scale < 0.0f)
                    scale = 0.0f;
                const float rate = 30.0f * scale;
                acc_deg += rate * dt_s;
                /* 大 yaw 独占误差(小 yaw 限幅置 0),否则领先量会被小 yaw 吃光;
                 * 领先量 = 速度/Kp + 死区,避免死区吃掉一部分速度 */
                small_limit = 0.0f;
                /* 再加 5° 抵消同步带虚位(实测 5~7°),否则小领先量会被空程吃掉 */
                target_world = big_yaw_world_deg + rate / SENTRY_BIG_YAW_ANGLE_KP +
                               SENTRY_BIG_YAW_ANGLE_DEADBAND + 5.0f;
                if (test_el > 14.0f || acc_deg > 340.0f)
                {
                    gimbal_test_mode = 0u;
                    test_ms = -1.0f;
                    LOG_INFO("[test] done");
                }
            }
            else if (gimbal_test_mode == 3u)
            {
                /* pitch 行程演示:等 3 s → 回中 → 上限 → 下限 → 上限 → 下限 → 回中。
                 * 30°/s 速率限制,每个航点停 0.8 s;大/小 yaw 保持不动。 */
                static const float wp_deg[6] = {0.0f, SENTRY_PITCH_MAX_ANGLE, SENTRY_PITCH_MIN_ANGLE,
                                                SENTRY_PITCH_MAX_ANGLE, SENTRY_PITCH_MIN_ANGLE, 0.0f};
                static float pitch_cmd = 0.0f;
                static float dwell_ms = 0.0f;
                static uint8_t wp = 0u;
                if (test_el <= 0.001f)
                {
                    pitch_cmd = gimbal_pitch_joint_deg();
                    dwell_ms = 0.0f;
                    wp = 0u;
                }
                target_world = big_yaw_world_deg;
                small_limit = 0.0f;
                if (test_el < 3.0f)
                {
                    pitch_target_override = pitch_cmd; /* 前 3 s 保持当前角 */
                    pitch_override_active = 1u;
                }
                else if (wp < 6u)
                {
                    const float step = 30.0f * dt_s;
                    const float d = wp_deg[wp] - pitch_cmd;
                    if (fabsf(d) <= step)
                    {
                        pitch_cmd = wp_deg[wp];
                        dwell_ms += dt_s * 1000.0f;
                        if (dwell_ms >= 800.0f)
                        {
                            wp++;
                            dwell_ms = 0.0f;
                        }
                    }
                    else
                    {
                        pitch_cmd += (d > 0.0f) ? step : -step;
                    }
                    pitch_target_override = pitch_cmd;
                    pitch_override_active = 1u;
                }
                else
                {
                    gimbal_test_mode = 0u;
                    test_ms = -1.0f;
                    LOG_INFO("[test] done");
                }
            }
            else if (gimbal_test_mode == 2u) /* 小 yaw ±125° 正弦,0.1 Hz,10 s */
            {
                const float amp = SENTRY_SMALL_YAW_LIMIT_DEG - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
                /* 前 2 s 从"当前小 yaw 角"平滑过渡到正弦,避免起步瞬间被拽到中位 */
                const float blend = (test_el < 2.0f) ? (test_el * 0.5f) : 1.0f;
                const float sine = amp * sinf(6.2831853f * 0.1f * test_el);
                target_world = big_yaw_world_deg + (1.0f - blend) * small_rel_deg + blend * sine;
                small_limit = amp;
                if (test_el > 10.0f)
                {
                    gimbal_test_mode = 0u;
                    test_ms = -1.0f;
                    LOG_INFO("[test] done");
                }
            }
        }
        float small_target;
        float big_target_world;
        sentry_gimbal_allocate(target_world, big_yaw_world_deg, small_limit, &small_target,
                               &big_target_world);
        /* 4. 大 yaw 角度环 → 速度环 → 力矩 */
        if (big_yaw_motor != NULL)
        {
#if SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE
            (void)big_target_world;
            gimbal_enable_big_yaw_feedback_only();
            pid_clear(&big_yaw_angle_pid);
            pid_clear(&big_yaw_speed_pid);
#else
            const float big_err = sentry_gimbal_wrap180(big_target_world - big_yaw_world_deg);
            dm_motor_enable(big_yaw_motor);
            if (gimbal_dm_online(big_yaw_motor))
            {
                float big_speed_ref =
                    pid_calculate(&big_yaw_angle_pid, big_yaw_world_deg,
                                  big_yaw_world_deg + big_err);
                float torque = pid_calculate(&big_yaw_speed_pid, big_yaw_world_rate_deg_s,
                                             big_speed_ref);
                MOTOR_LIMIT_MIN_MAX(torque, -SENTRY_BIG_YAW_MAX_TORQUE_NM,
                                    SENTRY_BIG_YAW_MAX_TORQUE_NM);
                big_yaw_torque_nm = torque;
                dm_motor_set_mit_torque(big_yaw_motor,
                                        torque * SENTRY_BIG_YAW_DIRECTION /
                                            SENTRY_BIG_YAW_TRANSMISSION_RATIO);
            }
            else
            {
                big_yaw_torque_nm = 0.0f;
                dm_motor_set_mit_torque(big_yaw_motor, 0.0f);
                pid_clear(&big_yaw_angle_pid);
                pid_clear(&big_yaw_speed_pid);
            }
#endif
        }

        /* 5. pitch 角度环 → 速度环 → 力矩 + 重力前馈(模式 1/2 断电,模式 3 用测试目标) */
        if (pitch_motor != NULL && (gimbal_test_mode == 0u || pitch_override_active))
        {
            dm_motor_enable(pitch_motor);
            if (gimbal_dm_online(pitch_motor))
            {
                /* 硬件保护:实测关节越界立即断力。连杆已在 2026-11-23 被顶断一次,
                 * 符号/参数错误时不能让角度环继续出力。 */
                if (gimbal_test_mode != 3u &&
                    (pitch_joint_deg > SENTRY_PITCH_MAX_ANGLE + 1.0f ||
                     pitch_joint_deg < SENTRY_PITCH_MIN_ANGLE - 1.0f))
                {
                    static float over_log_ms = 0.0f;
                    pitch_torque_nm = 0.0f;
                    dm_motor_set_mit_torque(pitch_motor, 0.0f);
                    pid_clear(&pitch_angle_pid);
                    pid_clear(&pitch_speed_pid);
                    if (now_ms - over_log_ms > 1000.0f)
                    {
                        over_log_ms = now_ms;
                        LOG_ERROR("[gimbal] pitch over-range %d, torque cut",
                                  (int)(pitch_joint_deg * 10.0f));
                    }
                }
                else
                {
                float pitch_target =
                    pitch_override_active ? pitch_target_override : gimbal_cmd_recv.pitch;
                MOTOR_LIMIT_MIN_MAX(pitch_target, SENTRY_PITCH_MIN_ANGLE, SENTRY_PITCH_MAX_ANGLE);
                float pitch_speed_ref =
                    pid_calculate(&pitch_angle_pid, pitch_joint_deg, pitch_target);
                float torque = pid_calculate(&pitch_speed_pid, pitch_joint_rate_deg_s, pitch_speed_ref);
                float gravity_ff =
                    SENTRY_PITCH_GRAVITY_A *
                        cosf(pitch_joint_deg * FW_DEG_TO_RAD -
                             SENTRY_PITCH_GRAVITY_PHASE_DEG * FW_DEG_TO_RAD) +
                    SENTRY_PITCH_GRAVITY_B;
                MOTOR_LIMIT_MIN_MAX(gravity_ff, -SENTRY_PITCH_GRAVITY_LIMIT_NM,
                                    SENTRY_PITCH_GRAVITY_LIMIT_NM);
                torque += gravity_ff;
                const float pitch_torque_lim =
                    (gimbal_test_mode == 3u) ? 4.0f : SENTRY_PITCH_MAX_TORQUE_NM;
                MOTOR_LIMIT_MIN_MAX(torque, -pitch_torque_lim, pitch_torque_lim);
                pitch_torque_nm = torque;
                dm_motor_set_mit_torque(pitch_motor,
                                        torque * SENTRY_PITCH_TORQUE_SIGN /
                                            SENTRY_PITCH_TRANSMISSION_RATIO);
                }
            }
            else
            {
                pitch_torque_nm = 0.0f;
                dm_motor_set_mit_torque(pitch_motor, 0.0f);
                pid_clear(&pitch_angle_pid);
                pid_clear(&pitch_speed_pid);
            }
        }

        /* 6. 小 yaw 相对角目标(最近圈展开) */
        if (small_yaw_motor != NULL)
        {
            dji_motor_enable(small_yaw_motor);
            float raw_target = gimbal_small_yaw_zero_deg() +
                               SENTRY_SMALL_YAW_DIRECTION * small_target;
            float current = small_yaw_motor->measure.total_angle;
            while (raw_target - current > 180.0f)
                raw_target -= 360.0f;
            while (raw_target - current < -180.0f)
                raw_target += 360.0f;
            dji_motor_set_ref(small_yaw_motor, raw_target);
        }
    }

    /* 7. 反馈发布(零力时也发,供 cmd 重锚定/回中) */
    if (gimbal_imu_data != NULL)
        gimbal_fb.imu = *gimbal_imu_data;
    gimbal_fb.big_yaw_world_deg = big_yaw_world_deg;
    uint32_t nav_saved = kernel_enter_critical();
    gimbal_fb.big_yaw_rel_deg = sentry_gimbal_wrap180(gimbal_big_yaw_joint_deg());
    gimbal_fb.big_yaw_sample_us = big_yaw_motor != NULL ? big_yaw_motor->rx_sample_us : 0u;
    kernel_exit_critical(nav_saved);
    gimbal_fb.small_yaw_rel_deg = small_rel_deg;
    gimbal_fb.pitch_deg = pitch_joint_deg;
    gimbal_fb.big_yaw_torque_nm = big_yaw_torque_nm;
    gimbal_fb.pitch_state = (pitch_motor != NULL) ? pitch_motor->measure.state : 0u;
    gimbal_fb.big_yaw_state = (big_yaw_motor != NULL) ? big_yaw_motor->measure.state : 0u;
    gimbal_fb.big_yaw_online = gimbal_dm_online(big_yaw_motor);
    msg_publish(gimbal_pub, (void *)&gimbal_fb);

    /* 8. 标定观测(阶段0):零力时 5 Hz 打印三轴原始反馈,供手转测同步带比/零位/虚位。
     *    整数缩放打印(rad→mrad, deg→cdeg),避免浮点占位符;只在零力状态输出,不影响控制。 */
    if (zero_force)
    {
        static float cal_log_ms = 0.0f;
        if (now_ms - cal_log_ms >= 200.0f)
        {
            cal_log_ms = now_ms;
            LOG_INFO("[cal] bigTot=%d bigRaw=%d bigVel=%d sEcd=%u sTot=%d pitchTot=%d imuY=%d imuP=%d",
                     (big_yaw_motor != NULL) ? (int)(big_yaw_motor->measure.total_position * 1000.0f) : 0,
                     (big_yaw_motor != NULL) ? (int)(big_yaw_motor->measure.position * 1000.0f) : 0,
                     (big_yaw_motor != NULL) ? (int)(big_yaw_motor->measure.velocity * 1000.0f) : 0,
                     (small_yaw_motor != NULL) ? (unsigned)small_yaw_motor->measure.ecd : 0u,
                     (small_yaw_motor != NULL) ? (int)(small_yaw_motor->measure.total_angle * 100.0f) : 0,
                     (pitch_motor != NULL) ? (int)(pitch_motor->measure.total_position * 1000.0f) : 0,
                     (int)(imu_yaw_deg * 100.0f), (int)(imu_pitch_deg * 100.0f));
        }
    }

    (void)dt_s; /* 本批力矩环不使用显式 dt;PID 内部走 DWT */
}

float gimbal_get_pitch_deg(void)
{
    return (gimbal_imu_data != NULL) ? gimbal_imu_data->Pitch : 0.0f;
}
