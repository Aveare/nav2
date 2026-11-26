/**
 * @file    ins_task.h
 * @brief   主机测试桩:替代 ins_task.h 的最小实现,仅供 PC 端编译测试
 * @author  JIT CR
 * @date    2026-10-29
 *
 * attitude_t 字段与 modules/imu/ins_task.h 的真实定义保持一致
 * (源码新增字段时此处同步,否则测试编译即失败——这是有意的守卫)。
 */
#pragma once
typedef struct
{
    float Gyro[3];
    float Accel[3];
    float Roll;
    float Pitch;
    float Yaw;
    float YawTotalAngle;
} attitude_t;
