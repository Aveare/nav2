/**
 * @file    gimbal.h
 * @brief   云台应用接口:初始化/任务与标定辅助
 * @layer   APP
 * @author  小猫大王
 */
#ifndef GIMBAL_H
#define GIMBAL_H

#include <stdint.h> // int8_t:本头文件在主机测试里也会被单独包含

/**
 * @brief  云台应用初始化:注册 yaw/pitch 电机与 IMU,订阅命令、发布反馈
 * @note   须在 RTOS 启动前调用,由 robot_init() 统一调度;只能调用一次。
 */
void gimbal_init();

/**
 * @brief  云台应用任务:角度环、速度环、前馈与解锁回中状态机
 * @note   由 robot_task() 以 200Hz 调用;内部持有静态状态,不可并发调用。
 */
void gimbal_task();

/**
 * @brief  当前 pitch 转子单圈编码器角(度,0~360),供零位标定读取
 */
float gimbal_get_pitch_ecd_deg(void);

/**
 * @brief  当前 pitch 转子累计角(度,解包后)
 */
float gimbal_get_pitch_total_angle(void);

/**
 * @brief  ECD 回正目标(IMU 角度域):由 Flash 标定的机械水平零位解算
 * @return 目标 IMU Pitch(度);未标定/解算异常时返回 0(= 旧的 IMU 水平回正)
 */
float gimbal_pitch_recenter_target(void);

/**
 * @brief  把当前 pitch 姿态保存为机械水平零位(写 Flash,须在零力状态下调用)
 * @return 0 成功,-1 失败
 */
int8_t gimbal_save_pitch_horizon_zero(void);

#endif // GIMBAL_H
