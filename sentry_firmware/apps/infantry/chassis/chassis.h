/**
 * @file    chassis.h
 * @brief   底盘应用接口:初始化与任务入口
 * @layer   APP
 * @author  小猫大王
 */
#ifndef CHASSIS_H
#define CHASSIS_H

/**
 * @brief  底盘应用初始化:注册四个轮电机、订阅命令、发布反馈
 * @note   须在 RTOS 启动前调用,由 robot_init() 统一调度;只能调用一次。
 */
void chassis_init();

/**
 * @brief  底盘应用任务:取命令 → 模式解算 → 运动学 → 电机参考
 * @note   由 robot_task() 以 200Hz 调用;内部持有静态状态,不可并发或多处调用。
 */
void chassis_task();

#endif // CHASSIS_H
