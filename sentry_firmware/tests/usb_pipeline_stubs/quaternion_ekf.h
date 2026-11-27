/**
 * @file    quaternion_ekf.h
 * @brief   哨兵 USB 管线测试 INS 状态桩
 * @layer   TESTS
 * @author  小猫大王
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
