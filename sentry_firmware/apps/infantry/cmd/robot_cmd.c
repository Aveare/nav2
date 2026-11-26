/**
 * @file    robot_cmd.c
 * @brief   cmd 应用:输入汇聚、仲裁调用与三话题发布
 * @layer   APP
 * @author  小猫大王
* @note    职责:读遥控器 → 按固定优先级裁决控制权(失联 > 急停挡 > 拨轮急停/锁存 > 运行)
*          → 生成三路控制命令 → 发布给 chassis / gimbal / shoot。
* @note    视觉/自主输入的仲裁扩展位:接入后在其运行分支内按"自主 > 视觉 > 遥控"次序仲裁,
*          并维护输入有效期。
* @note    2026-10-29 建立,作者 JIT CR。
* @note    2026-11-20 拆分(零行为变化):本文件保留 状态定义/Init/Task 编排/偏移角/标定触发
*          与对外读数;输入映射拆到 cmd_remote.c(遥控)、cmd_kbm.c(键鼠)、
*          cmd_autoaim.c(AX650 yaw 滤波),仲裁拆到 cmd_authority.c。
*          跨文件共享状态集中在本文件定义,声明见 robot_cmd_internal.h。
 */
// app
#include "robot_def.h"

robot_status_e robot_state; // 机器人整体工作状态
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "ax650_uart1.h"
#include "msg.h"
#include "general_def.h"
#include "dji_motor.h"
#include "gimbal.h"    // ECD 回正:pitch 编码器读取与回正目标
#include "telemetry.h" // 遥测:chcal 日志用
/* 战术档位的 SHOOT_RATE_ECO/BURST 由 robot_def.h → config.h → shoot.h
 * 传递进来;此处不再直接 include "shoot.h"(它位于 config/infantry_2026/,
 * 主机回归的包含路径里没有该子目录,直连会破坏可测试性)。 */
// bsp
#include "dwt_port.h"
#include "log_port.h"

// 编码器值自动转换成角度值
#define YAW_ALIGN_ANGLE (YAW_CHASSIS_ALIGN_ECD * DJI_ECD_TO_DEG) // 对齐时的角度,0-360

/* cmd应用的模块实例与交互信息 */
#ifdef GIMBAL_BOARD // 双板兼容
#include "can_comm.h"
static can_comm_t *cmd_can_comm;
#endif
#ifdef ONE_BOARD
static msg_pub_t chassis_cmd_pub;   // 底盘控制消息发布者
static msg_sub_t chassis_feed_sub; // 底盘反馈订阅者
#endif                                 // ONE_BOARD

chassis_ctrl_cmd_t chassis_cmd;             // 发给底盘的控制信息
/* 调试可写(ELF/DWARF):平移最大速度,mm/s。掉电恢复宏值;
 * 轮速仍受 CHASSIS_MAX_MOTOR_RPM 限幅,写大值不会直接越过电机上限。 */
volatile float chassis_max_speed_mm_s = CHASSIS_MAX_SPEED;
static chassis_upload_t chassis_fb_recv; // 从底盘收到的反馈

remote_rc_ctrl_t *rc_data; // 遥控器数据

static msg_pub_t gimbal_cmd_pub;     // 云台控制消息发布者
static msg_sub_t gimbal_feed_sub;   // 云台反馈订阅者
gimbal_ctrl_cmd_t gimbal_cmd;      // 传给云台的控制信息
gimbal_upload_t gimbal_fb_recv; // 从云台收到的反馈

static msg_pub_t shoot_cmd_pub;           // 发射控制消息发布者
static msg_sub_t shoot_feed_sub;         // 发射反馈订阅者
shoot_ctrl_cmd_t shoot_cmd;             // 传给发射的控制信息
static shoot_upload_t shoot_fb_recv; // 从发射收到的反馈

float ax650_raw_yaw_target; // 最近一帧有效0x0503原始yaw目标，供VOFA与滤波后目标对比
/* 解锁回中:解锁沿启动,yaw以限速渐进回相对零点;左杆接管/超时中止 */
uint8_t yaw_recentering = 0;
uint16_t recenter_cnt = 0;

/* 输入源:生产者为 cmd_update_input_source(cmd_kbm.c),消费者为 cmd_task 的分派 */
kbm_input_source_e input_source = INPUT_RC;

