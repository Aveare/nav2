/**
 * @file remote_control.h
 * @author DJI 2016
 * @author modified by neozng
 * @brief  遥控器模块定义头文件
 * @version beta
 * @date 2022-11-01
 *
 * @copyright Copyright (c) 2016 DJI corp
 * @copyright Copyright (c) 2022 HNU YueLu EC all rights reserved
 */
#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <stdint.h>
#include "main.h"
#include "usart.h"

// 遥控器数据读取:[0]当前 [1]上一次,用于按键持续按下/切换判断
#define LAST 1
#define TEMP 0

// 获取按键操作
#define KEY_PRESS 0
#define KEY_STATE 1
#define KEY_PRESS_WITH_CTRL 1
#define KEY_PRESS_WITH_SHIFT 2

// 遥控通道有效范围
#define RC_CH_VALUE_MIN ((uint16_t)364)
#define RC_CH_VALUE_OFFSET ((uint16_t)1024)
#define RC_CH_VALUE_MAX ((uint16_t)1684)

/* ----------------------- DJI遥控开关 ----------------------------- */
#define RC_SW_UP ((uint16_t)1)   // 上
#define RC_SW_MID ((uint16_t)3)  // 中
#define RC_SW_DOWN ((uint16_t)2) // 下
#define switch_is_down(s) (s == RC_SW_DOWN)
#define switch_is_mid(s) (s == RC_SW_MID)
#define switch_is_up(s) (s == RC_SW_UP)

/* ----------------------- FS遥控开关 ----------------------------- */
#define FS_RC_SW_UP ((uint16_t)0)
#define FS_RC_SW_MID ((uint16_t)1)
#define FS_RC_SW_DOWN ((uint16_t)2)
#define fs_switch_is_down(s) (s == FS_RC_SW_DOWN)
#define fs_switch_is_mid(s) (s == FS_RC_SW_MID)
#define fs_switch_is_up(s) (s == FS_RC_SW_UP)

/* ----------------------- 键盘键值索引 -------------------------------- */
#define Key_W 0
#define Key_S 1
#define Key_D 2
#define Key_A 3
#define Key_Shift 4
#define Key_Ctrl 5
#define Key_Q 6
#define Key_E 7
#define Key_R 8
#define Key_F 9
#define Key_G 10
#define Key_Z 11
#define Key_X 12
#define Key_C 13
#define Key_V 14
#define Key_B 15

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
} Key_t;

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

    Key_t key[3]; // [0]按下 [1]ctrl组合 [2]shift组合

    uint8_t key_count[3][16]; // 按键按下次数(上升沿计数)
} RC_ctrl_t;

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
} FS_RC_ctrl_t;

/**
 * @brief 初始化遥控器并注册到串口
 *
 * @attention 遥控器在C板上使用USART3(DBUS,需要反相器)
 */
RC_ctrl_t *RemoteControlInit(UART_HandleTypeDef *rc_usart_handle);
FS_RC_ctrl_t *FS_RemoteControlInit(UART_HandleTypeDef *rc_usart_handle);

/**
 * @brief 检查遥控器是否在线(未初始化或未收到有效帧均视为离线)
 *
 * @return uint8_t 1:在线 0:离线
 */
uint8_t RemoteControlIsOnline();
uint8_t FS_RemoteControlIsOnline();

#endif
