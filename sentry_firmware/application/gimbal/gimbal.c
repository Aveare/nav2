/**
 * @file    gimbal.c
 * @brief   云台应用:双轴闭环、前馈与模式切换
 * @layer   APP
 * @note    哨兵路径：大yaw普通模式锁机械正前方，小陀螺模式保持IMU世界角速度为零；pitch控制电机轴。
 * @note    角度反馈直接取 IMU 原值:变化率门限无法区分"真实快速运动"与"IMU 跳变",
 *          此路已实测证伪,不要重新引入。
 * @note    2026-10-29 建立,作者 JIT CR。
 */
#include "gimbal.h"
#include "robot_def.h"
#include "dji_motor.h"
#include "dmmotor.h"
#include "sentry_control.h"
#include "ins_task.h"
#include "message_center.h"
#include "cmd_watchdog.h"
#include "bsp_dwt.h"
#include "general_def.h"
#include "telemetry.h"
#include "bmi088.h"
#include "bsp_log.h" // LOGERROR 与标定日志(RTT 不支持 %f)
#include "nav_rtos.h"

static attitude_t *gimba_IMU_data; // 云台IMU数据
static DJIMotorInstance *yaw_motor, *pitch_motor;
#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
static DMMotorInstance *sentry_big_yaw_motor;
static SentryAxisController sentry_yaw_controller;
/* 关节角与世界角分别观察；控制域随小陀螺模式切换。 */
volatile float sentry_big_yaw_world_rad;
volatile float sentry_big_yaw_torque_nm;
volatile float sentry_big_yaw_speed_target_rad_s;
volatile uint8_t sentry_big_yaw_rearm_required;
volatile float sentry_big_yaw_position_rad;
volatile float sentry_big_yaw_target_rad =
    SENTRY_BIG_YAW_DIRECTION * SENTRY_BIG_YAW_FRONT_ALIGN_RAD /
    SENTRY_BIG_YAW_TRANSMISSION_RATIO;
volatile uint8_t sentry_big_yaw_feedback_online;
volatile uint8_t sentry_big_yaw_driver_state = 0xffu; // 首帧到达前没有电机状态
volatile uint8_t sentry_big_yaw_control_active;
volatile uint8_t sentry_big_yaw_in_deadband; // 1 = 误差在到位死区内,力矩已归零
static uint8_t sentry_big_yaw_band_latched;  // 到位滞回状态(1=在带内;出带需超过退出阈值)
static float sentry_big_yaw_torque_lpf;      // 力矩指令低通状态,Nm
static float sentry_big_yaw_last_ms;         // 上周期时刻(LPF 用)
static Subscriber_t *sentry_yaw_chassis_sub;
static Chassis_Ctrl_Cmd_s sentry_yaw_chassis_cmd;
static CmdWatchdog_t sentry_yaw_chassis_wd;
static uint32_t sentry_big_yaw_imu_update_count;
static uint8_t sentry_big_yaw_imu_seen_update;
static float sentry_big_yaw_imu_last_ms;
volatile uint8_t sentry_big_yaw_world_hold_active;
volatile uint8_t sentry_big_yaw_imu_online;
volatile float sentry_big_yaw_world_target_rad; // 兼容观察量：纯速度模式随实测航向更新，不参与闭环
enum { SENTRY_BIG_YAW_MAX_FEEDBACK_AGE_CYCLES = DM_ENABLE_RETRY_MS / DM_TASK_CYCLE };
#endif
#if CHASSIS_TUNING_ONLY && SENTRY_PITCH_CONTROL_ENABLE
static DMMotorInstance *sentry_pitch_motor;
static uint8_t sentry_pitch_reference_initialized;
static float sentry_pitch_start_rad, sentry_pitch_command_start_deg;
static float sentry_pitch_last_ms;
static SentryAxisController sentry_pitch_controller;
volatile float sentry_pitch_torque_nm;
volatile float sentry_pitch_speed_target_rad_s;
volatile uint16_t sentry_pitch_position_raw;
volatile float sentry_pitch_position_rad, sentry_pitch_target_rad;
volatile uint8_t sentry_pitch_feedback_online;
volatile uint8_t sentry_pitch_driver_state = 0xffu;
volatile uint8_t sentry_pitch_control_active;
volatile uint8_t sentry_pitch_rearm_required;
#endif
#if CHASSIS_TUNING_ONLY && SENTRY_SMALL_YAW_ENABLE
static DJIMotorInstance *sentry_small_yaw_motor;
static float sentry_small_yaw_target_deg;  // 目标关节角,deg(相对机械中位)
static uint8_t sentry_small_yaw_target_init;
static float sentry_small_yaw_last_ms;
volatile float sentry_small_yaw_ref_deg_s;    // 本周期下发的速度目标,deg/s
volatile float sentry_small_yaw_speed_deg_s;  // 编码器反馈,deg/s
volatile float sentry_small_yaw_target_out_deg; // 目标关节角观察量,deg
volatile uint8_t sentry_small_yaw_feedback_online;
volatile uint8_t sentry_small_yaw_control_active;
volatile float sentry_small_yaw_joint_deg;   // 相对机械中位的关节角,deg(已解缠)
volatile uint8_t sentry_small_yaw_limited;   // 0=行程内 1=贴预留边界 2=已越界
#endif

/* yaw积分限幅取100:水平轴无重力负载,大限幅相当于蓄能弹簧,饱和后必然过冲 */
static Publisher_t *gimbal_pub;                   // 云台反馈发布者(回传给cmd)
static Subscriber_t *gimbal_sub;                  // cmd控制消息订阅者
static Gimbal_Upload_Data_s gimbal_feedback_data; // 回传给cmd的云台状态
static Gimbal_Ctrl_Cmd_s gimbal_cmd_recv;         // 来自cmd的控制信息
static CmdWatchdog_t gimbal_cmd_wd;               // 命令断流看门狗

/* 目标角速度前馈(世界系,rad/s):不叠加底盘角速度——速度环反馈是IMU世界角速度,
 * 若再减底盘转速会把世界系目标错误改成相对系目标 */
static float yaw_speed_feedforward;
static float yaw_ref_last;
static float yaw_ref_last_ms;
/* pitch 目标角速度前馈(Δref/dt+低通,DEV-20261030-11):环路稳定后重新引入,
 * 治纯P恒速跟踪滞后(运动中落后目标7.5°,松杆后再滑一下);教训见config */
static float pitch_speed_feedforward;
static float pitch_ref_last;
static float pitch_ref_last_ms;
/* pitch 重力前馈(输出侧,单位=C620 电流码):I(θ)=A·cos(θ−φ)+B,参数由 P2 标定,
 * 增益为 0 时输出恒 0,与整定前行为完全一致 */