/* 统一自瞄状态(DEV-20261124-03):左中挡,或键鼠输入源下的 Shift 锁存。
 * 生产者 = cmd_task 每周期刷新;消费者 = cmd_remote_set / cmd_keyboard_mouse_set /
 * 回中判据 / AX650 模式。键鼠回落遥控时 Shift 锁存不参与(DEV-20261124-05)。 */
uint8_t auto_aim_active = 0;

/* 战术射速档位(DEV-20261121-11):默认常规档,键鼠 X 键循环切换。
 * 档位只决定"请求射速";能否真的打这么快由热量守卫裁决(爆发档不越热量上限)。 */
static fire_tier_e fire_tier = FIRE_TIER_NORMAL;

void cmd_fire_tier_cycle(void)
{
    fire_tier = (fire_tier_e)(((uint8_t)fire_tier + 1u) % 3u);
    const char *name = (fire_tier == FIRE_TIER_ECO)     ? "ECO"
                       : (fire_tier == FIRE_TIER_BURST) ? "BURST"
                                                        : "NORMAL";
    LOG_INFO("[CMD] fire tier -> %s", name);
}

fire_tier_e cmd_fire_tier_get(void)
{
    return fire_tier;
}

float cmd_fire_tier_rate(void)
{
    switch (fire_tier)
    {
    case FIRE_TIER_ECO:
        return SHOOT_RATE_ECO;
    case FIRE_TIER_BURST:
        return SHOOT_RATE_BURST;
    default:
        return SHOOT_RATE_DEFAULT;
    }
}

/* 小陀螺运行期档位(DEV-20261125-08):1=变速轨迹,0=恒速 CHASSIS_ROTATE_SPEED。
 * 键鼠 C 沿切换;手柄路径没有 C 键,调试器可直接写本变量(ELF/DWARF):
 * 标定时写 0 切匀速,标定完写 1 或重新上电恢复默认。
 * 编译开关 CHASSIS_SPIN_VARY_EN=0 时变速轨迹不参与编译,档位恒为恒速。 */
volatile uint8_t chassis_spin_vary = (CHASSIS_SPIN_VARY_EN != 0) ? 1u : 0u;

void cmd_spin_vary_toggle(void)
{
#if CHASSIS_SPIN_VARY_EN
    chassis_spin_vary = chassis_spin_vary ? 0u : 1u;
    LOG_INFO("[CMD] spin mode -> %s", chassis_spin_vary ? "VARY" : "CONST");
#else
    LOG_INFO("[CMD] spin mode locked CONST (CHASSIS_SPIN_VARY_EN=0)");
#endif
}

void cmd_init()
{
        rc_data = remote_control_init(&huart3); // C板DBUS在USART3(带反相器)

    gimbal_cmd_pub = msg_pub("gimbal_cmd", sizeof(gimbal_ctrl_cmd_t));
    gimbal_feed_sub = msg_sub("gimbal_feed", sizeof(gimbal_upload_t));
    shoot_cmd_pub = msg_pub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));
    shoot_feed_sub = msg_sub("shoot_feed", sizeof(shoot_upload_t));

#ifdef ONE_BOARD
    chassis_cmd_pub = msg_pub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    chassis_feed_sub = msg_sub("chassis_feed", sizeof(chassis_upload_t));
#endif // ONE_BOARD
#ifdef GIMBAL_BOARD
    can_comm_init_config_t comm_conf = {
        .can_config =
            {
                .can_handle = &hcan1,
                .tx_id = 0x312,
                .rx_id = 0x311,
            },
        .recv_data_len = sizeof(chassis_upload_t),
        .send_data_len = sizeof(chassis_ctrl_cmd_t),
    };
    cmd_can_comm = can_comm_init(&comm_conf);
#endif // GIMBAL_BOARD
    gimbal_cmd.pitch = 0;

    robot_state = ROBOT_STATUS_STOP; // 上电先急停,右上挡回中解锁
}

/**
 * @brief 由云台回传的yaw电机单圈角度计算与零位的误差,统一折回[-180,180]
 *        (跨编码器零点时不追错方向)
 */
static void cmd_calc_offset_angle()
{
    static float angle;
    angle = gimbal_fb_recv.yaw_motor_single_round_angle;
    chassis_cmd.offset_angle = angle - YAW_ALIGN_ANGLE;
    if (chassis_cmd.offset_angle > 180.0f)
        chassis_cmd.offset_angle -= 360.0f;
    else if (chassis_cmd.offset_angle < -180.0f)
        chassis_cmd.offset_angle += 360.0f;
}

