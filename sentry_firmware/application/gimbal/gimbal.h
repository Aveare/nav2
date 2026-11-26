/**
 * @file    gimbal.h
 * @brief   云台应用接口:初始化与任务入口
 * @layer   APP
 */
#ifndef GIMBAL_H
#define GIMBAL_H
#include <stdint.h>

/**
 * @brief  云台应用初始化:注册 yaw/pitch 电机与 IMU,订阅命令、发布反馈
 * @note   须在 RTOS 启动前调用,由 RobotInit() 统一调度;只能调用一次。
 */
void GimbalInit();

/**
 * @brief  云台应用任务:角度环、速度环、前馈与解锁回中状态机
 * @note   由 RobotTask() 以 200Hz 调用;内部持有静态状态,不可并发调用。
 */
void GimbalTask();

/**
 * @brief  原子快照大 yaw 相对底盘正前方转角(rad, 逆时针为正)与 CAN 采样时间(us)
 */
void GimbalGetBigYawFeedback(float *rel_rad, uint64_t *sample_us, uint8_t *online);


#endif // GIMBAL_H
