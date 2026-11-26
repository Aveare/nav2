/**
 * @file    robot_cmd.h
 * @brief   控制命令应用接口:输入源仲裁 + 各执行单元指令发布
 * @layer   APP
 */
#ifndef ROBOT_CMD_H
#define ROBOT_CMD_H

/**
 * @brief  控制命令应用初始化:注册遥控/图传输入,订阅各应用反馈
 * @note   须在 RTOS 启动前调用,由 RobotInit() 统一调度;只能调用一次。
 */
void RobotCMDInit();

/**
 * @brief  控制命令应用任务:裁决唯一控制源,把输入翻译成底盘/云台/发射指令
 * @note   由 RobotTask() 以 200Hz 调用。频率必须高于视觉发送频率,
 *         否则视觉目标更新率会超过指令下行能力。内部持有仲裁状态,不可并发调用。
 */
void RobotCMDTask();

#endif // !ROBOT_CMD_H