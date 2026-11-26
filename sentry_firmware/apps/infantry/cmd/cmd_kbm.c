/**
 * @file    cmd_kbm.c
 * @brief   键鼠输入:图传链路键鼠映射与灵敏度调档
 * @layer   APP
 * @author  小猫大王
* @note    从 robot_cmd.c 拆出;状态 last_*_count / kbm_* 只在本文件使用。
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

/* ----------------键鼠控制(图传链路,DEV-20261030-13)----------------
 * DBUS帧内键鼠字段由图传链路填充:图传未装时恒0(永不激活,天然安全),
 * 图传装好后键盘任意键/鼠标移动或点击即激活键鼠模式,超时自动回落遥控。
 * 仲裁/看门狗与输入源无关,遥控急停挡在键鼠模式下依然有效。 */

static float kbm_last_active_ms = 0.0f; // 键鼠最近活跃时刻(超时回落用)
static uint8_t spin_enabled = 0;        // 键鼠Q键小陀螺锁存
static uint8_t last_q_count = 0; // 键盘沿计数上期值(Q小陀螺/C匀速-变速/R单发/E三发/F回正/V降敏/B升敏/G重置用)
static uint8_t last_r_count = 0;
static uint8_t last_e_count = 0;
static uint8_t last_f_count = 0;
static uint8_t last_v_count = 0;
static uint8_t last_b_count = 0;
static uint8_t last_g_count = 0;
static uint8_t last_x_count = 0;    // X 键:战术射速档位循环(DEV-20261121-11)
static uint8_t last_c_count = 0;    // C 键:小陀螺匀速/变速切换(DEV-20261125-08)

/* 键鼠发射模式(DEV-20261124-09):R/E 切换,AUTO/SINGLE/BURST3 三态锁存;
 * 左键按当前模式发射。AUTO=按住连发,单发/三发=按住只登记一次,松手复位。 */
typedef enum
{
    KBM_FIRE_AUTO = 0,
    KBM_FIRE_SINGLE,
    KBM_FIRE_BURST3,
} kbm_fire_mode_e;

static kbm_fire_mode_e kbm_fire_mode = KBM_FIRE_AUTO;
static float kbm_sens_scale = 1.0f; // 鼠标灵敏度运行时倍率(V/B/G调,对KBM_*_SENS基准缩放)
static float kbm_yaw_filt = 0.0f;   // 鼠标x帧计数低通状态(只滤帧间抖动)
static float kbm_pitch_filt = 0.0f; // 鼠标y帧计数低通状态

/**
 * @brief 键鼠自瞄:Shift 按住有效,松开立即退回手动(DEV-20261124-09)
 * @return 1=Shift 按住且处于键鼠输入源、已解锁;0=其余
 * @note   Shift 走位域而非 key_count[](remote_control.c 第4/5位跳过计数),
 *         直接读电平即可。每次进入/退出都无锁存,避免"松手后仍自瞄"。
 */
uint8_t cmd_kbm_auto_aim_held(void)
{
    const uint8_t shift_now = (rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].shift) ? 1u : 0u;
    return (shift_now && input_source == INPUT_KBM && robot_state == ROBOT_STATUS_READY) ? 1u : 0u;
}

/**
 * @brief 键鼠活跃判定与输入源切换
 */
