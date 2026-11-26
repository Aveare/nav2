/**
 * @file    main.h
 * @brief   主机测试桩:替代 main.h 的最小实现,仅供 PC 端编译测试
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
#include <stdint.h>
typedef struct { int unused; } UART_HandleTypeDef;
typedef struct { int unused; } CAN_HandleTypeDef;
extern UART_HandleTypeDef huart3;
extern CAN_HandleTypeDef hcan1, hcan2;
