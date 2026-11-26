/**
 * @file    remote_control.h
 * @brief   遥控接口与通道/按键结构
 * @layer   COMPONENT
 * @author  DJI 2016
 * @author  modified by neozng
 * @author  modified by 小猫大王
* @version beta
* @date 2022-11-01
* @copyright Copyright (c) 2016 DJI corp
* @copyright Copyright (c) 2022 HNU YueLu EC all rights reserved
 */
#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <stdint.h>
#include "main.h"
#include "usart.h"

// 遥控器数据读取:[0]当前 [1]上一次,用于按键持续按下/切换判断
#define RC_SLOT_PREVIOUS 1
#define RC_SLOT_CURRENT 0

// 获取按键操作
#define REMOTE_KEY_PRESS            0
#define REMOTE_KEY_STATE            1
#define REMOTE_KEY_PRESS_WITH_CTRL  1
#define REMOTE_KEY_PRESS_WITH_SHIFT 2

// 遥控通道有效范围
#define RC_CH_VALUE_MIN    ((uint16_t)364)
#define RC_CH_VALUE_OFFSET ((uint16_t)1024)
#define RC_CH_VALUE_MAX    ((uint16_t)1684)

/* ----------------------- DJI遥控开关 ----------------------------- */
#define REMOTE_RC_SW_UP          ((uint16_t)1) // 上
#define REMOTE_RC_SW_MID         ((uint16_t)3) // 中
#define REMOTE_RC_SW_DOWN        ((uint16_t)2) // 下
#define REMOTE_RC_SWITCH_IS_DOWN(s) (s == REMOTE_RC_SW_DOWN)
#define REMOTE_RC_SWITCH_IS_MID(s)  (s == REMOTE_RC_SW_MID)
#define REMOTE_RC_SWITCH_IS_UP(s)   (s == REMOTE_RC_SW_UP)

/* ----------------------- FS遥控开关 ----------------------------- */
#define REMOTE_FS_SW_UP          ((uint16_t)0)
#define REMOTE_FS_SW_MID         ((uint16_t)1)
#define REMOTE_FS_SW_DOWN        ((uint16_t)2)
#define REMOTE_FS_SWITCH_IS_DOWN(s) (s == REMOTE_FS_SW_DOWN)
#define REMOTE_FS_SWITCH_IS_MID(s)  (s == REMOTE_FS_SW_MID)
#define REMOTE_FS_SWITCH_IS_UP(s)   (s == REMOTE_FS_SW_UP)

/* ----------------------- 键盘键值索引 -------------------------------- */
#define REMOTE_KEY_W     0
#define REMOTE_KEY_S     1
#define REMOTE_KEY_D     2
#define REMOTE_KEY_A     3
#define REMOTE_KEY_SHIFT 4
#define REMOTE_KEY_CTRL  5
#define REMOTE_KEY_Q     6
#define REMOTE_KEY_E     7
#define REMOTE_KEY_R     8
#define REMOTE_KEY_F     9
#define REMOTE_KEY_G     10
#define REMOTE_KEY_Z     11
#define REMOTE_KEY_X     12
#define REMOTE_KEY_C     13
#define REMOTE_KEY_V     14
#define REMOTE_KEY_B     15

/* ----------------------- 数据结构 ------------------------------------- */
// 位域键盘状态,keys成员用于整字memcpy
typedef union
{
    struct
    {
        uint16_t w : 1;
        uint16_t s : 1;
        uint16_t d : 1;
        uint16_t a : 1;
        uint16_t shift : 1;
        uint16_t ctrl : 1;
        uint16_t q : 1;
        uint16_t e : 1;
        uint16_t r : 1;
        uint16_t f : 1;
        uint16_t g : 1;
        uint16_t z : 1;
        uint16_t x : 1;
        uint16_t c : 1;
        uint16_t v : 1;
        uint16_t b : 1;
    };
    uint16_t keys;
} kbm_key_t;

typedef struct
{
    struct
    {
        int16_t rocker_l_; // 左水平
        int16_t rocker_l1; // 左竖直
        int16_t rocker_r_; // 右水平
        int16_t rocker_r1; // 右竖直
        int16_t dial;      // 侧边拨轮

        uint8_t switch_left;  // 左侧开关
        uint8_t switch_right; // 右侧开关
    } rc;
    struct
    {
        int16_t x;
        int16_t y;
        uint8_t press_l;
        uint8_t press_r;
    } mouse;

    kbm_key_t key[3]; // [0]按下 [1]ctrl组合 [2]shift组合

    uint8_t key_count[3][16]; // 按键按下次数(上升沿计数)
} remote_rc_ctrl_t;

typedef struct
{
    int16_t channel[10];

    struct
    {
        int16_t rocker_l_;
        int16_t rocker_l1;
        int16_t rocker_r_;
        int16_t rocker_r1;
        uint8_t switch_left_1;
        uint8_t switch_left_2;
        uint8_t switch_right_1;
        uint8_t switch_right_2;
        int16_t knob_l; // 左旋钮
        int16_t knob_r; // 右旋钮
    } rc;
} remote_fs_ctrl_t;

/**
 * @brief 初始化遥控器并注册到串口
 *
 * @attention 遥控器在C板上使用USART3(DBUS,需要反相器)
 */
remote_rc_ctrl_t *remote_control_init(UART_HandleTypeDef *rc_usart_handle);
remote_fs_ctrl_t *remote_fs_init(UART_HandleTypeDef *rc_usart_handle);

/**
 * @brief 检查遥控器是否在线(未初始化或未收到有效帧均视为离线)
 *
 * @return uint8_t 1:在线 0:离线
 */
uint8_t remote_control_is_online();
uint8_t remote_fs_is_online();

#endif