void cmd_update_input_source(void)
{
    const remote_rc_ctrl_t *rc = &rc_data[RC_SLOT_CURRENT];
    uint8_t mouse_active =
        (MATH_ABS(rc->mouse.x) > KBM_MOUSE_DEADZONE || MATH_ABS(rc->mouse.y) > KBM_MOUSE_DEADZONE ||
         rc->mouse.press_l || rc->mouse.press_r);
    uint8_t key_active = (rc->key[REMOTE_KEY_PRESS].keys != 0);
    const float now_ms = dwt_get_timeline_ms();

    if (mouse_active || key_active)
    {
        if (input_source != INPUT_KBM)
        {
            LOG_INFO("[CMD] input source: keyboard-mouse");
            /* 切入键鼠的瞬间同步沿计数基线:遥控模式下按过的Q/C/R/E/F/V/B/G不应在切入帧补触发 */
            last_q_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q];
            last_r_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R];
            last_e_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E];
            last_f_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F];
            last_v_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V];
            last_b_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B];
            last_g_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G];
            last_x_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X];
            last_c_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_C];
        }
        input_source = INPUT_KBM;
        kbm_last_active_ms = now_ms;
    }
    /* 小陀螺锁存期间不回落遥控:Q 是锁存指令,操作手不碰键鼠时也应持续旋转。
     * 遥控急停/失联仍由仲裁独立处理;要交回遥控,按 Q 关闭小陀螺后等超时即可。
     * (DEV-20261125-10:此前 3s 无输入回落遥控,右拨杆一接管底盘就退出 ROTATE,
     *  表现为"小陀螺自己停下"。) */
    else if (input_source == INPUT_KBM && !spin_enabled &&
             now_ms - kbm_last_active_ms > KBM_TIMEOUT_MS)
    {
        input_source = INPUT_RC; // 键鼠超时回落遥控
        LOG_INFO("[CMD] input source: remote control");
    }
}

/**
 * @brief 键鼠控制映射
 *      W/S/A/D平移 鼠标XY云台 左键连发 右键摩擦轮 Q小陀螺 C匀速/变速 R单发 E三发 F回正
 *      V/B/G灵敏度档 Shift自瞄toggle X射速档
 * @note  2026-11-24(DEV-20261124-03):删除 Shift 慢速(原乘 KBM_SLOW_RATIO),
 *        Shift 改为自瞄 toggle;自瞄时鼠标不再改云台目标(由 AX650 给),底盘平移与发射仍走键鼠。
 * @note  2026-11-25(DEV-20261125-08):C 沿切换小陀螺匀速/变速档。Tab 不在 DBUS
 *        16 键位内、客户端映射也不可行,键位直接定为 C。
 */