/**
 * @brief AX650 0x0503 世界系目标 → gimbal 目标角(自瞄统一解算,DEV-20261124-03)
 * @note  左中挡与键鼠 Shift 自瞄共用本函数;调用方负责置 gimbal_mode 与底盘模式。
 *        无有效目标/未解锁/失联时保持上一目标(不回中、不清零),由 vision_valid 把关。
 *        与拆分前 cmd_remote_set() 的 auto_aim 分支逐字一致,仅去掉 local auto_aim 判据。
 */
void cmd_auto_aim_set(void)
{
    const uint8_t right_up = REMOTE_RC_SWITCH_IS_UP(rc_data[RC_SLOT_CURRENT].rc.switch_right);
    ax650_uart1_state_t vision;
    ax650_uart1_get_state(&vision);
    const uint8_t vision_valid = robot_state == ROBOT_STATUS_READY && remote_control_is_online() &&
                                 !right_up && vision.command_valid && vision.command.target_id != 0u &&
                                 isfinite(vision.command.yaw) && isfinite(vision.command.pitch);
    if (vision_valid)
    {
        ax650_raw_yaw_target = vision.command.yaw;
        const float now_ms = dwt_get_timeline_ms();
#if YAW_AUTO_KF_ENABLE
        float filtered_yaw = cmd_yaw_auto_kalman_update(vision.command.yaw, vision.command.yaw_speed,
                                  vision.command.target_id, vision.command_count, now_ms);
#else
        float filtered_yaw = vision.command.yaw;
#endif
#if YAW_AUTO_SMOOTH_ENABLE
        gimbal_cmd.yaw = cmd_yaw_auto_smooth_update(
            filtered_yaw, gimbal_fb_recv.gimbal_imu_data.YawTotalAngle, now_ms);
#else
        gimbal_cmd.yaw = filtered_yaw;
#endif
        gimbal_cmd.pitch = vision.command.pitch;
        /* 键鼠自瞄路径没有 cmd_remote_set 末尾那道统一限幅,AX650 的 pitch 必须在这里
         * 过机械限位;越界会把枪管直接打到死点(DEV-20261124-05)。 */
        MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, PITCH_MIN_ANGLE, PITCH_MAX_ANGLE);
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

float cmd_get_ax650_raw_yaw(void)
{
    return ax650_raw_yaw_target;
}

uint8_t cmd_get_fire_tier(void)
{
    return (uint8_t)fire_tier; // 0=ECO 1=NORMAL 2=BURST
}

uint8_t cmd_get_auto_aim_active(void)
{
    return auto_aim_active;
}

uint8_t cmd_get_spin_mode(void)
{
    if (chassis_cmd.chassis_mode != CHASSIS_ROTATE)
        return 0;
    /* 匀速/变速为运行期档位(DEV-20261125-08),键鼠 C 沿切换;手柄标定可写变量 */
    return chassis_spin_vary ? 2u : 1u;
}

uint8_t cmd_get_friction_on(void)
{
    return (shoot_cmd.friction_mode == FRICTION_ON) ? 1u : 0u;
}

float cmd_get_fire_rate(void)
{
    return cmd_fire_tier_rate();
}

uint8_t cmd_get_loader_on(void)
{
    return (shoot_cmd.load_mode != LOADER_STOP) ? 1u : 0u;
}

/* ---- 机械零位标定触发(写 Flash) ---------------------------------------- */
/**
 * @brief  上挡零力 + 左纵杆推满保持 GIMBAL_CALIB_TRIGGER_HOLD_MS → 当前 pitch 姿态存为水平零位
 * @note   为什么必须零力:擦除 128 KB 扇区期间 CPU 冻结 1~2 s,控制环停摆、
 *         CAN 停发 → 电调丢帧保护归零 → 有力矩时云台会摔下来(见 components/calib/calib.c)。
 * @note   触发后需松开杆才能再次触发;保存成功/失败都打日志。
 */
static void cmd_calib_trigger()
{
    static uint32_t hold_cnt = 0;
    static uint8_t armed = 1;
    float rocker = (float)rc_data[RC_SLOT_CURRENT].rc.rocker_l1;

    if (robot_state != ROBOT_STATUS_STOP || !remote_control_is_online() || rocker < GIMBAL_CALIB_TRIGGER_ROCKER_L1)
    {
        hold_cnt = 0;
        armed = 1; // 松杆后重新武装
        return;
    }

    if (!armed)
        return;

    hold_cnt += 5u; // 本任务 200Hz → 每次 5 ms
    if (hold_cnt < GIMBAL_CALIB_TRIGGER_HOLD_MS)
        return;

    armed = 0;
    /* 触发瞬间先提示,再做整扇区擦写(CPU 会冻结 1~2 s,期间日志无法输出) */
    LOG_INFO("[CMD] 标定触发: 正在把当前 pitch 姿态写为机械水平零位…");
    if (gimbal_save_pitch_horizon_zero() == 0)
        LOG_INFO("[CMD] pitch 水平零位已写入 Flash,下次上电自动加载");
    else
        LOG_ERROR("[CMD] pitch 零位保存失败,仍使用 config 宏");
}

/* 机器人核心控制任务(200Hz) */
void cmd_task()
{
#ifdef ONE_BOARD
    msg_recv(chassis_feed_sub, (void *)&chassis_fb_recv);
#endif
#ifdef GIMBAL_BOARD
    chassis_fb_recv = *(chassis_upload_t *)can_comm_get(cmd_can_comm);
#endif
    msg_recv(shoot_feed_sub, &shoot_fb_recv);
    msg_recv(gimbal_feed_sub, &gimbal_fb_recv);

    // 云台与底盘正方向的夹角(供跟随与回中使用)
    cmd_calc_offset_angle();

    /* 云台目标角速度(操作手意图),给底盘做"提前刹车"(2026-11-21)。
     * 为什么不用 gz(IMU 实测):gz 要等云台自己减速完才归零,晚约 0.1 s;
     * 而目标角速度在操作手松杆那一刻就归零 —— 这 0.1 s 就是刹车能提前的量。
     * 来源是目标角差分,自动瞄准/回正会让目标跳变,故限幅 + 一阶低通。 */
    {
        static float yaw_last = 0.0f;
        static float rate_lp = 0.0f;
        float raw = (gimbal_cmd.yaw - yaw_last) / 0.005f; // 本任务 200Hz
        yaw_last = gimbal_cmd.yaw;
        if (raw > 600.0f)
            raw = 600.0f;
        else if (raw < -600.0f)
            raw = -600.0f;
        rate_lp += 0.35f * (raw - rate_lp);
        chassis_cmd.yaw_rate_cmd = rate_lp;
    }

    /* 输入源仲裁与自瞄统一状态(DEV-20261124-03):
     * auto_aim_active = 左中挡 || 键鼠 Shift toggle。两者任一有效即进自瞄。
     * 分派仍按 input_source:键鼠自瞄时保留 WASD 平移与鼠标发射,遥控自瞄时用右杆/拨轮;
     * 只有云台目标统一由 cmd_auto_aim_set() 从 AX650 给定,不在两路各写一份。 */
    cmd_update_input_source();
    {
        const uint8_t dial_auto_aim = REMOTE_RC_SWITCH_IS_MID(rc_data[RC_SLOT_CURRENT].rc.switch_left);
        /* toggle 必须每拍调用以维护 Shift 沿基线,是否参与自瞄再由输入源门控。
         * 键鼠超时回落遥控后若仍保留 Shift 锁存,操作手唯一的退出方式是右上挡急停;
         * 回落即把云台交还遥控挡位(DEV-20261124-05)。 */
        const uint8_t kbm_auto_aim = cmd_kbm_auto_aim_held();
        auto_aim_active =
            (uint8_t)(dial_auto_aim || (input_source == INPUT_KBM && kbm_auto_aim));
    }
#if AX650_UART1_ENABLE
    ax650_uart1_set_mode(auto_aim_active ? AX650_AUTO_AIM_MODE : AX650_TX_MODE);
#endif
    if (input_source == INPUT_KBM)
        cmd_keyboard_mouse_set();
    else
        cmd_remote_set();

#if GIMBAL_RECENTER_ENABLE
    /* 自瞄期间云台由 AX650 给世界系目标，底盘不得被 yaw 6020 的关节角带动。
     * 取消解锁或历史切挡留下的回中请求，避免 CHASSIS_ALIGN 覆盖无跟随/小陀螺模式。 */
    if (auto_aim_active)
    {
        yaw_recentering = 0;
        recenter_cnt = 0;
    }

    /* 小陀螺 → 跟随 的直接切换也触发一次回正(2026-11-17 新增)。
     * 为什么需要:原先 `yaw_recentering` 只在两处置位 —— F 键(键鼠通道,DR16 遥控没有 F)
     * 与解锁(上挡零力→中挡)。从**下档小陀螺直接拨回中档**既不经过上挡、也不按 F,
     * 于是"完全不会回正"。实测证据(2026-11-17 20:39 采集):两次切换分别在 58.67s 与 156.27s,
     * 切换后目标角停在 −70.1° 与 58.3° 长达十几秒纹丝不动 —— 不是慢,是没触发。
     * 触发后用与解锁回正完全相同的 90°/s 渐进闭环,不引入新逻辑。 */
    {
        static uint8_t last_chassis_mode_cmd = CHASSIS_FOLLOW_GIMBAL_YAW;
        if (!auto_aim_active &&
            last_chassis_mode_cmd == CHASSIS_ROTATE &&
            chassis_cmd.chassis_mode == CHASSIS_FOLLOW_GIMBAL_YAW)
        {
            yaw_recentering = 1;
            recenter_cnt = 0;
            LOG_INFO("[CMD] spin->follow, yaw recenter");
        }
        last_chassis_mode_cmd = chassis_cmd.chassis_mode;
    }

    /* 回中(2026-11-17 重写):**只动底盘,云台目标一个字节都不碰**。
     * 旧实现是 `gimbal_cmd.yaw -= delta`,即把云台世界目标往 −off 方向推 ——
     * 实测后果(2026-11-17 21:26 采集):①`Gyro[2]` 冲到 ±200°/s,云台自己在狂转;
     * ②关节角一变,跟随环立刻同时驱底盘,两者互抢,|off| 永远进不了死区;
     * ③于是每次都跑到 GIMBAL_RECENTER_TIMEOUT(3 s),转满 270°(=90°/s×3 s)后超时停住,
     *  目标角 −400.39→−670.39、−668.14→−934.99 两次都是这个形状。
     * 正确做法:云台保持当前世界方向(解锁时已按 YawTotalAngle 锚定,闭环本来就能稳到 2.4°/s),
     * 由**底盘**进 CHASSIS_ALIGN 模式恒速旋转把 offset 收到 0。
     * 回中期间允许平移,故不再因左杆动作中止;只保留"已回中"与"超时"两种退出。 */
    if (!auto_aim_active && robot_state == ROBOT_STATUS_READY && yaw_recentering)
    {
        if (fabsf(chassis_cmd.offset_angle) < GIMBAL_RECENTER_DEADBAND)
            yaw_recentering = 0; // 已回中
        else if (++recenter_cnt > GIMBAL_RECENTER_TIMEOUT)
            yaw_recentering = 0; // 超时兜底,防止 offset 卡死时一直占着 ALIGN 模式
        else
            chassis_cmd.chassis_mode = CHASSIS_ALIGN; // 交由底盘执行对齐
    }
#endif

    cmd_authority_arbitrate(); // 控制权仲裁

#if CHASSIS_M3508_FORCE_DISABLE
    /* 台架自瞄测试安全锁：只让底盘应用进入零力模式，四个 3508 的电流持续为 0。
     * 云台 yaw/pitch 使用独立命令，不受此开关影响。 */
    chassis_cmd.chassis_mode = CHASSIS_ZERO_FORCE;
    chassis_cmd.vx = 0.0f;
    chassis_cmd.vy = 0.0f;
    chassis_cmd.wz = 0.0f;
#endif

    /* 小陀螺档位随命令下发:chassis 在 CHASSIS_ROTATE 分支据此选恒速/变速
     * (DEV-20261125-08,协议字段 spin_vary) */
    chassis_cmd.spin_vary = chassis_spin_vary;

    // 推送三路控制消息
#ifdef ONE_BOARD
    msg_publish(chassis_cmd_pub, (void *)&chassis_cmd);
#endif
#ifdef GIMBAL_BOARD
    can_comm_send(cmd_can_comm, (void *)&chassis_cmd);
#endif
    msg_publish(shoot_cmd_pub, (void *)&shoot_cmd);
    msg_publish(gimbal_cmd_pub, (void *)&gimbal_cmd);

    cmd_calib_trigger(); // 零力状态下的零位标定触发(见上方说明)

    // 底盘命令遥测(fb为速度估算,chassis_estimate_speed实现前恒0)
    telemetry_push(&telemetry_chassis_cmd, chassis_cmd.vx, 0, chassis_cmd.wz,
                  chassis_cmd.offset_angle, (uint8_t)chassis_cmd.chassis_mode);
}
