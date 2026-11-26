/**
 * @file    robot_cmd.c
 * @brief   命令层:控制权仲裁与各应用控制分发
 * @layer   APP
 * @note    职责:读遥控器 → 按固定优先级裁决控制权(失联 > 急停挡 > 拨轮急停/锁存 > 运行)
 *          → 生成三路控制命令 → 发布给 chassis / gimbal / shoot。
 * @note    视觉/自主输入的仲裁扩展位:接入后在其运行分支内按"自主 > 视觉 > 遥控"次序仲裁,
 *          并维护输入有效期。
 * @note    2026-10-29 建立,作者 JIT CR。
 */
// app
#include "robot_def.h"
#include "robot_cmd.h"
#include "autonomy.h"
#include "ax650_usb.h"
// module
#include "remote_control.h"
#include "ins_task.h"
#include "master_process.h"
#include "message_center.h"
#include "general_def.h"
#include "dji_motor.h"
#include "telemetry.h"
// bsp
#include "bsp_dwt.h"
#include "bsp_log.h"

// 编码器值自动转换成角度值
#define YAW_ALIGN_ANGLE (YAW_CHASSIS_ALIGN_ECD * ECD_ANGLE_COEF_DJI)  // 对齐时的角度,0-360

/* cmd应用的模块实例与交互信息 */
#ifdef GIMBAL_BOARD // 双板兼容
#include "can_comm.h"
static CANCommInstance *cmd_can_comm;
#endif
#ifdef ONE_BOARD
static Publisher_t *chassis_cmd_pub;   // 底盘控制消息发布者
static Subscriber_t *chassis_feed_sub; // 底盘反馈订阅者
#endif                                 // ONE_BOARD

static Chassis_Ctrl_Cmd_s chassis_cmd_send;      // 发给底盘的控制信息
static Chassis_Upload_Data_s chassis_fetch_data; // 从底盘收到的反馈

static RC_ctrl_t *rc_data; // 遥控器数据

static Publisher_t *gimbal_cmd_pub;            // 云台控制消息发布者
static Subscriber_t *gimbal_feed_sub;          // 云台反馈订阅者
static Gimbal_Ctrl_Cmd_s gimbal_cmd_send;      // 传给云台的控制信息
static Gimbal_Upload_Data_s gimbal_fetch_data; // 从云台收到的反馈

static Publisher_t *shoot_cmd_pub;           // 发射控制消息发布者
static Subscriber_t *shoot_feed_sub;         // 发射反馈订阅者
static Shoot_Ctrl_Cmd_s shoot_cmd_send;      // 传给发射的控制信息
static Shoot_Upload_Data_s shoot_fetch_data; // 从发射收到的反馈

static Robot_Status_e robot_state; // 机器人整体工作状态
/* 解锁回中:解锁沿启动,yaw以限速渐进回相对零点;左杆接管/超时中止 */
#if GIMBAL_RECENTER_ENABLE
static uint8_t yaw_recentering = 0;
static uint16_t recenter_cnt = 0;
#endif

/* ----------------键鼠控制(图传链路,DEV-20261030-13)----------------
 * DBUS帧内键鼠字段由图传链路填充:图传未装时恒0(永不激活,天然安全),
 * 图传装好后键盘任意键/鼠标移动或点击即激活键鼠模式,超时自动回落遥控。
 * 仲裁/看门狗与输入源无关,遥控急停挡在键鼠模式下依然有效。 */
typedef enum
{
    INPUT_RC = 0, // DBUS遥控杆
    INPUT_KBM,    // 图传键鼠
} Input_Source_e;

static Input_Source_e input_source = INPUT_RC;
static float kbm_last_active_ms = 0.0f; // 键鼠最近活跃时刻(超时回落用)
static uint8_t spin_enabled = 0;        // 键鼠Q键小陀螺锁存
static uint8_t last_q_count = 0;        // 键盘沿计数上期值(Q小陀螺/R单发/E三发/F回正/V降敏/B升敏/G重置用)
static uint8_t last_r_count = 0;
static uint8_t last_e_count = 0;
static uint8_t last_f_count = 0;
static uint8_t last_v_count = 0;
static uint8_t last_b_count = 0;
static uint8_t last_g_count = 0;
static float kbm_sens_scale = 1.0f;     // 鼠标灵敏度运行时倍率(V/B/G调,对KBM_*_SENS基准缩放)
static float kbm_yaw_filt = 0.0f;       // 鼠标x帧计数低通状态(只滤帧间抖动)
static float kbm_pitch_filt = 0.0f;     // 鼠标y帧计数低通状态

