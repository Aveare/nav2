/**
 * @file    kalman_filter.h
 * @brief   卡尔曼滤波接口
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  modified by 小猫大王
* @version V1.2.2
* @date    2022/1/8
 */
#ifndef __KALMAN_FILTER_H
#define __KALMAN_FILTER_H

// cortex-m4 DSP lib
/*
#define __CC_ARM    // Keil
#define ARM_MATH_CM4
#define ARM_MATH_MATRIX_CHECK
#define ARM_MATH_ROUNDING
#define ARM_MATH_DSP    // define in arm_math.h
*/

#include "stm32f407xx.h"
#include "arm_math.h"
// #include "dsp/matrix_functions.h"
#include "math.h"
#include "stdint.h"
#include "stdlib.h"

/* 组件层不探测 RTOS 头;矩阵分配固定走标准 malloc,保持历史行为。 */
#ifndef MATH_MALLOC
#define MATH_MALLOC malloc
#endif

/* 矩阵类型:与 user_lib.h 同名同定义;不 include user_lib.h 以保持 MATH_MALLOC=malloc
 * 的历史选择(DEV-20261122-25)。 */
typedef arm_matrix_instance_f32 math_matrix_t;

typedef struct kf_t
{
    float *FilteredValue;
    float *MeasuredVector;
    float *ControlVector;

    uint8_t xhatSize;
    uint8_t uSize;
    uint8_t zSize;

    uint8_t UseAutoAdjustment;
    uint8_t MeasurementValidNum;

    uint8_t *MeasurementMap;      // 量测与状态的关系 how measurement relates to the state
    float *MeasurementDegree;     // 测量值对应H矩阵元素值 elements of each measurement in H
    float *MatR_DiagonalElements; // 量测方差 variance for each measurement
    float *StateMinVariance;      // 最小方差 避免方差过度收敛 suppress filter excessive convergence
    uint8_t *temp;

    // 配合用户定义函数使用,作为标志位用于判断是否要跳过标准KF中五个环节中的任意一个
    uint8_t SkipEq1, SkipEq2, SkipEq3, SkipEq4, SkipEq5;

    // definiion of struct mat: rows & cols & pointer to vars
    math_matrix_t xhat;      // x(k|k)
    math_matrix_t xhatminus; // x(k|k-1)
    math_matrix_t u;         // control vector u
    math_matrix_t z;         // measurement vector z
    math_matrix_t P;         // covariance matrix P(k|k)
    math_matrix_t Pminus;    // covariance matrix P(k|k-1)
    math_matrix_t F, FT;     // state transition matrix F FT
    math_matrix_t B;         // control matrix B
    math_matrix_t H, HT;     // measurement matrix H
    math_matrix_t Q;         // process noise covariance matrix Q
    math_matrix_t R;         // measurement noise covariance matrix R
    math_matrix_t K;         // kalman gain  K
    math_matrix_t S, temp_matrix, temp_matrix1, temp_vector, temp_vector1;

    int8_t MatStatus;

    // 用户定义函数,可以替换或扩展基准KF的功能
    void (*User_Func0_f)(struct kf_t *kf);
    void (*User_Func1_f)(struct kf_t *kf);
    void (*User_Func2_f)(struct kf_t *kf);
    void (*User_Func3_f)(struct kf_t *kf);
    void (*User_Func4_f)(struct kf_t *kf);
    void (*User_Func5_f)(struct kf_t *kf);
    void (*User_Func6_f)(struct kf_t *kf);

    // 矩阵存储空间指针
    float *xhat_data, *xhatminus_data;
    float *u_data;
    float *z_data;
    float *P_data, *Pminus_data;
    float *F_data, *FT_data;
    float *B_data;
    float *H_data, *HT_data;
    float *Q_data;
    float *R_data;
    float *K_data;
    float *S_data, *temp_matrix_data, *temp_matrix_data1, *temp_vector_data, *temp_vector_data1;
} kalman_filter_t;

extern uint16_t sizeof_float, sizeof_double;

void kalman_filter_init(kalman_filter_t *kf, uint8_t xhatSize, uint8_t uSize, uint8_t zSize);
void kalman_filter_measure(kalman_filter_t *kf);
void kalman_filter_xhat_minus_update(kalman_filter_t *kf);
void kalman_filter_pminus_update(kalman_filter_t *kf);
void kalman_filter_set_k(kalman_filter_t *kf);
void kalman_filter_xhat_update(kalman_filter_t *kf);
void kalman_filter_p_update(kalman_filter_t *kf);
float *kalman_filter_update(kalman_filter_t *kf);

#endif //__KALMAN_FILTER_H
