/**
 * @file    remote_control.c
 * @brief   遥控解析:DBUS/SBUS 帧校验与按键状态机
 * @layer   COMPONENT
 * @author  DJI / neozng
 * @author  modified by JIT CR (无效帧过滤与在线判定收紧)
 * @author  modified by 小猫大王
* @date    2026-10-29
 */
#include "remote_control.h"
#include "general_def.h"
#include "string.h"
#include "usart_port.h"
#include "memory.h"
#include "stdlib.h"
#include "daemon.h"
#include "log_port.h"

#define REMOTE_CONTROL_FRAME_SIZE 18u // DBUS遥控接收buffer大小
#define REMOTE_SBUS_FRAME_SIZE           25u // FS遥控buffer大小

// FS遥控开关档位中值与容差
#define REMOTE_SWITCH_HIGH      1807
#define REMOTE_SWITCH_MID       1024
#define REMOTE_SWITCH_LOW       240
#define REMOTE_SWITCH_TOLERANCE 200

// 遥控器数据:[0]当前 [1]上一次
static remote_rc_ctrl_t rc_ctrl[2];
static remote_fs_ctrl_t fs_rc_ctrl[2];
static uint8_t rc_init_flag = 0;
static volatile uint8_t rc_frame_received = 0; // 只有收到完整有效帧才报告在线

// 遥控器串口实例(单例,不封装)
static usart_t *rc_usart_instance;
static daemon_t *rc_daemon_instance;

/**
 * @brief 摇杆矫正:超出±660的值视为无效,置0
 */
static void remote_rectify_joystick()
{
    for (uint8_t i = 0; i < 5; ++i)
        if (MATH_ABS(*(&rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i)) > 660)
            *(&rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i) = 0;
}

/**
 * @brief DBUS协议解析:摇杆/拨轮/开关/鼠标/键盘
 */
static void remote_sbus_to_rc(const uint8_t *sbus_buf)
{
    // 摇杆:11位通道解算后减中值偏置
    rc_ctrl[RC_SLOT_CURRENT].rc.rocker_r_ =
        ((sbus_buf[0] | (sbus_buf[1] << 8)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 0
    rc_ctrl[RC_SLOT_CURRENT].rc.rocker_r1 =
        (((sbus_buf[1] >> 3) | (sbus_buf[2] << 5)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 1
    rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ =
        (((sbus_buf[2] >> 6) | (sbus_buf[3] << 2) | (sbus_buf[4] << 10)) & 0x07ff) -
        RC_CH_VALUE_OFFSET; //!< Channel 2
    rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l1 =
        (((sbus_buf[4] >> 1) | (sbus_buf[5] << 7)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 3
    rc_ctrl[RC_SLOT_CURRENT].rc.dial =
        ((sbus_buf[16] | (sbus_buf[17] << 8)) & 0x07FF) - RC_CH_VALUE_OFFSET; // 左侧拨轮
    remote_rectify_joystick();
    // 开关,0左1右
    rc_ctrl[RC_SLOT_CURRENT].rc.switch_right = ((sbus_buf[5] >> 4) & 0x0003);     //!< Switch right
    rc_ctrl[RC_SLOT_CURRENT].rc.switch_left = ((sbus_buf[5] >> 4) & 0x000C) >> 2; //!< Switch left

    // 鼠标
    rc_ctrl[RC_SLOT_CURRENT].mouse.x = (sbus_buf[6] | (sbus_buf[7] << 8)); //!< Mouse X axis
    rc_ctrl[RC_SLOT_CURRENT].mouse.y = (sbus_buf[8] | (sbus_buf[9] << 8)); //!< Mouse Y axis
    rc_ctrl[RC_SLOT_CURRENT].mouse.press_l = sbus_buf[12];                 //!< Mouse Left Is Press ?
    rc_ctrl[RC_SLOT_CURRENT].mouse.press_r = sbus_buf[13];                 //!< Mouse Right Is Press ?

    // 键盘位域直接memcpy(小端低字节在前)
    *(uint16_t *)&rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS] = (uint16_t)(sbus_buf[14] | (sbus_buf[15] << 8));
    if (rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].ctrl)
        rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_CTRL] = rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS];
    else
        memset(&rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_CTRL], 0, sizeof(kbm_key_t));
    if (rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].shift)
        rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_SHIFT] = rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS];
    else
        memset(&rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_SHIFT], 0, sizeof(kbm_key_t));

    uint16_t key_now = rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys,
             key_last = rc_ctrl[RC_SLOT_PREVIOUS].key[REMOTE_KEY_PRESS].keys,
             key_with_ctrl = rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_CTRL].keys,
             key_with_shift = rc_ctrl[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS_WITH_SHIFT].keys,
             key_last_with_ctrl = rc_ctrl[RC_SLOT_PREVIOUS].key[REMOTE_KEY_PRESS_WITH_CTRL].keys,
             key_last_with_shift = rc_ctrl[RC_SLOT_PREVIOUS].key[REMOTE_KEY_PRESS_WITH_SHIFT].keys;

    // 逐位检测上升沿,更新按下计数(4,5位为ctrl/shift,跳过)
    for (uint16_t i = 0, j = 0x1; i < 16; j <<= 1, i++)
    {
        if (i == 4 || i == 5)
            continue;
        if ((key_now & j) && !(key_last & j) && !(key_with_ctrl & j) && !(key_with_shift & j))
            rc_ctrl[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS][i]++;
        if ((key_with_ctrl & j) && !(key_last_with_ctrl & j))
            rc_ctrl[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS_WITH_CTRL][i]++;
        if ((key_with_shift & j) && !(key_last_with_shift & j))
            rc_ctrl[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS_WITH_SHIFT][i]++;
    }

    memcpy(&rc_ctrl[RC_SLOT_PREVIOUS], &rc_ctrl[RC_SLOT_CURRENT], sizeof(remote_rc_ctrl_t)); // 保存上一次数据
}

