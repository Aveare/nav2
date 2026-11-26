/**
 * @file    shoot.h
 * @brief   发射应用接口:初始化与任务入口
 * @layer   APP
 */
#ifndef SHOOT_H
#define SHOOT_H

/**
 * @brief  发射应用初始化:注册双摩擦轮与拨盘电机,订阅命令、发布反馈
 * @note   须在 RTOS 启动前调用,由 RobotInit() 统一调度;只能调用一次。
 */
void ShootInit();

/**
 * @brief  发射应用任务:摩擦轮速度环、拨盘连发/单发/三发状态机、堵转保护
 * @note   由 RobotTask() 以 200Hz 调用;内部持有静态状态,不可并发调用。
 */
void ShootTask();

#endif // SHOOT_H