static float pitch_gravity_ff;
#if PITCH_FF_CALIB_LOG
static uint16_t pitch_calib_cnt; // 标定日志分频计数(200Hz / 100 = 0.5s)
#endif
/* 惯量+摩擦前馈(输出域,直达电调给定):稳态自动归零,不改变平衡点 */
static float yaw_inertia_ff;
static float yaw_alpha_f;
static float yaw_gyro_last;
static float yaw_gyro_last_ms;
#if YAW_INERTIA_FF_SRC
static float yaw_vref_hold;    // 参考速度零阶保持,rad/s
static float yaw_vref_lp;      // 保持值的一阶低通,rad/s
static float yaw_vref_lp_last; // 上一周期低通值,用于差分求 α_ref
static uint8_t yaw_ref_jump;   // 本周期 ref 被判为跳变(重锚定/回中)
#endif
#if YAW_FF_CALIB_LOG
static uint16_t yaw_calib_cnt; // 标定日志分频计数(200Hz / 100 = 0.5s)
#endif

#if YAW_INERTIA_FF_SRC
/**
 * @brief 由参考速度副本求参考角加速度 α_ref(deg/s²),仅 YAW_INERTIA_FF_SRC=1 使用
 * @param dt_s 本周期时长,s
 * @note 参考速度在键鼠路径上是 14ms 一次的脉冲(只在 ref 变化那一拍非零),不能直接差分:
 *       先零阶保持把脉冲变成阶梯,再一阶低通抹平阶梯转角,最后差分。
 *       跳变周期(ref 重锚定/回中)整条链清零,与前馈的跳变判据保持一致。
 */
static float UpdateYawAlphaRef(float dt_s)
{
    if (yaw_ref_jump)
    {
        yaw_vref_hold = 0.0f;
        yaw_vref_lp = 0.0f;
        yaw_vref_lp_last = 0.0f;
        return 0.0f;
    }
    if (yaw_speed_feedforward != 0.0f)
        yaw_vref_hold = yaw_speed_feedforward; // 脉冲到来才更新,其余周期保持
    yaw_vref_lp += YAW_VREF_LP * (yaw_vref_hold - yaw_vref_lp);
    float a_ref = (yaw_vref_lp - yaw_vref_lp_last) / dt_s * RAD_2_DEGREE; // rad/s² -> deg/s²
    yaw_vref_lp_last = yaw_vref_lp;
    return a_ref;
}
#endif

/**
 * @brief 更新 yaw 输出侧前馈(惯量 + 摩擦),200Hz 由 GimbalTask 调用
 * @param now_ms 当前时刻,ms(与其它前馈共用同一时间基准)
 * @note 惯量项主语是世界系 α:yaw 轴与底盘自转同为竖直轴,惯性系欧拉方程
 *       `J·α_world = τ_motor − τ_friction(ω_joint)` 里没有 α_chassis 项,
 *       底盘加减速只改变关节转速、进而改变摩擦,已由摩擦项覆盖(见 .Doc/yaw轴深化改造计划 §3.1)。
 *       摩擦项主语是关节转速 speed_aps(GM6020 无减速比,转子速度即关节速度)。
 */
static void UpdateYawFeedforward(float now_ms)
{
    float dt_ms = now_ms - yaw_gyro_last_ms;
    float gyro_deg = gimba_IMU_data->Gyro[2] * RAD_2_DEGREE;

    if (dt_ms > 0.0f && dt_ms <= 100.0f)
    {
        float dt_s = dt_ms * 0.001f;
#if YAW_INERTIA_FF_SRC
        float a = UpdateYawAlphaRef(dt_s);
#else
        float a = (gyro_deg - yaw_gyro_last) / dt_s;
#endif
        yaw_alpha_f += YAW_INERTIA_FF_LP * (a - yaw_alpha_f);
        LIMIT_MIN_MAX(yaw_alpha_f, -YAW_INERTIA_FF_ALPHA_MAX, YAW_INERTIA_FF_ALPHA_MAX);
        yaw_inertia_ff = YAW_INERTIA_FF_GAIN * yaw_alpha_f * YAW_CURRENT_DOMAIN_SCALE
                       + YAW_FRICTION_FF_B * yaw_motor->measure.speed_aps * YAW_CURRENT_DOMAIN_SCALE;
    }
    else
        yaw_inertia_ff = 0.0f; // dt 异常周期保持 0,不用坏时间戳外推
    yaw_gyro_last = gyro_deg;
    yaw_gyro_last_ms = now_ms;
}

#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
_Static_assert(SENTRY_BIG_YAW_IMU_DIRECTION == 1.0f || SENTRY_BIG_YAW_IMU_DIRECTION == -1.0f,
               "IMU direction must be +1 or -1");
#endif
#if CHASSIS_TUNING_ONLY && (SENTRY_BIG_YAW_CONTROL_ENABLE || SENTRY_PITCH_CONTROL_ENABLE)
_Static_assert(SENTRY_BIG_YAW_TRANSMISSION_RATIO > 0.0f,
               "External transmission ratios must be positive");
_Static_assert((SENTRY_BIG_YAW_DIRECTION == 1.0f || SENTRY_BIG_YAW_DIRECTION == -1.0f) &&
               (SENTRY_PITCH_DIRECTION == 1.0f || SENTRY_PITCH_DIRECTION == -1.0f),
               "Axis direction must be +1 or -1");
/* 参考串级结构，角度rad、速度rad/s、输出Nm；不使用DJI码值增益。 */
static void InitSentryController(SentryAxisController *axis, bool yaw)
{
    const SentryAxisConfig config = {
        .angle = {
            .Kp = yaw ? SENTRY_BIG_YAW_ANGLE_KP : SENTRY_PITCH_ANGLE_KP,
            .MaxOut = (yaw ? SENTRY_BIG_YAW_MAX_SPEED_DEG_S : SENTRY_PITCH_MAX_SPEED_DEG_S) * DEGREE_2_RAD,
            .DeadBand = (yaw ? SENTRY_BIG_YAW_DEADBAND_DEG : SENTRY_PITCH_DEADBAND_DEG) * DEGREE_2_RAD,
            .Improve = PID_Derivative_On_Measurement,
            .DerivativeLimit = SENTRY_AXIS_D_LIMIT_DEG_S * DEGREE_2_RAD,
        },
        .speed = {
            .Kp = yaw ? SENTRY_BIG_YAW_SPEED_KP : SENTRY_PITCH_SPEED_KP,
            .Ki = yaw ? SENTRY_BIG_YAW_SPEED_KI : SENTRY_PITCH_SPEED_KI,
            .MaxOut = yaw ? SENTRY_BIG_YAW_TORQUE_NM : SENTRY_PITCH_TORQUE_NM,
            .IntegralLimit = yaw ? SENTRY_BIG_YAW_INTEGRAL_NM : SENTRY_PITCH_INTEGRAL_NM,
            .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_OutputFilter,
            .Output_LPF_RC = SENTRY_AXIS_OUTPUT_RC_S,
        },
        .kd_near = yaw ? SENTRY_BIG_YAW_KD_NEAR : 0.0f,
        .kd_far = yaw ? SENTRY_BIG_YAW_KD_FAR : 0.0f,
        .blend_lo_rad = SENTRY_BIG_YAW_KD_BLEND_LO_DEG * DEGREE_2_RAD,
        .blend_hi_rad = SENTRY_BIG_YAW_KD_BLEND_HI_DEG * DEGREE_2_RAD,
        .feedforward_gain = SENTRY_AXIS_FF_GAIN,
        .feedforward_lpf = SENTRY_AXIS_FF_LPF,
    };
    SentryAxisInit(axis, &config);
}
#endif

