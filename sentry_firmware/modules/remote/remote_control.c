/**
 * @file    remote_control.c
 * @author  DJI / neozng
 * @author  modified by JIT CR (无效帧过滤与在线判定收紧)
 * @brief   遥控器帧解析、在线判定与控制量换算
 * @date    2026-10-29
 */
#include "remote_control.h"
#include "string.h"
#include "bsp_usart.h"
#include "memory.h"
#include "stdlib.h"
#include "daemon.h"
#include "bsp_log.h"

#define REMOTE_CONTROL_FRAME_SIZE 18u // DBUS遥控接收buffer大小
#define SBUS_FRAME_SIZE 25u           // FS遥控buffer大小

// FS遥控开关档位中值与容差
#define SWITCH_HIGH 1807
#define SWITCH_MID 1024
#define SWITCH_LOW 240
#define SWITCH_TOLERANCE 200

// 遥控器数据:[0]当前 [1]上一次
static RC_ctrl_t rc_ctrl[2];
static FS_RC_ctrl_t fs_rc_ctrl[2];
static uint8_t rc_init_flag = 0;
static volatile uint8_t rc_frame_received = 0; // 只有收到完整有效帧才报告在线

// 遥控器串口实例(单例,不封装)
static USARTInstance *rc_usart_instance;
static DaemonInstance *rc_daemon_instance;

/**
 * @brief 摇杆矫正:超出±660的值视为无效,置0
 */
static void RectifyRCjoystick()
{
    for (uint8_t i = 0; i < 5; ++i)
        if (abs(*(&rc_ctrl[TEMP].rc.rocker_l_ + i)) > 660)
            *(&rc_ctrl[TEMP].rc.rocker_l_ + i) = 0;
}

/**
 * @brief DBUS协议解析:摇杆/拨轮/开关/鼠标/键盘
 */