/**
 * @brief 键鼠活跃判定与输入源切换
 */
static void UpdateInputSource(void)
{
    const RC_ctrl_t *rc = &rc_data[TEMP];
    uint8_t mouse_active = (abs(rc->mouse.x) > KBM_MOUSE_DEADZONE ||
                            abs(rc->mouse.y) > KBM_MOUSE_DEADZONE ||
                            rc->mouse.press_l || rc->mouse.press_r);
    uint8_t key_active = (rc->key[KEY_PRESS].keys != 0);
    const float now_ms = DWT_GetTimeline_ms();

    if (mouse_active || key_active)
    {
        if (input_source != INPUT_KBM)
        {
            LOGINFO("[CMD] input source: keyboard-mouse");
            /* 切入键鼠的瞬间同步沿计数基线:遥控模式下按过的Q/R/E/F/V/B/G不应在切入帧补触发 */
            last_q_count = rc->key_count[KEY_PRESS][Key_Q];
            last_r_count = rc->key_count[KEY_PRESS][Key_R];
            last_e_count = rc->key_count[KEY_PRESS][Key_E];
            last_f_count = rc->key_count[KEY_PRESS][Key_F];
            last_v_count = rc->key_count[KEY_PRESS][Key_V];
            last_b_count = rc->key_count[KEY_PRESS][Key_B];
            last_g_count = rc->key_count[KEY_PRESS][Key_G];
        }
        input_source = INPUT_KBM;
        kbm_last_active_ms = now_ms;
    }
    else if (input_source == INPUT_KBM && now_ms - kbm_last_active_ms > KBM_TIMEOUT_MS)
    {
        input_source = INPUT_RC; // 键鼠超时回落遥控
        LOGINFO("[CMD] input source: remote control");
    }
}

/**
 * @brief 键鼠控制映射
 *      W/S/A/D平移(Shift慢速) 鼠标XY云台 左键连发 右键摩擦轮 Q小陀螺 R单发 E三发 F回正 V/B/G灵敏度档
 */
