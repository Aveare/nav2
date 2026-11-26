/**
 * @file    ins_task.h
 * @brief   主机测试桩:替代 ins_task.h 的最小实现,仅供 PC 端编译测试
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-29
 *
 * attitude_t 字段与 components/imu/ins_task.h 的真实定义保持一致
 * (源码新增字段时此处同步,否则测试编译即失败——这是有意的守卫)。
 */
#pragma once
#define IMU_AXIS_X 0
#define IMU_AXIS_Y 1
#define IMU_AXIS_Z 2

typedef struct
{
    float Gyro[3];
    float Accel[3];
    float Roll;
    float Pitch;
    float Yaw;
    float YawTotalAngle;
} attitude_t;

/* 真实 ins_task.h 的实现返回 ins_state_t *;主机测试只按 attitude_t 布局使用,
 * 桩里直接返回同布局指针,避免隐式 int 返回在 64 位主机上截断。 */
attitude_t *ins_init(void);
