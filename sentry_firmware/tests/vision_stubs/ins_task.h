/**
 * @file    ins_task.h
 * @brief   vision_link 主机测试用 INS 桩
 * @layer   TESTS
 */
#pragma once
#include <stdint.h>

typedef struct
{
    uint8_t Initialized;
    uint64_t UpdateCount;
    float q[4];
} ekf_state_t;

extern ekf_state_t ekf_ins;
