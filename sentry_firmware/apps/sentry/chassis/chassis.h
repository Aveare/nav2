/**
 * @file    chassis.h
 * @brief   哨兵底盘应用接口:初始化与任务入口
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_CHASSIS_H
#define SENTRY_CHASSIS_H

/**
 * @brief 底盘初始化:注册四轮 M3508、裁判系统接收、订阅命令
 * @note 须在 RTOS 启动前调用,由 assembly 统一调度;只能调用一次。
 */
void chassis_init(void);

/**
 * @brief 底盘任务:模式解算 → 跟随律 → 运动学 → 电机参考
 * @note 由 robot_task() 以 200Hz 调用;内部持有斜坡/IMU 状态,不可并发调用。
 */
void chassis_task(void);

#endif // SENTRY_CHASSIS_H
