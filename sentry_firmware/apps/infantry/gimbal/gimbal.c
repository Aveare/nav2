/**
 * @file    gimbal.c
 * @brief   云台应用:双轴闭环、前馈调度与三模式
 * @layer   APP
 * @author  小猫大王
* @note    yaw(GM6020,命令帧电压/电流可切,见 YAW_ESC_CURRENT_MODE):IMU 世界系双闭环 +
* 目标角速度前馈 + 惯量/摩擦前馈
*          + Kd/Kp 按工况调度;pitch(3508):IMU 双闭环。
* @note    角度反馈直接取 IMU 原值:变化率门限无法区分"真实快速运动"与"IMU 跳变",
*          此路已实测证伪,不要重新引入。
* @note    2026-10-29 建立,作者 JIT CR。
 */
#include "gimbal.h"
#include "gimbal_internal.h"
#include "robot_def.h"
#include "robot_cmd.h"
#include "dji_motor.h"
#include "ins_task.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "dwt_port.h"
#include "general_def.h"
#include "telemetry.h"
#include "calib.h"   // 机械零位标定记录(ECD 回正)
#include "log_port.h" // log_float_to_str/LOG_INFO:RTT 不支持 %f;ECD 回正异常也要报错
#if YAW_AUTO_VOFA_USART6_ENABLE
#include "usart_port.h"
#include "usart.h"
#include <string.h>
#endif

attitude_t *gimbal_imu_data; // 云台IMU数据
static dji_motor_t *yaw_motor;
dji_motor_t *pitch_motor;
#if YAW_AUTO_VOFA_USART6_ENABLE
static usart_t *yaw_vofa_usart;
#endif

/* yaw积分限幅取100:水平轴无重力负载,大限幅相当于蓄能弹簧,饱和后必然过冲 */
static msg_pub_t gimbal_pub;                   // 云台反馈发布者(回传给cmd)
static msg_sub_t gimbal_sub;                  // cmd控制消息订阅者
static gimbal_upload_t gimbal_fb; // 回传给cmd的云台状态
static gimbal_ctrl_cmd_t gimbal_cmd_recv;         // 来自cmd的控制信息
static cmd_watchdog_t gimbal_cmd_wd;               // 命令断流看门狗

/* 目标角速度前馈(世界系,rad/s):不叠加底盘角速度——速度环反馈是IMU世界角速度,
 * 若再减底盘转速会把世界系目标错误改成相对系目标 */
static float yaw_speed_feedforward;
static float yaw_kp_dps_lp = YAW_SPEED_KP_LOW; // 速度环Kp调度一阶低通状态(DEV-20261124-11)
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
static float gimbal_update_yaw_alpha_ref(float dt_s)
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
    float a_ref = (yaw_vref_lp - yaw_vref_lp_last) / dt_s * FW_RAD_TO_DEG; // rad/s² -> deg/s²
    yaw_vref_lp_last = yaw_vref_lp;
    return a_ref;
}
#endif

/**
 * @brief 更新 yaw 输出侧前馈(惯量 + 摩擦),200Hz 由 gimbal_task 调用
 * @param now_ms 当前时刻,ms(与其它前馈共用同一时间基准)
 * @note 惯量项主语是世界系 α:yaw 轴与底盘自转同为竖直轴,惯性系欧拉方程
 *       `J·α_world = τ_motor − τ_friction(ω_joint)` 里没有 α_chassis 项,
 *       底盘加减速只改变关节转速、进而改变摩擦,已由摩擦项覆盖(见 .Doc/yaw轴深化改造计划 §3.1)。
 *       摩擦项主语是关节转速 speed_aps(GM6020 无减速比,转子速度即关节速度)。
 */
static void gimbal_update_yaw_feedforward(float now_ms)
{
    float dt_ms = now_ms - yaw_gyro_last_ms;
    float gyro_deg = gimbal_imu_data->Gyro[2] * FW_RAD_TO_DEG;

    if (dt_ms > 0.0f && dt_ms <= 100.0f)
    {
        float dt_s = dt_ms * 0.001f;
#if YAW_INERTIA_FF_SRC
        float a = gimbal_update_yaw_alpha_ref(dt_s);
#else
        float a = (gyro_deg - yaw_gyro_last) / dt_s;
#endif
        yaw_alpha_f += YAW_INERTIA_FF_LP * (a - yaw_alpha_f);
        MOTOR_LIMIT_MIN_MAX(yaw_alpha_f, -YAW_INERTIA_FF_ALPHA_MAX, YAW_INERTIA_FF_ALPHA_MAX);
        yaw_inertia_ff =
            YAW_INERTIA_FF_GAIN * yaw_alpha_f * YAW_CURRENT_DOMAIN_SCALE +
            YAW_FRICTION_FF_B * yaw_motor->measure.speed_aps * YAW_CURRENT_DOMAIN_SCALE;
    }
    else
        yaw_inertia_ff = 0.0f; // dt 异常周期保持 0,不用坏时间戳外推
    yaw_gyro_last = gyro_deg;
    yaw_gyro_last_ms = now_ms;
}

