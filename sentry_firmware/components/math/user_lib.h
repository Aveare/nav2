/**
 * @file    user_lib.h
 * @brief   数学工具接口(含 math_matrix_t)
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  modified by 小猫大王
* @version V1.0.0
* @date    2021/2/18
 */
#ifndef _USER_LIB_H
#define _USER_LIB_H

#include "stdint.h"
#include <stdbool.h>
#include <stdlib.h>
#include "stm32f407xx.h"
#include "arm_math.h"
#include "general_def.h" /* FW_PI 等常数唯一来源(framework) */

#ifndef MATH_MALLOC
#define MATH_MALLOC malloc
#endif

#define MATH_SIN(x) (arm_sin_f32(x))
#define MATH_COS(x) (arm_cos_f32(x))

typedef arm_matrix_instance_f32 math_matrix_t;
// 若运算速度不够,可以使用q31代替f32,但是精度会降低
#define MATH_MAT_ADD       arm_mat_add_f32
#define MATH_MAT_SUB  arm_mat_sub_f32
#define MATH_MAT_MUL  arm_mat_mult_f32
#define MATH_MAT_TRANSPOSE arm_mat_trans_f32
#define MATH_MAT_INVERSE   arm_mat_inverse_f32


#define MATH_ANGLE_WRAP_360(val, angle)                                                                \
    do                                                                                             \
    {                                                                                              \
        (val) = (angle) - (int)(angle);                                                            \
        (val) += (int)(angle) % 360;                                                               \
    } while (0)

#define MATH_ANGLE_WRAP_180(val)                                                                \
    do                                                                                             \
    {                                                                                              \
        if ((val) > 180)                                                                           \
            (val) -= 360;                                                                          \
    } while (0)

#define MATH_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MATH_MAX(a, b) ((a) > (b) ? (a) : (b))

// 快速开方
float math_sqrt(float x);
// 限幅函数
float math_float_clamp(float Value, float minValue, float maxValue);

int math_float_round(float raw);

float *math_norm3d(float *v);

void math_cross3d(float *v1, float *v2, float *res);

float math_dot3d(float *v1, float *v2);

float math_average_filter(float new_data, float *buf, uint8_t len);

#endif