static void KeyboardMouseSet(void)
{
    const RC_ctrl_t *rc = &rc_data[TEMP];
    const Key_t *key = &rc->key[KEY_PRESS];

    /* 底盘平移:W/S/A/D合成,Shift慢速;斜向由chassis端等比例限幅,斜坡平滑。
     * 左右方向由KBM_STRAFE_DIR裁决(本车vx/vy系数是整体取反标定的,不可纯理论推断) */
    float speed = CHASSIS_MAX_SPEED * (key->shift ? KBM_SLOW_RATIO : 1.0f);
    float vx = 0.0f, vy = 0.0f;
    if (key->w) vx += speed;
    if (key->s) vx -= speed;
    if (key->a) vy += speed * KBM_STRAFE_DIR; // A=左
    if (key->d) vy -= speed * KBM_STRAFE_DIR; // D=右
    chassis_cmd_send.vx = vx;
    chassis_cmd_send.vy = vy;

    /* Q沿切换小陀螺/普通行驶；哨兵普通行驶由鼠标控制底盘转向。 */
    if (rc->key_count[KEY_PRESS][Key_Q] != last_q_count)
    {
        spin_enabled = !spin_enabled;
        last_q_count = rc->key_count[KEY_PRESS][Key_Q];
        LOGINFO("[CMD] kb spin %s", spin_enabled ? "on" : "off");
    }
#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
    chassis_cmd_send.chassis_mode = spin_enabled ? CHASSIS_ROTATE : CHASSIS_RC_CONTROL;
    chassis_cmd_send.wz = 0.0f;
#else
    chassis_cmd_send.chassis_mode = spin_enabled ? CHASSIS_ROTATE : CHASSIS_FOLLOW_GIMBAL_YAW;
#endif

    /* 灵敏度运行时调档:V降/B升(乘STEP) G重置基准;沿计数比对仿Q键,RTT回显当前倍率 */
    uint8_t sens_changed = 0;
    if (rc->key_count[KEY_PRESS][Key_V] != last_v_count)
    {
        last_v_count = rc->key_count[KEY_PRESS][Key_V];
        kbm_sens_scale *= KBM_SENS_SCALE_STEP; // 降档
        sens_changed = 1;
    }
    if (rc->key_count[KEY_PRESS][Key_B] != last_b_count)
    {
        last_b_count = rc->key_count[KEY_PRESS][Key_B];
        kbm_sens_scale /= KBM_SENS_SCALE_STEP; // 升档
        sens_changed = 1;
    }
    if (rc->key_count[KEY_PRESS][Key_G] != last_g_count)
    {
        last_g_count = rc->key_count[KEY_PRESS][Key_G];
        kbm_sens_scale = 1.0f; // 重置到config基准
        sens_changed = 1;
    }
    if (sens_changed)
    {
        LIMIT_MIN_MAX(kbm_sens_scale, KBM_SENS_SCALE_MIN, KBM_SENS_SCALE_MAX);
        char sens_str[16];
        Float2Str(sens_str, kbm_sens_scale);
        LOGINFO("[CMD] kb sens x%s", sens_str);
    }

    /* 云台:鼠标帧计数→目标角速度→按控制周期积分(FPS式线性1:1,DEV-20261106-01)。
     * 必须门控已解锁:yaw/pitch是增量累加,停机状态下晃鼠标会把目标角累积走
     * (解锁后pitch会先甩到累积位置);与RemoteControlSet的杆量累加门控一致。
     * DBUS帧计数=14ms内位移,同帧计数驻留约2.8个控制周期:除以帧周期换算成角速度再按dt
     * 积分后每计数恰好施加一次,灵敏度与帧率/控制率解耦(旧位置域方案的增益漂移源)。
     * 固定增益无加速曲线(CSGO式,保肌肉记忆);低通只滤帧间抖动;V/B/G实时调灵敏度 */
    if (robot_state == ROBOT_READY && RemoteControlIsOnline())
    {
        float mx = (abs(rc->mouse.x) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.x : 0.0f;
        float my = (abs(rc->mouse.y) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.y : 0.0f;
        kbm_yaw_filt += KBM_INPUT_LP * (mx - kbm_yaw_filt);     // 输入低通:只滤帧间抖动,不蓄手感
        kbm_pitch_filt += KBM_INPUT_LP * (my - kbm_pitch_filt);
        float yaw_rate = -kbm_yaw_filt * KBM_YAW_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        float pitch_rate = kbm_pitch_filt * KBM_PITCH_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        #if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
        LIMIT_MIN_MAX(yaw_rate, -SENTRY_CHASSIS_MOUSE_RATE_DEG_S, SENTRY_CHASSIS_MOUSE_RATE_DEG_S);
#else
        LIMIT_MIN_MAX(yaw_rate, -KBM_YAW_MAX_RATE, KBM_YAW_MAX_RATE);
#endif
        LIMIT_MIN_MAX(pitch_rate, -PITCH_MOUSE_RATE_DEG_S, PITCH_MOUSE_RATE_DEG_S);
#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
        if (!spin_enabled) chassis_cmd_send.wz = yaw_rate; // 大yaw锁电池方向，鼠标横移控制底盘转向
#else
        gimbal_cmd_send.yaw += yaw_rate * KBM_CMD_DT_S;
#endif
        gimbal_cmd_send.pitch += pitch_rate * KBM_CMD_DT_S * KBM_PITCH_DIR; // 上滑=枪口上抬
        LIMIT_MIN_MAX(gimbal_cmd_send.pitch, PITCH_COMMAND_MIN_DEG, PITCH_COMMAND_MAX_DEG);
    }
    else
    {
        kbm_yaw_filt = 0.0f; // 未解锁/失联时清零,避免解锁后残留滤波值突跳
        kbm_pitch_filt = 0.0f;
    }
    gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
    gimbal_cmd_send.small_yaw_rate_deg_s = 0.0f; // 键鼠暂不控小 yaw(摇杆链路独有,防止上一条路径的残值滞留)

    /* 发射:右键摩擦轮(按住开),左键按住连发,R沿单发(标定后生效) */
    shoot_cmd_send.shoot_mode = SHOOT_ON;
    shoot_cmd_send.friction_mode = rc->mouse.press_r ? FRICTION_ON : FRICTION_OFF;
    shoot_cmd_send.shoot_rate = SHOOT_RATE_DEFAULT;
    if (rc->key_count[KEY_PRESS][Key_R] != last_r_count)
    {
        last_r_count = rc->key_count[KEY_PRESS][Key_R];
        shoot_cmd_send.load_mode = LOAD_1_BULLET; // 单发请求(shoot端沿检测,标定后生效)
    }
    else if (rc->key_count[KEY_PRESS][Key_E] != last_e_count)
    {
        last_e_count = rc->key_count[KEY_PRESS][Key_E];
        shoot_cmd_send.load_mode = LOAD_3_BULLET; // 三发请求(shoot端沿检测,逐发推进)
    }
    else
        shoot_cmd_send.load_mode = (shoot_cmd_send.friction_mode == FRICTION_ON && rc->mouse.press_l)
                                       ? LOAD_BURSTFIRE
                                       : LOAD_STOP;

    /* F沿触发yaw回正,复用解锁回中状态机 */
    if (rc->key_count[KEY_PRESS][Key_F] != last_f_count)
    {
        last_f_count = rc->key_count[KEY_PRESS][Key_F];
#if GIMBAL_RECENTER_ENABLE
        yaw_recentering = 1;
        recenter_cnt = 0;
#endif
    }
}

/* -------------------------控制权仲裁-------------------------
 * 每周期按固定优先级裁决唯一控制来源,输出动作集中处理,
 * 避免多处if各自为政导致安全状态遗漏。
 */
typedef enum
{
    AUTH_RC_LOST = 0,  // 遥控失联:最高优先级,失联即零力,恢复后仍需原解锁流程
    AUTH_KILL_SWITCH,  // 急停挡位:左上/右上挡
    AUTH_STOP_LATCHED, // 拨轮急停或此前急停尚未解锁
    AUTH_RUN,          // 正常运行
} Authority_Source_e;

static Authority_Source_e authority_source;      // 本周期裁决结果
static Authority_Source_e authority_source_last; // 上周期结果,日志边沿触发用

void RobotCMDInit()
{
    rc_data = RemoteControlInit(&huart3); // C板DBUS在USART3(带反相器)
    // 视觉通道:master_process模块VisionInit,本车暂未接入

    gimbal_cmd_pub = PubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
    gimbal_feed_sub = SubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
    shoot_cmd_pub = PubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
    shoot_feed_sub = SubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));

#ifdef ONE_BOARD
    chassis_cmd_pub = PubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
    chassis_feed_sub = SubRegister("chassis_feed", sizeof(Chassis_Upload_Data_s));
#endif // ONE_BOARD
#ifdef GIMBAL_BOARD
    CANComm_Init_Config_s comm_conf = {
        .can_config = {
            .can_handle = &hcan1,
            .tx_id = 0x312,
            .rx_id = 0x311,
        },
        .recv_data_len = sizeof(Chassis_Upload_Data_s),
        .send_data_len = sizeof(Chassis_Ctrl_Cmd_s),
    };
    cmd_can_comm = CANCommInit(&comm_conf);
#endif // GIMBAL_BOARD
    gimbal_cmd_send.pitch = 0;

    robot_state = ROBOT_STOP; // 上电先急停,右上挡回中解锁
}

