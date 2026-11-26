/**
 * @file    quaternion_ekf.h
 * @brief   四元数 EKF 接口与状态输出
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  modified by 小猫大王
* @version V1.2.0
* @date    2022/3/8
 */
#include <stdbool.h>
#ifndef _QUAT_EKF_H
#define _QUAT_EKF_H
#include "kalman_filter.h"

/* boolean type definitions */

typedef struct
{
    uint8_t Initialized;
    kalman_filter_t IMU_QuaternionEKF;
    uint8_t ConvergeFlag;
    uint8_t StableFlag;
    uint64_t ErrorCount;
    uint64_t UpdateCount;

    float q[4];        // 四元数估计值
    float GyroBias[3]; // 陀螺仪零偏估计值

    float Gyro[3];
    float Accel[3];

    float OrientationCosine[3];

    float accLPFcoef;
    float gyro_norm;
    float accl_norm;
    float AdaptiveGainScale;

    float Roll;
    float Pitch;
    float Yaw;

    float YawTotalAngle;

    float Q1; // 四元数更新过程噪声
    float Q2; // 陀螺仪零偏过程噪声
    float R;  // 加速度计量测噪声

    float dt; // 姿态更新周期
    math_matrix_t ChiSquare;
    float ChiSquare_Data[1];      // 卡方检验检测函数
    float ChiSquareTestThreshold; // 卡方检验阈值
    float lambda;                 // 渐消因子

    int16_t YawRoundCount;

    float YawAngleLast;
} ekf_state_t;

extern ekf_state_t ekf_ins;
extern float chiSquare;
extern float ChiSquareTestThreshold;
void ekf_imu_quaternion_ekf_init(float *init_quaternion, float process_noise1, float process_noise2,
                            float measure_noise, float lambda, float lpf);
void ekf_imu_quaternion_ekf_update(float gx, float gy, float gz, float ax, float ay, float az, float dt);

#endif
