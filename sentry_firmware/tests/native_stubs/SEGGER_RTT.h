/**
 * @file    SEGGER_RTT.h
 * @brief   主机测试桩(B8):ports/stm32f4/log_port.h 依赖的最小 RTT 接口,
 * @layer   TESTS
 *          CAN 过滤器金标准等主机用例经 -Itests/native_stubs 取得。
 */
#pragma once
#include <stdint.h>
#define RTT_CTRL_RESET "[0m"
#define RTT_CTRL_CLEAR "[2J"
#define RTT_CTRL_TEXT_BRIGHT_GREEN "[1;32m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW "[1;33m"
#define RTT_CTRL_TEXT_BRIGHT_RED "[1;31m"
unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char *s);
