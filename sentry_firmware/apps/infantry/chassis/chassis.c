/**
 * @file    chassis.c
 * @brief   底盘应用:模式仲裁、跟随律与麦轮解算
 * @layer   APP
 * @author  小猫大王
* @note    右手系:平面视图中 x 前进、y 向右、z 逆时针为正。
* @note    本文件源自跃鹿 basic_framework(作者 NeoZeng,上游版本 0.1 / 2022-12-04),
*          由 JIT CR 于 2026-10-26 完成整车适配:45° 全向轮运动学与速度环换算。
 */
#include "chassis.h"
#include "chassis_internal.h"
#include "chassis_spin.h"
#include "chassis_power.h"
#include "robot_def.h"
#include "super_cap.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "ins_task.h" // 跟随/前馈取云台世界角速度 ins_state.Gyro[2]
#include "referee_task.h"

#include "general_def.h"
#include "dwt_port.h"
#include "fault_port.h" // fault_hold_active:开机故障文本独占期(DEV-20261121-09)
#include "referee_ui.h"
#include "arm_math.h"
#include <math.h>      // sqrtf/fabsf:小陀螺轮速余量与刹车判据
#include "telemetry.h" // P1 速度估计验证:底盘反馈遥测环 + [chcal] 标定日志
#if CHASSIS_VOFA_UART1_ENABLE
#include "usart_port.h" // 视觉口改作调试口:实例注册与非阻塞发送
#include "usart.h"     // huart1(PA9/PB7)
#include <string.h>    // memcpy:JustFloat 帧拼装
#endif
#if CHASSIS_CALIB_LOG
#include "log_port.h" // log_float_to_str/LOG_INFO(RTT 不支持 %f)
#endif

/* 根据robot_def.h中的宏自动计算的参数 */

/* mm/s ↔ 转子 deg/s 的唯一换算系数:正解与逆解共用,禁止各自写一份(几何一改就会静默走偏) */

/* chassis_estimate_speed() 的对称消元要求四个力臂相等(即云台居中)。设了偏心就必须重推逆解,
 * 这里用静态断言把"改了偏心但忘了改逆解"挡在编译期。 */
_Static_assert(CHASSIS_GIMBAL_OFFSET_X == 0.0f && CHASSIS_GIMBAL_OFFSET_Y == 0.0f,
               "EstimateSpeed 逆解假设云台居中;设置 CENTER_GIMBAL_OFFSET 后须重推逆解公式");

/* 底盘是单例,不需要单独的结构体 */
#ifdef CHASSIS_BOARD // 底盘板用板载IMU获取底盘转动角速度
#include "can_comm.h"
#include "ins_task.h"
static can_comm_t *chassis_can_comm; // 双板通信
attitude_t *Chassis_IMU_data;
#endif // CHASSIS_BOARD
#ifdef ONE_BOARD
static msg_pub_t chassis_pub;     // 发布底盘反馈
static msg_sub_t chassis_sub;    // 订阅底盘控制命令
static cmd_watchdog_t chassis_cmd_wd; // 命令断流看门狗(仅单板)
#if !YAW_AUTO_VOFA_USART6_ENABLE
static referee_info_t *referee_data;       // 裁判系统反馈数据(热量/血量/弹速,二期接入cmd)
static referee_interactive_info_t ui_data; // UI绘制所需的机器人状态(二期填充真实模式)
#endif
static float chassis_vx_ramp, chassis_vy_ramp;      // 平移斜坡状态
static float chassis_vx_accel_state, chassis_vy_accel_state; // S 曲线加速度状态
static float chassis_last_ms;                       // 上周期时刻,算斜坡步长用
static float gz_last_dps = 0.0f;                    // 上周期云台角速度(deg/s),算角加速度用
static float gz_rate_f = 0.0f;                      // 云台角加速度(deg/s^2),低通后,供 AFF
static float ff_slew = 0.0f;                        // 前馈爬升限速后的值(deg/s)
static attitude_t *chassis_IMU = 0;                 // 云台IMU指针(与gimbal.c共用同一ins_state实例)
#endif                                              // ONE_BOARD
chassis_ctrl_cmd_t chassis_cmd_recv;                // 收到的控制命令
static chassis_upload_t chassis_fb; // 回传的反馈数据
/* 轮速逆解出的底盘真实速度(单写者=chassis_task)。
 * 刻意**不塞进 chassis_upload_t**:那是对板协议结构体,有布局守卫、双板必须同步;
 * 当前消费者只有遥测与后续功率控制(同任务),先留文件内,需要跨任务时再单独加字段。 */
float est_vx, est_vy, est_wz; // mm/s, mm/s, deg/s
/* 调试可写(ELF/DWARF):小陀螺匀速档转速,deg/s。掉电恢复宏值;
 * 写入时在 ROTATE 分支夹到 [0, CHASSIS_SPIN_HIGH_DPS],防止误写把底盘甩飞。
 * 变速档的正弦中心仍用 CHASSIS_SPIN_CRUISE_DPS,不受本变量影响。 */
volatile float chassis_rotate_speed_deg_s = CHASSIS_ROTATE_SPEED;
/* 调试可写(ELF/DWARF):匀速档**带平移**时的自转速度,deg/s。默认 520,带平移时由
 * 7500rpm 轮速 governor 夹低(1500 平移约 344、2000 约 285);
 * 纯自转匀速仍用 chassis_rotate_speed_deg_s。 */
volatile float chassis_rotate_speed_trans_deg_s = CHASSIS_ROTATE_SPEED_TRANS;
/* 调试可写(ELF/DWARF):小陀螺平移方向偏置角,deg。默认宏值;
 * 正/负号决定补偿方向,写 0 可看纯漂移。实车标定用,掉电恢复宏值。 */
volatile float chassis_translate_bias_deg = CHASSIS_TRANSLATE_BIAS_ANGLE;
/* 调试可写:1=用档位平均转速算偏置(不跟随正弦/相位游走),0=用瞬时 wz。 */
volatile uint8_t chassis_bias_mean_mode = 0;
/* 调试可写:漂移补偿模式。0=旋转平移指令(旧);1=底盘系横向速度前馈(新)。
 * 前进/后退漂移角不同 → 漂移是横向速度而非旋转,用 1。 */
volatile uint8_t chassis_translate_bias_mode = 0;
/* 调试可写:横向速度前馈系数,mm/s per deg/s。漂移横向速度 ≈ k*wz。 */
volatile float chassis_translate_bias_mm_s_per_dps = 0.0f;
/* 调试可写:平移斜坡加速度,mm/s²。默认宏值;调小可减少起步/刹车打滑
 * (自转占用摩擦圆时,5000mm/s² 的起步牵引容易滑)。0/NaN 回退宏值。 */
volatile float chassis_vx_ramp_accel = CHASSIS_VX_RAMP;
volatile float chassis_vy_ramp_accel = CHASSIS_VY_RAMP;
/* 调试可写:平移 jerk 上限,mm/s³(S 曲线加速度变化率)。默认宏值;0=退回线性斜坡。 */
volatile float chassis_vx_jerk = CHASSIS_VX_JERK;
volatile float chassis_vy_jerk = CHASSIS_VY_JERK;
/* 调试可写:减速段平移斜坡参数(松杆/反向扳杆时使用)。默认宏值;
 * <=0 回退到加速段参数(对称斜坡)。加速段始终用上面的 ramp_accel/jerk,不受影响。 */
volatile float chassis_vx_brake_ramp = CHASSIS_VX_BRAKE_RAMP;
volatile float chassis_vy_brake_ramp = CHASSIS_VY_BRAKE_RAMP;
volatile float chassis_vx_brake_jerk = CHASSIS_VX_BRAKE_JERK;
volatile float chassis_vy_brake_jerk = CHASSIS_VY_BRAKE_JERK;
/* 小陀螺自转功率 scale:DEV-20261126-33 起退役,恒 1(总 scale 永远作用于四轮电流);
 * 保留变量仅为 [pwr] ss= 兼容与旧调试脚本读取。 */
volatile float chassis_spin_power_scale = 1.0f;
/* 调试可写:小陀螺模式平移最大速度,mm/s。<=0 退回 CHASSIS_MAX_SPEED。
 * 与纯平移同上限 2000;自转由 7500rpm 轮速 governor 自动夹低(2000 平移约 285)。 */
volatile float chassis_spin_max_speed_mm_s = CHASSIS_SPIN_MAX_SPEED;
/* 调试可写:小陀螺模式轮转子转速上限,rpm。<=0 回退宏值,超过 6000 夹到 6000。 */
volatile float chassis_spin_wheel_rpm_max = CHASSIS_SPIN_WHEEL_RPM_MAX;
/* 调试可写:刹车时自转削减比例(0~1)。0=关闭;越大刹车越优先,自转让出摩擦圆越多。 */
volatile float chassis_spin_brake_gain = CHASSIS_SPIN_BRAKE_GAIN;
/* 调试可写:平移段变速带下/上界,deg/s。默认 150~450(纯自转 100~520 的平移段收窄);
 * 高段实际还会被轮速余量夹低。0=回退默认。 */