/**
 * @brief 由云台回传的yaw电机单圈角度计算与零位的误差,统一折回[-180,180]
 *        (跨编码器零点时不追错方向)
 */
#if !CHASSIS_TUNING_ONLY
static void CalcOffsetAngle()
{
    static float angle;
    angle = gimbal_fetch_data.yaw_motor_single_round_angle;
    chassis_cmd_send.offset_angle = angle - YAW_ALIGN_ANGLE;
    if (chassis_cmd_send.offset_angle > 180.0f)
        chassis_cmd_send.offset_angle -= 360.0f;
    else if (chassis_cmd_send.offset_angle < -180.0f)
        chassis_cmd_send.offset_angle += 360.0f;
}

#endif

/**
 * @brief 遥控模式:右下自转，右中调试手动/旧整车跟随，右上急停
 */
static void RemoteControlSet()
{
    if (switch_is_down(rc_data[TEMP].rc.switch_right))
    {
        chassis_cmd_send.chassis_mode = CHASSIS_ROTATE;
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
    }
    else if (switch_is_mid(rc_data[TEMP].rc.switch_right))
    {
#if CHASSIS_TUNING_ONLY
        chassis_cmd_send.chassis_mode = CHASSIS_RC_CONTROL;
#else
        chassis_cmd_send.chassis_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
#endif
        gimbal_cmd_send.gimbal_mode = GIMBAL_GYRO_MODE;
    }

#if !(CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE)
    /* 松杆重锚定:左杆回中沿把yaw目标设为当前IMU角,角度误差立即归零,
     * 云台不再追转动中残留的目标-姿态差 */
    static uint8_t yaw_stick_active = 0;
    if (abs(rc_data[TEMP].rc.rocker_l_) > 10)
        yaw_stick_active = 1;
    else if (yaw_stick_active)
    {
        yaw_stick_active = 0;
        if (robot_state == ROBOT_READY && RemoteControlIsOnline())
            gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
    }

#endif

    // 角目标增量:200Hz下左横杆最大180deg/s,左纵杆60deg/s
    if (robot_state == ROBOT_READY && RemoteControlIsOnline() &&
        !switch_is_up(rc_data[TEMP].rc.switch_right))
    {
#if !(CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE)
        if (abs(rc_data[TEMP].rc.rocker_l_) > 10)
            gimbal_cmd_send.yaw -= 180.0f * 0.005f / 660.0f * rc_data[TEMP].rc.rocker_l_;
#endif
        if (abs(rc_data[TEMP].rc.rocker_l1) > 10)
            gimbal_cmd_send.pitch += PITCH_REMOTE_RATE_DEG_S * 0.005f / 660.0f * rc_data[TEMP].rc.rocker_l1;
    }
    LIMIT_MIN_MAX(gimbal_cmd_send.pitch, PITCH_COMMAND_MIN_DEG, PITCH_COMMAND_MAX_DEG);

    /* [用户指定] 以已标定的大yaw正前方为前进参考，右杆前后、左右均反向；合速度限CHASSIS_MAX_SPEED;
     * 死区加迟滞(进入30/退出20),滤静止噪声防指令打摆 */
    float forward = rc_data[TEMP].rc.rocker_r1;
    float right = rc_data[TEMP].rc.rocker_r_;
    static uint8_t rc_moving = 0;
    if (fabsf(forward) > CHASSIS_RC_DEADBAND || fabsf(right) > CHASSIS_RC_DEADBAND)
        rc_moving = 1;
    else if (fabsf(forward) < CHASSIS_RC_RELEASE && fabsf(right) < CHASSIS_RC_RELEASE)
        rc_moving = 0;
    if (rc_moving)
    {
        float norm = sqrtf(forward * forward + right * right);
        chassis_cmd_send.vx = CHASSIS_MAX_SPEED * forward / norm;
        chassis_cmd_send.vy = CHASSIS_MAX_SPEED * right / norm;
    }
    else
        chassis_cmd_send.vx = chassis_cmd_send.vy = 0;

#if CHASSIS_TUNING_ONLY
    /* 左下挡:左横杆改控**小 yaw**(见 config 的小 yaw 段),此时底盘转向不再由左横杆驱动;
     * 其余挡位左横杆仍作底盘转向。两者互斥,避免一个杆同时驱动两个轴。 */
    if (switch_is_down(rc_data[TEMP].rc.switch_left))
    {
        float stick = -rc_data[TEMP].rc.rocker_l_;
        gimbal_cmd_send.small_yaw_rate_deg_s =
            (abs((int)stick) < CHASSIS_RC_DEADBAND)
                ? 0.0f
                : SENTRY_SMALL_YAW_STICK_DIR * SENTRY_SMALL_YAW_STICK_RATE_DEG_S * stick / 660.0f;
        chassis_cmd_send.wz = 0.0f;
    }
    else
    {
        gimbal_cmd_send.small_yaw_rate_deg_s = 0.0f;
        float turn = -rc_data[TEMP].rc.rocker_l_;
        if (abs((int)turn) < CHASSIS_RC_DEADBAND)
            chassis_cmd_send.wz = 0.0f;
        else
            chassis_cmd_send.wz = CHASSIS_ROTATE_SPEED * turn / 660.0f;
    }
#endif

    /* 发射:**摩擦轮由拨轮扳机驱动**(与拨弹盘同源启动),左下挡不再开关摩擦轮。
     * 压过 DIAL_TRIGGER_ZONE 即进触发区(不分深浅)→ 摩擦轮 ON 且允许拨弹;
     * 松手沿:短拨登记单发,长按则连发至松手。进/出阈值构成迟滞,滤拨轮边界抖动防误触发。
     * 松手后轮子不会立刻停:shoot 侧有 FRICTION_STOP_DELAY_MS 的延时尾窗,让末发还吃得到轮速。 */
    shoot_cmd_send.shoot_mode = SHOOT_ON;
    shoot_cmd_send.shoot_rate = SHOOT_RATE_DEFAULT; // 连发射速(发/s),config定值
    {
        static uint8_t dial_in_trigger = 0; // 拨轮处于扳机区内(切回遥控/重启后按0起步,安全)
        static float dial_press_ms = 0.0f;  // 本次压入触发区的起始时刻
        const int16_t dial = rc_data[TEMP].rc.dial;
        const float now_ms = DWT_GetTimeline_ms();
        const uint8_t in_zone = (dial <= DIAL_TRIGGER_ZONE) ? 1u : 0u;
        const uint8_t released = (dial > DIAL_TRIGGER_RELEASE) ? 1u : 0u;

        shoot_cmd_send.friction_mode = in_zone ? FRICTION_ON : FRICTION_OFF;

        if (in_zone && !dial_in_trigger) // 进入扳机区,开始计时
        {
            dial_in_trigger = 1;
            dial_press_ms = now_ms;
        }

        if (dial_in_trigger && released) // 松手沿:短拨登记单发,长按已连发则不补发
        {
            dial_in_trigger = 0;
            shoot_cmd_send.load_mode =
                (now_ms - dial_press_ms < DIAL_SINGLE_HOLD_MS) ? LOAD_1_BULLET : LOAD_STOP;
        }
        else if (dial_in_trigger && in_zone) // 区内按住:达到时长后连发直至松手
        {
            shoot_cmd_send.load_mode =
                (now_ms - dial_press_ms >= DIAL_SINGLE_HOLD_MS) ? LOAD_BURSTFIRE : LOAD_STOP;
        }
        else
        {
            shoot_cmd_send.load_mode = LOAD_STOP;
        }
    }
}