void GimbalInit()
{
    gimba_IMU_data = INS_Init(); // IMU先初始化,姿态数据指针供电机配置引用
    // YAW
    Motor_Init_Config_s yaw_config = {
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = SENTRY_BIG_YAW_DM_CAN_ID,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 24.0f * DEGREE_2_RAD,
                .Ki = 0,
                .Kd = 5.8f * DEGREE_2_RAD, // 初值,实际由UpdateYawDamping()每周期覆盖
                /* D项限幅:高速转动时截断D,避免"转得越快刹得越狠"导致不跟手 */
                .DerivativeLimit = 8.0f,
                .DeadBand = 0.5,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 100,
                .MaxOut = 1200.0f * DEGREE_2_RAD, // 目标角速度上限deg/s
            },
            .speed_PID = {
                .Kp = 20.0f / DEGREE_2_RAD, // =1146,输入/反馈均为rad/s(调度每周期覆盖)
                /* Ki 属输出侧增益,须乘域换算系数;漏乘时积分相对强 1.5 倍(电流模式),
                 * 表现为残留过冲。电压模式下系数为 1,与历史整定值完全一致 */
                .Ki = 2.0f / DEGREE_2_RAD * YAW_CURRENT_DOMAIN_SCALE,
                .Kd = 0,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 100,
                .MaxOut = YAW_ESC_OUT_MAX, // 随命令帧类型切换(电流 16384 / 电压 25000)
            },
            .other_angle_feedback_ptr = &gimba_IMU_data->YawTotalAngle,
            .other_speed_feedback_ptr = &gimba_IMU_data->Gyro[2],
            .speed_feedforward_ptr = &yaw_speed_feedforward, // 目标角速度前馈
            .current_feedforward_ptr = &yaw_inertia_ff,      // 惯量+摩擦前馈(输出侧)
        },
        .controller_setting_init_config = {
            /* 反馈必须用IMU(OTHER_FEED):编码器闭环会锁死云台相对车架角,与跟随律数学上冲突
             * (底盘会一直转到限幅而offset永不归零)。改编码器必须同时重写跟随律。 */
            .angle_feedback_source = OTHER_FEED,
            .speed_feedback_source = OTHER_FEED,
            .feedforward_flag = CURRENT_AND_SPEED_FEEDFORWARD,
            /* 命令帧类型由 config 的 YAW_ESC_CURRENT_MODE 选择,驱动侧按此决定发
             * 0x1FE/0x2FE(电流)还是 0x1FF/0x2FF(电压);同发送分组内混用会 fail-fast。
             * 2026-10 电流帧实测零扭矩(电调固件不支持),故默认仍走电压帧 */
            .control_mode = YAW_ESC_CURRENT_MODE ? MOTOR_CONTROL_CURRENT : MOTOR_CONTROL_VOLTAGE,
            .outer_loop_type = ANGLE_LOOP,
            .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = GM6020};
    // PITCH
    Motor_Init_Config_s pitch_config = {
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = SENTRY_PITCH_DM_CAN_ID,
        },
        .controller_param_init_config = {
            .angle_PID = {
                .Kp = 8.0f * DEGREE_2_RAD, // deg误差输出rad/s
                .Ki = 0,
                .Kd = 0,
                .DeadBand = 0.3f, // 到位即停(误差<0.3°不再修正):杀特定角度的小极限环(轻碰即消失的
                                  // 稳定小振荡,DEV-09);yaw同款0.5°先例。过大会表现为准位偏差
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .IntegralLimit = 100,
                .MaxOut = 180.0f * DEGREE_2_RAD,
            },
            .speed_PID = {
                .Kp = 50.0f / DEGREE_2_RAD, // 反馈为Gyro[0]的rad/s
                .Ki = 10.0f / DEGREE_2_RAD, // 折中档:20/D2R低频喘振("一顿一顿"),2/D2R起步
                                            // 力矩建立慢(上推延迟,DEV-08实测);仍喘→5/D2R,仍延迟→15/D2R
                .Kd = 0,
                /* 输出低通:上抬(高电流工作点)中陀螺噪声xKp2865的力矩纹波被机械啮合放大成
                 * 异响(DEV-10,只上抬有/下抬无/停即消);RC=0.008s@1kHz截止~20Hz,推杆动态(<5Hz)无损。
                 * DEV-20261105-04:水平及以上角度上抬异响复发(连续嗡嗡/上抬有/停即消,DEV-10特征),
                 * 按调参档案§2路径RC 0.005→0.008。上抬变"肉"→RC回调0.006;异响仍在→转重力前馈
                 * (须先架空松杆标定各角度稳态电流,DEC台账:DEV-05盲上否决先例) */
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement | PID_OutputFilter,
                .Output_LPF_RC = 0.01f,
                .IntegralLimit = 1200, // MaxOut的30%:600实测扛不住水平附近重力(特定角度极限环+
                                       // 回水平不到位,DEV-09),提至1200;若喘振加重→重力前馈接管
                .MaxOut = 8000, // C620原始电流给定。2026-11-15 判据实验:4000→8000(满量程16384≈±20A,
                                // 8000≈±9.8A)。背景:最低位 -26.45° 处 Output 顶 4000 连续 12s、θ 纹丝不动、
                                // 实际电流 4018 码,判断力不够。若 8000 仍不动→机械楔死/供给限流,须回退并查机械;
                                // 若 8000 能动但发热上升→缺力属实,前馈标定接管后回退到 4000~6000。
            },
            .other_angle_feedback_ptr = &gimba_IMU_data->Pitch,
            .other_speed_feedback_ptr = (&gimba_IMU_data->Gyro[0]),
            .speed_feedforward_ptr = &pitch_speed_feedforward, // 目标角速度前馈(治松杆续滑,DEV-11)
            .current_feedforward_ptr = &pitch_gravity_ff,      // 重力前馈(输出侧,P2 标定后启用)
        },
        .controller_setting_init_config = {
            .angle_feedback_source = OTHER_FEED,
            .speed_feedback_source = OTHER_FEED,
            .feedforward_flag = CURRENT_AND_SPEED_FEEDFORWARD,
            .outer_loop_type = ANGLE_LOOP,
            .close_loop_type = SPEED_LOOP | ANGLE_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = M3508,
    };
