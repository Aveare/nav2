/**
 * @file    config_board.h
 * @brief   哨兵板型与总线安全开关
 * @layer   CONFIG
 * @author  小猫大王
 * @note    单板方案:一块 STM32F407IGHx 同时控制底盘、三轴云台、发射与裁判链路。
 */
#pragma once

/* ---------- 板型(只能定义一个) ---------- */
#define ONE_BOARD // 单板控制整车
// #define CHASSIS_BOARD
// #define GIMBAL_BOARD

#define VISION_USE_VCP // [配置] USB CDC VCP 同流承载 NV v1 与 RMCV 0x0503/0x1022

/* 底盘台架安全锁:1=四个底盘 M3508 强制零输出(云台/发射不受影响)。
 * 首次理线、架空测试、焊云台时置 1;需要跑底盘时改回 0 重烧。
 * 默认 0 = 正常出力:上电仍是 ZERO_FORCE/STOP,须走解锁流程才会动。 */
#ifndef SENTRY_CHASSIS_FORCE_DISABLE
#define SENTRY_CHASSIS_FORCE_DISABLE 0
#endif
