/**
 * @file    sentry_hooks.h
 * @brief   哨兵装配钩子:姿态任务启动前的一次性外设初始化
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_HOOKS_H
#define SENTRY_HOOKS_H

/**
 * @brief 姿态任务启动前钩子:AX650 USART1 链路初始化
 * @note  由 app_shell 的 INS 任务在 ins_init() 之后、首拍 ins_task() 之前调用。
 */
void sentry_ins_init(void);

#endif // SENTRY_HOOKS_H
