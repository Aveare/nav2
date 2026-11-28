/**
 * @file gimbal.c
 * @brief 哨兵三轴独立控制，移植自RM_shaobin2的在机调试行为
 * @layer APP
 * @author 小猫大王
 * @note 大yaw普通回正/小陀螺纯世界速度补偿；pitch为电机轴编码器闭环(命令/限位按枪管俯仰角经 DIR/传动比换算)；小yaw独立相对角。
 */
#include "gimbal.h"
#include "sentry_def.h"
#include "config.h"
#include "dji_motor.h"
#include "dm_motor.h"
#include "sentry_control.h"
#include "gimbal_alloc.h"
_Static_assert(DM_STARTUP_WAIT_MS == DM_DRIVER_STARTUP_WAIT_MS && DM_ENABLE_RETRY_MS == DM_DRIVER_ENABLE_RETRY_MS, "DM scheduling mismatch");
#include "ins_task.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "general_def.h"
#include "dwt_port.h"
#include "kernel.h"
#include "log_port.h"
#include "daemon.h"
#include <math.h>
#include <stdbool.h>
#include "fault_port.h"
#include "usart_port.h"
#include "usart.h"
#include <string.h>
/* 运行期可写(ELF/DWARF):3 小时台架调参入口;默认值等于 config,不改变上电行为。 */
volatile float sentry_rt_big_yaw_angle_kp = SENTRY_BIG_YAW_ANGLE_KP;
volatile float sentry_rt_big_yaw_speed_kp = SENTRY_BIG_YAW_SPEED_KP;
volatile float sentry_rt_big_yaw_speed_ki = SENTRY_BIG_YAW_SPEED_KI;
volatile float sentry_rt_big_yaw_torque_limit_nm = SENTRY_BIG_YAW_TORQUE_NM;
volatile float sentry_rt_small_yaw_max_out = SENTRY_SMALL_YAW_MAX_OUT;
volatile float sentry_rt_pitch_zero_rad = SENTRY_PITCH_ZERO_RAD;
volatile float sentry_rt_pitch_min_deg = SENTRY_PITCH_MIN_DEG;
volatile float sentry_rt_pitch_max_deg = SENTRY_PITCH_MAX_DEG;
volatile float sentry_rt_pitch_gravity_a = SENTRY_PITCH_GRAVITY_A;
volatile float sentry_rt_pitch_gravity_b = SENTRY_PITCH_GRAVITY_B;
volatile float sentry_rt_pitch_gravity_phase_deg = SENTRY_PITCH_GRAVITY_PHASE_DEG;
volatile float sentry_rt_small_yaw_limit_deg = SENTRY_SMALL_YAW_LIMIT_DEG;
volatile float sentry_rt_big_yaw_fric_ff_nm = SENTRY_BIG_YAW_FRICTION_FF_NM;
volatile float sentry_rt_small_yaw_carrier_inj = SENTRY_SMALL_YAW_CARRIER_INJ;
volatile float sentry_rt_pitch_ratio = SENTRY_PITCH_TRANSMISSION_RATIO;
volatile float sentry_rt_pitch_torque_sign = SENTRY_PITCH_TORQUE_SIGN;
static usart_t *gimbal_vofa_usart;
static attitude_t *gimbal_imu_data;
static msg_pub_t gimbal_pub;
static msg_sub_t gimbal_sub;
static sentry_gimbal_cmd_t gimbal_cmd_recv;
static cmd_watchdog_t gimbal_cmd_wd;
static sentry_gimbal_feed_t gimbal_fb;
static dm_motor_t *sentry_big_yaw_motor;
/* 本次上电的车头参考(关节 rad/deg);valid=0 时退回 SENTRY_BIG_YAW_FRONT_ALIGN_RAD。 */
static float sentry_big_yaw_front_align_rad;
volatile float sentry_big_yaw_front_align_deg;
volatile uint8_t sentry_big_yaw_front_align_valid;
static sentry_axis_controller_t sentry_yaw_controller;
/* 关节角与世界角分别观察；控制域随小陀螺模式切换。 */
volatile float sentry_big_yaw_world_rad;
volatile float sentry_big_yaw_world_speed_rad_s; // 载体世界角速度(IMU Gyro[2]),小yaw注入与遥测共用
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
static uint8_t sentry_big_yaw_healthy_streak; // 连续健康拍数,重锚锁存自动解除用
static uint8_t sentry_big_yaw_auto_aim_active; // 上一拍是否在自瞄世界位置环
static float sentry_small_yaw_auto_target_deg; // 大 yaw 分配出的小 yaw 目标,deg
static float sentry_small_yaw_trim_deg;        // 自瞄小 yaw 慢配平状态,deg
static uint8_t sentry_small_yaw_auto_valid;    // 1=本拍有小 yaw 分配目标
static float sentry_big_yaw_torque_lpf;      // 力矩指令低通状态,Nm
static float sentry_big_yaw_last_ms;         // 上周期时刻(LPF 用)
static msg_sub_t sentry_yaw_chassis_sub;
static chassis_ctrl_cmd_t sentry_yaw_chassis_cmd;
static cmd_watchdog_t sentry_yaw_chassis_wd;
static uint32_t sentry_big_yaw_imu_update_count;
static uint8_t sentry_big_yaw_imu_seen_update;
static float sentry_big_yaw_imu_last_ms;
volatile uint8_t sentry_big_yaw_world_hold_active;
volatile uint8_t sentry_big_yaw_imu_online;
volatile float sentry_big_yaw_world_target_rad; // 兼容观察量：纯速度模式随实测航向更新，不参与闭环
enum { SENTRY_BIG_YAW_MAX_FEEDBACK_AGE_CYCLES = DM_ENABLE_RETRY_MS / DM_TASK_CYCLE };