/**
 * @brief FS遥控(SBUS)协议解析
 */
static void remote_fs_to_rc(const uint8_t *sbus_buf)
{
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_r_ = ((sbus_buf[1] | sbus_buf[2] << 8) & 0x07FF) - 1024u;
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_r1 = ((sbus_buf[2] >> 3 | sbus_buf[3] << 5) & 0x07FF) - 1024u;
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l1 =
        ((sbus_buf[3] >> 6 | sbus_buf[4] << 2 | sbus_buf[5] << 10) & 0x07FF) - 1024u;
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ = ((sbus_buf[5] >> 1 | sbus_buf[6] << 7) & 0x07FF) - 1024u;

    for (uint8_t i = 0; i < 4; ++i)
        if (*(&fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i) > 660)
            *(&fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i) = 660;
        else if (*(&fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i) < -660)
            *(&fs_rc_ctrl[RC_SLOT_CURRENT].rc.rocker_l_ + i) = -660;

    // 旋钮
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.knob_l = ((sbus_buf[6] >> 4 | sbus_buf[7] << 4) & 0x07FF) - 240u;
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.knob_r =
        ((sbus_buf[7] >> 7 | sbus_buf[8] << 1 | sbus_buf[9] << 9) & 0x07FF) - 240u;
    // 拨杆0上，1中，2下
    fs_rc_ctrl[RC_SLOT_CURRENT].channel[6] = ((sbus_buf[9] >> 2 | sbus_buf[10] << 6) & 0x07FF);
    fs_rc_ctrl[RC_SLOT_CURRENT].channel[7] = ((sbus_buf[10] >> 5 | sbus_buf[11] << 3) & 0x07FF);
    fs_rc_ctrl[RC_SLOT_CURRENT].channel[8] = ((sbus_buf[12] | sbus_buf[13] << 8) & 0x07FF);
    fs_rc_ctrl[RC_SLOT_CURRENT].channel[9] = ((sbus_buf[13] >> 3 | sbus_buf[14] << 5) & 0x07FF);
    for (int i = 6; i <= 9; i++)
    {
        uint16_t val = fs_rc_ctrl[RC_SLOT_CURRENT].channel[i];
        if (val > REMOTE_SWITCH_HIGH - REMOTE_SWITCH_TOLERANCE && val < REMOTE_SWITCH_HIGH + REMOTE_SWITCH_TOLERANCE)
            fs_rc_ctrl[RC_SLOT_CURRENT].channel[i] = 2; // 下挡
        else if (val > REMOTE_SWITCH_MID - REMOTE_SWITCH_TOLERANCE && val < REMOTE_SWITCH_MID + REMOTE_SWITCH_TOLERANCE)
            fs_rc_ctrl[RC_SLOT_CURRENT].channel[i] = 1; // 中挡
        else if (val > REMOTE_SWITCH_LOW - REMOTE_SWITCH_TOLERANCE && val < REMOTE_SWITCH_LOW + REMOTE_SWITCH_TOLERANCE)
            fs_rc_ctrl[RC_SLOT_CURRENT].channel[i] = 0; // 上挡
        else
            fs_rc_ctrl[RC_SLOT_CURRENT].channel[i] = 1; // 非法值默认归中
    }

    fs_rc_ctrl[RC_SLOT_CURRENT].rc.switch_left_1 = fs_rc_ctrl[RC_SLOT_CURRENT].channel[6];
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.switch_left_2 = fs_rc_ctrl[RC_SLOT_CURRENT].channel[7];
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.switch_right_1 = fs_rc_ctrl[RC_SLOT_CURRENT].channel[8];
    fs_rc_ctrl[RC_SLOT_CURRENT].rc.switch_right_2 = fs_rc_ctrl[RC_SLOT_CURRENT].channel[9];

    memcpy(&fs_rc_ctrl[RC_SLOT_PREVIOUS], &fs_rc_ctrl[RC_SLOT_CURRENT], sizeof(remote_fs_ctrl_t));
}