#if CHASSIS_TUNING_ONLY
    (void)yaw_config;
    (void)pitch_config;
#if SENTRY_BIG_YAW_CONTROL_ENABLE
    MIT_Init_Config_s sentry_big_yaw_config = {
        .controller_param_init_config = {
            .Kp = 0.0f, // 仅发送外部PID计算的力矩，禁止叠加电机位置PD
            .Kd = 0.0f,
        },
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .can_init_config = {
            .can_handle = &hcan1,
            .tx_id = SENTRY_BIG_YAW_DM_CAN_ID,
            .rx_id = SENTRY_BIG_YAW_DM_MASTER_ID,
        },
        .torque_min = -SENTRY_BIG_YAW_TORQUE_RANGE_NM,
        .torque_max = SENTRY_BIG_YAW_TORQUE_RANGE_NM,
    };
    InitSentryController(&sentry_yaw_controller, true);
    sentry_big_yaw_motor = MITMotorInit(&sentry_big_yaw_config);
    if (sentry_big_yaw_motor != NULL)
        DMMotorStop(sentry_big_yaw_motor);
    else
        LOGERROR("[sentry] big yaw DM registration failed");
#endif
#if SENTRY_PITCH_CONTROL_ENABLE
    MIT_Init_Config_s sentry_pitch_config = {
        .controller_param_init_config = {
            .Kp = 0.0f,
            .Kd = 0.0f,
        },
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = SENTRY_PITCH_DM_CAN_ID,
            .rx_id = SENTRY_PITCH_DM_MASTER_ID,
        },
        /* 显式声明力矩映射量程,不再依赖驱动默认 ±10:量程必须与驱动器 TMAX 一致,
         * 不一致时指令出力与反馈力矩都会按比例缩放(见 config 注释)。 */
        .torque_min = -SENTRY_PITCH_TORQUE_RANGE_NM,
        .torque_max = SENTRY_PITCH_TORQUE_RANGE_NM,
    };
    InitSentryController(&sentry_pitch_controller, false);
    sentry_pitch_motor = MITMotorInit(&sentry_pitch_config);
    if (sentry_pitch_motor != NULL)
        DMMotorStop(sentry_pitch_motor);
    else
        LOGERROR("[sentry] pitch DM registration failed");
#endif
#if SENTRY_SMALL_YAW_ENABLE
    /* 小 yaw:GM6020,速度环常开做自由稳像。反馈用电机自身编码器(相对大 yaw 的速度),
     * 稳定所需的"母体世界角速度"不进反馈,而是从参考值注入(见 SentrySmallYawTask)。 */
    Motor_Init_Config_s small_yaw_config = {
        .can_init_config = {
            .can_handle = SENTRY_SMALL_YAW_CAN_BUS == 2u ? &hcan2 : &hcan1,
            .tx_id = SENTRY_SMALL_YAW_ESC_ID,
        },
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = SENTRY_SMALL_YAW_SPEED_KP, // 输出码/(deg/s)
                .Ki = SENTRY_SMALL_YAW_SPEED_KI, // 自由轴刻意不给积分,理由见config
                .Kd = 0,
                .DeadBand = SENTRY_SMALL_YAW_DEADBAND_DEG_S,
                .Improve = PID_Integral_Limit | PID_OutputFilter,
                .IntegralLimit = SENTRY_SMALL_YAW_INTEGRAL_LIMIT,
                .Output_LPF_RC = SENTRY_SMALL_YAW_OUTPUT_RC_S,
                .MaxOut = SENTRY_SMALL_YAW_MAX_OUT,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .control_mode = SENTRY_SMALL_YAW_CURRENT_MODE ? MOTOR_CONTROL_CURRENT : MOTOR_CONTROL_VOLTAGE,
            .outer_loop_type = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP,
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = GM6020,
    };
    sentry_small_yaw_motor = DJIMotorInit(&small_yaw_config);
    if (sentry_small_yaw_motor != NULL)
        DJIMotorStop(sentry_small_yaw_motor);
    else
        LOGERROR("[sentry] small yaw GM6020 registration failed");
#endif
#else
    // 上电时电机闭环在当前角度,保持静止,收到遥控数据再动;先零力,按CMD入口解锁
    yaw_motor = DJIMotorInit(&yaw_config);
    pitch_motor = DJIMotorInit(&pitch_config);
    DJIMotorStop(yaw_motor);
    DJIMotorStop(pitch_motor);
#endif

    gimbal_pub = PubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    gimbal_sub = SubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
    sentry_yaw_chassis_sub = SubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
#endif
}

/* 按角度误差|err|调度yaw角度环Kd:远离目标低Kd保跟手,接近目标全Kd防松杆回弹;
 * 区间内线性过渡保证Kd连续(跳变会让速度目标突变,激起振荡) */
static void UpdateYawDamping(void)
{
    float a = fabsf(gimbal_cmd_recv.yaw - gimba_IMU_data->YawTotalAngle);
    float kd;
    if (a >= YAW_KD_BLEND_HI)
        kd = YAW_KD_FAR;
    else if (a <= YAW_KD_BLEND_LO)
        kd = YAW_KD_NEAR;
    else
        kd = YAW_KD_FAR + (YAW_KD_NEAR - YAW_KD_FAR) *
                              (a - YAW_KD_BLEND_LO) / (YAW_KD_BLEND_HI - YAW_KD_BLEND_LO);
    yaw_motor->motor_controller.angle_PID.Kd = kd * DEGREE_2_RAD;
}

