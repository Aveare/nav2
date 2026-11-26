/**
 * @file    SEGGER_RTT.h
 * @brief   主机测试桩(B8):log_port.h 的同目录遮蔽绕过 include 路径桩,
 * @layer   TESTS
 *          故 ax650 主机测试需要本文件提供所需宏/声明。
 */
#pragma once
#include <stdint.h>
#define RTT_CTRL_RESET "[0m"
#define RTT_CTRL_CLEAR "[2J"
#define RTT_CTRL_TEXT_BRIGHT_GREEN "[1;32m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW "[1;33m"
#define RTT_CTRL_TEXT_BRIGHT_RED "[1;31m"
unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char *s);
