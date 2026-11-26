/**
 * @file    robot_task.h
 * @brief   任务接线声明:句柄、启停宏与创建入口
 * @layer   APP
 * @author  小猫大王
* @note    任务实现与创建在 robot_task.c;创建入口仅由 robot.c 的 robot_init() 调用。
* @note    2026-10-29 建立,作者 JIT CR。
 */
#pragma once

#include "main.h"
#include "kernel.h" /* B8:任务句柄/创建原语走 kernel(T05),不再直用 CMSIS-OS */

#include "robot.h"
#include "config.h"
#include "ins_task.h"
#include "motor_task.h"
#include "referee_task.h"
#include "daemon.h"
#include "ht_motor.h"
#include "buzzer.h"

#include "log_port.h"

/* ---------------- 任务启停配置 ----------------
 * 任务接线一律用本处宏启停(1=启用,0=停用),禁止用注释启停:
 * 注释掉的代码不会随宏配置变化,也无法被 grep 统计。
 * 新增任务照此格式添加宏,改动后需重新编译。 */
#if YAW_AUTO_VOFA_USART6_ENABLE
#define ROBOT_TASK_UI_ENABLE 0 // USART6 由自瞄 yaw 调试占用，暂停裁判 UI
#else
#define ROBOT_TASK_UI_ENABLE 1 // 裁判系统 UI 任务:一期仅打通链路,硬件未接时自行等待不阻塞其他任务
#endif

extern kernel_task_t ins_task_handle;
extern kernel_task_t robot_task_handle;
extern kernel_task_t motor_task_handle;
extern kernel_task_t daemon_task_handle;
extern kernel_task_t ui_task_handle;

/**
 * @brief  创建所有持续运行的 FreeRTOS 任务
 * @note   仅由 robot.c 的 robot_init() 调用,且必须在 RTOS 启动前执行;
 *         各任务是否创建由上方 ROBOT_TASK_*_ENABLE 宏决定。
 */
void robot_task_init();