void cmd_keyboard_mouse_set(void)
{
    const remote_rc_ctrl_t *rc = &rc_data[RC_SLOT_CURRENT];
    const kbm_key_t *key = &rc->key[REMOTE_KEY_PRESS];

    /* 底盘平移:W/S/A/D合成,全速;斜向由chassis端等比例限幅,斜坡平滑。
     * 左右方向由KBM_STRAFE_DIR裁决(本车vx/vy系数是整体取反标定的,不可纯理论推断)
     * 最大速度读运行期变量(ELF 可写),本拍只读一次,避免写入时前后不一致。 */
    const float max_speed = chassis_max_speed_mm_s;
    float speed = max_speed;
    float vx = 0.0f, vy = 0.0f;
    if (key->w)
        vx += speed;
    if (key->s)
        vx -= speed;
    if (key->a)
        vy += speed * KBM_STRAFE_DIR; // A=左
    if (key->d)
        vy -= speed * KBM_STRAFE_DIR; // D=右

    /* 斜向归一化(DEV-20261124-12):W+D 同时按会让合速度到 2121mm/s,
     * 超过 CHASSIS_MAX_SPEED=1500;与遥控路径一致,按合速度等比缩放到上限,
     * 只改幅值不改方向。 */
    float norm = sqrtf(vx * vx + vy * vy);
    if (norm > max_speed)
    {
        const float scale = max_speed / norm;
        vx *= scale;
        vy *= scale;
    }
    chassis_cmd.vx = vx;
    chassis_cmd.vy = vy;

    /* 模式:Q沿切换小陀螺/跟随;C沿切换匀速/变速档位(DEV-20261125-08)。
     * Tab 不在 DBUS 16 键位内,客户端映射也不可行,键位直接定为 C。 */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q] != last_q_count)
    {
        spin_enabled = !spin_enabled;
        last_q_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q];
        LOG_INFO("[CMD] kb spin %s", spin_enabled ? "on" : "off");
    }
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_C] != last_c_count)
    {
        last_c_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_C];
        cmd_spin_vary_toggle();
    }
    /* 自瞄时底盘不跟随 yaw(与遥控自瞄右中挡一致),Q 小陀螺仍优先。 */
    chassis_cmd.chassis_mode = spin_enabled ? CHASSIS_ROTATE
                                            : (auto_aim_active ? CHASSIS_NO_FOLLOW
                                                               : CHASSIS_FOLLOW_GIMBAL_YAW);

    /* 灵敏度运行时调档:V降/B升(乘STEP) G重置基准;沿计数比对仿Q键,RTT回显当前倍率 */
    uint8_t sens_changed = 0;
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V] != last_v_count)
    {
        last_v_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_V];
        kbm_sens_scale *= KBM_SENS_SCALE_STEP; // 降档
        sens_changed = 1;
    }
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B] != last_b_count)
    {
        last_b_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_B];
        kbm_sens_scale /= KBM_SENS_SCALE_STEP; // 升档
        sens_changed = 1;
    }
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G] != last_g_count)
    {
        last_g_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_G];
        kbm_sens_scale = 1.0f; // 重置到config基准
        sens_changed = 1;
    }
    /* 战术射速档位:X 沿循环 ECO→NORMAL→BURST(DEV-20261121-11) */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X] != last_x_count)
    {
        last_x_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_X];
        cmd_fire_tier_cycle();
    }
    if (sens_changed)
    {
        MOTOR_LIMIT_MIN_MAX(kbm_sens_scale, KBM_SENS_SCALE_MIN, KBM_SENS_SCALE_MAX);
        char sens_str[16];
        log_float_to_str(sens_str, kbm_sens_scale);
        LOG_INFO("[CMD] kb sens x%s", sens_str);
    }

    /* 云台:自瞄时目标由 AX650 统一解算(cmd_auto_aim_set),鼠标增量不再叠加;
     * 手动时鼠标帧计数→目标角速度→按控制周期积分(FPS式线性1:1,DEV-20261106-01)。
     * 必须门控已解锁:yaw/pitch是增量累加,停机状态下晃鼠标会把目标角累积走
     * (解锁后pitch会先甩到累积位置);与cmd_remote_set的杆量累加门控一致。
     * DBUS帧计数=14ms内位移,同帧计数驻留约2.8个控制周期:除以帧周期换算成角速度再按dt
     * 积分后每计数恰好施加一次,灵敏度与帧率/控制率解耦(旧位置域方案的增益漂移源)。
     * 固定增益无加速曲线(CSGO式,保肌肉记忆);低通只滤帧间抖动;V/B/G实时调灵敏度 */
    if (auto_aim_active)
    {
        kbm_yaw_filt = 0.0f; // 自瞄期间清滤波状态,退出后从当前鼠标位置重新起步
        kbm_pitch_filt = 0.0f;
        cmd_auto_aim_set();
        gimbal_cmd.gimbal_mode = GIMBAL_AUTO_AIM_MODE;
    }
    else if (robot_state == ROBOT_STATUS_READY && remote_control_is_online())
    {
        float mx = (MATH_ABS(rc->mouse.x) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.x : 0.0f;
        float my = (MATH_ABS(rc->mouse.y) > KBM_MOUSE_DEADZONE) ? (float)rc->mouse.y : 0.0f;
        kbm_yaw_filt += KBM_INPUT_LP * (mx - kbm_yaw_filt); // 输入低通:只滤帧间抖动,不蓄手感
        kbm_pitch_filt += KBM_INPUT_LP * (my - kbm_pitch_filt);
        float yaw_rate = -kbm_yaw_filt * KBM_YAW_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        float pitch_rate = kbm_pitch_filt * KBM_PITCH_SENS * kbm_sens_scale / KBM_DBUS_FRAME_S;
        MOTOR_LIMIT_MIN_MAX(yaw_rate, -KBM_YAW_MAX_RATE, KBM_YAW_MAX_RATE);
        MOTOR_LIMIT_MIN_MAX(pitch_rate, -KBM_PITCH_MAX_RATE, KBM_PITCH_MAX_RATE);
        gimbal_cmd.yaw += yaw_rate * KBM_CMD_DT_S;
        gimbal_cmd.pitch += pitch_rate * KBM_CMD_DT_S * KBM_PITCH_DIR; // 上滑=枪口上抬
        MOTOR_LIMIT_MIN_MAX(gimbal_cmd.pitch, PITCH_MIN_ANGLE, PITCH_MAX_ANGLE);
        gimbal_cmd.gimbal_mode = GIMBAL_GYRO_MODE;
    }
    else
    {
        kbm_yaw_filt = 0.0f; // 未解锁/失联时清零,避免解锁后残留滤波值突跳
        kbm_pitch_filt = 0.0f;
        gimbal_cmd.gimbal_mode = GIMBAL_GYRO_MODE;
    }

    /* 发射:右键摩擦轮(按住开);R/E 切发射模式,左键按当前模式发射(DEV-20261124-09)。
     * 单发/三发模式下按住左键只登记一次(shoot 端沿检测),松手复位;
     * AUTO 模式按住左键持续连发。切换模式时要求左键已松开,避免走火。 */
    shoot_cmd.shoot_mode = SHOOT_ON;
    shoot_cmd.friction_mode = rc->mouse.press_r ? FRICTION_ON : FRICTION_OFF;
    shoot_cmd.shoot_rate = cmd_fire_tier_rate(); // 请求射速由战术档位决定(DEV-20261121-11)
    shoot_cmd.fire_tier = cmd_fire_tier_get();

    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R] != last_r_count)
    {
        last_r_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_R];
        if (!rc->mouse.press_l)
        {
            kbm_fire_mode = (kbm_fire_mode == KBM_FIRE_SINGLE) ? KBM_FIRE_AUTO : KBM_FIRE_SINGLE;
            LOG_INFO("[CMD] kb fire mode %s", (kbm_fire_mode == KBM_FIRE_SINGLE) ? "single" : "auto");
        }
    }
    else if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E] != last_e_count)
    {
        last_e_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_E];
        if (!rc->mouse.press_l)
        {
            kbm_fire_mode = (kbm_fire_mode == KBM_FIRE_BURST3) ? KBM_FIRE_AUTO : KBM_FIRE_BURST3;
            LOG_INFO("[CMD] kb fire mode %s", (kbm_fire_mode == KBM_FIRE_BURST3) ? "burst3" : "auto");
        }
    }

    if (shoot_cmd.friction_mode != FRICTION_ON || !rc->mouse.press_l)
        shoot_cmd.load_mode = LOADER_STOP;
    else if (kbm_fire_mode == KBM_FIRE_SINGLE)
        shoot_cmd.load_mode = LOADER_SINGLE;
    else if (kbm_fire_mode == KBM_FIRE_BURST3)
        shoot_cmd.load_mode = LOADER_BURST3;
    else
        shoot_cmd.load_mode = LOADER_BURST;

    /* F沿触发yaw回正,复用解锁回中状态机 */
    if (rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F] != last_f_count)
    {
        last_f_count = rc->key_count[REMOTE_KEY_PRESS][REMOTE_KEY_F];
#if GIMBAL_RECENTER_ENABLE
        yaw_recentering = 1;
        recenter_cnt = 0;
#endif
    }
}

uint8_t cmd_get_fire_mode(void)
{
    switch (kbm_fire_mode)
    {
    case KBM_FIRE_SINGLE:
        return 1u;
    case KBM_FIRE_BURST3:
        return 2u;
    default:
        return 3u; // AUTO
    }
}
