/**
 * @file    kernel.h
 * @brief   vision_link 主机测试用内核桩
 * @layer   TESTS
 */
#pragma once
#include <stdint.h>

uint32_t kernel_tick_ms(void);

/* 单线程主机测试:临界区为空实现 */
static inline uint32_t kernel_enter_critical(void) { return 0u; }
static inline void kernel_exit_critical(uint32_t saved) { (void)saved; }