/**
 * @brief 三路控制消息的统一安全态
 */
static void ApplySafeOutputs(void)
{
    gimbal_cmd_send.gimbal_mode = GIMBAL_ZERO_FORCE;
    gimbal_cmd_send.small_yaw_rate_deg_s = 0.0f;
    chassis_cmd_send.chassis_mode = CHASSIS_ZERO_FORCE;
    shoot_cmd_send.shoot_mode = SHOOT_OFF;
    shoot_cmd_send.friction_mode = FRICTION_OFF;
    shoot_cmd_send.load_mode = LOAD_STOP;
}

/**
 * @brief 控制权仲裁,随RobotCMDTask以200Hz执行
 *        优先级: 遥控失联 > 急停挡位 > 拨轮急停/锁存 > 正常运行
 * @note  锁存语义:任一停止来源都进入ROBOT_STOP,须右上+左中+全回中才解除;
 *        解除当周期仍保持零力。来源变化时打印一次(停机期间不刷屏)。
 */
static void AuthorityArbitrate()
{
    // 1.按优先级裁决本周期控制来源
    if (!RemoteControlIsOnline())
        authority_source = AUTH_RC_LOST;
    else if (switch_is_up(rc_data[TEMP].rc.switch_left) ||
             switch_is_up(rc_data[TEMP].rc.switch_right))
        authority_source = AUTH_KILL_SWITCH;
    else if (rc_data[TEMP].rc.dial > 300 || robot_state == ROBOT_STOP)
        authority_source = AUTH_STOP_LATCHED; // 拨轮急停,或此前急停未解锁
    else
        authority_source = AUTH_RUN;

    // 2.停止来源一律进入ROBOT_STOP等待解锁
    if (authority_source != AUTH_RUN)
        robot_state = ROBOT_STOP;

    // 3.来源变化时打印一次
    if (authority_source != authority_source_last)
    {
        switch (authority_source)
        {
        case AUTH_RC_LOST:
            LOGERROR("[CMD] authority: RC lost, zero force");
            break;
        case AUTH_KILL_SWITCH:
            LOGERROR("[CMD] authority: kill switch, zero force");
            break;
        case AUTH_STOP_LATCHED:
            LOGERROR("[CMD] authority: dial stop or latched, zero force");
            break;
        default:
            LOGINFO("[CMD] authority: normal control");
            break;
        }
        authority_source_last = authority_source;
    }

    // 4.统一输出动作:非RUN一律安全态
    if (authority_source != AUTH_RUN)
        ApplySafeOutputs();

    // 5.解锁流程:右上+左中+全回中解除锁存(右上挡本周期仍零力)
    if (RemoteControlIsOnline() && switch_is_up(rc_data[TEMP].rc.switch_right) &&
        switch_is_mid(rc_data[TEMP].rc.switch_left) && abs(rc_data[TEMP].rc.dial) <= 10 &&
        abs(rc_data[TEMP].rc.rocker_l_) <= 10 && abs(rc_data[TEMP].rc.rocker_l1) <= 10 &&
        abs(rc_data[TEMP].rc.rocker_r_) <= 10 && abs(rc_data[TEMP].rc.rocker_r1) <= 10)
    {
        robot_state = ROBOT_READY;
#if CHASSIS_TUNING_ONLY && SENTRY_PITCH_CONTROL_ENABLE
        gimbal_cmd_send.pitch = 0.0f; // 配合pitch任务按本次实测位置重新建立零增量参考
#endif
        /* 解锁沿先把yaw目标重锚定到当前IMU世界角:停机期间手转/随动会积累巨大
         * 世界角误差,解锁瞬间角度环会满输出slam;重锚定后由回中以90deg/s渐进回正 */
        gimbal_cmd_send.yaw = gimbal_fetch_data.gimbal_imu_data.YawTotalAngle;
        /* pitch目标保持上电0°(IMU水平):解锁后回水平是既有"回正"行为的一部分
         * (2026-10-30实测:改成锚定当前角后"回水平"消失,已恢复原版) */
#if GIMBAL_RECENTER_ENABLE
        yaw_recentering = 1; // 每次解锁触发一次回正前方
        recenter_cnt = 0;
#endif
        LOGINFO("[CMD] reinstate, robot ready");
    }
}