/**
 * @brief DBUS接收回调:逐帧校验有效性,无效帧清零控制量且不喂狗(判定离线)
 * @note  DEV-20261121-06:bsp 改为循环 DMA 后,一次消费可能含多帧(recv_len 为本次字节数),
 *        必须按 18B 定长逐帧切分。**18B 帧无帧头,丢 1 字节后无法自愈** ——
 *        一旦某帧校验失败,本块剩余数据全部丢弃,由 daemon(100ms 无有效帧)清控制量并
 *        重启接收重新对齐;这段"丢字节 → 恢复"的窗口内遥控按失联处理(零力),是定长帧
 *        协议的物理限制,不是本实现引入的缺陷。
 */
static void remote_control_rx_callback()
{
    uint16_t total = rc_usart_instance->recv_len; // 入口:装配缓冲中的可用总长
    uint8_t *buff = rc_usart_instance->recv_buff;
    uint16_t off = 0;

    while ((uint16_t)(total - off) >= REMOTE_CONTROL_FRAME_SIZE)
    {
        uint8_t *frame = &buff[off];

        // 有效性校验:开关非0(0为非法档位)、前四路摇杆在有效范围内;
        // 辅助拨轮沿用remote_rectify_joystick的越界归零,不因此丢帧
        uint8_t invalid = (((frame[5] >> 4) & 0x03) == 0) || (((frame[5] >> 6) & 0x03) == 0);
        uint16_t channel[4] = {
            (frame[0] | (frame[1] << 8)) & 0x07ff,
            ((frame[1] >> 3) | (frame[2] << 5)) & 0x07ff,
            ((frame[2] >> 6) | (frame[3] << 2) | (frame[4] << 10)) & 0x07ff,
            ((frame[4] >> 1) | (frame[5] << 7)) & 0x07ff,
        };
        for (uint8_t i = 0; i < 4; ++i)
            if (channel[i] < RC_CH_VALUE_MIN || channel[i] > RC_CH_VALUE_MAX)
                invalid |= 1;

        if (invalid)
        {
            rc_frame_received = 0;
            rc_daemon_instance->temp_count = 0;
            memset(rc_ctrl, 0, sizeof(rc_ctrl)); // 立即清除控制量,防止残帧驱动
            rc_usart_instance->recv_len = total; // 定长帧错位:整块丢弃,等 daemon 重启对齐
            return;
        }

        remote_sbus_to_rc(frame);
        rc_frame_received = 1;
        daemon_reload(rc_daemon_instance); // 有效帧解析完成后才喂狗
        off = (uint16_t)(off + REMOTE_CONTROL_FRAME_SIZE);
    }

    /* 只消费完整帧:剩下的半帧(轮询把一帧切成两半)留给下一拍拼装 */
    rc_usart_instance->recv_len = off;
}