static dm_motor_t *sentry_pitch_motor;
static uint8_t sentry_pitch_reference_initialized;
static float sentry_pitch_last_ms;
static float sentry_pitch_abs_deg;
#if SENTRY_PITCH_ZERO_AT_BOOT
static float sentry_pitch_boot_ref_rad;
#endif
/* 调试符号:ZERO_AT_BOOT=0 时不再写入,恒 0.0 / 0。 */
volatile float sentry_pitch_boot_ref_deg;
volatile uint8_t sentry_pitch_boot_ref_valid;
static sentry_axis_controller_t sentry_pitch_controller;
volatile float sentry_pitch_torque_nm;
volatile float sentry_pitch_speed_target_rad_s;
volatile uint16_t sentry_pitch_position_raw;
volatile float sentry_pitch_position_rad, sentry_pitch_target_rad;
volatile uint8_t sentry_pitch_feedback_online;
volatile uint8_t sentry_pitch_driver_state = 0xffu;
volatile uint8_t sentry_pitch_control_active;
volatile uint8_t sentry_pitch_rearm_required;

static dji_motor_t *sentry_small_yaw_motor;
static float sentry_small_yaw_target_deg;  // 目标关节角,deg(相对机械中位)
static uint8_t sentry_small_yaw_target_init;
static uint8_t sentry_small_yaw_auto_prev;
static float sentry_small_yaw_last_ms;
volatile float sentry_small_yaw_ref_deg_s;    // 本周期下发的速度目标,deg/s
volatile float sentry_small_yaw_speed_deg_s;  // 编码器反馈,deg/s
volatile float sentry_small_yaw_target_out_deg; // 目标关节角观察量,deg
volatile uint8_t sentry_small_yaw_feedback_online;
volatile uint8_t sentry_small_yaw_control_active;
volatile float sentry_small_yaw_joint_deg;   // 相对机械中位的关节角,deg(已解缠)
volatile uint8_t sentry_small_yaw_limited;   // 0=行程内 1=贴预留边界 2=已越界