/* 机器人核心控制任务(200Hz) */
void RobotCMDTask()
{
#ifdef ONE_BOARD
    SubGetMessage(chassis_feed_sub, (void *)&chassis_fetch_data);
#endif
#ifdef GIMBAL_BOARD
    chassis_fetch_data = *(Chassis_Upload_Data_s *)CANCommGet(cmd_can_comm);
#endif
    SubGetMessage(shoot_feed_sub, &shoot_fetch_data);
    SubGetMessage(gimbal_feed_sub, &gimbal_fetch_data);

    /* 输入源分发:键鼠活跃即切键鼠模式(超时回落),遥控左中/下挡走杆控;
     * 左上急停由仲裁兜底,与输入源无关 */
    UpdateInputSource();
#if AX650_USB_ENABLE
    ax650_usb_set_mode(switch_is_mid(rc_data[TEMP].rc.switch_left) ? AX650_AUTO_AIM_MODE : AX650_TX_MODE);
#endif
    if (input_source == INPUT_KBM)
        KeyboardMouseSet();
    else if (switch_is_mid(rc_data[TEMP].rc.switch_left) || switch_is_down(rc_data[TEMP].rc.switch_left))
        RemoteControlSet();

#if CHASSIS_TUNING_ONLY
    // 大yaw朝向电池，与底盘相对固定，移动命令使用车体坐标。
    chassis_cmd_send.offset_angle = 0.0f;
#else
    CalcOffsetAngle();
#endif

#if GIMBAL_RECENTER_ENABLE
    /* 解锁回中:用实时offset闭环渐进转回正前方,与底盘跟随同向收敛不打架。
     * 中止条件:左杆出死区(用户接管)/超时/已回中。
     * 转向反了(越转越偏)改GIMBAL_RECENTER_DIR为-1。 */
    if (robot_state == ROBOT_READY && yaw_recentering)
    {
        if (abs(rc_data[TEMP].rc.rocker_l_) > 10 || abs(rc_data[TEMP].rc.rocker_l1) > 10 ||
            ++recenter_cnt > GIMBAL_RECENTER_TIMEOUT)
            yaw_recentering = 0;
        else
        {
            float off = chassis_cmd_send.offset_angle;
            if (fabsf(off) < GIMBAL_RECENTER_DEADBAND)
                yaw_recentering = 0; // 已回中
            else
            {
                float step = GIMBAL_RECENTER_SPEED * 0.005f; // deg/s x 5ms(200Hz)
                float delta = GIMBAL_RECENTER_DIR * off;
                LIMIT_MIN_MAX(delta, -step, step);
                gimbal_cmd_send.yaw -= delta;
            }
        }
    }
#endif

    /* 导航只接管底盘;遥控安全仲裁仍在最后执行。右杆推动可立即回手动。 */
    if (SENTRY_AUTONOMY_ENABLE &&
        switch_is_mid(rc_data[TEMP].rc.switch_left) &&
        switch_is_mid(rc_data[TEMP].rc.switch_right) &&
        abs(rc_data[TEMP].rc.rocker_r_) <= CHASSIS_RC_DEADBAND &&
        abs(rc_data[TEMP].rc.rocker_r1) <= CHASSIS_RC_DEADBAND)
    {
        sentry_auto_cmd_t navigation;
        int valid = autonomy_get_command(&navigation);
        chassis_cmd_send.chassis_mode = valid ? CHASSIS_NAVIGATION : CHASSIS_ZERO_FORCE;
        chassis_cmd_send.vx = valid ? navigation.vx : 0.0f;
        chassis_cmd_send.vy = valid ? navigation.vy : 0.0f;
        chassis_cmd_send.wz = valid ? navigation.wz : 0.0f;
    }

    AuthorityArbitrate(); // 控制权仲裁

    // 视觉数据设置(接入视觉后在此填充VisionSetFlag)

    // 推送三路控制消息
#ifdef ONE_BOARD
    PubPushMessage(chassis_cmd_pub, (void *)&chassis_cmd_send);
#endif
#ifdef GIMBAL_BOARD
    CANCommSend(cmd_can_comm, (void *)&chassis_cmd_send);
#endif
    PubPushMessage(shoot_cmd_pub, (void *)&shoot_cmd_send);
    PubPushMessage(gimbal_cmd_pub, (void *)&gimbal_cmd_send);

    // 底盘命令遥测(fb为速度估算,EstimateSpeed实现前恒0)
    TelemetryPush(&telemetry_chassis_cmd, chassis_cmd_send.vx, 0,
                  chassis_cmd_send.wz, chassis_cmd_send.offset_angle,
                  (uint8_t)chassis_cmd_send.chassis_mode);
}
