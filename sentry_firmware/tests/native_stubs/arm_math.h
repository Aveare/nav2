/**
 * @file    arm_math.h
 * @brief   主机测试桩:替代 arm_math.h 的最小实现,仅供 PC 端编译测试
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
#include <math.h>
typedef struct
{
    uint16_t numRows, numCols;
    float *pData;
} arm_matrix_instance_f32;

/* 主机内联实现:与 CMSIS-DSP f32 语义一致(线性精度足够,金标准确定性由 harness 保证) */
static inline float arm_sin_f32(float x)
{
    return sinf(x);
}
static inline float arm_cos_f32(float x)
{
    return cosf(x);
}
static inline void arm_mat_init_f32(arm_matrix_instance_f32 *m, uint16_t r, uint16_t c, float *d)
{
    m->numRows = r;
    m->numCols = c;
    m->pData = d;
}
