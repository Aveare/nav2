/**
 * @file    robot.h
 * @brief   整车装配入口:按板型初始化各应用并串联任务
 * @layer   APP
 * @note    通过 robot_def.h 的板型宏(ONE_BOARD / CHASSIS_BOARD / GIMBAL_BOARD)
 *          适配单板与双板配置;换机器人型号只改 config 与板型宏,本文件不动。
 */
#ifndef ROBOT_H
#define ROBOT_H

/**
 * @brief  整车初始化:先 BSP,再各应用 Init,最后创建任务
 * @note   须在 RTOS 启动前调用,是唯一需要放进 main 的初始化函数。
 *         内部会关中断,依赖中断的延时必须改用 DWT_Delay()。
 */
void RobotInit();

/**
 * @brief  整车任务:按顺序调用 cmd / gimbal / shoot / chassis 各应用任务
 * @note   由 RTOS 以 200Hz 调度。顺序有意义:cmd 先发布指令,执行单元当拍消费。
 */
void RobotTask();

#endif