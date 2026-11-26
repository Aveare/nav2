/**
 * @file    dwt_port.h
 * @brief   主机测试桩:DWT 时基(与 tests/ax650_uart1_test.c 的注入 tick 同源)
 * @author  小猫大王
 * @layer   TESTS
 */
#pragma once

float dwt_get_timeline_ms(void);
