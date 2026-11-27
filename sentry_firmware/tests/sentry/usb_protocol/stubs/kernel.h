/**
 * @file    kernel.h
 * @brief   哨兵 USB 双协议主机测试内核桩
 * @layer   TESTS
 * @author  小猫大王
 */
#pragma once

#include <stdint.h>

uint32_t kernel_tick_ms(void);
uint32_t kernel_enter_critical(void);
void kernel_exit_critical(uint32_t saved);