#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
static void SentryBigYawTask(float now_ms)
{
    DMMotorInstance *motor = sentry_big_yaw_motor;
    if (motor == NULL) return;
    const float fixed_target = SENTRY_BIG_YAW_DIRECTION * SENTRY_BIG_YAW_FRONT_ALIGN_RAD /
                              SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    const bool chassis_fresh = !CmdWatchdogExpired(&sentry_yaw_chassis_wd, now_ms,
                                                  CMD_WATCHDOG_TIMEOUT_MS);
    const bool command_active = chassis_fresh &&
        sentry_yaw_chassis_cmd.chassis_mode != CHASSIS_ZERO_FORCE &&
        (gimbal_cmd_recv.gimbal_mode == GIMBAL_GYRO_MODE ||
         gimbal_cmd_recv.gimbal_mode == GIMBAL_FREE_MODE);
    const bool world_requested = sentry_yaw_chassis_cmd.chassis_mode == CHASSIS_ROTATE;
    const bool online = motor->rx_init_flag &&
        motor->rx_refresh_dt < SENTRY_BIG_YAW_MAX_FEEDBACK_AGE_CYCLES;
    const float position = SENTRY_BIG_YAW_DIRECTION * motor->measure.total_position /
                           SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    const float speed = SENTRY_BIG_YAW_DIRECTION * motor->measure.velocity /
                        SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    const float world = SENTRY_BIG_YAW_IMU_DIRECTION * gimba_IMU_data->YawTotalAngle * DEGREE_2_RAD;
    /* C板只随大yaw转；水平安装时Z轴角速度对应航向角速度，单位rad/s。 */
    const float world_speed = SENTRY_BIG_YAW_IMU_DIRECTION * gimba_IMU_data->Gyro[2];
    /* 跨INS任务明确读取快照；低位只用于判断推进，回绕到0仍然有效。 */
    const uint64_t imu_snapshot = *(const volatile uint64_t *)&QEKF_INS.UpdateCount;
    const uint32_t imu_updates = (uint32_t)imu_snapshot;
    if (imu_updates != sentry_big_yaw_imu_update_count)
    {
        sentry_big_yaw_imu_seen_update = 1u;
        sentry_big_yaw_imu_update_count = imu_updates;
        sentry_big_yaw_imu_last_ms = now_ms;
    }
    const float imu_age_ms = now_ms - sentry_big_yaw_imu_last_ms;
    sentry_big_yaw_imu_online = QEKF_INS.Initialized && sentry_big_yaw_imu_seen_update &&
        imu_age_ms >= 0.0f && imu_age_ms <= SENTRY_BIG_YAW_IMU_TIMEOUT_MS &&
        isfinite(world) && isfinite(world_speed);
    const bool healthy = online && motor->measure.state == DM_STATE_ENABLE &&
        isfinite(position) && isfinite(speed) &&
        (!world_requested || sentry_big_yaw_imu_online);
    sentry_big_yaw_position_rad = position;
    sentry_big_yaw_world_rad = world;
    sentry_big_yaw_feedback_online = online;
    sentry_big_yaw_driver_state = motor->rx_init_flag ? motor->measure.state : 0xffu;
    if (!command_active) sentry_big_yaw_rearm_required = 0u;
    else if (sentry_big_yaw_control_active && !healthy) sentry_big_yaw_rearm_required = 1u;

    if (healthy && command_active && !sentry_big_yaw_rearm_required)
    {
        if (world_requested != (sentry_big_yaw_world_hold_active != 0u))
        {
            /* 坐标域切换时清历史，避免旧积分/微分和力矩低通造成瞬间冲击。 */
            SentryAxisReset(&sentry_yaw_controller);
            sentry_big_yaw_band_latched = 0u;
            sentry_big_yaw_torque_lpf = 0.0f;
            sentry_big_yaw_last_ms = now_ms;
            sentry_big_yaw_world_hold_active = world_requested ? 1u : 0u;
        }
        /* 小陀螺不保存角度目标，观察值随实测变化；只有普通回正计算角度误差。 */
        if (world_requested) sentry_big_yaw_world_target_rad = world;
        const float error = world_requested ? 0.0f : remainderf(fixed_target - position, PI2);
        const float target_for_ctrl = position + error;
        sentry_big_yaw_target_rad = position + error; // 当前等效关节目标，rad

        if (!world_requested)
        {
            if (fabsf(error) <= SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG * DEGREE_2_RAD)
                sentry_big_yaw_band_latched = 1u;
            else if (fabsf(error) >= SENTRY_BIG_YAW_TARGET_DEADBAND_DEG * DEGREE_2_RAD)
                sentry_big_yaw_band_latched = 0u;
        }
        /* 稳像期间持续闭环，即使误差为零也保留速度阻尼，不能到位撤力。 */
        const bool in_deadband = !world_requested && sentry_big_yaw_band_latched;
        sentry_big_yaw_in_deadband = in_deadband ? 1u : 0u;
        const bool updated = world_requested
            ? SentryAxisUpdateSpeed(&sentry_yaw_controller, world_speed, 0.0f, now_ms)
            : SentryAxisUpdate(&sentry_yaw_controller, position, speed,
                               in_deadband ? position : target_for_ctrl, now_ms);
        if (updated)
        {
            float torque_cmd = sentry_yaw_controller.torque_nm;
            if (in_deadband)
            {
                PIDClear(&sentry_yaw_controller.angle);
                PIDClear(&sentry_yaw_controller.speed);
                torque_cmd = 0.0f;
                sentry_big_yaw_speed_target_rad_s = 0.0f;
            }
            else
                sentry_big_yaw_speed_target_rad_s = sentry_yaw_controller.speed_target_rad_s;

            const float dt_s = (now_ms - sentry_big_yaw_last_ms) * 0.001f;
            sentry_big_yaw_last_ms = now_ms;
            if (dt_s > 0.0f && dt_s < 0.1f)
            {
                const float rc = SENTRY_BIG_YAW_TORQUE_LPF_RC_S;
                const float alpha = (rc > 0.0f) ? dt_s / (rc + dt_s) : 1.0f;
                sentry_big_yaw_torque_lpf += alpha * (torque_cmd - sentry_big_yaw_torque_lpf);
            }
            sentry_big_yaw_torque_nm = sentry_big_yaw_torque_lpf;
            DMMotorSetMITTorque(motor, sentry_big_yaw_torque_nm * SENTRY_BIG_YAW_DIRECTION /
                                       SENTRY_BIG_YAW_TRANSMISSION_RATIO);
            DMMotorEnable(motor);
            sentry_big_yaw_control_active = 1u;
            return;
        }
        sentry_big_yaw_rearm_required = 1u;
    }
    DMMotorStop(motor);
    DMMotorSetMITTorque(motor, 0.0f);
    SentryAxisReset(&sentry_yaw_controller);
    sentry_big_yaw_torque_nm = sentry_big_yaw_speed_target_rad_s = 0.0f;
    sentry_big_yaw_torque_lpf = 0.0f;
    sentry_big_yaw_last_ms = now_ms;
    sentry_big_yaw_band_latched = 0u;
    sentry_big_yaw_target_rad = fixed_target;
    sentry_big_yaw_control_active = 0u;
    sentry_big_yaw_world_hold_active = 0u;
    sentry_big_yaw_in_deadband = 0u;
}
#endif