static void gimbal_init_controller(sentry_axis_controller_t *axis, bool yaw)
{
    const sentry_axis_config_t config = {
        .angle = {
            .Kp = yaw ? SENTRY_BIG_YAW_ANGLE_KP : SENTRY_PITCH_ANGLE_KP,
            .MaxOut = (yaw ? SENTRY_BIG_YAW_MAX_SPEED_DEG_S : SENTRY_PITCH_MAX_SPEED_DEG_S) * FW_DEG_TO_RAD,
            .DeadBand = (yaw ? SENTRY_BIG_YAW_DEADBAND_DEG : SENTRY_PITCH_DEADBAND_DEG) * FW_DEG_TO_RAD,
            .Improve = PID_Derivative_On_Measurement,
            .DerivativeLimit = SENTRY_AXIS_D_LIMIT_DEG_S * FW_DEG_TO_RAD,
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
        .blend_lo_rad = SENTRY_BIG_YAW_KD_BLEND_LO_DEG * FW_DEG_TO_RAD,
        .blend_hi_rad = SENTRY_BIG_YAW_KD_BLEND_HI_DEG * FW_DEG_TO_RAD,
        .feedforward_gain = SENTRY_AXIS_FF_GAIN,
        .feedforward_lpf = SENTRY_AXIS_FF_LPF,
    };
    sentry_axis_init(axis, &config);
}
void gimbal_init(void)
{
    gimbal_imu_data = ins_init();
    dm_motor_mit_init_config_t sentry_big_yaw_config = {
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
    gimbal_init_controller(&sentry_yaw_controller, true);
    sentry_big_yaw_motor = dm_motor_mit_init(&sentry_big_yaw_config);
    if (sentry_big_yaw_motor != NULL)
        dm_motor_stop(sentry_big_yaw_motor);
    else
        LOG_ERROR("[sentry] big yaw DM registration failed");
    dm_motor_mit_init_config_t sentry_pitch_config = {
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
    gimbal_init_controller(&sentry_pitch_controller, false);
    sentry_pitch_motor = dm_motor_mit_init(&sentry_pitch_config);
    if (sentry_pitch_motor != NULL)
        dm_motor_stop(sentry_pitch_motor);
    else
        LOG_ERROR("[sentry] pitch DM registration failed");
    /* 小 yaw:GM6020,速度环常开做自由稳像。反馈用电机自身编码器(相对大 yaw 的速度),
     * 稳定所需的"母体世界角速度"不进反馈,而是从参考值注入(gimbal_small_yaw_task,
     * 系数 SENTRY_SMALL_YAW_CARRIER_INJ,默认 0=只保持关节角)。 */
    motor_init_config_t small_yaw_config = {
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
    sentry_small_yaw_motor = dji_motor_init(&small_yaw_config);
    if (sentry_small_yaw_motor != NULL)
        dji_motor_stop(sentry_small_yaw_motor);
    else
        LOG_ERROR("[sentry] small yaw GM6020 registration failed");


    gimbal_pub = msg_pub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(sentry_gimbal_cmd_t));
    sentry_yaw_chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
#if GIMBAL_VOFA_UART1_ENABLE
    /* USART1 独占给 VOFA,沿用 CubeMX 的 921600 8N1;这里只做 Init 阶段句柄注入。 */
    usart_init_config_t gimbal_vofa_config = {
        .recv_buff_size = 1u,
        .usart_handle = &huart1, // REDFLAG-EXEMPT: Init 句柄注入(AGENTS.md 允许口径)
        .module_callback = 0,
        .tx_only = 1u,
    };
    gimbal_vofa_usart = usart_register(&gimbal_vofa_config);
#endif
}
static float gimbal_front_align_rad(void)
{
#if SENTRY_BIG_YAW_FRONT_ALIGN_AT_UNLOCK
    if (sentry_big_yaw_front_align_valid)
        return sentry_big_yaw_front_align_rad;
#endif
    return SENTRY_BIG_YAW_DIRECTION * SENTRY_BIG_YAW_FRONT_ALIGN_RAD /
           SENTRY_BIG_YAW_TRANSMISSION_RATIO;
}

static void gimbal_big_yaw_task(float now_ms)
{
    dm_motor_t *motor = sentry_big_yaw_motor;
    if (motor == NULL) return;
    const bool chassis_fresh = !cmd_watchdog_expired(&sentry_yaw_chassis_wd, now_ms,
                                                  CMD_WATCHDOG_TIMEOUT_MS);
    const bool auto_aim = (gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE) &&
                          (gimbal_cmd_recv.aim_valid != 0u) && SENTRY_AUTO_AIM_ENABLE;
    const bool command_active = SENTRY_BIG_YAW_OUTPUT_ENABLE && chassis_fresh &&
        sentry_yaw_chassis_cmd.chassis_mode != CHASSIS_ZERO_FORCE &&
        (gimbal_cmd_recv.mode == GIMBAL_GYRO_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_FREE_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE);
    const bool world_requested =
        (sentry_yaw_chassis_cmd.chassis_mode == CHASSIS_ROTATE) && !auto_aim;
    const bool online = motor->rx_init_flag &&
        motor->rx_refresh_dt < SENTRY_BIG_YAW_MAX_FEEDBACK_AGE_CYCLES;
    const float position = SENTRY_BIG_YAW_DIRECTION * motor->measure.total_position /
                           SENTRY_BIG_YAW_TRANSMISSION_RATIO;
    const float speed = SENTRY_BIG_YAW_DIRECTION * motor->measure.velocity /
                        SENTRY_BIG_YAW_TRANSMISSION_RATIO;
#if SENTRY_BIG_YAW_FRONT_ALIGN_AT_UNLOCK
    /* 解锁标定车头:每次从零力态进入受控态的那一拍,把当前关节角锁为前方参考。
     * 退出受控即清除,下次解锁重新标定——操作习惯与步兵"解锁回正"一致:
     * 解锁前把枪口摆到车头方向,解锁后该姿态即正前。 */
    static uint8_t front_align_armed;
    if (!command_active)
    {
        sentry_big_yaw_front_align_valid = 0u;
        front_align_armed = 0u;
    }
    else if (!front_align_armed && online && isfinite(position))
    {
        sentry_big_yaw_front_align_rad = position;
        sentry_big_yaw_front_align_deg = position * FW_RAD_TO_DEG;
        sentry_big_yaw_front_align_valid = 1u;
        front_align_armed = 1u;
    }
#endif
    const float fixed_target = gimbal_front_align_rad();
    const float world = SENTRY_BIG_YAW_IMU_DIRECTION * gimbal_imu_data->YawTotalAngle * FW_DEG_TO_RAD;
    /* C板只随大yaw转；水平安装时Z轴角速度对应航向角速度，单位rad/s。 */
    const float world_speed = SENTRY_BIG_YAW_IMU_DIRECTION * gimbal_imu_data->Gyro[2];
    /* 跨INS任务明确读取快照；低位只用于判断推进，回绕到0仍然有效。 */
    const uint64_t imu_snapshot = *(const volatile uint64_t *)&ekf_ins.UpdateCount;
    const uint32_t imu_updates = (uint32_t)imu_snapshot;
    if (imu_updates != sentry_big_yaw_imu_update_count)
    {
        sentry_big_yaw_imu_seen_update = 1u;
        sentry_big_yaw_imu_update_count = imu_updates;
        sentry_big_yaw_imu_last_ms = now_ms;
    }
    const float imu_age_ms = now_ms - sentry_big_yaw_imu_last_ms;
    sentry_big_yaw_imu_online = ekf_ins.Initialized && sentry_big_yaw_imu_seen_update &&
        imu_age_ms >= 0.0f && imu_age_ms <= SENTRY_BIG_YAW_IMU_TIMEOUT_MS &&
        isfinite(world) && isfinite(world_speed);
    const bool temp_ok = motor->measure.T_Mos < SENTRY_DM_TEMP_LIMIT_C &&
                         motor->measure.T_Rotor < SENTRY_DM_TEMP_LIMIT_C;
    const bool healthy = online && motor->measure.state == DM_STATE_ENABLE &&
        isfinite(position) && isfinite(speed) && temp_ok &&
        (!world_requested || sentry_big_yaw_imu_online);
    sentry_big_yaw_position_rad = position;
    sentry_big_yaw_world_rad = world;
    sentry_big_yaw_world_speed_rad_s = world_speed;
    sentry_big_yaw_feedback_online = online;
    sentry_big_yaw_driver_state = motor->rx_init_flag ? motor->measure.state : 0xffu;
    if (!command_active)
    {
        sentry_big_yaw_rearm_required = 0u;
        sentry_big_yaw_healthy_streak = 0u;
    }
    else if (sentry_big_yaw_control_active && !healthy)
    {
        sentry_big_yaw_rearm_required = 1u;
        sentry_big_yaw_healthy_streak = 0u;
    }
    else if (healthy)
    {
        /* 瞬时毛刺(任务超时/反馈掉帧)在连续健康后自动恢复,不再永久卸力;
         * 硬故障(超温/失联/掉使能)期间 healthy=false,锁存保持。 */
        if (sentry_big_yaw_healthy_streak < 255u) sentry_big_yaw_healthy_streak++;
        if (sentry_big_yaw_rearm_required &&
            sentry_big_yaw_healthy_streak >= SENTRY_BIG_YAW_REARM_HEALTHY_CYCLES)
            sentry_big_yaw_rearm_required = 0u;
    }

    if (healthy && command_active && !sentry_big_yaw_rearm_required)
    {
        if (auto_aim != (sentry_big_yaw_auto_aim_active != 0u) ||
            world_requested != (sentry_big_yaw_world_hold_active != 0u))
        {
            /* 坐标域切换时清历史，避免旧积分/微分和力矩低通造成瞬间冲击。 */
            sentry_axis_reset(&sentry_yaw_controller);
            sentry_big_yaw_band_latched = 0u;
            sentry_big_yaw_torque_lpf = 0.0f;
            sentry_big_yaw_last_ms = now_ms;
            sentry_big_yaw_world_hold_active = world_requested ? 1u : 0u;
            if (auto_aim && sentry_big_yaw_auto_aim_active == 0u)
            {
                /* 进入自瞄:小 yaw 配平从当前关节角起步,避免回中位跳变。 */
                float limit_deg = sentry_rt_small_yaw_limit_deg;
                if (!(limit_deg >= 20.0f && limit_deg <= 136.0f))
                    limit_deg = SENTRY_SMALL_YAW_LIMIT_DEG;
                const float limit = limit_deg - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
                sentry_small_yaw_trim_deg = fminf(limit, fmaxf(-limit, sentry_small_yaw_joint_deg));
            }
            sentry_big_yaw_auto_aim_active = auto_aim ? 1u : 0u;
        }
        /* 运行期可写增益:默认=config;非有限值回退,避免调试器误写 NaN。 */
        sentry_yaw_controller.angle.Kp = isfinite(sentry_rt_big_yaw_angle_kp)
                                             ? sentry_rt_big_yaw_angle_kp
                                             : SENTRY_BIG_YAW_ANGLE_KP;
        sentry_yaw_controller.speed.Kp = isfinite(sentry_rt_big_yaw_speed_kp)
                                             ? sentry_rt_big_yaw_speed_kp
                                             : SENTRY_BIG_YAW_SPEED_KP;
        sentry_yaw_controller.speed.Ki = isfinite(sentry_rt_big_yaw_speed_ki)
                                             ? sentry_rt_big_yaw_speed_ki
                                             : SENTRY_BIG_YAW_SPEED_KI;
        float torque_limit = sentry_rt_big_yaw_torque_limit_nm;
        if (!(torque_limit >= 2.0f && torque_limit <= SENTRY_BIG_YAW_TORQUE_RANGE_NM))
            torque_limit = SENTRY_BIG_YAW_TORQUE_NM;
        sentry_yaw_controller.speed.MaxOut = torque_limit;
        /* 自瞄角度环上限比普通回正高,底盘小陀螺时仍要留出跟随余量。 */
        sentry_yaw_controller.angle.MaxOut =
            (auto_aim ? SENTRY_AUTO_AIM_BIG_YAW_MAX_SPEED_DEG_S : SENTRY_BIG_YAW_MAX_SPEED_DEG_S) *
            FW_DEG_TO_RAD;

        float target_for_ctrl;
        if (auto_aim)
        {
            /* 大 yaw 负责快速世界环;小 yaw 只做慢配平,目标之和恒等于世界目标。
             * 若让小 yaw 吃掉全部误差,大 yaw 目标会等于自身世界角,底盘持续旋转时失效。 */
            float limit_deg = sentry_rt_small_yaw_limit_deg;
            if (!(limit_deg >= 20.0f && limit_deg <= 136.0f))
                limit_deg = SENTRY_SMALL_YAW_LIMIT_DEG;
            const float small_limit = limit_deg - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
            const float big_world_deg = world * FW_RAD_TO_DEG;
            const float err_deg = remainderf(gimbal_cmd_recv.big_yaw - big_world_deg, 360.0f);
            const float trim_target = fminf(small_limit, fmaxf(-small_limit, err_deg));
            sentry_small_yaw_trim_deg +=
                SENTRY_AUTO_AIM_SMALL_TRIM_LP * (trim_target - sentry_small_yaw_trim_deg);
            float small_deg = 0.0f;
            float big_deg = 0.0f;
            sentry_gimbal_split(gimbal_cmd_recv.big_yaw, big_world_deg, sentry_small_yaw_trim_deg,
                                small_limit, &small_deg, &big_deg);
            sentry_small_yaw_auto_target_deg = small_deg;
            sentry_small_yaw_auto_valid = 1u;
            target_for_ctrl = world + remainderf(big_deg * FW_DEG_TO_RAD - world, FW_TWO_PI);
            sentry_big_yaw_world_target_rad = big_deg * FW_DEG_TO_RAD;
        }
        else
        {
            sentry_small_yaw_auto_valid = 0u;
            /* 小陀螺不保存角度目标，观察值随实测变化；只有普通回正计算角度误差。 */
            if (world_requested) sentry_big_yaw_world_target_rad = world;
            const float error = world_requested ? 0.0f : remainderf(fixed_target - position, FW_TWO_PI);
            target_for_ctrl = position + error;

            if (!world_requested)
            {
                if (fabsf(error) <= SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG * FW_DEG_TO_RAD)
                    sentry_big_yaw_band_latched = 1u;
                else if (fabsf(error) >= SENTRY_BIG_YAW_TARGET_DEADBAND_DEG * FW_DEG_TO_RAD)
                    sentry_big_yaw_band_latched = 0u;
            }
        }
        sentry_big_yaw_target_rad = target_for_ctrl;
        /* 稳像期间持续闭环，即使误差为零也保留速度阻尼，不能到位撤力。 */
        const bool in_deadband = !auto_aim && !world_requested && sentry_big_yaw_band_latched;
        sentry_big_yaw_in_deadband = in_deadband ? 1u : 0u;
        const bool updated = (world_requested && !auto_aim)
            ? sentry_axis_update_speed(&sentry_yaw_controller, world_speed, 0.0f, now_ms)
            : sentry_axis_update(&sentry_yaw_controller, auto_aim ? world : position,
                                 auto_aim ? world_speed : speed,
                                 (auto_aim || !in_deadband) ? target_for_ctrl : position, now_ms);
        if (updated)
        {
            float torque_cmd = sentry_yaw_controller.torque_nm;
            if (in_deadband)
            {
                pid_clear(&sentry_yaw_controller.angle);
                pid_clear(&sentry_yaw_controller.speed);
                torque_cmd = 0.0f;
                sentry_big_yaw_speed_target_rad_s = 0.0f;
            }
            else
                sentry_big_yaw_speed_target_rad_s = sentry_yaw_controller.speed_target_rad_s;

            if (world_requested)
            {
                /* 小陀螺摩擦前馈:同步带/轴承沿底盘旋向拖拽载体,拖拽力矩与关节速度反号,
                 * 故前馈与关节速度同号(制动方向)。PI 积分只补残差,消除进入拖拽。 */
                float fric_ff = sentry_rt_big_yaw_fric_ff_nm;
                if (!(fric_ff >= 0.0f && fric_ff <= 5.0f))
                    fric_ff = SENTRY_BIG_YAW_FRICTION_FF_NM;
                if (fric_ff > 0.0f)
                {
                    const float s =
                        fminf(1.0f, fmaxf(-1.0f, speed / SENTRY_BIG_YAW_FRICTION_FF_RAMP_RAD_S));
                    torque_cmd += fric_ff * s;
                    /* 总力矩仍夹在运行期力矩上限内:前馈替代的是积分原本承担的份额,
                     * 不放大输出包络(超出会顶到 DM MIT 映射量程)。 */
                    torque_cmd = fminf(torque_limit, fmaxf(-torque_limit, torque_cmd));
                }
            }

            const float dt_s = (now_ms - sentry_big_yaw_last_ms) * 0.001f;
            sentry_big_yaw_last_ms = now_ms;
            if (dt_s > 0.0f && dt_s < 0.1f)
            {
                const float rc = SENTRY_BIG_YAW_TORQUE_LPF_RC_S;
                const float alpha = (rc > 0.0f) ? dt_s / (rc + dt_s) : 1.0f;
                sentry_big_yaw_torque_lpf += alpha * (torque_cmd - sentry_big_yaw_torque_lpf);
            }
            sentry_big_yaw_torque_nm = sentry_big_yaw_torque_lpf;
            dm_motor_set_mit_torque(motor, sentry_big_yaw_torque_nm * SENTRY_BIG_YAW_DIRECTION /
                                       SENTRY_BIG_YAW_TRANSMISSION_RATIO);
            dm_motor_enable(motor);
            sentry_big_yaw_control_active = 1u;
            return;
        }
        sentry_big_yaw_rearm_required = 1u;
    }
    dm_motor_stop(motor);
    dm_motor_set_mit_torque(motor, 0.0f);
    sentry_axis_reset(&sentry_yaw_controller);
    sentry_big_yaw_torque_nm = sentry_big_yaw_speed_target_rad_s = 0.0f;
    sentry_big_yaw_torque_lpf = 0.0f;
    sentry_big_yaw_last_ms = now_ms;
    sentry_big_yaw_band_latched = 0u;
    sentry_big_yaw_target_rad = fixed_target;
    sentry_big_yaw_control_active = 0u;
    sentry_big_yaw_world_hold_active = 0u;
    sentry_big_yaw_auto_aim_active = 0u;
    sentry_small_yaw_auto_valid = 0u;
    sentry_small_yaw_trim_deg = 0.0f;
    sentry_big_yaw_in_deadband = 0u;
}

static void gimbal_pitch_task(float now_ms)
{
    dm_motor_t *motor = sentry_pitch_motor;
    if (motor == NULL) return;
    const bool online = motor->rx_init_flag &&
        motor->rx_refresh_dt < (int)(DM_ENABLE_RETRY_MS / DM_TASK_CYCLE);
    const bool auto_aim = (gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE) &&
                          (gimbal_cmd_recv.aim_valid != 0u) && SENTRY_AUTO_AIM_PITCH_ENABLE;
    static uint8_t pitch_auto_prev;
    if (pitch_auto_prev && !auto_aim)
        sentry_pitch_reference_initialized = 0u; // 自瞄退回手动:目标从当前角重新爬向手动命令,不跳变
    pitch_auto_prev = auto_aim ? 1u : 0u;
    /* 自瞄给 pitch 更高的速度/力矩上限与目标爬升率;手动保持原值。 */
    sentry_pitch_controller.angle.MaxOut =
        (auto_aim ? SENTRY_AUTO_AIM_PITCH_MAX_SPEED_DEG_S : SENTRY_PITCH_MAX_SPEED_DEG_S) *
        FW_DEG_TO_RAD;
    sentry_pitch_controller.speed.MaxOut =
        auto_aim ? SENTRY_AUTO_AIM_PITCH_TORQUE_NM : SENTRY_PITCH_TORQUE_NM;
    const bool command_active = SENTRY_PITCH_CONTROL_ENABLE &&
        (gimbal_cmd_recv.mode == GIMBAL_GYRO_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_FREE_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE);
    /* 闭环量是 4310 电机轴角/速度;摇杆命令与软限位是枪管俯仰角,经 DIR/传动比换算到电机轴。 */
    const float position = motor->measure.total_position;
    const float speed = motor->measure.velocity;
#if SENTRY_PITCH_ZERO_AT_BOOT
    /* 上电校准:首个有效反馈即 0 位(上电前枪管放行程中间)。 */
    if (!sentry_pitch_boot_ref_valid && online && isfinite(position))
    {
        sentry_pitch_boot_ref_rad = position;
        sentry_pitch_boot_ref_deg = position * FW_RAD_TO_DEG;
        sentry_pitch_boot_ref_valid = 1u;
    }
#endif
    const float pitch_zero =
#if SENTRY_PITCH_ZERO_AT_BOOT
        sentry_pitch_boot_ref_valid ? sentry_pitch_boot_ref_rad :
#endif
                                      (isfinite(sentry_rt_pitch_zero_rad) ? sentry_rt_pitch_zero_rad
                                                                          : SENTRY_PITCH_ZERO_RAD);
    /* 相对 pitch 零位的软限位(零位 = 上电锁存或固定水平,见 SENTRY_PITCH_ZERO_AT_BOOT):
     * 运行期可写;实测角越界带裕量后立即断力。 */
    float pitch_min_deg = isfinite(sentry_rt_pitch_min_deg) ? sentry_rt_pitch_min_deg : SENTRY_PITCH_MIN_DEG;
    float pitch_max_deg = isfinite(sentry_rt_pitch_max_deg) ? sentry_rt_pitch_max_deg : SENTRY_PITCH_MAX_DEG;
    if (!(pitch_min_deg < pitch_max_deg))
    {
        pitch_min_deg = SENTRY_PITCH_MIN_DEG;
        pitch_max_deg = SENTRY_PITCH_MAX_DEG;
    }
    /* 传动比 R = 电机轴角 / 枪管俯仰角,运行期可写,非法值回退 config。 */
    float pitch_ratio = sentry_rt_pitch_ratio;
    if (!(isfinite(pitch_ratio) && pitch_ratio >= 0.05f && pitch_ratio <= 20.0f))
        pitch_ratio = SENTRY_PITCH_TRANSMISSION_RATIO;
    /* 软限位以枪管俯仰角定义(负=下压,正=上抬),按 DIR/R 换算到电机轴后再排出上下界:
     * DIR=-1 时"上抬"落在电机轴负侧,不能直接把 min/max 当电机轴的上下界用。 */
    const float pitch_min_cmd_rad =
        pitch_zero + SENTRY_PITCH_DIRECTION * pitch_min_deg / pitch_ratio * FW_DEG_TO_RAD;
    const float pitch_max_cmd_rad =
        pitch_zero + SENTRY_PITCH_DIRECTION * pitch_max_deg / pitch_ratio * FW_DEG_TO_RAD;
    const float pitch_min_rad = fminf(pitch_min_cmd_rad, pitch_max_cmd_rad);
    const float pitch_max_rad = fmaxf(pitch_min_cmd_rad, pitch_max_cmd_rad);
    const float pitch_margin_rad = SENTRY_PITCH_LIMIT_MARGIN_DEG * FW_DEG_TO_RAD;
    const bool pitch_in_range = position >= pitch_min_rad - pitch_margin_rad &&
                                position <= pitch_max_rad + pitch_margin_rad;
    const bool healthy = online && motor->measure.state == DM_STATE_ENABLE &&
        isfinite(motor->measure.total_position) && isfinite(motor->measure.velocity) &&
        isfinite(gimbal_cmd_recv.pitch) && isfinite(gimbal_cmd_recv.pitch_abs) &&
        motor->measure.T_Mos < SENTRY_DM_TEMP_LIMIT_C &&
        motor->measure.T_Rotor < SENTRY_DM_TEMP_LIMIT_C && pitch_in_range;
    const float dt = fminf(0.02f, fmaxf(0.0f, (now_ms - sentry_pitch_last_ms) * 0.001f));
    sentry_pitch_last_ms = now_ms;
    sentry_pitch_position_raw = motor->measure.position_raw;
    sentry_pitch_position_rad = position;
    /* 枪管世界俯仰观察值 = 平台 IMU 俯仰 + 枪管相对角(电机轴角 × DIR / R)。 */
    sentry_pitch_abs_deg = gimbal_imu_data->Pitch +
        (position - pitch_zero) / pitch_ratio * FW_RAD_TO_DEG * SENTRY_AUTO_AIM_PITCH_WORLD_DIR;
    sentry_pitch_feedback_online = online;
    sentry_pitch_driver_state = motor->rx_init_flag ? motor->measure.state : 0xffu;
    if (!command_active) sentry_pitch_rearm_required = 0u;
    else if (sentry_pitch_control_active && !healthy) sentry_pitch_rearm_required = 1u;

    if (healthy && command_active && !sentry_pitch_rearm_required)
    {
        if (!sentry_pitch_reference_initialized)
        {
            /* 首次进入以当前实测角为爬升起点:目标从当前位置按 RATE 爬向命令,首拍不满速追目标。 */
            sentry_pitch_target_rad = position;
            sentry_pitch_reference_initialized = 1u;
        }
        float requested;
        if (auto_aim)
        {
            /* 视觉给世界枪管 pitch:关节目标 = 上电零位 + (世界目标 - 平台俯仰) × DIR / R。
             * 小角度线性近似;限位用上电零位 + 相对软限位兜底。 */
            const float platform_pitch = gimbal_imu_data->Pitch;
            requested = pitch_zero +
                        (gimbal_cmd_recv.pitch_abs - platform_pitch) / pitch_ratio * FW_DEG_TO_RAD *
                            SENTRY_AUTO_AIM_PITCH_WORLD_DIR;
            MOTOR_LIMIT_MIN_MAX(requested, pitch_min_rad, pitch_max_rad);
        }
        else
        {
            /* 手动命令 pitch = 相对上电零位的枪管俯仰角(cmd 层已夹在 MIN/MAX_ANGLE):
             * 电机轴目标 = 上电零位 + DIR × 命令 / R。零位是上电锁存的,不随解锁/重入时的
             * 位置漂移,所以"保持 0 位"始终指上电标定的那个位置。 */
            requested = pitch_zero +
                        SENTRY_PITCH_DIRECTION * gimbal_cmd_recv.pitch / pitch_ratio * FW_DEG_TO_RAD;
            MOTOR_LIMIT_MIN_MAX(requested, pitch_min_rad, pitch_max_rad);
        }
        /* 爬升限速以枪管度计(摇杆命令 20°/s),乘 R 换算到电机轴。 */
        const float step =
            (auto_aim ? SENTRY_AUTO_AIM_PITCH_RATE_DEG_S : SENTRY_PITCH_RATE_DEG_S) *
            pitch_ratio * FW_DEG_TO_RAD * dt;
        sentry_pitch_target_rad += fminf(step, fmaxf(-step, requested - sentry_pitch_target_rad));
        if (sentry_axis_update(&sentry_pitch_controller, position, speed, sentry_pitch_target_rad, now_ms))
        {
            /* 重力前馈:τ = A·cos(θ−φ)+B,N·m;A=B=0 时恒 0,保持手动行为不变。 */
            const float g_a = isfinite(sentry_rt_pitch_gravity_a) ? sentry_rt_pitch_gravity_a : 0.0f;
            const float g_b = isfinite(sentry_rt_pitch_gravity_b) ? sentry_rt_pitch_gravity_b : 0.0f;
            const float g_phase = isfinite(sentry_rt_pitch_gravity_phase_deg)
                                      ? sentry_rt_pitch_gravity_phase_deg
                                      : 0.0f;
            float gravity_ff =
                g_a * cosf((sentry_pitch_abs_deg - g_phase) * FW_DEG_TO_RAD) + g_b;
            MOTOR_LIMIT_MIN_MAX(gravity_ff, -SENTRY_PITCH_GRAVITY_LIMIT_NM,
                                SENTRY_PITCH_GRAVITY_LIMIT_NM);
            float torque_total = sentry_pitch_controller.torque_nm + gravity_ff;
            MOTOR_LIMIT_MIN_MAX(torque_total, -SENTRY_PITCH_MAX_TORQUE_NM,
                                SENTRY_PITCH_MAX_TORQUE_NM);
            sentry_pitch_torque_nm = torque_total;
            sentry_pitch_speed_target_rad_s = sentry_pitch_controller.speed_target_rad_s;
            /* 稳定性符号:编码器闭环下"PID 正输出 → 电机朝增大位置的方向转"为负反馈。
             * 运行期只接受 ±1,其余(含 NaN/0)回退 config,供台架首推反向时在线翻转。 */
            const float torque_sign = (sentry_rt_pitch_torque_sign < 0.0f)
                                          ? -1.0f
                                          : ((sentry_rt_pitch_torque_sign > 0.0f)
                                                 ? 1.0f
                                                 : SENTRY_PITCH_TORQUE_SIGN);
            dm_motor_set_mit_torque(motor, sentry_pitch_torque_nm * torque_sign);
            dm_motor_enable(motor);
            sentry_pitch_control_active = 1u;
            return;
        }
        sentry_pitch_rearm_required = 1u;
    }
    dm_motor_stop(motor);
    dm_motor_set_mit_torque(motor, 0.0f);
    sentry_axis_reset(&sentry_pitch_controller);
    sentry_pitch_reference_initialized = 0u;
    sentry_pitch_torque_nm = sentry_pitch_speed_target_rad_s = 0.0f;
    sentry_pitch_target_rad = position;
    sentry_pitch_control_active = 0u;
}

static void gimbal_small_yaw_task(float now_ms)
{
    dji_motor_t *motor = sentry_small_yaw_motor;
    if (motor == NULL) return;

    const bool auto_aim = (gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE) &&
                          (gimbal_cmd_recv.aim_valid != 0u) && SENTRY_AUTO_AIM_ENABLE;
    const bool command_active = SENTRY_SMALL_YAW_ENABLE &&
        (gimbal_cmd_recv.mode == GIMBAL_GYRO_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_FREE_MODE ||
         gimbal_cmd_recv.mode == GIMBAL_AUTO_AIM_MODE);
    const bool online = daemon_is_online(motor->daemon);
    float small_max_out = sentry_rt_small_yaw_max_out;
    if (!(small_max_out >= 4000.0f && small_max_out <= SENTRY_SMALL_YAW_MAX_OUT))
        small_max_out = SENTRY_SMALL_YAW_MAX_OUT;
    motor->motor_controller.speed_PID.MaxOut = small_max_out;
    sentry_small_yaw_feedback_online = online ? 1u : 0u;
    sentry_small_yaw_speed_deg_s = motor->measure.speed_aps;

    /* 周期时间:目标积分用;异常值(负/超长)本拍不积分,避免 dt 抖动把目标推飞 */
    const float dt_ms = now_ms - sentry_small_yaw_last_ms;
    sentry_small_yaw_last_ms = now_ms;
    const float dt_s = (dt_ms > 0.0f && dt_ms <= 50.0f) ? dt_ms * 0.001f : 0.0f;

    if (!(command_active && online))
    {
        sentry_small_yaw_target_init = 0u; // 掉线/急停后重新解锁时回到中位
        dji_motor_stop(motor);
        dji_motor_set_ref(motor, 0);
        sentry_small_yaw_ref_deg_s = 0.0f;
        sentry_small_yaw_control_active = 0u;
        sentry_small_yaw_limited = 0u;
        sentry_small_yaw_auto_prev = 0u;
        return;
    }

    /* 相对中位解缠后的关节角:中位 ecd 靠近回绕点,裸比较会在零点处误判(见 config) */
    const float c = (float)motor->measure.ecd - (float)SENTRY_SMALL_YAW_CENTER_ECD;
    const float wrapped = (c > 4096.0f) ? (c - 8192.0f) : ((c < -4096.0f) ? (c + 8192.0f) : c);
    const float joint_deg = wrapped * (360.0f / 8192.0f);
    sentry_small_yaw_joint_deg = joint_deg;

    /* 目标角:自瞄由大 yaw 分配;手动解锁首拍归零,之后按摇杆比例积分 */
    float limit_deg = sentry_rt_small_yaw_limit_deg;
    if (!(limit_deg >= 20.0f && limit_deg <= 136.0f))
        limit_deg = SENTRY_SMALL_YAW_LIMIT_DEG;
    const float target_limit = limit_deg - SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG;
    if (auto_aim)
    {
        sentry_small_yaw_target_deg = sentry_small_yaw_auto_valid
                                          ? sentry_small_yaw_auto_target_deg
                                          : joint_deg;
        sentry_small_yaw_target_init = 1u;
        sentry_small_yaw_auto_prev = 1u;
    }
    else
    {
        if (sentry_small_yaw_auto_prev)
        {
            /* 自瞄退回手动:从当前关节角起步,避免回中位跳变 */
            sentry_small_yaw_target_deg = joint_deg;
            sentry_small_yaw_target_init = 1u;
            sentry_small_yaw_auto_prev = 0u;
        }
        else if (!sentry_small_yaw_target_init)
        {
            sentry_small_yaw_target_deg = 0.0f;
            sentry_small_yaw_target_init = 1u;
        }
        sentry_small_yaw_target_deg += gimbal_cmd_recv.small_yaw_rate_deg_s * dt_s;
    }
    if (sentry_small_yaw_target_deg > target_limit) sentry_small_yaw_target_deg = target_limit;
    else if (sentry_small_yaw_target_deg < -target_limit) sentry_small_yaw_target_deg = -target_limit;
    sentry_small_yaw_target_out_deg = sentry_small_yaw_target_deg;

    /* 角度 P:软化死区(扣掉带宽)保证边界出力连续,不会在死区边缘跳变 */
    const float err = sentry_small_yaw_target_deg - joint_deg;
    const float db = SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG;
    float err_eff = (fabsf(err) <= db) ? 0.0f : (err - ((err > 0.0f) ? db : -db));
    float ref = SENTRY_SMALL_YAW_ANGLE_KP * err_eff;
    /* 母体世界角速度注入:载体(大yaw座)晃动时小yaw反向补偿,枪口世界稳像;
     * 大yaw世界环管低频,小yaw直驱补高频。仅 IMU 在线时生效,系数运行期可写,默认 0。 */
    float carrier_inj = sentry_rt_small_yaw_carrier_inj;
    if (!(carrier_inj >= -2.0f && carrier_inj <= 2.0f))
        carrier_inj = SENTRY_SMALL_YAW_CARRIER_INJ;
    if (carrier_inj != 0.0f && sentry_big_yaw_imu_online)
        ref -= carrier_inj * sentry_big_yaw_world_speed_rad_s * FW_RAD_TO_DEG;
    if (ref > SENTRY_SMALL_YAW_MAX_SPEED_DEG_S) ref = SENTRY_SMALL_YAW_MAX_SPEED_DEG_S;
    else if (ref < -SENTRY_SMALL_YAW_MAX_SPEED_DEG_S) ref = -SENTRY_SMALL_YAW_MAX_SPEED_DEG_S;

    if (SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE && fabsf(joint_deg) >= limit_deg)
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

    sentry_small_yaw_ref_deg_s = ref;
    dji_motor_set_ref(motor, ref);
    dji_motor_enable(motor);
    sentry_small_yaw_control_active = 1u;
}
void gimbal_task(void)
{
    const float now_ms = dwt_get_timeline_ms();
    if (msg_recv(gimbal_sub, &gimbal_cmd_recv)) cmd_watchdog_feed(&gimbal_cmd_wd, now_ms);
    if (cmd_watchdog_expired(&gimbal_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS))
        gimbal_cmd_recv.mode = GIMBAL_ZERO_FORCE;
    if (msg_recv(sentry_yaw_chassis_sub, &sentry_yaw_chassis_cmd))
        cmd_watchdog_feed(&sentry_yaw_chassis_wd, now_ms);
    gimbal_big_yaw_task(now_ms);
    gimbal_pitch_task(now_ms);
    gimbal_small_yaw_task(now_ms);
    gimbal_fb.imu = *gimbal_imu_data;
    gimbal_fb.big_yaw_world_deg = sentry_big_yaw_world_rad * FW_RAD_TO_DEG;
    gimbal_fb.big_yaw_rel_deg =
        remainderf(sentry_big_yaw_position_rad - gimbal_front_align_rad(), FW_TWO_PI) * FW_RAD_TO_DEG;
    gimbal_fb.small_yaw_rel_deg = sentry_small_yaw_joint_deg;
    gimbal_fb.small_yaw_target_deg = sentry_small_yaw_target_out_deg;
    gimbal_fb.pitch_deg = sentry_pitch_position_rad * FW_RAD_TO_DEG;
    gimbal_fb.pitch_abs_deg = sentry_pitch_abs_deg;
    gimbal_fb.big_yaw_torque_nm = sentry_big_yaw_torque_nm;
    gimbal_fb.pitch_state = sentry_pitch_driver_state;
    gimbal_fb.big_yaw_state = sentry_big_yaw_driver_state;
    gimbal_fb.big_yaw_online = sentry_big_yaw_feedback_online;
    gimbal_fb.pitch_online = sentry_pitch_feedback_online;
    gimbal_fb.small_yaw_online = sentry_small_yaw_feedback_online;
    gimbal_fb.big_yaw_sample_us =
        (sentry_big_yaw_motor != NULL) ? sentry_big_yaw_motor->rx_sample_us : 0u;
    gimbal_fb.small_yaw_sample_us =
        sentry_small_yaw_feedback_online ? ((uint64_t)kernel_tick_ms() * 1000u) : 0u;
    gimbal_fb.pitch_sample_us =
        (sentry_pitch_motor != NULL) ? sentry_pitch_motor->rx_sample_us : 0u;
    msg_publish(gimbal_pub, &gimbal_fb);
#if GIMBAL_VOFA_UART1_ENABLE
    /* VOFA JustFloat 三通道:
     * 0 大 yaw DM 多圈位置 rad;1 小 yaw GM6020 单圈编码器 0~8191;
     * 2 pitch DM 多圈位置 rad。帧尾 00 00 80 7F 是协议定界符。 */
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
                (sentry_big_yaw_motor != NULL) ? sentry_big_yaw_motor->measure.total_position : 0.0f,
                (sentry_small_yaw_motor != NULL) ? (float)sentry_small_yaw_motor->measure.ecd : 0.0f,
                (sentry_pitch_motor != NULL) ? sentry_pitch_motor->measure.total_position : 0.0f,
            };
            memcpy(gimbal_vofa_frame, channels, sizeof(channels));
            gimbal_vofa_frame[sizeof(channels) + 0u] = 0x00u;
            gimbal_vofa_frame[sizeof(channels) + 1u] = 0x00u;
            gimbal_vofa_frame[sizeof(channels) + 2u] = 0x80u;
            gimbal_vofa_frame[sizeof(channels) + 3u] = 0x7Fu;
            (void)usart_try_send_it(gimbal_vofa_usart, gimbal_vofa_frame,
                                    sizeof(gimbal_vofa_frame));
        }
    }
#endif
}
float gimbal_get_pitch_deg(void) { return sentry_pitch_position_rad * FW_RAD_TO_DEG; }
