/**
 * @file    shoot.h
 * @brief   哨兵发射应用接口:初始化与任务入口
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_SHOOT_H
#define SENTRY_SHOOT_H

/**
 * @brief 发射初始化:注册双摩擦轮/拨盘,初始化热量守卫,订阅命令
 * @note 须在 RTOS 启动前调用,由 assembly 统一调度;只能调用一次。
 */
void shoot_init(void);

/**
 * @brief 发射任务:摩擦轮/拨盘、单发三发连发状态机、热量闸门与堵转保护
 * @note 由 robot_task() 以 200Hz 调用;内部持有待发队列/堵转状态,不可并发调用。
 */
void shoot_task(void);

#endif // SENTRY_SHOOT_H
