/**
 * @file    robot_task.h
 * @brief   FreeRTOS 任务接线:启停宏、任务句柄与创建入口
 * @layer   APP
 * @note    任务实现与创建在 robot_task.c;创建入口仅由 robot.c 的 RobotInit() 调用。
 * @note    2026-10-29 建立,作者 JIT CR。
 */
#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "robot.h"
#include "ins_task.h"
#include "motor_task.h"
#include "referee_task.h"
#include "master_process.h"
#include "daemon.h"
#include "HT04.h"
#include "buzzer.h"

#include "bsp_log.h"

/* ---------------- 任务启停配置 ----------------
 * 任务接线一律用本处宏启停(1=启用,0=停用),禁止用注释启停:
 * 注释掉的代码不会随宏配置变化,也无法被 grep 统计。
 * 新增任务照此格式添加宏,改动后需重新编译。 */
#define TASK_UI_ENABLE 1 // 裁判系统 UI 任务:一期仅打通链路,硬件未接时自行等待不阻塞其他任务(DEV-20261030-12)

extern osThreadId insTaskHandle;
extern osThreadId robotTaskHandle;
extern osThreadId motorTaskHandle;
extern osThreadId daemonTaskHandle;
extern osThreadId uiTaskHandle;

/**
 * @brief  创建所有持续运行的 FreeRTOS 任务
 * @note   仅由 robot.c 的 RobotInit() 调用,且必须在 RTOS 启动前执行;
 *         各任务是否创建由上方 TASK_*_ENABLE 宏决定。
 */
void OSTaskInit();
