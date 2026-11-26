/**
 * @file    gimbal_internal.h
 * @brief   云台内部状态共享接口
 * @layer   APP
 * @author  小猫大王
* @note    2026-11-20 拆分时建立。共享状态仍定义在 gimbal.c,这里只做 extern 声明。
 */
#ifndef GIMBAL_INTERNAL_H
#define GIMBAL_INTERNAL_H

#include "robot_def.h"
#include "dji_motor.h"
#include "ins_task.h"

extern dji_motor_t *pitch_motor; // yaw 电机仍由 gimbal.c 私有
extern attitude_t *gimbal_imu_data;    // 云台 IMU(与 chassis 共用同一 ins_state 实例)

#endif // GIMBAL_INTERNAL_H