#if CHASSIS_TUNING_ONLY && SENTRY_PITCH_CONTROL_ENABLE
static void SentryPitchTask(float now_ms)
{
    DMMotorInstance *motor = sentry_pitch_motor;
    if (motor == NULL) return;
    const bool online = motor->rx_init_flag &&
        motor->rx_refresh_dt < (int)(DM_ENABLE_RETRY_MS / DM_TASK_CYCLE);
    const bool command_active = gimbal_cmd_recv.gimbal_mode == GIMBAL_GYRO_MODE ||
                                gimbal_cmd_recv.gimbal_mode == GIMBAL_FREE_MODE;
    const bool healthy = online && motor->measure.state == DM_STATE_ENABLE &&
        isfinite(motor->measure.total_position) && isfinite(motor->measure.velocity) &&
        isfinite(gimbal_cmd_recv.pitch);
    /* 连杆几何未知，闭环量仅为4310输出轴角/速度，不宣称是实际俯仰角。 */
    const float position = motor->measure.total_position;
    const float speed = motor->measure.velocity;
    const float dt = fminf(0.02f, fmaxf(0.0f, (now_ms - sentry_pitch_last_ms) * 0.001f));
    sentry_pitch_last_ms = now_ms;
    sentry_pitch_position_raw = motor->measure.position_raw;
    sentry_pitch_position_rad = position;
    sentry_pitch_feedback_online = online;
    sentry_pitch_driver_state = motor->rx_init_flag ? motor->measure.state : 0xffu;
    if (!command_active) sentry_pitch_rearm_required = 0u;
    else if (sentry_pitch_control_active && !healthy) sentry_pitch_rearm_required = 1u;

    if (healthy && command_active && !sentry_pitch_rearm_required)
    {
        if (!sentry_pitch_reference_initialized)
        {
            sentry_pitch_start_rad = position;
            sentry_pitch_command_start_deg = gimbal_cmd_recv.pitch;
            sentry_pitch_target_rad = position;
            sentry_pitch_reference_initialized = 1u;
        }
        const float travel = SENTRY_PITCH_TRAVEL_DEG * DEGREE_2_RAD;
        const float delta = SENTRY_PITCH_DIRECTION *
                            (gimbal_cmd_recv.pitch - sentry_pitch_command_start_deg) * DEGREE_2_RAD;
        const float requested = sentry_pitch_start_rad + fminf(travel, fmaxf(-travel, delta));
        const float step = SENTRY_PITCH_RATE_DEG_S * DEGREE_2_RAD * dt;
        sentry_pitch_target_rad += fminf(step, fmaxf(-step, requested - sentry_pitch_target_rad));
        if (SentryAxisUpdate(&sentry_pitch_controller, position, speed, sentry_pitch_target_rad, now_ms))
        {
            sentry_pitch_torque_nm = sentry_pitch_controller.torque_nm;
            sentry_pitch_speed_target_rad_s = sentry_pitch_controller.speed_target_rad_s;
            DMMotorSetMITTorque(motor, sentry_pitch_torque_nm);
            DMMotorEnable(motor);
            sentry_pitch_control_active = 1u;
            return;
        }
        sentry_pitch_rearm_required = 1u;
    }
    DMMotorStop(motor);
    DMMotorSetMITTorque(motor, 0.0f);
    SentryAxisReset(&sentry_pitch_controller);
    sentry_pitch_reference_initialized = 0u;
    sentry_pitch_torque_nm = sentry_pitch_speed_target_rad_s = 0.0f;
    sentry_pitch_target_rad = position;
    sentry_pitch_control_active = 0u;
}
#endif

#if CHASSIS_TUNING_ONLY && SENTRY_SMALL_YAW_ENABLE
/**
 * @brief 小 yaw 位置保持:角度环锁目标(中位 0 点 + 摇杆增量),手推不动
 * @note  控制链:target_deg(相对中位) → 角度 P → 速度目标 → 驱动内速度环 → 电压码。
 *        速度环反馈是编码器相对速度(1kHz 本地环),角度环在 C 板 200Hz。
 * @note  **不做世界系保持**:IMU 装在大yaw上，量不到小yaw独立转动;
 *        小yaw保持相对大yaw的关节目标，大yaw是否稳像由其自身模式决定。
 * @note  目标角在限位内夹紧(±(LIMIT−MARGIN)),正常操作到不了机械端;关节角若已被外力
 *        推过 ±LIMIT 则走固定速度回程,而不是继续按角度环追。
 * @note  解锁首拍目标固定为 0(中位),即"回正"行为;之后只由摇杆增量改变。
 */
static void SentrySmallYawTask(float now_ms)
{
    DJIMotorInstance *motor = sentry_small_yaw_motor;
    if (motor == NULL) return;

    const bool command_active = gimbal_cmd_recv.gimbal_mode == GIMBAL_GYRO_MODE ||
                                gimbal_cmd_recv.gimbal_mode == GIMBAL_FREE_MODE;
    const bool online = DaemonIsOnline(motor->daemon);
    sentry_small_yaw_feedback_online = online ? 1u : 0u;
    sentry_small_yaw_speed_deg_s = motor->measure.speed_aps;

    /* 周期时间:目标积分用;异常值(负/超长)本拍不积分,避免 dt 抖动把目标推飞 */
    const float dt_ms = now_ms - sentry_small_yaw_last_ms;
    sentry_small_yaw_last_ms = now_ms;
    const float dt_s = (dt_ms > 0.0f && dt_ms <= 50.0f) ? dt_ms * 0.001f : 0.0f;

    if (!(command_active && online))
    {
        sentry_small_yaw_target_init = 0u; // 掉线/急停后重新解锁时回到中位
        DJIMotorStop(motor);
        DJIMotorSetRef(motor, 0);
        sentry_small_yaw_ref_deg_s = 0.0f;
        sentry_small_yaw_control_active = 0u;
        sentry_small_yaw_limited = 0u;
        return;
    }

    /* 相对中位解缠后的关节角:中位 ecd 靠近回绕点,裸比较会在零点处误判(见 config) */
    const float c = (float)motor->measure.ecd - (float)SENTRY_SMALL_YAW_CENTER_ECD;
    const float wrapped = (c > 4096.0f) ? (c - 8192.0f) : ((c < -4096.0f) ? (c + 8192.0f) : c);
    const float joint_deg = wrapped * (360.0f / 8192.0f);
    sentry_small_yaw_joint_deg = joint_deg;

    /* 目标角:解锁首拍归零(回正),之后按摇杆比例积分,并夹在行程余量内 */
    if (!sentry_small_yaw_target_init)
    {
        sentry_small_yaw_target_deg = 0.0f;
        sentry_small_yaw_target_init = 1u;
    }
    const float target_limit = SENTRY_SMALL_YAW_LIMIT_DEG - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
    sentry_small_yaw_target_deg += gimbal_cmd_recv.small_yaw_rate_deg_s * dt_s;
    if (sentry_small_yaw_target_deg > target_limit) sentry_small_yaw_target_deg = target_limit;
    else if (sentry_small_yaw_target_deg < -target_limit) sentry_small_yaw_target_deg = -target_limit;
    sentry_small_yaw_target_out_deg = sentry_small_yaw_target_deg;

    /* 角度 P:软化死区(扣掉带宽)保证边界出力连续,不会在死区边缘跳变 */
    const float err = sentry_small_yaw_target_deg - joint_deg;
    const float db = SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG;
    float err_eff = (fabsf(err) <= db) ? 0.0f : (err - ((err > 0.0f) ? db : -db));
    float ref = SENTRY_SMALL_YAW_ANGLE_KP * err_eff;
    if (ref > SENTRY_SMALL_YAW_MAX_SPEED_DEG_S) ref = SENTRY_SMALL_YAW_MAX_SPEED_DEG_S;
    else if (ref < -SENTRY_SMALL_YAW_MAX_SPEED_DEG_S) ref = -SENTRY_SMALL_YAW_MAX_SPEED_DEG_S;

#if SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE
    if (fabsf(joint_deg) >= SENTRY_SMALL_YAW_LIMIT_DEG)
    {
        /* 关节角已被推过机械限位(装配偏置/外力硬推):放弃角度环,按固定速度往行程内拉 */
        ref = (joint_deg > 0.0f ? -1.0f : 1.0f) * SENTRY_SMALL_YAW_RETURN_DEG_S;
        sentry_small_yaw_limited = 2u;
    }
    else if (fabsf(joint_deg) >= target_limit)
    {
        sentry_small_yaw_limited = 1u; // 已抵近预留余量边界
    }
    else
    {
        sentry_small_yaw_limited = 0u;
    }
#endif

    sentry_small_yaw_ref_deg_s = ref;
    DJIMotorSetRef(motor, ref);
    DJIMotorEnable(motor);
    sentry_small_yaw_control_active = 1u;
}
#endif

