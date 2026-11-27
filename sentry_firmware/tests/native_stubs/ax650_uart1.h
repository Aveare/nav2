/**
 * @file    ax650_uart1.h
 * @brief   CMD 主机测试用 AX650 下行快照桩
 * @layer   TESTS
 */
#pragma once
#include <stdint.h>
#include "ax650_pose.h"

typedef struct
{
    ax650_command_t command;
    uint32_t command_rx_ms;
    uint32_t command_count;
    uint8_t command_valid;
} ax650_uart1_state_t;

void ax650_uart1_set_mode(uint8_t mode);
void ax650_uart1_get_state(ax650_uart1_state_t *out);