static void sbus_to_rc(const uint8_t *sbus_buf)
{
    // 摇杆:11位通道解算后减中值偏置
    rc_ctrl[TEMP].rc.rocker_r_ = ((sbus_buf[0] | (sbus_buf[1] << 8)) & 0x07ff) - RC_CH_VALUE_OFFSET;      //!< Channel 0
    rc_ctrl[TEMP].rc.rocker_r1 = (((sbus_buf[1] >> 3) | (sbus_buf[2] << 5)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 1
    rc_ctrl[TEMP].rc.rocker_l_ = (((sbus_buf[2] >> 6) | (sbus_buf[3] << 2) | (sbus_buf[4] << 10)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 2
    rc_ctrl[TEMP].rc.rocker_l1 = (((sbus_buf[4] >> 1) | (sbus_buf[5] << 7)) & 0x07ff) - RC_CH_VALUE_OFFSET; //!< Channel 3
    rc_ctrl[TEMP].rc.dial = ((sbus_buf[16] | (sbus_buf[17] << 8)) & 0x07FF) - RC_CH_VALUE_OFFSET;          // 左侧拨轮
    RectifyRCjoystick();
    // 开关,0左1右
    rc_ctrl[TEMP].rc.switch_right = ((sbus_buf[5] >> 4) & 0x0003);     //!< Switch right
    rc_ctrl[TEMP].rc.switch_left = ((sbus_buf[5] >> 4) & 0x000C) >> 2; //!< Switch left

    // 鼠标
    rc_ctrl[TEMP].mouse.x = (sbus_buf[6] | (sbus_buf[7] << 8)); //!< Mouse X axis
    rc_ctrl[TEMP].mouse.y = (sbus_buf[8] | (sbus_buf[9] << 8)); //!< Mouse Y axis
    rc_ctrl[TEMP].mouse.press_l = sbus_buf[12];                 //!< Mouse Left Is Press ?
    rc_ctrl[TEMP].mouse.press_r = sbus_buf[13];                 //!< Mouse Right Is Press ?

    // 键盘位域直接memcpy(小端低字节在前)
    *(uint16_t *)&rc_ctrl[TEMP].key[KEY_PRESS] = (uint16_t)(sbus_buf[14] | (sbus_buf[15] << 8));
    if (rc_ctrl[TEMP].key[KEY_PRESS].ctrl)
        rc_ctrl[TEMP].key[KEY_PRESS_WITH_CTRL] = rc_ctrl[TEMP].key[KEY_PRESS];
    else
        memset(&rc_ctrl[TEMP].key[KEY_PRESS_WITH_CTRL], 0, sizeof(Key_t));
    if (rc_ctrl[TEMP].key[KEY_PRESS].shift)
        rc_ctrl[TEMP].key[KEY_PRESS_WITH_SHIFT] = rc_ctrl[TEMP].key[KEY_PRESS];
    else
        memset(&rc_ctrl[TEMP].key[KEY_PRESS_WITH_SHIFT], 0, sizeof(Key_t));

    uint16_t key_now = rc_ctrl[TEMP].key[KEY_PRESS].keys,
             key_last = rc_ctrl[LAST].key[KEY_PRESS].keys,
             key_with_ctrl = rc_ctrl[TEMP].key[KEY_PRESS_WITH_CTRL].keys,
             key_with_shift = rc_ctrl[TEMP].key[KEY_PRESS_WITH_SHIFT].keys,
             key_last_with_ctrl = rc_ctrl[LAST].key[KEY_PRESS_WITH_CTRL].keys,
             key_last_with_shift = rc_ctrl[LAST].key[KEY_PRESS_WITH_SHIFT].keys;

    // 逐位检测上升沿,更新按下计数(4,5位为ctrl/shift,跳过)
    for (uint16_t i = 0, j = 0x1; i < 16; j <<= 1, i++)
    {
        if (i == 4 || i == 5)
            continue;
        if ((key_now & j) && !(key_last & j) && !(key_with_ctrl & j) && !(key_with_shift & j))
            rc_ctrl[TEMP].key_count[KEY_PRESS][i]++;
        if ((key_with_ctrl & j) && !(key_last_with_ctrl & j))
            rc_ctrl[TEMP].key_count[KEY_PRESS_WITH_CTRL][i]++;
        if ((key_with_shift & j) && !(key_last_with_shift & j))
            rc_ctrl[TEMP].key_count[KEY_PRESS_WITH_SHIFT][i]++;
    }

    memcpy(&rc_ctrl[LAST], &rc_ctrl[TEMP], sizeof(RC_ctrl_t)); // 保存上一次数据
}

/**
 * @brief FS遥控(SBUS)协议解析
 */
static void fs_to_rc(const uint8_t *sbus_buf)
{
    fs_rc_ctrl[TEMP].rc.rocker_r_ = ((sbus_buf[1] | sbus_buf[2] << 8) & 0x07FF) - 1024u;
    fs_rc_ctrl[TEMP].rc.rocker_r1 = ((sbus_buf[2] >> 3 | sbus_buf[3] << 5) & 0x07FF) - 1024u;
    fs_rc_ctrl[TEMP].rc.rocker_l1 = ((sbus_buf[3] >> 6 | sbus_buf[4] << 2 | sbus_buf[5] << 10) & 0x07FF) - 1024u;
    fs_rc_ctrl[TEMP].rc.rocker_l_ = ((sbus_buf[5] >> 1 | sbus_buf[6] << 7) & 0x07FF) - 1024u;

    for (uint8_t i = 0; i < 4; ++i)
        if (*(&fs_rc_ctrl[TEMP].rc.rocker_l_ + i) > 660)
            *(&fs_rc_ctrl[TEMP].rc.rocker_l_ + i) = 660;
        else if (*(&fs_rc_ctrl[TEMP].rc.rocker_l_ + i) < -660)
            *(&fs_rc_ctrl[TEMP].rc.rocker_l_ + i) = -660;

    // 旋钮
    fs_rc_ctrl[TEMP].rc.knob_l = ((sbus_buf[6] >> 4 | sbus_buf[7] << 4) & 0x07FF) - 240u;
    fs_rc_ctrl[TEMP].rc.knob_r = ((sbus_buf[7] >> 7 | sbus_buf[8] << 1 | sbus_buf[9] << 9) & 0x07FF) - 240u;
    // 拨杆0上，1中，2下
    fs_rc_ctrl[TEMP].channel[6] = ((sbus_buf[9] >> 2 | sbus_buf[10] << 6) & 0x07FF);
    fs_rc_ctrl[TEMP].channel[7] = ((sbus_buf[10] >> 5 | sbus_buf[11] << 3) & 0x07FF);
    fs_rc_ctrl[TEMP].channel[8] = ((sbus_buf[12] | sbus_buf[13] << 8) & 0x07FF);
    fs_rc_ctrl[TEMP].channel[9] = ((sbus_buf[13] >> 3 | sbus_buf[14] << 5) & 0x07FF);
    for (int i = 6; i <= 9; i++)
    {
        uint16_t val = fs_rc_ctrl[TEMP].channel[i];
        if (val > SWITCH_HIGH - SWITCH_TOLERANCE && val < SWITCH_HIGH + SWITCH_TOLERANCE)
            fs_rc_ctrl[TEMP].channel[i] = 2; // 下挡
        else if (val > SWITCH_MID - SWITCH_TOLERANCE && val < SWITCH_MID + SWITCH_TOLERANCE)
            fs_rc_ctrl[TEMP].channel[i] = 1; // 中挡
        else if (val > SWITCH_LOW - SWITCH_TOLERANCE && val < SWITCH_LOW + SWITCH_TOLERANCE)
            fs_rc_ctrl[TEMP].channel[i] = 0; // 上挡
        else
            fs_rc_ctrl[TEMP].channel[i] = 1; // 非法值默认归中
    }

    fs_rc_ctrl[TEMP].rc.switch_left_1 = fs_rc_ctrl[TEMP].channel[6];
    fs_rc_ctrl[TEMP].rc.switch_left_2 = fs_rc_ctrl[TEMP].channel[7];
    fs_rc_ctrl[TEMP].rc.switch_right_1 = fs_rc_ctrl[TEMP].channel[8];
    fs_rc_ctrl[TEMP].rc.switch_right_2 = fs_rc_ctrl[TEMP].channel[9];

    memcpy(&fs_rc_ctrl[LAST], &fs_rc_ctrl[TEMP], sizeof(FS_RC_ctrl_t));
}

/**
 * @brief DBUS接收回调:先校验帧有效性,无效帧清零控制量且不喂狗(判定离线)
 */
static void RemoteControlRxCallback()
{
    const uint8_t *buff = rc_usart_instance->recv_buff;

    // 有效性校验:帧长、开关非0(0为非法档位)、前四路摇杆在有效范围内;
    // 辅助拨轮沿用RectifyRCjoystick的越界归零,不因此丢帧
    uint8_t invalid = rc_usart_instance->recv_len != REMOTE_CONTROL_FRAME_SIZE;
    invalid |= (((buff[5] >> 4) & 0x03) == 0) || (((buff[5] >> 6) & 0x03) == 0);
    uint16_t channel[4] = {
        (buff[0] | (buff[1] << 8)) & 0x07ff,
        ((buff[1] >> 3) | (buff[2] << 5)) & 0x07ff,
        ((buff[2] >> 6) | (buff[3] << 2) | (buff[4] << 10)) & 0x07ff,
        ((buff[4] >> 1) | (buff[5] << 7)) & 0x07ff,
    };
    for (uint8_t i = 0; i < 4; ++i)
        if (channel[i] < RC_CH_VALUE_MIN || channel[i] > RC_CH_VALUE_MAX)
            invalid |= 1;

    if (invalid)
    {
        rc_frame_received = 0;
        rc_daemon_instance->temp_count = 0;
        memset(rc_ctrl, 0, sizeof(rc_ctrl)); // 立即清除控制量,防止残帧驱动
        return;
    }

    sbus_to_rc(rc_usart_instance->recv_buff);
    rc_frame_received = 1;
    DaemonReload(rc_daemon_instance); // 有效帧解析完成后才喂狗
}

/**
 * @brief FS遥控接收回调
 */
static void FS_RemoteControlRxCallback()
{
    DaemonReload(rc_daemon_instance);
    fs_to_rc(rc_usart_instance->recv_buff);
    rc_frame_received = 1;
}

/**
 * @brief 遥控器离线回调(注册到守护进程)
 */
static void RCLostCallback(void *id)
{
    rc_frame_received = 0;
    memset(rc_ctrl, 0, sizeof(rc_ctrl)); // 清空遥控器数据
    USARTServiceInit(rc_usart_instance); // 尝试重新启动接收
    LOGWARNING("[rc] remote control lost");
}

static void FS_RCLostCallback(void *id)
{
    rc_frame_received = 0;
    memset(fs_rc_ctrl, 0, sizeof(fs_rc_ctrl));
    USARTServiceInit(rc_usart_instance);
    LOGWARNING("[rc] remote control lost");
}

RC_ctrl_t *RemoteControlInit(UART_HandleTypeDef *rc_usart_handle)
{
    USART_Init_Config_s conf;
    conf.module_callback = RemoteControlRxCallback;
    conf.usart_handle = rc_usart_handle;
    conf.recv_buff_size = REMOTE_CONTROL_FRAME_SIZE;
    rc_usart_instance = USARTRegister(&conf);

    // 守护:100ms未收到有效帧视为离线(遥控实际帧率约70Hz)
    Daemon_Init_Config_s daemon_conf = {
        .reload_count = 10,
        .callback = RCLostCallback,
        .owner_id = NULL,
    };
    rc_daemon_instance = DaemonRegister(&daemon_conf);

    rc_frame_received = 0;
    rc_init_flag = 1;
    return rc_ctrl;
}

FS_RC_ctrl_t *FS_RemoteControlInit(UART_HandleTypeDef *rc_usart_handle)
{
    USART_Init_Config_s conf;
    conf.module_callback = FS_RemoteControlRxCallback;
    conf.usart_handle = rc_usart_handle;
    conf.recv_buff_size = SBUS_FRAME_SIZE;
    rc_usart_instance = USARTRegister(&conf);

    Daemon_Init_Config_s daemon_conf = {
        .reload_count = 10,
        .callback = FS_RCLostCallback,
        .owner_id = NULL,
    };
    rc_daemon_instance = DaemonRegister(&daemon_conf);

    rc_init_flag = 1;
    return fs_rc_ctrl;
}

uint8_t RemoteControlIsOnline()
{
    if (rc_init_flag && rc_frame_received)
        return DaemonIsOnline(rc_daemon_instance);
    return 0;
}

uint8_t FS_RemoteControlIsOnline()
{
    if (rc_init_flag)
        return DaemonIsOnline(rc_daemon_instance);
    return 0;
}