volatile float chassis_spin_trans_low_dps = CHASSIS_SPIN_TRANS_LOW_DPS;
volatile float chassis_spin_trans_high_dps = CHASSIS_SPIN_TRANS_HIGH_DPS;
/* 调试可写:平移→纯自转交接保持时长,ms;0=直接切。 */
volatile float chassis_spin_handoff_hold_ms = CHASSIS_SPIN_HANDOFF_HOLD_MS;
/* 调试可写:交接 RAMP/HOLD 段速度环积分限幅比例(0~1);1=不压,0=去积分。 */
volatile float chassis_spin_handoff_integral_scale = CHASSIS_SPIN_HANDOFF_INTEGRAL_SCALE;
/* 调试可写:平移切入后自转保持低位的时长,ms;0=关闭入口保持。 */
volatile float chassis_spin_entry_hold_ms = CHASSIS_SPIN_ENTRY_HOLD_MS;
/* 调试可写:入口保持段的自转带宽,deg/s(默认 30 → 150~180)。 */
volatile float chassis_spin_entry_band_dps = CHASSIS_SPIN_ENTRY_BAND_DPS;
/* 调试可写:总功率 scale 下限(0.20);模型需要更低时由 POWER_SCALE_MIN 兜底。 */
volatile float chassis_power_min_scale = CHASSIS_POWER_MIN_SCALE;
/* 调试可写:平移切入瞬态窗口,ms;只影响积分压限/入口保持,不再开关总 scale。 */
volatile float chassis_power_trans_entry_ms = CHASSIS_POWER_TRANS_ENTRY_MS;
/* 调试可写:交接结束后的积分压限尾巴,ms。 */
volatile float chassis_power_trans_tail_ms = CHASSIS_POWER_TRANS_TAIL_MS;
/* 调试可写(32 位,ELF 写方便):小陀螺档位覆盖,优先级高于键鼠 C。
 * 0=跟随 chassis_cmd_recv.spin_vary;1=强制匀速;2=强制变速。
 * 手柄/台架没有 C 键时直接写本变量;写回 0 恢复 C 键控制。 */
volatile uint32_t chassis_spin_mode_override = 0;
/* 本拍生效档位(1=变速 0=匀速),供偏置参考与 [pwr] sv= 使用。 */
static uint8_t chassis_spin_vary_active = 1u;
/* 上拍平移是否在占轮速:匀速档据此选 260(平移)/300(纯自转)。 */
static uint8_t chassis_spin_trans_active = 0u;
/* 小陀螺轨迹内部状态(单写者=chassis_task,供下一拍使用,滞后 5ms):
 * center=当前轨迹中心转速, cap=当前轮速允许的最大自转, brake=刹车削减系数,
 * mod=当前调制限幅;flow=平移/交接/纯自转状态机。 */
typedef enum
{
    SPIN_FLOW_TRANS = 0, // 平移中:用 150~450 带
    SPIN_FLOW_RAMP,      // 刚松杆:向最近纯自转端点(100/520)爬升
    SPIN_FLOW_HOLD,      // 到端点保持,等轮速收敛
    SPIN_FLOW_PURE,      // 纯自转:100~520 正弦
} spin_flow_e;
static float chassis_spin_center = CHASSIS_SPIN_CRUISE_DPS;
static float chassis_spin_wz_cap = CHASSIS_SPIN_HIGH_DPS;
static float chassis_spin_brake = 1.0f;
static float chassis_spin_mod_limit = CHASSIS_SPIN_MOD_LIMIT;
static spin_flow_e chassis_spin_flow = SPIN_FLOW_PURE;
static float chassis_spin_handoff_target = CHASSIS_SPIN_LOW_DPS;
static float chassis_spin_handoff_wz = 0.0f;
static float chassis_spin_handoff_ms = 0.0f;
static float chassis_spin_last_wz = 0.0f;
/* 入口保持与瞬态总 scale 的计时(单写者=chassis_task): */
static float chassis_spin_entry_ms = 0.0f;         // 入口自转保持剩余,ms
static uint8_t chassis_spin_prev_trans = 0u;       // 上拍 trans_active,做上升沿
static float chassis_power_transient_ms = 0.0f;    // 瞬态总 scale 窗口剩余,ms
static uint8_t chassis_power_transient_active = 0u; // 本拍瞬态总 scale 标志
#if CHASSIS_CALIB_LOG
static uint16_t chassis_calib_cnt; // 标定日志分频计数
/* 调试可写:标定日志分频,200Hz/div;默认 100=2Hz(0.5s),写 10=20Hz(瞬态复采),
 * 写 1=200Hz(仅短时,日志量很大)。运行期改,不用重编译。 */
volatile uint16_t chassis_calib_log_div = 100u;
#endif

dji_motor_t *motor_lf, *motor_rf, *motor_lb, *motor_rb; // left right forward back

#if CHASSIS_VOFA_UART1_ENABLE
/* USART1(视觉口)独占实例:本配置下 AX650 链路不初始化,口归底盘调试用。 */
static usart_t *chassis_vofa_usart;
#endif

/* 私有中介变量,设为静态避免参数传递开销 */
float chassis_vx, chassis_vy;     // 云台系速度投影到底盘系
float vt_lf, vt_rf, vt_lb, vt_rb; // 解算后的轮速,待限幅

/**
 * @brief 小陀螺平移限速:CHASSIS_ROTATE 下按合速度等比缩到运行期上限
 * @note  命令在云台系,等比缩不改方向;缩放发生在偏置/斜坡之前,减速仍受 S 曲线约束。
 *        上限 <=0 或超过 CHASSIS_MAX_SPEED 时按 CHASSIS_MAX_SPEED 兜底(不额外限速)。
 */
static void chassis_spin_limit_translate(void)
{
    float cap = chassis_spin_max_speed_mm_s;
    if (!(cap > 0.0f) || cap > CHASSIS_MAX_SPEED)
        cap = CHASSIS_MAX_SPEED;
    const float norm = sqrtf(chassis_cmd_recv.vx * chassis_cmd_recv.vx +
                             chassis_cmd_recv.vy * chassis_cmd_recv.vy);
    if (norm > cap)
    {
        const float scale = cap / norm;
        chassis_cmd_recv.vx *= scale;
        chassis_cmd_recv.vy *= scale;
    }
}

/**
 * @brief 刹车优先:平移在减速时按减速度比例削减自转,把摩擦圆让给前后刹车
 * @return 本拍自转系数,CHASSIS_SPIN_BRAKE_MIN~1
 * @note  用上一拍的 S 曲线加速度状态:本函数在模式解算(switch)里调用,而斜坡块在其后,
 *        5ms 滞后对刹车无害。只在速度方向与加速度相反(真减速)且速度高于
 *        CHASSIS_SPIN_BRAKE_SPEED_MIN 时触发;低通 25ms,比轨迹 α 限幅快。
 *        松开自转削减后由低通自然恢复,不会在下一次刹车前残留。
 */
static float chassis_spin_brake_factor(void)
{
    float demand = 0.0f;
    if (chassis_vx_ramp * chassis_vx_accel_state < 0.0f)
    {
        const float a_ref = (chassis_vx_ramp_accel > 0.0f) ? chassis_vx_ramp_accel : CHASSIS_VX_RAMP;
        demand = fabsf(chassis_vx_accel_state) / a_ref;
    }
    if (chassis_vy_ramp * chassis_vy_accel_state < 0.0f)
    {
        const float a_ref = (chassis_vy_ramp_accel > 0.0f) ? chassis_vy_ramp_accel : CHASSIS_VY_RAMP;
        const float d = fabsf(chassis_vy_accel_state) / a_ref;
        if (d > demand)
            demand = d;
    }
    if (demand > 1.0f)
        demand = 1.0f;

    const float vmag = sqrtf(chassis_vx_ramp * chassis_vx_ramp +
                             chassis_vy_ramp * chassis_vy_ramp);
    float gain_max = chassis_spin_brake_gain;
    if (!(gain_max > 0.0f))
        gain_max = 0.0f;
    if (gain_max > 1.0f)
        gain_max = 1.0f;

    float tgt = 1.0f;
    if (vmag > CHASSIS_SPIN_BRAKE_SPEED_MIN && demand > 0.0f)
        tgt = 1.0f - gain_max * demand;
    if (tgt < CHASSIS_SPIN_BRAKE_MIN)
        tgt = CHASSIS_SPIN_BRAKE_MIN;
    if (tgt > 1.0f)
        tgt = 1.0f;

    chassis_spin_brake += CHASSIS_SPIN_BRAKE_LP * (tgt - chassis_spin_brake);
    if (!(chassis_spin_brake >= CHASSIS_SPIN_BRAKE_MIN)) // NaN 兜底
        chassis_spin_brake = CHASSIS_SPIN_BRAKE_MIN;
    if (chassis_spin_brake > 1.0f)
        chassis_spin_brake = 1.0f;
    return chassis_spin_brake;
}

/**
 * @brief 按平移占用重算自转余量,并驱动平移/纯自转交接状态机
 * @param now_ms 本拍时刻,ms(DWT 时间线)
 * @note  45°麦轮几何:平移 1mm/s 占 0.7071*WHEEL_RATIO deg/s 转子,
 *        自转 1deg/s 占 CENTER*0.7071*WHEEL_RATIO deg/s。取四轮最坏值 |vx|+|vy|,
 *        所以用投影到底盘系(含平移偏置/横向前馈)的 vx/vy。
 *        平移时用 150~450 带(高段被轮速余量夹低);停平移后进入交接:
 *        先向最近端点(100 或 520)爬升并保持 200ms,再从该端点相位重启 100~520
 *        正弦(DEV-20261126-28)。非 ROTATE 复位。
 */