/**
 * @brief FS遥控接收回调
 */
static void remote_fs_rx_callback()
{
    daemon_reload(rc_daemon_instance);
    remote_fs_to_rc(rc_usart_instance->recv_buff);
    rc_frame_received = 1;
}

/**
 * @brief 遥控器离线回调(注册到守护进程)
 */
static void remote_rc_lost_callback(void *id)
{
    rc_frame_received = 0;
    memset(rc_ctrl, 0, sizeof(rc_ctrl)); // 清空遥控器数据
    usart_service_init(rc_usart_instance); // 尝试重新启动接收
    LOG_WARN("[rc] remote control lost");
}

static void remote_fs_lost_callback(void *id)
{
    rc_frame_received = 0;
    memset(fs_rc_ctrl, 0, sizeof(fs_rc_ctrl));
    usart_service_init(rc_usart_instance);
    LOG_WARN("[rc] remote control lost");
}

remote_rc_ctrl_t *remote_control_init(UART_HandleTypeDef *rc_usart_handle)
{
    usart_init_config_t conf = {0}; // 必须清零:tx_only 未初始化会随机关掉接收(DEV-20261121-12)
    conf.module_callback = remote_control_rx_callback;
    conf.usart_handle = rc_usart_handle;
    conf.recv_buff_size = REMOTE_CONTROL_FRAME_SIZE;
    rc_usart_instance = usart_register(&conf);

    // 守护:100ms未收到有效帧视为离线(遥控实际帧率约70Hz)
    daemon_init_config_t daemon_conf = {
        .reload_count = 10,
        .callback = remote_rc_lost_callback,
        .owner_id = NULL,
    };
    rc_daemon_instance = daemon_register(&daemon_conf);

    rc_frame_received = 0;
    rc_init_flag = 1;
    return rc_ctrl;
}

remote_fs_ctrl_t *remote_fs_init(UART_HandleTypeDef *rc_usart_handle)
{
    usart_init_config_t conf = {0}; // 必须清零:tx_only 未初始化会随机关掉接收(DEV-20261121-12)
    conf.module_callback = remote_fs_rx_callback;
    conf.usart_handle = rc_usart_handle;
    conf.recv_buff_size = REMOTE_SBUS_FRAME_SIZE;
    rc_usart_instance = usart_register(&conf);

    daemon_init_config_t daemon_conf = {
        .reload_count = 10,
        .callback = remote_fs_lost_callback,
        .owner_id = NULL,
    };
    rc_daemon_instance = daemon_register(&daemon_conf);

    rc_init_flag = 1;
    return fs_rc_ctrl;
}

uint8_t remote_control_is_online()
{
    if (rc_init_flag && rc_frame_received)
        return daemon_is_online(rc_daemon_instance);
    return 0;
}

uint8_t remote_fs_is_online()
{
    if (rc_init_flag)
        return daemon_is_online(rc_daemon_instance);
    return 0;
}