void gimbal_init()
{
    gimbal_imu_data = ins_init(); // IMU先初始化,姿态数据指针供电机配置引用
    // YAW
    motor_init_config_t yaw_config = {
        .can_init_config =
            {
                .can_handle = &hcan1,
                .tx_id = YAW_MOTOR_ID, // CAN1 GM6020拨码ID5
            },
        .controller_param_init_config =
            {
                .angle_PID =
                    {
                        /* RM-007:Kp/Kd/DeadBand 由运行时唯一写者每拍覆盖(手动/自瞄三元
                         * + gimbal_update_yaw_damping),此处不再提供假初值。
                         * Ki=0 是唯一写者:角度环无积分项。 */
                        .Ki = 0,
                        /* D项限幅:高速转动时截断D,避免"转得越快刹得越狠"导致不跟手 */
                        .DerivativeLimit = YAW_ANGLE_PID_D_LIMIT,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .IntegralLimit = YAW_ANGLE_PID_IL,
                        .MaxOut = YAW_ANGLE_PID_MAXOUT * FW_DEG_TO_RAD, // 目标角速度上限deg/s
                    },
                .speed_PID =
                    {
                        /* RM-007:Kp(双路调度)/Ki/IL(手动/自瞄三元)由运行时每拍覆盖,
                         * 此处不提供假初值。Ki 属输出侧增益,须乘域换算系数(YAW_CURRENT_
                         * DOMAIN_SCALE),漏乘时积分相对强 1.5 倍(电流模式)——该坑在
                         * 运行时写点同样存在,改调度时勿漏。Kd=0 为唯一写者:速度环无 D 项。 */
                        .Kd = 0,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .MaxOut = YAW_ESC_OUT_MAX, // 随命令帧类型切换(电流 16384 / 电压 25000)
                    },
                .other_angle_feedback_ptr = &gimbal_imu_data->YawTotalAngle,
                .other_speed_feedback_ptr = &gimbal_imu_data->Gyro[2],
                .speed_feedforward_ptr = &yaw_speed_feedforward, // 目标角速度前馈
                .current_feedforward_ptr = &yaw_inertia_ff,      // 惯量+摩擦前馈(输出侧)
            },
        .controller_setting_init_config =
            {
                /* 反馈必须用IMU(OTHER_FEED):编码器闭环会锁死云台相对车架角,与跟随律数学上冲突
                 * (底盘会一直转到限幅而offset永不归零)。改编码器必须同时重写跟随律。 */
                .angle_feedback_source = OTHER_FEED,
                .speed_feedback_source = OTHER_FEED,
                .feedforward_flag = CURRENT_AND_SPEED_FEEDFORWARD,
                /* 命令帧类型由 config 的 YAW_ESC_CURRENT_MODE 选择,驱动侧按此决定发
                 * 0x1FE/0x2FE(电流)还是 0x1FF/0x2FF(电压);同发送分组内混用会 fail-fast。
                 * 2026-10 电流帧实测零扭矩(电调固件不支持),故默认仍走电压帧 */
                .control_mode =
                    YAW_ESC_CURRENT_MODE ? MOTOR_CONTROL_CURRENT : MOTOR_CONTROL_VOLTAGE,
                .outer_loop_type = ANGLE_LOOP,
                .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = GM6020};
    // PITCH
    motor_init_config_t pitch_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
                .tx_id = PITCH_MOTOR_ID, // CAN2 M3508+C620 ID4
            },
        .controller_param_init_config =
            {
                .angle_PID =
                    {
                        .Kp = PITCH_ANGLE_PID_KP * FW_DEG_TO_RAD, // deg误差输出rad/s
                        .Ki = 0,
                        .Kd = 0,
                        .DeadBand =
                            PITCH_ANGLE_PID_DEADBAND, // 到位即停(误差<此值不再修正):杀特定角度小极限环(DEV-09),yaw
                                                      // 同款 0.5° 先例。
                                                      // **2026-11-17 0.3→0.6
                                                      // 判据实验**(用户要求):转 yaw 时 pitch
                                                      // 偶发刺响, 怀疑 Pitch 姿态被 yaw 转动扰出
                                                      // ±0.3° 后反复穿越死区边界,每次穿越都是一次
                                                      // 速度目标阶跃(死区内 Output/ITerm
                                                      // 清零),听感即"咔/刺"。 加倍死区 =
                                                      // 把这类小扰动整段吞掉、不再触发修正。
                                                      // 代价:准位偏差上限由 0.3° 放宽到
                                                      // 0.6°(叠加重力残差);边界阶跃幅值也由
                                                      // Kp×0.3≈2.4°/s 变为
                                                      // Kp×0.6≈4.8°/s(触发时更重、但次数更少)。
                                                      // 判据:刺响消失 且 准位误差可接受;否则回退
                                                      // 0.3,改走"反馈域去耦"路线(总计划书 P1)。
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .IntegralLimit = PITCH_ANGLE_PID_IL,
                        .MaxOut = PITCH_ANGLE_PID_MAXOUT * FW_DEG_TO_RAD,
                    },
                .speed_PID =
                    {
                        .Kp = PITCH_SPEED_PID_KP / FW_DEG_TO_RAD, // 反馈为Gyro[0]的rad/s
                        .Ki =
                            PITCH_SPEED_PID_KI /
                            FW_DEG_TO_RAD, // 折中档:20/D2R低频喘振("一顿一顿"),2/D2R起步
                                          // 力矩建立慢(上推延迟,DEV-08实测);仍喘→5/D2R,仍延迟→15/D2R
                        .Kd = 0,
                        /* 输出低通:上抬(高电流工作点)中陀螺噪声xKp2865的力矩纹波被机械啮合放大成
                         * 异响(DEV-10,只上抬有/下抬无/停即消);RC=0.008s@1kHz截止~20Hz,推杆动态(<5Hz)无损。
                         * DEV-20261105-04:水平及以上角度上抬异响复发(连续嗡嗡/上抬有/停即消,DEV-10特征),
                         * 按调参档案§2路径RC
                         * 0.005→0.008。上抬变"肉"→RC回调0.006;异响仍在→转重力前馈
                         * (须先架空松杆标定各角度稳态电流,DEC台账:DEV-05盲上否决先例) */
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement | PID_OutputFilter,
                        .Output_LPF_RC = PITCH_SPEED_PID_RC,
                        .IntegralLimit =
                            PITCH_SPEED_PID_IL, // MaxOut的18.75%。历史:600扛不住水平附近重力(特定角度极限环+
                                                // 回水平不到位,DEV-09)→ 1200;2026-11-16
                                                // 车头加装自瞄模块后解锁回水平 卡在 −6.8°、Iout
                                                // 顶死 1200、电流 3950 码(40s 只爬 0.8°)→ 1200→3000
                                                // (**该实测数据后被判定为暂停核心读数的伪影,见
                                                // DEV-20261116-03**)。 2026-11-16
                                                // 重力前馈标定并启用(GAIN 1.0)后,稳态力矩由前馈承担,
                                                // 实测 Iout 从长期顶死 3000 降到 ±320 →
                                                // 积分只需微调,余量不必留大: 3000→1500。留 1500
                                                // 而非 1200 是因为大角度阶跃时仍需短暂积分支撑;
                                                // 若出现"某角度突然软一下/回位不足"→ 回调
                                                // 2000;若超调/来回荡 → 降到 1200。
                        .MaxOut =
                            PITCH_SPEED_PID_MAXOUT, // C620原始电流给定(16384≈±20A,5000≈±6.1A,仍高于
                                                    // M3508 连续 10A 额定的余量需求)。
                                                    // 历史:2026-11-15 判据实验 4000→8000(最低位
                                                    // -26.45° 处 Output 顶 4000 连续 12s、 θ
                                                    // 纹丝不动、实际电流 4018 码,判断力不够)。
                                                    // 2026-11-16 重力前馈(GAIN 1.0)接管稳态后
                                                    // Output 均值≈0,8000 的峰值权限只在
                                                    // 静摩擦挣脱那一瞬间起作用,过冲/一顿一顿由此而来
                                                    // → 8000→5000。 回退:起停变肉/大角度跟不上 →
                                                    // 6000;仍有冲一下 → 4000~4500;
                                                    // 若出现"顶不动"(Output 长时间贴 5000 且 θ
                                                    // 不动)→ 先查机械,不要回拨 MaxOut。
                    },
                .other_angle_feedback_ptr = &gimbal_imu_data->Pitch,
                .other_speed_feedback_ptr = (&gimbal_imu_data->Gyro[0]),
                .speed_feedforward_ptr =
                    &pitch_speed_feedforward,                 // 目标角速度前馈(治松杆续滑,DEV-11)
                .current_feedforward_ptr = &pitch_gravity_ff, // 重力前馈(输出侧,P2 标定后启用)
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = OTHER_FEED,
                .speed_feedback_source = OTHER_FEED,
                .feedforward_flag = CURRENT_AND_SPEED_FEEDFORWARD,
                .outer_loop_type = ANGLE_LOOP,
                .close_loop_type = SPEED_LOOP | ANGLE_LOOP,
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = M3508,
    };
    // 上电时电机闭环在当前角度,保持静止,收到遥控数据再动;先零力,按CMD入口解锁
    yaw_motor = dji_motor_init(&yaw_config);
    pitch_motor = dji_motor_init(&pitch_config);
    dji_motor_stop(yaw_motor);
    dji_motor_stop(pitch_motor);

#if YAW_AUTO_VOFA_USART6_ENABLE
    /* USART6 在该配置下由自瞄 yaw 调试占用；裁判注册和 UI 任务同步关闭。 */
    usart_init_config_t vofa_config = {
        .recv_buff_size = 1u,
        .usart_handle = &huart6,
        .module_callback = 0,
        .tx_only = 1, // 只发不收
    };
    yaw_vofa_usart = usart_register(&vofa_config);
#endif

    gimbal_pub = msg_pub("gimbal_feed", sizeof(gimbal_upload_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(gimbal_ctrl_cmd_t));
}

/* 按角度误差|err|调度yaw角度环Kd:远离目标低Kd保跟手,接近目标全Kd防松杆回弹;
 * 区间内线性过渡保证Kd连续(跳变会让速度目标突变,激起振荡) */
static void gimbal_update_yaw_damping(void)
{
    float a = fabsf(gimbal_cmd_recv.yaw - gimbal_imu_data->YawTotalAngle);
    const uint8_t auto_aim = gimbal_cmd_recv.gimbal_mode == GIMBAL_AUTO_AIM_MODE;
    const float kd_near = auto_aim ? YAW_AUTO_KD_NEAR : YAW_KD_NEAR;
    const float kd_far = auto_aim ? YAW_AUTO_KD_FAR : YAW_KD_FAR;
    const float blend_lo = auto_aim ? YAW_AUTO_KD_BLEND_LO : YAW_KD_BLEND_LO;
    const float blend_hi = auto_aim ? YAW_AUTO_KD_BLEND_HI : YAW_KD_BLEND_HI;
    float kd;
    if (a >= blend_hi)
        kd = kd_far;
    else if (a <= blend_lo)
        kd = kd_near;
    else
        kd = kd_near + (kd_far - kd_near) * (a - blend_lo) / (blend_hi - blend_lo);
    yaw_motor->motor_controller.angle_PID.Kd = kd * FW_DEG_TO_RAD;
}

#if YAW_AUTO_VOFA_USART6_ENABLE
/**
 * @brief USART6 向 VOFA+ 发送自瞄 yaw JustFloat 数据，50Hz。
 * @note 通道顺序：
 *  0平滑目标角deg，1实际角deg，2角度误差deg，3角度环总输出rad/s，
 *  4速度前馈rad/s，5速度目标rad/s，6实际角速度rad/s，7速度误差rad/s，
 *  8速度环总输出，9输出侧前馈，10最终电调给定估算，11电机反馈电流，
 *  12 AX650串口原始yaw目标deg。各级Kp/Ki/Kd及P/I/D分项不再发送。
 */
static void gimbal_send_auto_vofa(void)
{
    enum
    {
        CHANNEL_COUNT = 13,
        FRAME_SIZE = (CHANNEL_COUNT + 1) * sizeof(float)
    };
    static uint8_t frame[FRAME_SIZE];
    static uint8_t divider;

    if (gimbal_cmd_recv.gimbal_mode != GIMBAL_AUTO_AIM_MODE)
    {
        divider = 0u;
        return;
    }
    if (++divider < YAW_AUTO_VOFA_SEND_DIVIDER)
        return;
    divider = 0u;
    if (!usart_is_ready(yaw_vofa_usart))
        return;

    const pid_instance_t *angle = &yaw_motor->motor_controller.angle_PID;
    const pid_instance_t *speed = &yaw_motor->motor_controller.speed_PID;
    const float channels[CHANNEL_COUNT] = {
        gimbal_cmd_recv.yaw,
        gimbal_imu_data->YawTotalAngle,
        gimbal_cmd_recv.yaw - gimbal_imu_data->YawTotalAngle,
        angle->Output,
        yaw_speed_feedforward,
        speed->Ref,
        speed->Measure,
        speed->Err,
        speed->Output,
        yaw_inertia_ff,
        speed->Output + yaw_inertia_ff,
        (float)yaw_motor->measure.real_current,
        cmd_get_ax650_raw_yaw(),
    };
    memcpy(frame, channels, sizeof(channels));
    frame[sizeof(channels) + 0u] = 0x00u;
    frame[sizeof(channels) + 1u] = 0x00u;
    frame[sizeof(channels) + 2u] = 0x80u;
    frame[sizeof(channels) + 3u] = 0x7Fu;
    usart_try_send_it(yaw_vofa_usart, frame, sizeof(frame));
}
#endif

/* 云台控制核心任务(200Hz) */
void gimbal_task()
{
    // 获取云台控制数据(收到才刷新看门狗)
    const float cmd_now_ms = dwt_get_timeline_ms(); // 喂狗与判活用同一时刻
    if (msg_recv(gimbal_sub, &gimbal_cmd_recv))
        cmd_watchdog_feed(&gimbal_cmd_wd, cmd_now_ms);

    // 命令断流:超时未收到命令切零力,避免云台按最后一帧目标持续出力
    if (cmd_watchdog_expired(&gimbal_cmd_wd, cmd_now_ms, CMD_WATCHDOG_TIMEOUT_MS))
        gimbal_cmd_recv.gimbal_mode = GIMBAL_ZERO_FORCE;

    /* 目标角速度前馈 = 本周期ref增量/dt:自转中推杆时不必先积累大误差才凑出角速度;
     * 单周期增量超过合法上限 -> 目标跳变(重锚定/回中),本周期前馈给0 */
    {
        float dt_ms = cmd_now_ms - yaw_ref_last_ms;
        float delta_ref = gimbal_cmd_recv.yaw - yaw_ref_last;
        if (dt_ms > 0.0f && dt_ms <= 100.0f && fabsf(delta_ref) <= YAW_REF_FF_MAX_DELTA)
            yaw_speed_feedforward = YAW_REF_FF_GAIN * delta_ref / (dt_ms * 0.001f) * FW_DEG_TO_RAD;
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
            float ff_target = PITCH_REF_FF_GAIN * delta_ref_p / (dt_p_ms * 0.001f) * FW_DEG_TO_RAD;
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
        float g =
            PITCH_GRAVITY_A * cosf((gimbal_imu_data->Pitch - PITCH_GRAVITY_PHASE) * FW_DEG_TO_RAD) +
            PITCH_GRAVITY_OFFSET;
        MOTOR_LIMIT_MIN_MAX(g, -PITCH_GRAVITY_FF_LIMIT, PITCH_GRAVITY_FF_LIMIT);
        pitch_gravity_ff = PITCH_GRAVITY_FF_GAIN * g;
    }

    /* 惯量+摩擦前馈:累加 α(世界系)与关节转速,输出到电调给定域;细节与域约定见函数注释 */
    gimbal_update_yaw_feedforward(cmd_now_ms);

    /* AX650 自瞄与手动控制使用两套 yaw PID。切换时清掉上一套参数留下的积分和历史项，
     * 防止从手动切入自瞄后先释放旧积分，造成第一下反冲。 */
    {
        const uint8_t auto_aim = gimbal_cmd_recv.gimbal_mode == GIMBAL_AUTO_AIM_MODE;
        static uint8_t last_auto_aim;
        if (auto_aim != last_auto_aim)
        {
            pid_clear(&yaw_motor->motor_controller.angle_PID);
            pid_clear(&yaw_motor->motor_controller.speed_PID);
            last_auto_aim = auto_aim;
        }
        yaw_motor->motor_controller.angle_PID.Kp =
            (auto_aim ? YAW_AUTO_ANGLE_KP : YAW_ANGLE_PID_KP) * FW_DEG_TO_RAD;
        yaw_motor->motor_controller.angle_PID.DeadBand =
            auto_aim ? YAW_AUTO_ANGLE_DEADBAND : YAW_ANGLE_PID_DEADBAND;
        yaw_motor->motor_controller.speed_PID.Ki =
            (auto_aim ? YAW_AUTO_SPEED_KI : YAW_SPEED_PID_KI) / FW_DEG_TO_RAD *
            YAW_CURRENT_DOMAIN_SCALE;
        yaw_motor->motor_controller.speed_PID.IntegralLimit =
            auto_aim ? YAW_AUTO_SPEED_INTEGRAL_LIMIT : YAW_SPEED_PID_IL;
    }

    // 两个调度必须在ref确定后、电机任务PID计算前刷新
    gimbal_update_yaw_damping();

    /* 速度环Kp双路调度,取较大者(2026-11-17 新增第二路):
     * ① 按电机相对转速:高速区反电动势侵蚀扭矩,线性提Kp补扭矩(历史 DEV-116,原逻辑保留)
     * ② 按云台世界角速度(新增):解决"顺着自转方向打杆时滞后大"的问题——
     *    顺向打杆时 rel≈|−300+ω| 反而变小(如 ω=+200 时 rel=100),按①只能拿 LOW=20,
     *    而逆向打杆 rel 会爬到 450 拿 HIGH;两方向吃到的增益差一倍,这就是顺向滞后的根源。
     *    用世界角速度做第二路:稳态自转时 ω≈0 → 仍拿 LOW(鸡头稳定性零影响),
     *    任何方向打杆时 ω 变大 → 拿到 HIGH,两个方向对称。
     * 取 max 而非替换:不会降低任何现有工况的增益,只补"世界角速度大"的场合,风险最小。 */
    {
        const uint8_t auto_aim = gimbal_cmd_recv.gimbal_mode == GIMBAL_AUTO_AIM_MODE;
        const float kp_low = auto_aim ? YAW_AUTO_SPEED_KP_LOW : YAW_SPEED_KP_LOW;
        const float kp_high = auto_aim ? YAW_AUTO_SPEED_KP_HIGH : YAW_SPEED_KP_HIGH;
        float rel = fabsf(yaw_motor->measure.speed_aps);
        float kp_by_rel;
        if (rel >= YAW_SPEED_KP_BLEND_HI)
            kp_by_rel = kp_high;
        else if (rel <= YAW_SPEED_KP_BLEND_LO)
            kp_by_rel = kp_low;
        else
            kp_by_rel = kp_low + (kp_high - kp_low) * (rel - YAW_SPEED_KP_BLEND_LO) /
                                     (YAW_SPEED_KP_BLEND_HI - YAW_SPEED_KP_BLEND_LO);

        float world = 0.0f;
        if (gimbal_imu_data != 0)
            world = fabsf(gimbal_imu_data->Gyro[2] * FW_RAD_TO_DEG);

        float kp_by_world;
        if (world >= YAW_SPEED_KP_WORLD_HI)
            kp_by_world = kp_high;
        else if (world <= YAW_SPEED_KP_WORLD_LO)
            kp_by_world = kp_low;
        else
            kp_by_world = kp_low + (kp_high - kp_low) * (world - YAW_SPEED_KP_WORLD_LO) /
                                       (YAW_SPEED_KP_WORLD_HI - YAW_SPEED_KP_WORLD_LO);

        float kp_dps = fmaxf(kp_by_rel, kp_by_world);
        /* 一阶低通(DEV-20261124-11):抑制变速时 rel 跨 300 拐点、以及底盘
         * roll/pitch 耦合让 world 短暂越阈值造成的 Kp 快速抖动。
         * 只平滑过渡,不改变稳态调度值;时间常数约 12ms(200Hz 下)。 */
        yaw_kp_dps_lp += YAW_SPEED_KP_LP * (kp_dps - yaw_kp_dps_lp);
        yaw_motor->motor_controller.speed_PID.Kp = yaw_kp_dps_lp / FW_DEG_TO_RAD * YAW_CURRENT_DOMAIN_SCALE;
    }

    switch (gimbal_cmd_recv.gimbal_mode)
    {
    case GIMBAL_ZERO_FORCE: // 停止
        dji_motor_stop(yaw_motor);
        dji_motor_stop(pitch_motor);
        break;
    case GIMBAL_GYRO_MODE:     // IMU反馈闭环(小陀螺/跟随均用此模式)
    case GIMBAL_AUTO_AIM_MODE: // IMU反馈闭环，yaw 使用自瞄独立 PID
    // GIMBAL_FREE_MODE 已裁剪(与GYRO_MODE逻辑相同),枚举保留协议兼容,需要时恢复同分支
    case GIMBAL_FREE_MODE:
        dji_motor_enable(yaw_motor);
        dji_motor_enable(pitch_motor);
        dji_motor_change_feed(yaw_motor, ANGLE_LOOP, OTHER_FEED);
        dji_motor_change_feed(yaw_motor, SPEED_LOOP, OTHER_FEED);
        dji_motor_change_feed(pitch_motor, ANGLE_LOOP, OTHER_FEED);
        dji_motor_change_feed(pitch_motor, SPEED_LOOP, OTHER_FEED);
        dji_motor_set_ref(yaw_motor, gimbal_cmd_recv.yaw);
        dji_motor_set_ref(pitch_motor, gimbal_cmd_recv.pitch);
        break;
    default:
        break;
    }

    // 回传cmd:IMU姿态+yaw电机单圈角度(供跟随律计算offset)
    gimbal_fb.gimbal_imu_data = *gimbal_imu_data;
    gimbal_fb.yaw_motor_single_round_angle = yaw_motor->measure.angle_single_round;

    /* 控制遥测(单写者=本任务),ref/fb/out供Ozone示波与现场回放;out取最终环输出。
     * extra 在 P2 标定期改为**关节转速**(摩擦前馈的自变量),标定结束后可恢复为世界系角速度。 */
    telemetry_push(&telemetry_yaw, gimbal_cmd_recv.yaw, gimbal_imu_data->YawTotalAngle,
                  yaw_motor->motor_controller.speed_PID.Output, yaw_motor->measure.speed_aps,
                  (uint8_t)gimbal_cmd_recv.gimbal_mode);
#if YAW_AUTO_VOFA_USART6_ENABLE
    gimbal_send_auto_vofa();
#endif
#if YAW_FF_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行,四列覆盖 P2 拟合所需的全部量 */
    if (++yaw_calib_cnt >= 100)
    {
        char s_out[16], s_spd[16], s_cur[16], s_ff[16];
        yaw_calib_cnt = 0;
        log_float_to_str(s_out, yaw_motor->motor_controller.speed_PID.Output);
        log_float_to_str(s_spd, yaw_motor->measure.speed_aps);
        log_float_to_str(s_cur, yaw_motor->measure.real_current);
        log_float_to_str(s_ff, yaw_inertia_ff);
        LOG_INFO("[yawcal] out=%s spd=%s cur=%s ff=%s", s_out, s_spd, s_cur, s_ff);
    }
#endif
    telemetry_push(&telemetry_pitch, gimbal_cmd_recv.pitch, gimbal_imu_data->Pitch,
                  pitch_motor->motor_controller.speed_PID.Output,
                  gimbal_imu_data->Gyro[0] * FW_RAD_TO_DEG, (uint8_t)gimbal_cmd_recv.gimbal_mode);
    /* 注:extra 在 P1 阶段保持 Gyro[0]——B7 敲击辨识要看振动通道(此时 out/cur 全为 0);
     * P2 重力标定前再切成输出电流码(见 .Doc/detail/pitch轴优化计划.md P0-2)。 */
#if PITCH_FF_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行,角度与稳态电流成对,供 P2 拟合 A/φ/B */
    if (++pitch_calib_cnt >= 100)
    {
        char p_ang[16], p_out[16], p_spd[16], p_cur[16];
        pitch_calib_cnt = 0;
        log_float_to_str(p_ang, gimbal_imu_data->Pitch);
        log_float_to_str(p_out, pitch_motor->motor_controller.speed_PID.Output);
        log_float_to_str(p_spd, pitch_motor->measure.speed_aps);
        log_float_to_str(p_cur, pitch_motor->measure.real_current);
        LOG_INFO("[pitchcal] ang=%s out=%s spd=%s cur=%s", p_ang, p_out, p_spd, p_cur);
    }
#endif

    msg_publish(gimbal_pub, (void *)&gimbal_fb);
}
