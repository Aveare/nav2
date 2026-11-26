/**
 * @file    kernel.h
 * @brief   主机测试桩:framework kernel 时间原语(ax650 套件只用到 tick_ms)
 * @author  小猫大王
 * @layer   TESTS
 * @note    完整 kernel API 由 tests/fwk_posix 提供;此处按需最小化。
 */
#pragma once

#include <stdint.h>

uint32_t kernel_tick_ms(void);
