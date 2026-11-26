/**
 * @file    ins_task.h
 * @brief   姿态接口:ins_state_t 与初始化
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  annotation and modification by NeoZeng
 * @author  modified by 小猫大王
* @version V2.0.0
* @date    2022/2/23
 */
#ifndef __INS_TASK_H
#define __INS_TASK_H

#include "stdint.h"
#include "bmi088_driver.h"
#include "quaternion_ekf.h"

#define IMU_AXIS_X 0
#define IMU_AXIS_Y 1
#define IMU_AXIS_Z 2

#define INS_TASK_PERIOD 1

typedef struct
{
    float Gyro[3];  // 角速度
    float Accel[3]; // 加速度
    // 还需要增加角速度数据
    float Roll;
    float Pitch;
    float Yaw;
    float YawTotalAngle;
} attitude_t; // 最终解算得到的角度,以及yaw转动的总角度(方便多圈控制)

typedef struct
{
    float q[4]; // 四元数估计值

    float MotionAccel_b[3]; // 机体坐标加速度
    float MotionAccel_n[3]; // 绝对系加速度

    float AccelLPF; // 加速度低通滤波系数

    // bodyframe在绝对系的向量表示
    float xn[3];
    float yn[3];
    float zn[3];

    // 加速度在机体系和XY两轴的夹角
    // float atanxz;
    // float atanyz;

    // IMU量测值
    float Gyro[3];  // 角速度
    float Accel[3]; // 加速度
    // 位姿
    float Roll;
    float Pitch;
    float Yaw;
    float YawTotalAngle;

    uint8_t init;
} ins_state_t;

/* 用于修正安装误差的参数 */
typedef struct
{
    uint8_t flag;

    float scale[3];

    float Yaw;
    float Pitch;
    float Roll;
} ins_imu_param_t;

/**
 * @brief 初始化惯导解算系统
 *
 */
attitude_t *ins_init(void);

/**
 * @brief 此函数放入实时系统中,以1kHz频率运行
 *        p.s. kernel_task_delay(1);
 *
 */
void ins_task(void);

/**
 * @brief 四元数更新函数,即实现dq/dt=0.5Ωq
 *
 * @param q  四元数
 * @param gx
 * @param gy
 * @param gz
 * @param dt 距离上次调用的时间间隔
 */
void ins_quaternion_update(float *q, float gx, float gy, float gz, float dt);

/**
 * @brief 四元数转换成欧拉角 ZYX
 *
 * @param q
 * @param Yaw
 * @param Pitch
 * @param Roll
 */
void ins_quaternion_to_euler(float *q, float *Yaw, float *Pitch, float *Roll);

/**
 * @brief ZYX欧拉角转换为四元数
 *
 * @param Yaw
 * @param Pitch
 * @param Roll
 * @param q
 */
void ins_euler_to_quaternion(float Yaw, float Pitch, float Roll, float *q);

/**
 * @brief 机体系到惯性系的变换函数
 *
 * @param vecBF body frame
 * @param vecEF earth frame
 * @param q
 */
void ins_body_to_earth(const float *vecBF, float *vecEF, float *q);

/**
 * @brief 惯性系转换到机体系
 *
 * @param vecEF
 * @param vecBF
 * @param q
 */
void ins_earth_to_body(const float *vecEF, float *vecBF, float *q);

#endif
