/**
 * @file    bsp_dwt.h
 * @brief   主机测试桩:替代 bsp_dwt.h 的最小实现,仅供 PC 端编译测试
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
#include <stdint.h>
float dwt_get_delta_t(uint32_t *count);
uint32_t dwt_get_timeline_ms(void);