static void chassis_spin_headroom_update(float now_ms)
{
    static float last_ms = 0.0f;
    float dt_ms = now_ms - last_ms;
    last_ms = now_ms;
    if (!(dt_ms > 0.0f) || dt_ms > 100.0f)
        dt_ms = 5.0f;

    if (chassis_cmd_recv.chassis_mode != CHASSIS_ROTATE)
    {
        chassis_spin_center = CHASSIS_SPIN_CRUISE_DPS;
        chassis_spin_wz_cap = CHASSIS_SPIN_HIGH_DPS;
        chassis_spin_brake = 1.0f;
        chassis_spin_mod_limit = CHASSIS_SPIN_MOD_LIMIT;
        chassis_spin_flow = SPIN_FLOW_PURE;
        chassis_spin_handoff_target = CHASSIS_SPIN_LOW_DPS;
        chassis_spin_handoff_wz = 0.0f;
        chassis_spin_handoff_ms = 0.0f;
        chassis_spin_last_wz = 0.0f;
        chassis_spin_entry_ms = 0.0f;
        chassis_spin_prev_trans = 0u;
        chassis_spin_trans_active = 0u;
        chassis_power_transient_ms = 0.0f;
        chassis_power_transient_active = 0u;
        return;
    }

    float rpm_max = chassis_spin_wheel_rpm_max;
    if (!(rpm_max > 0.0f))
        rpm_max = CHASSIS_SPIN_WHEEL_RPM_MAX;
    if (rpm_max > CHASSIS_MAX_MOTOR_RPM)
        rpm_max = CHASSIS_MAX_MOTOR_RPM;

    const float trans_wheel =
        (fabsf(chassis_vx) + fabsf(chassis_vy)) * 0.70710678f * CHASSIS_WHEEL_RATIO;
    const float headroom = rpm_max * FW_RPM_TO_DEG_S - trans_wheel;

    float wz_cap = 0.0f;
    if (headroom > 0.0f)
        wz_cap = headroom / (CHASSIS_LF_CENTER * 0.70710678f * CHASSIS_WHEEL_RATIO);
    if (wz_cap > CHASSIS_SPIN_HIGH_DPS)
        wz_cap = CHASSIS_SPIN_HIGH_DPS;
    chassis_spin_wz_cap = wz_cap;

    const int trans_active = (trans_wheel > CHASSIS_SPIN_MOD_TRANS_MIN_DPS);

    /* 平移切入上升沿:开两个窗口
     * ① 瞬态总 scale(DEV-20261126-31):覆盖 S 曲线加速段,对四轮电流生效;
     * ② 入口自转保持(DEV-20261126-31):变速档先把自转压在 150~180,
     *    200ms 后再放宽到 150~320,避免平移和自转同时加速。 */
    if (trans_active && !chassis_spin_prev_trans)
    {
        float entry_ms = chassis_power_trans_entry_ms;
        if (!(entry_ms > 0.0f))
            entry_ms = 0.0f;
        if (entry_ms > 2000.0f)
            entry_ms = 2000.0f;
        chassis_power_transient_ms = entry_ms;

        if (chassis_spin_vary_active)
        {
            float hold = chassis_spin_entry_hold_ms;
            if (!(hold > 0.0f))
                hold = 0.0f;
            if (hold > 1000.0f)
                hold = 1000.0f;
            chassis_spin_entry_ms = hold;

            /* 入口保持直接置中心/调制(150~180、中心 165),不再等 100ms 中心低通:
             * 20Hz 实测 200ms 只到 ct=184,延长到 300ms 并直接置位。 */
            float low = chassis_spin_trans_low_dps;
            if (!(low >= 0.0f) || low > CHASSIS_SPIN_HIGH_DPS)
                low = CHASSIS_SPIN_TRANS_LOW_DPS;
            float band = chassis_spin_entry_band_dps;
            if (!(band >= 0.0f) || band > 200.0f)
                band = CHASSIS_SPIN_ENTRY_BAND_DPS;
            const float entry_high = low + band;
            chassis_spin_center = (low + entry_high) * 0.5f;
            chassis_spin_mod_limit =
                (entry_high + low > 1.0f) ? ((entry_high - low) / (entry_high + low)) : 0.09f;
        }
    }
    chassis_spin_prev_trans = (uint8_t)(trans_active ? 1u : 0u);
    chassis_spin_trans_active = (uint8_t)(trans_active ? 1u : 0u);

    if (chassis_power_transient_ms > 0.0f)
    {
        chassis_power_transient_ms -= dt_ms;
        if (chassis_power_transient_ms < 0.0f)
            chassis_power_transient_ms = 0.0f;
    }
    if (chassis_spin_entry_ms > 0.0f)
    {
        chassis_spin_entry_ms -= dt_ms;
        if (chassis_spin_entry_ms < 0.0f)
            chassis_spin_entry_ms = 0.0f;
    }

    /* ---------------- 状态机 ----------------
     * 变速档:平移中=TRANS;松杆沿=TRANS→RAMP(选最近端点);RAMP 到达→HOLD;
     * HOLD 结束→PURE(从端点相位重启正弦)。匀速档不参与交接。 */
    if (!chassis_spin_vary_active)
    {
        chassis_spin_flow = SPIN_FLOW_PURE;
        chassis_spin_handoff_ms = 0.0f;
    }
    else if (trans_active)
    {
        if (chassis_spin_flow == SPIN_FLOW_RAMP || chassis_spin_flow == SPIN_FLOW_HOLD)
        {
            /* 交接途中又推平移:从当前交接速度重启发正弦,避免跳回旧相位 */
            chassis_spin_reset((uint32_t)(now_ms * 1000.0f) ^ 0x9E3779B9u,
                               chassis_spin_handoff_wz);
        }
        chassis_spin_flow = SPIN_FLOW_TRANS;
        chassis_spin_handoff_ms = 0.0f;
    }
    else if (chassis_spin_flow == SPIN_FLOW_TRANS)
    {
        /* 松杆沿:按当前正弦输出选最近端点(小于 310 去 100,否则去 520) */
        const float last = chassis_spin_last_wz;
        chassis_spin_handoff_target =
            (last < CHASSIS_SPIN_CRUISE_DPS) ? CHASSIS_SPIN_LOW_DPS : CHASSIS_SPIN_HIGH_DPS;
        chassis_spin_handoff_wz = last;
        chassis_spin_handoff_ms = 0.0f;
        chassis_spin_flow = SPIN_FLOW_RAMP;
    }

    /* 轨迹带目标:平移段用显式 low/high(受轮速余量夹低),其余回纯自转 100~520。 */
    float center_tgt = CHASSIS_SPIN_CRUISE_DPS;
    float mod_tgt = CHASSIS_SPIN_MOD_LIMIT;
    if (chassis_spin_flow == SPIN_FLOW_TRANS)
    {
        float low = chassis_spin_trans_low_dps;
        float high = chassis_spin_trans_high_dps;
        if (!(low >= 0.0f) || low > CHASSIS_SPIN_HIGH_DPS)
            low = CHASSIS_SPIN_TRANS_LOW_DPS;
        if (!(high >= 0.0f) || high > CHASSIS_SPIN_HIGH_DPS)
            high = CHASSIS_SPIN_TRANS_HIGH_DPS;
        if (high > wz_cap)
            high = wz_cap;
        if (low > high)
            low = high;
        /* 入口保持段窄带:150~180(默认),避免切入时自转和加速同时拉高轮速 */
        if (chassis_spin_entry_ms > 0.0f && chassis_spin_vary_active)
        {
            float band = chassis_spin_entry_band_dps;
            if (!(band >= 0.0f) || band > 200.0f)
                band = CHASSIS_SPIN_ENTRY_BAND_DPS;
            const float entry_high = low + band;
            if (high > entry_high)
                high = entry_high;
            if (low > high)
                low = high;
        }
        center_tgt = (low + high) * 0.5f;
        mod_tgt = (high + low > 1.0f) ? ((high - low) / (high + low)) : 0.05f;
    }
    if (center_tgt < CHASSIS_SPIN_CENTER_MIN_DPS)
        center_tgt = CHASSIS_SPIN_CENTER_MIN_DPS;
    if (center_tgt > CHASSIS_SPIN_CRUISE_DPS)
        center_tgt = CHASSIS_SPIN_CRUISE_DPS;
    if (mod_tgt < 0.05f)
        mod_tgt = 0.05f;
    if (mod_tgt > 0.80f)
        mod_tgt = 0.80f;

    chassis_spin_mod_limit += CHASSIS_SPIN_MOD_LIMIT_LP * (mod_tgt - chassis_spin_mod_limit);
    if (chassis_spin_mod_limit < 0.05f)
        chassis_spin_mod_limit = 0.05f;
    if (chassis_spin_mod_limit > 0.80f)
        chassis_spin_mod_limit = 0.80f;

    chassis_spin_center += CHASSIS_SPIN_CENTER_LP * (center_tgt - chassis_spin_center);
    if (chassis_spin_center < CHASSIS_SPIN_CENTER_MIN_DPS)
        chassis_spin_center = CHASSIS_SPIN_CENTER_MIN_DPS;
    if (chassis_spin_center > CHASSIS_SPIN_CRUISE_DPS)
        chassis_spin_center = CHASSIS_SPIN_CRUISE_DPS;
}