/* 云台控制核心任务(200Hz) */
void GimbalTask()
{
    // 获取云台控制数据(收到才刷新看门狗)
    const float cmd_now_ms = DWT_GetTimeline_ms(); // 喂狗与判活用同一时刻
    if (SubGetMessage(gimbal_sub, &gimbal_cmd_recv))
        CmdWatchdogFeed(&gimbal_cmd_wd, cmd_now_ms);

    // 命令断流:超时未收到命令切零力,避免云台按最后一帧目标持续出力
    if (CmdWatchdogExpired(&gimbal_cmd_wd, cmd_now_ms, CMD_WATCHDOG_TIMEOUT_MS))
        gimbal_cmd_recv.gimbal_mode = GIMBAL_ZERO_FORCE;

#if CHASSIS_TUNING_ONLY
#if SENTRY_BIG_YAW_CONTROL_ENABLE
    if (SubGetMessage(sentry_yaw_chassis_sub, &sentry_yaw_chassis_cmd))
        CmdWatchdogFeed(&sentry_yaw_chassis_wd, cmd_now_ms);
    SentryBigYawTask(cmd_now_ms);
    gimbal_feedback_data.yaw_motor_single_round_angle =
        sentry_big_yaw_position_rad * RAD_2_DEGREE;
#endif
#if SENTRY_PITCH_CONTROL_ENABLE
    SentryPitchTask(cmd_now_ms);
#endif
#if SENTRY_SMALL_YAW_ENABLE
    SentrySmallYawTask(cmd_now_ms);
#endif
    gimbal_feedback_data.gimbal_imu_data = *gimba_IMU_data;
    PubPushMessage(gimbal_pub, (void *)&gimbal_feedback_data);
    return;
#endif

    /* 目标角速度前馈 = 本周期ref增量/dt:自转中推杆时不必先积累大误差才凑出角速度;
     * 单周期增量超过合法上限 -> 目标跳变(重锚定/回中),本周期前馈给0 */
    {
        float dt_ms = cmd_now_ms - yaw_ref_last_ms;
        float delta_ref = gimbal_cmd_recv.yaw - yaw_ref_last;
        if (dt_ms > 0.0f && dt_ms <= 100.0f && fabsf(delta_ref) <= YAW_REF_FF_MAX_DELTA)
            yaw_speed_feedforward =
                YAW_REF_FF_GAIN * delta_ref / (dt_ms * 0.001f) * DEGREE_2_RAD;
        else
            yaw_speed_feedforward = 0.0f;
#if YAW_INERTIA_FF_SRC
        /* 跳变判据同时用于 α_ref 链的清零,避免重锚定后参考速度副本拖着旧值 */
        yaw_ref_jump = (dt_ms <= 0.0f || dt_ms > 100.0f || fabsf(delta_ref) > YAW_REF_FF_MAX_DELTA);
#endif
        yaw_ref_last = gimbal_cmd_recv.yaw;
        yaw_ref_last_ms = cmd_now_ms;
    }

    /* pitch目标角速度前馈 = Δref/dt(一阶低通):运动中前馈直接给出目标速度,
     * 消除纯P恒速跟踪滞后(松杆后再滑一下,DEV-20261030-11)。
     * 阈值按DBUS 14ms帧周期算(config);跳变(重锚定/限位钳位)立即清零,正常推杆走低通 */
    {
        float dt_p_ms = cmd_now_ms - pitch_ref_last_ms;
        float delta_ref_p = gimbal_cmd_recv.pitch - pitch_ref_last;
        if (dt_p_ms > 0.0f && dt_p_ms <= 100.0f && fabsf(delta_ref_p) <= PITCH_REF_FF_MAX_DELTA)
        {
            float ff_target = PITCH_REF_FF_GAIN * delta_ref_p / (dt_p_ms * 0.001f) * DEGREE_2_RAD;
            pitch_speed_feedforward += PITCH_REF_FF_LP * (ff_target - pitch_speed_feedforward);
        }
        else
            pitch_speed_feedforward = 0.0f; // 重锚定/限位钳位:立即清零,不低通拖尾
        pitch_ref_last = gimbal_cmd_recv.pitch;
        pitch_ref_last_ms = cmd_now_ms;
    }

    /* pitch 重力前馈 = A·cos(θ−φ)+B,θ 取 IMU 欧拉 Pitch(重力是世界系量,不能用编码器角)。
     * 前馈自成一路:不受速度环 MaxOut 钳制(叠加发生在速度环之后),故在此独立限幅。
     * GAIN=0 时输出恒 0,与整定前行为一致;参数未标定前严禁调大(DEC-004:盲上已否决一次)。 */
    {
        float g = PITCH_GRAVITY_A * cosf((gimba_IMU_data->Pitch - PITCH_GRAVITY_PHASE) * DEGREE_2_RAD)
                  + PITCH_GRAVITY_OFFSET;
        LIMIT_MIN_MAX(g, -PITCH_GRAVITY_FF_LIMIT, PITCH_GRAVITY_FF_LIMIT);
        pitch_gravity_ff = PITCH_GRAVITY_FF_GAIN * g;
    }

    /* 惯量+摩擦前馈:累加 α(世界系)与关节转速,输出到电调给定域;细节与域约定见函数注释 */
    UpdateYawFeedforward(cmd_now_ms);

    // 两个调度必须在ref确定后、电机任务PID计算前刷新
    UpdateYawDamping();

    /* 速度环Kp按电机相对转速调度:高速区反电动势侵蚀扭矩,线性提Kp补扭矩;
     * 低速区维持历史稳定值,零风险 */
    {
        float rel = fabsf(yaw_motor->measure.speed_aps);
        float kp_dps;
        if (rel >= YAW_SPEED_KP_BLEND_HI)
            kp_dps = YAW_SPEED_KP_HIGH;
        else if (rel <= YAW_SPEED_KP_BLEND_LO)
            kp_dps = YAW_SPEED_KP_LOW;
        else
            kp_dps = YAW_SPEED_KP_LOW + (YAW_SPEED_KP_HIGH - YAW_SPEED_KP_LOW) *
                                          (rel - YAW_SPEED_KP_BLEND_LO) /
                                          (YAW_SPEED_KP_BLEND_HI - YAW_SPEED_KP_BLEND_LO);
        yaw_motor->motor_controller.speed_PID.Kp = kp_dps / DEGREE_2_RAD * YAW_CURRENT_DOMAIN_SCALE;
    }

    switch (gimbal_cmd_recv.gimbal_mode)
    {
    case GIMBAL_ZERO_FORCE: // 停止
        DJIMotorStop(yaw_motor);
        DJIMotorStop(pitch_motor);
        break;
    case GIMBAL_GYRO_MODE: // IMU反馈闭环(小陀螺/跟随均用此模式)
    // GIMBAL_FREE_MODE 已裁剪(与GYRO_MODE逻辑相同),枚举保留协议兼容,需要时恢复同分支
    case GIMBAL_FREE_MODE:
        DJIMotorEnable(yaw_motor);
#if !CHASSIS_TUNING_ONLY
        DJIMotorEnable(pitch_motor);
#endif
        DJIMotorChangeFeed(yaw_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(yaw_motor, SPEED_LOOP, OTHER_FEED);
#if !CHASSIS_TUNING_ONLY
        DJIMotorChangeFeed(pitch_motor, ANGLE_LOOP, OTHER_FEED);
        DJIMotorChangeFeed(pitch_motor, SPEED_LOOP, OTHER_FEED);
#endif
        DJIMotorSetRef(yaw_motor, gimbal_cmd_recv.yaw);
#if !CHASSIS_TUNING_ONLY
        DJIMotorSetRef(pitch_motor, gimbal_cmd_recv.pitch);
#endif
        break;
    default:
        break;
    }

    // 回传cmd:IMU姿态+yaw电机单圈角度(供跟随律计算offset)
    gimbal_feedback_data.gimbal_imu_data = *gimba_IMU_data;
    gimbal_feedback_data.yaw_motor_single_round_angle = yaw_motor->measure.angle_single_round;

    /* 控制遥测(单写者=本任务),ref/fb/out供Ozone示波与现场回放;out取最终环输出。
     * extra 在 P2 标定期改为**关节转速**(摩擦前馈的自变量),标定结束后可恢复为世界系角速度。 */
    TelemetryPush(&telemetry_yaw, gimbal_cmd_recv.yaw, gimba_IMU_data->YawTotalAngle,
                  yaw_motor->motor_controller.speed_PID.Output,
                  yaw_motor->measure.speed_aps,
                  (uint8_t)gimbal_cmd_recv.gimbal_mode);
#if YAW_FF_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行,四列覆盖 P2 拟合所需的全部量 */
    if (++yaw_calib_cnt >= 100)
    {
        char s_out[16], s_spd[16], s_cur[16], s_ff[16];
        yaw_calib_cnt = 0;
        Float2Str(s_out, yaw_motor->motor_controller.speed_PID.Output);
        Float2Str(s_spd, yaw_motor->measure.speed_aps);
        Float2Str(s_cur, yaw_motor->measure.real_current);
        Float2Str(s_ff, yaw_inertia_ff);
        LOGINFO("[yawcal] out=%s spd=%s cur=%s ff=%s", s_out, s_spd, s_cur, s_ff);
    }
#endif
    TelemetryPush(&telemetry_pitch, gimbal_cmd_recv.pitch, gimba_IMU_data->Pitch,
                  pitch_motor->motor_controller.speed_PID.Output,
                  gimba_IMU_data->Gyro[0] * RAD_2_DEGREE,
                  (uint8_t)gimbal_cmd_recv.gimbal_mode);
    /* 注:extra 在 P1 阶段保持 Gyro[0]——B7 敲击辨识要看振动通道(此时 out/cur 全为 0);
     * P2 重力标定前再切成输出电流码(见 .Doc/pitch轴优化计划.md P0-2)。 */
#if PITCH_FF_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行,角度与稳态电流成对,供 P2 拟合 A/φ/B */
    if (++pitch_calib_cnt >= 100)
    {
        char p_ang[16], p_out[16], p_spd[16], p_cur[16];
        pitch_calib_cnt = 0;
        Float2Str(p_ang, gimba_IMU_data->Pitch);
        Float2Str(p_out, pitch_motor->motor_controller.speed_PID.Output);
        Float2Str(p_spd, pitch_motor->measure.speed_aps);
        Float2Str(p_cur, pitch_motor->measure.real_current);
        LOGINFO("[pitchcal] ang=%s out=%s spd=%s cur=%s", p_ang, p_out, p_spd, p_cur);
    }
#endif

    PubPushMessage(gimbal_pub, (void *)&gimbal_feedback_data);
}

void GimbalGetBigYawFeedback(float *rel_rad, uint64_t *sample_us, uint8_t *online)
{
    float angle = 0.0f;
    uint64_t ts = 0u;
    uint8_t ok = 0u;
#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
    if (sentry_big_yaw_motor != NULL)
    {
        const uint32_t saved = nav_enter_critical();
        const float total_pos = sentry_big_yaw_motor->measure.total_position;
        ts = sentry_big_yaw_motor->rx_sample_us;
        ok = sentry_big_yaw_feedback_online;
        nav_exit_critical(saved);
        const float pos_rad = SENTRY_BIG_YAW_DIRECTION * total_pos / SENTRY_BIG_YAW_TRANSMISSION_RATIO;
        const float zero_rad = SENTRY_BIG_YAW_DIRECTION * SENTRY_BIG_YAW_FRONT_ALIGN_RAD / SENTRY_BIG_YAW_TRANSMISSION_RATIO;
        angle = remainderf(pos_rad - zero_rad, 6.28318530718f);
    }
#endif
    if (rel_rad != NULL) *rel_rad = angle;
    if (sample_us != NULL) *sample_us = ts;
    if (online != NULL) *online = ok;
}