void chassis_init()
{
#ifdef ONE_BOARD
    /* 云台角速度前馈需要IMU姿态;ins_init幂等,返回与gimbal.c相同的实例 */
    chassis_IMU = ins_init();
#endif
    // 四个轮子参数一样,改tx_id即可(本车CAN1:左前4/右前1/左后2/右后3)
    motor_init_config_t chassis_motor_config = {
        .can_init_config.can_handle = &hcan1,
        .controller_param_init_config =
            {
                .speed_PID =
                    {
                        .Kp = CHASSIS_SPEED_PID_KP, // 速度反馈为deg/s域(M3508实测)
                        .Ki = CHASSIS_SPEED_PID_KI, // 消除纯P速度跟踪滞后
                        .Kd = 0,
                        .IntegralLimit = CHASSIS_SPEED_PID_IL,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .MaxOut = CHASSIS_SPEED_PID_MAXOUT,
                        .Output_LPF_RC = CHASSIS_SPEED_PID_RC,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .outer_loop_type = SPEED_LOOP,
                .close_loop_type = SPEED_LOOP,
            },
        .motor_type = M3508,
    };
    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_LF;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_lf = dji_motor_init(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_RF;
    motor_rf = dji_motor_init(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_LB;
    motor_lb = dji_motor_init(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_RB;
    motor_rb = dji_motor_init(&chassis_motor_config);
    dji_motor_stop(motor_lf);
    dji_motor_stop(motor_rf);
    dji_motor_stop(motor_lb);
    dji_motor_stop(motor_rb);

#if CHASSIS_POWER_CTRL_EN
    /* 功率系数是跨拍静态量,初始化必须回到 1.0,否则上一局/上一次运行的限幅残留
     * (主机整链 golden 的"rerun differs"就是漏了这一步) */
    chassis_power_reset();
#endif

    /* 裁判系统UI任务注册(DEV-20261030-12一期:仅通链路,UI内为假数据循环RobotModeTest,
     * 硬件联调通过后接真实模式数据再关测试函数)。硬件未接时referee_my_ui_init会等robot_id,不阻塞其他任务 */
#if !YAW_AUTO_VOFA_USART6_ENABLE
    referee_data = referee_ui_task_init(&huart6, &ui_data);
#endif

#if CHASSIS_VOFA_UART1_ENABLE
    /* 视觉口改作底盘调试口:只做"实例注册 + 句柄注入"(模式允许),不在此处发数据;
     * 波特率/帧格式沿用 CubeMX 的 USART1 = 921600 8N1,不再调 usart_configure8_e1。 */
    usart_init_config_t chassis_vofa_config = {
        .recv_buff_size = 1u,
        .usart_handle = &huart1, // REDFLAG-EXEMPT: Init 句柄注入(AGENTS.md 合规口径),G4 redflags
                                 // 已豁免(B0a-②,2026-11-22)
        .module_callback = 0,    // 只发不收:VOFA 不需要下行
        .tx_only = 1,            // 显式关闭接收:否则循环 DMA 会持续写入无人消费(DEV-20261121-06)
    };
    chassis_vofa_usart = usart_register(&chassis_vofa_config);
#endif

#ifdef CHASSIS_BOARD
    Chassis_IMU_data = ins_init();

    can_comm_init_config_t comm_conf = {
        .can_config =
            {
                .can_handle = &hcan2,
                .tx_id = 0x311,
                .rx_id = 0x312,
            },
        .recv_data_len = sizeof(chassis_ctrl_cmd_t),
        .send_data_len = sizeof(chassis_upload_t),
    };
    chassis_can_comm = can_comm_init(&comm_conf);
#endif // CHASSIS_BOARD

#ifdef ONE_BOARD
    chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    chassis_pub = msg_pub("chassis_feed", sizeof(chassis_upload_t));
#endif // ONE_BOARD
}

#if CHASSIS_VOFA_UART1_ENABLE
/**
 * @brief 向 VOFA+ 发送底盘 JustFloat 数据,200Hz(分频见 CHASSIS_VOFA_SEND_DIVIDER)。
 * @note  通道顺序(全部 float,单位写在后面):
 *   0 vx指令mm/s   1 vx斜坡后mm/s   2 vx逆解mm/s
 *   3 vy指令mm/s   4 vy斜坡后mm/s   5 vy逆解mm/s
 *   6 wz指令deg/s  7 wz逆解deg/s
 *   8 云台-底盘夹角deg  9 IMU世界系yaw角速度deg/s
 *  10..13 四轮速度参考deg/s(转子域,LF/RF/LB/RB)  14..17 四轮实测速度deg/s(转子域)
 *  18..21 四轮电流原始码(C620 满量程 16384)   22 底盘功率W   23 缓冲能量J   24 底盘模式枚举
 * @note  22/23 来自裁判系统;未接裁判系统时恒为 -1。
 * @note  通道 9 的坑:IMU 装在云台上,小陀螺瞄准保持时云台世界系静止,它读到的就是 0;
 *        只有"不跟随 + 不自转"时它才等于底盘转速。真实转速仍靠计时法独立测量。
 * @note  帧尾 0x00 0x00 0x80 0x7F 是 VOFA JustFloat 的帧定界符,靠它切帧,不能改。
 * @note  先查忙再拼帧:IT 发送未完成时直接丢本帧,绝不覆盖正在发送的缓冲。
 */
static void chassis_send_vofa(void)
{
    /* 开机故障文本独占期:暂停 JustFloat,避免把 [FAULT] 文本淹掉(DEV-20261121-09) */
    if (fault_hold_active())
        return;

    enum
    {
        CHANNEL_COUNT = 25,
        FRAME_SIZE = (CHANNEL_COUNT + 1) * sizeof(float)
    };
    static uint8_t frame[FRAME_SIZE];
    static uint8_t divider;

    if (++divider < CHASSIS_VOFA_SEND_DIVIDER)
        return;
    divider = 0u;
    if (!usart_is_ready(chassis_vofa_usart))
        return;

    /* 斜坡后的实际速度目标:双板没有斜坡环,退化为直通指令 */
    float vx_ramp = chassis_cmd_recv.vx;
    float vy_ramp = chassis_cmd_recv.vy;
#ifdef ONE_BOARD
    vx_ramp = chassis_vx_ramp;
    vy_ramp = chassis_vy_ramp;
#endif

    float gz = 0.0f;
#ifdef ONE_BOARD
    if (chassis_IMU != 0)
        gz = chassis_IMU->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG;
#else
    if (Chassis_IMU_data != 0)
        gz = Chassis_IMU_data->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG;
#endif

    float power = -1.0f, buffer_energy = -1.0f;
#if defined(ONE_BOARD) && !YAW_AUTO_VOFA_USART6_ENABLE
    if (referee_data != 0)
    {
        power = referee_data->PowerHeatData.chassis_power;
        buffer_energy = (float)referee_data->PowerHeatData.buffer_energy;
    }
#endif

    const float channels[CHANNEL_COUNT] = {
        chassis_cmd_recv.vx,
        vx_ramp,
        est_vx,
        chassis_cmd_recv.vy,
        vy_ramp,
        est_vy,
        chassis_cmd_recv.wz,
        est_wz,
        chassis_cmd_recv.offset_angle,
        gz,
        vt_lf,
        vt_rf,
        vt_lb,
        vt_rb,
        motor_lf->measure.speed_aps,
        motor_rf->measure.speed_aps,
        motor_lb->measure.speed_aps,
        motor_rb->measure.speed_aps,
        (float)motor_lf->measure.real_current,
        (float)motor_rf->measure.real_current,
        (float)motor_lb->measure.real_current,
        (float)motor_rb->measure.real_current,
        power,
        buffer_energy,
        (float)chassis_cmd_recv.chassis_mode,
    };
    memcpy(frame, channels, sizeof(channels));
    frame[sizeof(channels) + 0u] = 0x00u;
    frame[sizeof(channels) + 1u] = 0x00u;
    frame[sizeof(channels) + 2u] = 0x80u;
    frame[sizeof(channels) + 3u] = 0x7Fu;
    usart_try_send_it(chassis_vofa_usart, frame, sizeof(frame));
}
#endif

/* 机器人底盘控制核心任务(200Hz) */
void chassis_task()
{
    // 获取控制命令(收到才算"命令在流",未收到由看门狗兜底)
    const float cmd_now_ms = dwt_get_timeline_ms();
#ifdef ONE_BOARD
    /* 取消息与喂狗用同一时刻,避免两次调用间时间推进造成的边界抖动 */
    if (msg_recv(chassis_sub, &chassis_cmd_recv))
        cmd_watchdog_feed(&chassis_cmd_wd, cmd_now_ms);
#endif
#ifdef CHASSIS_BOARD
    // 双板:数据来自CAN,断流由can_comm自带daemon判定
    chassis_cmd_recv = *(chassis_ctrl_cmd_t *)can_comm_get(chassis_can_comm);
#endif

    /* 小陀螺模式平移限速:给自转留轮速余量,并降低刹车动能(DEV-20261126-25) */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE)
        chassis_spin_limit_translate();

#if CHASSIS_SPIN_VARY_EN
    /* 小陀螺轨迹的进入沿与周期基准:只在 ROTATE 的变速档内推进。
     * 切出 ROTATE 或降到匀速档都复位,再次进入从巡航段重新起步(DEV-20261124-06/25-08)。 */
    static uint8_t spin_active = 0;
    static float spin_last_ms = 0.0f;
    if (chassis_cmd_recv.chassis_mode != CHASSIS_ROTATE)
        spin_active = 0;
#endif

    // 按模式设定旋转速度
    switch (chassis_cmd_recv.chassis_mode)
    {
    case CHASSIS_NO_FOLLOW: // 不旋转,保持全向机动
        chassis_cmd_recv.wz = 0;
        break;
    case CHASSIS_ALIGN: // 回中:只旋底盘把关节角收到 0,云台由自身闭环保持世界方向(2026-11-17)
    {
        /* 为什么不复用跟随律:跟随律是"前馈 + 线性位置项"的**跟踪**律,增益与死区按跟踪工况定;
         * 回中时云台不动、前馈恒为 0,剩下的位置项在接近死区处会反复过冲(一顿一顿)。
         * 回中要的是"恒速靠近 + 末端线性收敛",故单独一路。
         * 符号与跟随环位置项一致(sgn(off)>0 时正转即可减小 |off|,由跟随环的稳定性反证)。 */
        float off = chassis_cmd_recv.offset_angle;
        float sgn = (off > 0.0f) ? 1.0f : -1.0f;
        float cmd = sgn * CHASSIS_ALIGN_SPEED;

        /* 末端按比例降速:离死区越近给得越少,避免整速冲进死区再反向修正 */
        if (fabsf(off) < CHASSIS_ALIGN_SOFT)
            cmd *= fabsf(off) / CHASSIS_ALIGN_SOFT;

        chassis_cmd_recv.wz = cmd;
        break;
    }
    case CHASSIS_FOLLOW_GIMBAL_YAW: // 跟随云台:wz=位置项+速度阻尼项+角速度前馈
    {
        /* ===== 跟随律:一阶角度环 + 角速度前馈 + 额度分配限幅(2026-11-20 重写) =====
         * 结构参考官方 Development-Board-C(2020 standard_robot)的 chassis_angle_pid:
         * 位置式、目标相对角可配、输出限幅;本车额外保留角速度前馈(官方没有)。
         *
         * 相对 1027 DEV-88 版的四处结构改动(是重写,不是微调):
         *   ① 位置项由"小角度线性 + 大角度平方"统一为线性 wz_pos = KP × err。
         *      平方律斜率随误差变,2026-11-20 实测的 3.16Hz 自激正好落在斜率最大(≈11)的工作点;
         *      线性化后穿越频率与工作点无关,KP 才能按稳定性单独定。
         *   ② 限幅改"额度分配"而非"先加后削":前馈先占额度,但给位置项强制保留
         *      CHASSIS_FOLLOW_P_RESERVE。88 版三项加完才接到 300,云台快转时前馈单项顶穿,
         *      与位置项互顶 → 实测 wz 撞 −300、off −7.73~+2.81°。
         *   ③ 删除 offset 差分阻尼(3Hz 低通 + ecd 差分):一阶环里它只拖慢响应,不提供稳定裕度。
         *      需要阻尼时改用实测相对速率,由 CHASSIS_FOLLOW_KD 打开(默认 0 = 关)。
         *   ④ 前馈系数 0.9 → 1.0(零滞后跟踪的理论值),靠 ② 的额度分配才安全。
         *
         * 目标相对角 CHASSIS_FOLLOW_TARGET_OFFSET 默认 0;设非 0 即"侧身跟随/扭腰",
         * 对应官方的 chassis_relative_angle_set 与上交的 twistGapAngle。 */
        const float lim = (CHASSIS_FOLLOW_WZ_LIMIT > 0.0f) ? CHASSIS_FOLLOW_WZ_LIMIT : 1.0e6f;
        const float ff_cap = lim * (1.0f - CHASSIS_FOLLOW_P_RESERVE);

        /* --- 误差:先减可配目标角,再折回 ±180 --- */
        float err = chassis_cmd_recv.offset_angle - CHASSIS_FOLLOW_TARGET_OFFSET;
        if (err > 180.0f)
            err -= 360.0f;
        else if (err < -180.0f)
            err += 360.0f;

        /* --- 软死区:扣掉死区宽度,边界处输出连续 --- */
        if (fabsf(err) < CHASSIS_FOLLOW_DEADBAND)
            err = 0.0f;
        else
            err -= (err > 0.0f) ? CHASSIS_FOLLOW_DEADBAND : -CHASSIS_FOLLOW_DEADBAND;

        /* --- 前馈:云台绕世界 yaw 的真实角速度,无微分延迟 --- */
        float gz_dps = (chassis_IMU != 0) ? chassis_IMU->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG : 0.0f;

        /* 前馈取限速后的值(一周期前算出):起步按 FF_SLEW 爬,到顶后精确等于 FF×gz。 */
        float ff = ff_slew;

        /* --- 位置项:线性律 --- */
        float wz_pos = CHASSIS_FOLLOW_KP * err;

        /* --- 加速度前馈:云台角加速度 × 系数,补底盘惯量(伺服三环的 AFF) ---
         * 匀速时 α=0 → 本项恒零,不进稳态;只有打杆/松杆这种加速度突变段才出力。
         * gz_rate_f 在下面的斜坡段里算(那里有 dt),所以用的是一周期前的值(5ms)。 */
        float wz_aff = 0.0f;
        if (CHASSIS_FOLLOW_AFF > 0.0f)
        {
            /* 双门控:①确实在转(否则静止噪声被微分放大成持续抖动);
             *         ②α 超过噪声量级(否则小扰动也让 AFF 出力)。 */
            float spd_now = (fabsf(gz_dps) > fabsf(est_wz)) ? fabsf(gz_dps) : fabsf(est_wz);
            /* 默认只在减速段(α<0)出力:加速段 FF+P 已在把底盘推到极限,
             * 再叠 AFF 会冲过云台(2026-11-21 实车体感确认)。 */
#if CHASSIS_FOLLOW_AFF_ACCEL_ON
            int aff_ok = 1;
#else
            /* 刹车判据:**加速度与速度反向**(|ω| 在减小),与转向正负无关。
             * 原来写成 gz_rate_f < 0 是错的:正向转向时它等价于减速,负向转向时完全反了 ——
             * 负向加速段(gz −132→−240,α<0)被误判成刹车、给出同向加力,底盘冲过云台;
             * 负向减速段(gz −245→−23,α>0)被误判成加速、AFF 罢工,off 一路冲到 31.7°(2026-11-21
             * 实测)。 */
            int aff_ok = (gz_rate_f * gz_dps) < 0.0f;
#endif
            if (aff_ok && spd_now > CHASSIS_FOLLOW_AFF_GATE &&
                fabsf(gz_rate_f) > CHASSIS_FOLLOW_AFF_RATE_MIN)
            {
                wz_aff = CHASSIS_FOLLOW_AFF * gz_rate_f;
                MOTOR_LIMIT_MIN_MAX(wz_aff, -CHASSIS_FOLLOW_AFF_LIMIT, CHASSIS_FOLLOW_AFF_LIMIT);
            }
        }

        /* --- 额度分配:只在"总和会越限"时才让前馈让位 ---
         * 平时(未饱和)直接相加,不引入任何稳态误差;
         * 一旦 ff+pos 超过 lim,说明底盘转不到那么快 —— 此时若按"先加后削"直接截断,
         * 位置项会被前馈整段挤掉,云台减速后靠积累的 offset 一次性甩回来(88 版的互顶)。
         * 故饱和时先把前馈压到 lim×(1−P_RESERVE),给位置项留出强制额度。
         * 代价:云台转速落在 (ff_cap, lim] 区间时,稳态会多出 (lim−ff_cap)/KP ≈ 9° 的落后 ——
         * 这是"保住修正力"换来的,只在满杆急转时出现。 */
        float wz_cmd = ff + wz_pos + wz_aff;
        if (fabsf(wz_cmd) > lim)
        {
            MOTOR_LIMIT_MIN_MAX(ff, -ff_cap, ff_cap);
            MOTOR_LIMIT_MIN_MAX(wz_pos, -(lim - fabsf(ff)), lim - fabsf(ff));
            wz_cmd = ff + wz_pos + wz_aff;
        }

        /* --- 可选阻尼:实测相对角速度 (云台 gz − 底盘逆解 est_wz),默认关闭 ---
         * 不用 offset 差分(要 3Hz 低通,滞后十几 ms)。est_wz 在 chassis_estimate_speed() 里更新,
         * 位于本 switch 之后 → 用的是一周期前(5ms)的值,对阻尼项无害。 */
        /* --- 提前刹车:操作手松手了,但云台还在转、底盘还在跑 ---
         * yaw_rate_cmd 来自 cmd 层(目标角微分),松杆瞬间就归零;
         * 而 gz 要等云台自己减速完才降,晚约 0.1 s。用前者可以早刹。 */
        if (CHASSIS_FOLLOW_BRAKE_ANT > 0.0f)
        {
            float cmd_rate = chassis_cmd_recv.yaw_rate_cmd;
            /* 判据只留"操作手松手"+"底盘还在跑",**不再要求云台还在转**。
             * 原来带 |gz|>30,云台一停刹车就撒手 —— 而那时底盘还在滑,正是最需要刹的时刻
             * (和 AFF 绑在云台 α 上同一个毛病)。刹车必须只跟底盘自己的速度走。 */
            if (fabsf(cmd_rate) < CHASSIS_FOLLOW_BRAKE_ON_RATE && fabsf(est_wz) > 40.0f)
                wz_cmd -= CHASSIS_FOLLOW_BRAKE_ANT * est_wz;
        }

        if (CHASSIS_FOLLOW_KD > 0.0f && chassis_IMU != 0)
        {
            /* **只压不推**(2026-11-21):只在"底盘已经比云台快"(gap<0)时出力,
             * 底盘落后时完全不出力 —— 避免像 KD 双向那样在起步段帮着加速。
             * 作用:追上云台后不会"突然加速冲过去"。 */
            float gap = gz_dps - est_wz;
            if (gap * gz_dps < 0.0f)
                wz_cmd += CHASSIS_FOLLOW_KD * gap;
        }

        /* --- 起步硬约束:追平之后不许超过云台(2026-11-21) ---
         * 条件三条同时满足才钳:①已追平 |est|>=|gz| ②云台在加速或匀速(α·gz>=0)
         * ③指令与云台同向且幅值超过 |gz|。
         * 减速段不钳,否则 gz 掉到几十而 est 仍高时会把刹车一起砍掉。 */
#if CHASSIS_FOLLOW_NO_OVERSHOOT
        if (gz_dps * gz_rate_f >= 0.0f && fabsf(est_wz) >= fabsf(gz_dps) &&
            gz_dps * wz_cmd > 0.0f && fabsf(wz_cmd) > fabsf(gz_dps))
        {
            wz_cmd = gz_dps;
        }
#endif

        chassis_cmd_recv.wz = wz_cmd;

        /* 总限幅兜底:两项都已按 lim 分配,这里只防浮点累积误差 */
        MOTOR_LIMIT_MIN_MAX(chassis_cmd_recv.wz, -lim, lim);
        break;
    }
    case CHASSIS_ROTATE: // 小陀螺:运行期档位选恒速/变速(键鼠 C,DEV-20261125-08)
        /* 档位覆盖(32 位变量,优先于 C):1=强制匀速,2=强制变速,0=跟随 cmd。
         * 手柄/台架没有 C 键时写 chassis_spin_mode_override;编译关掉变速时恒为匀速。 */
        chassis_spin_vary_active = 0u;
#if CHASSIS_SPIN_VARY_EN
        chassis_spin_vary_active = chassis_cmd_recv.spin_vary ? 1u : 0u;
        if (chassis_spin_mode_override == 1u)
            chassis_spin_vary_active = 0u;
        else if (chassis_spin_mode_override == 2u)
            chassis_spin_vary_active = 1u;
#endif
#if CHASSIS_SPIN_VARY_EN
        if (chassis_spin_vary_active)
        {
            if (!spin_active)
            {
                /* 种子混入进入时刻:固定种子会让轨迹被对手录一次就复现。
                 * 初值取实测转速:匀速→变速切换时从当前转速接着走,不先掉到 0 再爬升。 */
                chassis_spin_reset((uint32_t)(cmd_now_ms * 1000.0f) ^ 0x9E3779B9u, est_wz);
                spin_active = 1;
                spin_last_ms = cmd_now_ms;
            }
            float spin_dt_ms = cmd_now_ms - spin_last_ms;
            spin_last_ms = cmd_now_ms;
            if (!(spin_dt_ms > 0.0f) || spin_dt_ms > 50.0f)
                spin_dt_ms = 5.0f;

            if (chassis_spin_flow == SPIN_FLOW_RAMP)
            {
                /* 向最近端点爬升;到端点后交给 HOLD,先稳一下再重启正弦 */
                chassis_spin_handoff_wz =
                    chassis_ramp_to(chassis_spin_handoff_wz, chassis_spin_handoff_target,
                                    CHASSIS_SPIN_ALPHA_MAX, spin_dt_ms * 0.001f);
                if (fabsf(chassis_spin_handoff_wz - chassis_spin_handoff_target) < 1.0f)
                {
                    chassis_spin_handoff_wz = chassis_spin_handoff_target;
                    chassis_spin_flow = SPIN_FLOW_HOLD;
                    float hold = chassis_spin_handoff_hold_ms;
                    if (!(hold > 0.0f))
                        hold = 0.0f;
                    chassis_spin_handoff_ms = hold;
                }
                chassis_cmd_recv.wz = chassis_spin_handoff_wz;
            }
            else if (chassis_spin_flow == SPIN_FLOW_HOLD)
            {
                chassis_cmd_recv.wz = chassis_spin_handoff_target;
                chassis_spin_handoff_ms -= spin_dt_ms;
                if (chassis_spin_handoff_ms <= 0.0f)
                {
                    chassis_spin_handoff_ms = 0.0f;
                    /* 交接结束时中心/调制必须已经是纯自转值,否则重启后的第一拍
                     * 目标会从端点被 LP 中的旧中心拉低(端点 520 变 507 之类)。
                     * 直接置纯值;后续 LP 目标仍是纯值,不会再动。 */
                    chassis_spin_center = CHASSIS_SPIN_CRUISE_DPS;
                    chassis_spin_mod_limit = CHASSIS_SPIN_MOD_LIMIT;
                    chassis_spin_handoff((uint32_t)(cmd_now_ms * 1000.0f) ^ 0x9E3779B9u,
                                         chassis_spin_handoff_target,
                                         chassis_spin_handoff_target > CHASSIS_SPIN_CRUISE_DPS);
                    chassis_spin_flow = SPIN_FLOW_PURE;
                    /* 交接结束后总 scale 再留一段尾巴,避免电流恢复造成二次尖峰 */
                    float tail = chassis_power_trans_tail_ms;
                    if (!(tail > 0.0f))
                        tail = 0.0f;
                    if (tail > 1000.0f)
                        tail = 1000.0f;
                    chassis_power_transient_ms = tail;
                }
            }
            else
            {
                chassis_cmd_recv.wz = chassis_spin_update(spin_dt_ms, chassis_spin_center,
                                                          chassis_spin_mod_limit);
            }
            /* 松杆沿选端点用"正弦输出"而不是实测:实测在打滑/刹车削减时偏低 */
            chassis_spin_last_wz = chassis_cmd_recv.wz;
        }
        else
        {
            /* 匀速档:复位进入沿,下次切回变速时从当时的实测转速重新起步 */
            spin_active = 0;
            float wz = chassis_spin_trans_active ? chassis_rotate_speed_trans_deg_s
                                                 : chassis_rotate_speed_deg_s;
            MOTOR_LIMIT_MIN_MAX(wz, 0.0f, chassis_spin_wz_cap);
            chassis_cmd_recv.wz = wz;
        }
#else
        {
            float wz = chassis_spin_trans_active ? chassis_rotate_speed_trans_deg_s
                                                 : chassis_rotate_speed_deg_s;
            MOTOR_LIMIT_MIN_MAX(wz, 0.0f, chassis_spin_wz_cap);
            chassis_cmd_recv.wz = wz;
        }
#endif
        /* 刹车优先:平移在减速时自转先让出摩擦圆,前后刹车力不再被自转分走
         * (DEV-20261126-25)。系数 25ms 低通,刹车结束自动恢复。 */
        chassis_cmd_recv.wz *= chassis_spin_brake_factor();
        /* 小陀螺功率优先让给平移:总功率超限时只压自转,平移指令不动
         * (DEV-20261126-24)。缩放值由功率块每拍更新。 */
        chassis_cmd_recv.wz *= chassis_spin_power_scale;
        break;
    default: // CHASSIS_ZERO_FORCE:零力由电机层与看门狗处理,本处不改 wz
        break;
    }

#ifdef ONE_BOARD
    /* 命令断流保护:CMD任务卡死/断流时底盘不得按最后一帧速度行驶 */
    if (cmd_watchdog_expired(&chassis_cmd_wd, cmd_now_ms, CMD_WATCHDOG_TIMEOUT_MS))
    {
        chassis_cmd_recv.chassis_mode = CHASSIS_ZERO_FORCE;
        chassis_cmd_recv.vx = 0.0f;
        chassis_cmd_recv.vy = 0.0f;
        chassis_cmd_recv.wz = 0.0f;
    }

    /* 小陀螺平移方向偏置(DEV-20261103-03):自转存在固定方向的漂移(实测正转向右微漂),
     * 平移指令预先旋转固定补偿角使轨迹回正;仅ROTATE模式生效,斜坡之前施加(斜坡作用于补偿后指令) */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE && chassis_translate_bias_mode == 0 &&
        chassis_translate_bias_deg != 0.0f)
    {
        /* 打滑漂移近似与自转速度成正比。默认按瞬时 wz 比例调度(DEV-20261124-12);
         * 若变速正弦/相位游走让偏置摆动、实际打滑跟不上,把 chassis_bias_mean_mode 写 1,
         * 改用档位平均转速(变速 310 / 匀速 520)算固定偏置(DEV-20261126-19)。
         * 偏置角本身可由 chassis_translate_bias_deg 在线改写,用于实车找符号/幅度。 */
        float bias_ref = chassis_cmd_recv.wz;
        if (chassis_bias_mean_mode)
            /* 用当前实际档位速度,不用固定宏:变速中心会随平移轮速余量下移
             * (DEV-20261126-25),固定 310 会把偏置补过头。 */
            bias_ref = (chassis_spin_vary_active
                            ? chassis_spin_center
                            : fminf(chassis_spin_trans_active ? chassis_rotate_speed_trans_deg_s
                                                  : chassis_rotate_speed_deg_s,
                                          chassis_spin_wz_cap)) *
                       chassis_spin_brake * chassis_spin_power_scale;
        float bias_deg = chassis_translate_bias_deg * (bias_ref / CHASSIS_ROTATE_SPEED);
        MOTOR_LIMIT_MIN_MAX(bias_deg, -30.0f, 30.0f);
        const float bias_rad = bias_deg * FW_DEG_TO_RAD;
        const float cb = arm_cos_f32(bias_rad);
        const float sb = arm_sin_f32(bias_rad);
        const float vx_bias = chassis_cmd_recv.vx * cb - chassis_cmd_recv.vy * sb;
        const float vy_bias = chassis_cmd_recv.vx * sb + chassis_cmd_recv.vy * cb;
        chassis_cmd_recv.vx = vx_bias;
        chassis_cmd_recv.vy = vy_bias;
    }

    /* 平移斜坡。(2026-11-20 跟随律重写时移除了 offset 变化率低通:
     * 跟随环不再用 offset 差分做阻尼,这段 3Hz 低通随之删除。)
     * 单位注意:DWT毫秒 vs 加速度/s^2,必须先换算成秒(否则斜坡永冻)。
     * dt异常(>100ms)时本周期保持原值不推进。 */
    {
        float dt_ms = cmd_now_ms - chassis_last_ms;
        chassis_last_ms = cmd_now_ms;
        if (dt_ms > 0.0f && dt_ms <= 100.0f)
        {
            float dt_sec = dt_ms * 0.001f;
            /* 云台角加速度(供跟随环 AFF):gz 差分 + 一阶低通。
             * 不在跟随态也照常累计,否则切入跟随那一帧会拿陈旧值算出一个假尖峰。 */
            {
                float gz_dps = (chassis_IMU != 0) ? chassis_IMU->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG : 0.0f;
                float gz_rate = (gz_dps - gz_last_dps) / dt_sec;
                gz_rate_f += CHASSIS_FOLLOW_AFF_LP * (gz_rate - gz_rate_f);
                gz_last_dps = gz_dps;

                /* 前馈爬升限速:只压"变大",下落立即跟随。 */
                {
                    float ff_tgt = CHASSIS_FOLLOW_FF * gz_dps;
                    if (CHASSIS_FOLLOW_FF_SLEW > 0.0f && ff_tgt * ff_slew > 0.0f &&
                        fabsf(ff_tgt) > fabsf(ff_slew))
                    {
                        float step = CHASSIS_FOLLOW_FF_SLEW * dt_sec;
                        ff_slew += (ff_tgt > 0.0f) ? step : -step;
                        if (fabsf(ff_slew) > fabsf(ff_tgt))
                            ff_slew = ff_tgt;
                    }
                    else
                    {
                        ff_slew = ff_tgt;
                    }
                }
            }
            /* 加速/减速分段(DEV-20261127):减速(松杆收油或反向扳杆)用独立更大的
             * 减速度/jerk 缩短停车距离;加速段保持原参数,避免起步变猛/打滑。
             * 判据:cur*target<0 覆盖反向;|target|<|cur| 覆盖松杆。
             * 起步时 cur=0,两式都不成立,仍走加速段参数。 */
            const uint8_t vx_braking =
                (chassis_vx_ramp * chassis_cmd_recv.vx < 0.0f) ||
                (fabsf(chassis_cmd_recv.vx) < fabsf(chassis_vx_ramp));
            const uint8_t vy_braking =
                (chassis_vy_ramp * chassis_cmd_recv.vy < 0.0f) ||
                (fabsf(chassis_cmd_recv.vy) < fabsf(chassis_vy_ramp));
            float vx_accel = (chassis_vx_ramp_accel > 0.0f) ? chassis_vx_ramp_accel : CHASSIS_VX_RAMP;
            float vy_accel = (chassis_vy_ramp_accel > 0.0f) ? chassis_vy_ramp_accel : CHASSIS_VY_RAMP;
            float vx_jerk = (chassis_vx_jerk > 0.0f) ? chassis_vx_jerk : CHASSIS_VX_JERK;
            float vy_jerk = (chassis_vy_jerk > 0.0f) ? chassis_vy_jerk : CHASSIS_VY_JERK;
            if (vx_braking)
            {
                if (chassis_vx_brake_ramp > 0.0f)
                    vx_accel = chassis_vx_brake_ramp;
                if (chassis_vx_brake_jerk > 0.0f)
                    vx_jerk = chassis_vx_brake_jerk;
            }
            if (vy_braking)
            {
                if (chassis_vy_brake_ramp > 0.0f)
                    vy_accel = chassis_vy_brake_ramp;
                if (chassis_vy_brake_jerk > 0.0f)
                    vy_jerk = chassis_vy_brake_jerk;
            }
            chassis_vx_ramp = chassis_ramp_to_jerk(chassis_vx_ramp, chassis_cmd_recv.vx, vx_accel,
                                                   vx_jerk, dt_sec, &chassis_vx_accel_state);
            chassis_vy_ramp = chassis_ramp_to_jerk(chassis_vy_ramp, chassis_cmd_recv.vy, vy_accel,
                                                   vy_jerk, dt_sec, &chassis_vy_accel_state);
            /* 注:wz 刻意不加斜坡。2026-11-16 试过"只对修正量限加速、前馈直通"(1200°/s²),
             * 实测反摆均值 +22.2°/s(无斜坡 +11.8),斜坡拖慢刹车、底盘多转、误差更大 → 否决。 */
        }
    }
#endif

    // 零力模式停止电机,其余使能
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE)
    {
        dji_motor_stop(motor_lf);
        dji_motor_stop(motor_rf);
        dji_motor_stop(motor_lb);
        dji_motor_stop(motor_rb);
    }
    else
    {
        dji_motor_enable(motor_lf);
        dji_motor_enable(motor_rf);
        dji_motor_enable(motor_lb);
        dji_motor_enable(motor_rb);
    }

    // 云台系速度按offset角旋转投影到底盘系(底盘逆时针旋转为正)
    static float sin_theta, cos_theta;
    cos_theta = arm_cos_f32(chassis_cmd_recv.offset_angle * FW_DEG_TO_RAD);
    sin_theta = arm_sin_f32(chassis_cmd_recv.offset_angle * FW_DEG_TO_RAD);
    chassis_vx = chassis_vx_ramp * cos_theta + chassis_vy_ramp * sin_theta;
    chassis_vy = -chassis_vx_ramp * sin_theta + chassis_vy_ramp * cos_theta;

    /* 横向漂移前馈(模式1):漂移是底盘系横向速度、≈k*wz,与前进/后退方向无关。
     * 旧的旋转补偿在后退时会反向,所以前进/后退漂移角不同(DEV-20261126-21)。
     * k 由 chassis_translate_bias_mm_s_per_dps 在线标定,符号/大小都试。 */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE && chassis_translate_bias_mode == 1)
    {
        float bias_ref = chassis_cmd_recv.wz;
        if (chassis_bias_mean_mode)
            bias_ref = (chassis_spin_vary_active
                            ? chassis_spin_center
                            : fminf(chassis_spin_trans_active ? chassis_rotate_speed_trans_deg_s
                                                  : chassis_rotate_speed_deg_s,
                                          chassis_spin_wz_cap)) *
                       chassis_spin_brake * chassis_spin_power_scale;
        chassis_vy += chassis_translate_bias_mm_s_per_dps * bias_ref;
    }

    /* 用本拍最终平移量(投影+偏置后)重算下一拍自转轮速余量:
     * 平移占掉的轮速从自转预算里扣,轨迹中心下移而不是被 6000rpm 限幅削顶。 */
    chassis_spin_headroom_update(cmd_now_ms);

    chassis_mecanum_calculate();

    /* 功率限幅:2026 协议 V1.2.0 已删 0x0202 的 chassis_power(前 8 字节保留),
     * 改为电机模型预测。上限仍取 0x0201 的 chassis_power_limit(该字段有效,实测 120W)。
     * 裁判未接(referee_data=NULL)时回退 config 兜底值。
     * DEV-20261126-12:预测用未限幅的电流指令(current_cmd),不用低通后的反馈电流;
     * 衰减系数直接乘电机输出电流,二次方程精确解(304 口径)。 */
    float power_scale = 1.0f;
    float power_p_pred = 0.0f;
    float power_p_post = 0.0f;
    float power_limit_w = 0.0f;
#if CHASSIS_POWER_CTRL_EN
    {
        const float i_cmd[4] = {
            motor_lf->current_cmd,
            motor_rf->current_cmd,
            motor_lb->current_cmd,
            motor_rb->current_cmd,
        };
        const float w_dps[4] = {
            motor_lf->measure.speed_aps,
            motor_rf->measure.speed_aps,
            motor_lb->measure.speed_aps,
            motor_rb->measure.speed_aps,
        };
        float limit_w = CHASSIS_POWER_LIMIT_DEFAULT;
        if (referee_data != 0 && (float)referee_data->GameRobotState.chassis_power_limit > 1.0f)
        {
            limit_w = (float)referee_data->GameRobotState.chassis_power_limit;
            /* 裁判按机器人等级下发(本车实测 120W),本队规则上限 90W:
             * 取较小值,避免等级配置偏高把允许功率放宽 30W。 */
            if (limit_w > CHASSIS_POWER_LIMIT_MAX)
                limit_w = CHASSIS_POWER_LIMIT_MAX;
        }
        /* 控制余量:DEV-20261126-26 按用户要求 10→5W,控制目标 85W。
         * 注意 DEV-20261126-16 实测超调约 10W,模型未标定时实际可能到 95W;
         * 比赛前未重拟合建议回 10W。留 20W 下限,避免配置/裁判异常时把底盘限到几乎没力。 */
        limit_w -= CHASSIS_POWER_MARGIN_W;
        if (limit_w < 20.0f)
            limit_w = 20.0f;
        /* 缓冲能量衰减(参考 304 的超电 SOC 衰减):缓冲越低,上限越低,
         * 优先让缓冲回充。**必须确认收到过 0x0202**:裁判离线时 be 恒 0,
         * 若只判 referee_data 非空,会把上限误砍一半(DEV-20261126-15)。
         * **be=0 也不衰减**:未进比赛/缓冲字段无效时裁判同样给 0,会把 80W 再砍成
         * 40W,导致小陀螺+平移被压到几乎没力(DEV-20261126-20)。真·缓冲耗尽
         * 由裁判自身 5s 断电保护兜底。 */
        if (referee_data != 0 && referee_data->PowerHeatTick > 0u)
        {
            float be = (float)referee_data->PowerHeatData.buffer_energy;
            if (be > 0.0f)
            {
                if (be > CHASSIS_POWER_BUFFER_MAX_J)
                    be = CHASSIS_POWER_BUFFER_MAX_J;
                const float atten = CHASSIS_POWER_BUFFER_MIN_SCALE +
                                    (1.0f - CHASSIS_POWER_BUFFER_MIN_SCALE) *
                                        (be / CHASSIS_POWER_BUFFER_MAX_J);
                limit_w *= atten;
            }
        }
        power_limit_w = limit_w;
        power_scale = chassis_power_scale(i_cmd, w_dps, limit_w, chassis_power_min_scale,
                                          &power_p_pred, &power_p_post);

        /* DEV-20261126-33:总 power scale 作用于电动轮电流,不再有 ROTATE
         * 稳态"平移免疫 + ss 只压自转"的分叉。floor 0.20 在 power 模块内应用;
         * DEV-20261127-200745:制动轮(i 与 ω 反号)退出预算,output_scale 保持 1.0,
         * 由 chassis_limit_output 逐轮判定。pt 只用于积分压限/入口保持计时。 */
        chassis_spin_power_scale = 1.0f;
        chassis_power_transient_active =
            (uint8_t)((chassis_spin_flow == SPIN_FLOW_RAMP ||
                       chassis_spin_flow == SPIN_FLOW_HOLD) ||
                      (chassis_power_transient_ms > 0.0f));
    }
#endif
    /* 瞬态窗口(切入/松杆交接/尾巴)压速度环积分:P 项保留,I 项降到 25%,
     * 削掉瞬态里由平移段积累的那部分电流(DEV-20261126-28/31)。 */
    float integral_scale = 1.0f;
    if (chassis_power_transient_active)
    {
        integral_scale = chassis_spin_handoff_integral_scale;
        if (!(integral_scale >= 0.0f))
            integral_scale = 1.0f;
        if (integral_scale > 1.0f)
            integral_scale = 1.0f;
    }
    chassis_limit_output(power_scale, integral_scale);

    chassis_estimate_speed();

    /* 底盘反馈遥测(单写者=本任务):ref=vx指令,fb=逆解vx,out=逆解vy,extra=逆解wz */
    telemetry_push(&telemetry_chassis_fb, chassis_cmd_recv.vx, est_vx, est_vy, est_wz,
                  (uint8_t)chassis_cmd_recv.chassis_mode);
#if CHASSIS_VOFA_UART1_ENABLE
    /* VOFA 波形:放在遥测之后、标定日志之前,本拍控制量均已算完,发的是最新值 */
    chassis_send_vofa();
#endif
#if CHASSIS_CALIB_LOG
    /* 标定日志(P1 速度估计验证用;P2 再加 scale 列):
     * gz = IMU 世界系 yaw 角速度。**注意它不能当底盘 wz 用**——小陀螺瞄准保持时云台世界系静止,
     * 装在云台上的 IMU 读到的就是 0;只有"不跟随 + 不自转"时 gz 才等于底盘转速。
     * 自转工况的真实转速靠**计时法**(转 10 圈取平均)独立测量,见 .Doc/robots/infantry/design/底盘优化计划.md V3。
     * p = 底盘功率(0x0202, W),be = 缓冲能量(J):功率控制标定与"到底会不会掉缓冲"的判据。 */
    uint16_t calib_div = chassis_calib_log_div;
    if (calib_div < 1u)
        calib_div = 1u;
    if (calib_div > 1000u)
        calib_div = 1000u;
    if (++chassis_calib_cnt >= calib_div) // 默认 200Hz/100 = 2Hz
    {
        char c_vx[16], c_vy[16], c_wz[16], c_gz[16], c_be[16], c_p[16], c_off[16];
        chassis_calib_cnt = 0;
        log_float_to_str(c_vx, est_vx);
        log_float_to_str(c_vy, est_vy);
        log_float_to_str(c_wz, est_wz);
        log_float_to_str(c_gz, chassis_IMU ? chassis_IMU->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG : 0.0f);
        /* off = 云台-底盘夹角(deg)。云台回正时应≈0;不为 0 会让"前进"斜着走,
         * 是"不开小陀螺也左偏"的首要排查项(DEV-20261126-19)。 */
        log_float_to_str(c_off, chassis_cmd_recv.offset_angle);
#if YAW_AUTO_VOFA_USART6_ENABLE
        log_float_to_str(c_be, -1.0f); // USART6 被自瞄 yaw VOFA 占用时没有裁判数据
        log_float_to_str(c_p, -1.0f);
#else
        log_float_to_str(c_be,
                         (referee_data != 0 && referee_data->PowerHeatTick > 0u)
                             ? (float)referee_data->PowerHeatData.buffer_energy
                             : -1.0f);
        log_float_to_str(c_p, referee_data ? referee_data->PowerHeatData.chassis_power : -1.0f);
#endif
        LOG_INFO("[chcal] vx=%s vy=%s wz=%s gz=%s off=%s p=%s be=%s", c_vx, c_vy, c_wz, c_gz, c_off,
                 c_p, c_be);

        /* 功率模型标定行(DEV-20261126-12):i=未限幅电流指令(原始码),
         * w=转子转速(deg/s),p=衰减系数为 1 时的模型预测功率,s=本拍衰减系数,
         * lim=有效上限,be=缓冲能量。用 tools/fit_power_model.py 拟合 K0..K5。 */
        {
            char c_i0[16], c_i1[16], c_i2[16], c_i3[16];
            char c_w0[16], c_w1[16], c_w2[16], c_w3[16];
            char c_pp[16], c_po[16], c_ss[16], c_ll[16], c_sps[16], c_ct[16], c_bg[16], c_md[16],
                c_hd[16];
            log_float_to_str(c_i0, motor_lf->current_cmd);
            log_float_to_str(c_i1, motor_rf->current_cmd);
            log_float_to_str(c_i2, motor_lb->current_cmd);
            log_float_to_str(c_i3, motor_rb->current_cmd);
            log_float_to_str(c_w0, motor_lf->measure.speed_aps);
            log_float_to_str(c_w1, motor_rf->measure.speed_aps);
            log_float_to_str(c_w2, motor_lb->measure.speed_aps);
            log_float_to_str(c_w3, motor_rb->measure.speed_aps);
            log_float_to_str(c_pp, power_p_pred);
            log_float_to_str(c_po, power_p_post);
            log_float_to_str(c_ss, power_scale);
            log_float_to_str(c_ll, power_limit_w);
            log_float_to_str(c_sps, chassis_spin_power_scale);
            log_float_to_str(c_ct, chassis_spin_center);
            log_float_to_str(c_bg, chassis_spin_brake);
            log_float_to_str(c_md, chassis_spin_mod_limit);
            log_float_to_str(c_hd, chassis_spin_handoff_ms);
            LOG_INFO("[pwri] i=%s,%s,%s,%s w=%s,%s,%s,%s", c_i0, c_i1, c_i2, c_i3, c_w0, c_w1,
                     c_w2, c_w3);
            LOG_INFO(
                "[pwr] p=%s s=%s po=%s lim=%s be=%s ss=%s ct=%s bg=%s md=%s hd=%s sv=%d hf=%d pt=%d",
                c_pp, c_ss, c_po, c_ll, c_be, c_sps, c_ct, c_bg, c_md, c_hd,
                (int)chassis_spin_vary_active, (int)chassis_spin_flow,
                (int)chassis_power_transient_active);
        }
    }
#endif

#ifdef ONE_BOARD
    msg_publish(chassis_pub, (void *)&chassis_fb);
#endif
#ifdef CHASSIS_BOARD
    can_comm_send(chassis_can_comm, (void *)&chassis_fb);
#endif
}
