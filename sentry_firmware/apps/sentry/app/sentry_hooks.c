/**
 * @file    sentry_hooks.c
 * @brief   哨兵姿态任务启动前装配钩子
 * @layer   APP
 * @author  小猫大王
 * @note    AX650 已关闭时本钩子不占 USART1;UART1 由 gimbal_init 注册给 VOFA。
 */
#include "sentry_hooks.h"
#include "config.h"
#include "ax650_uart1.h"
#include "usart.h"

void sentry_ins_init(void)
{
#if AX650_UART1_ENABLE
    ax650_uart1_init(&huart1); // REDFLAG-EXEMPT: Init 句柄注入(AGENTS.md 允许口径)
#endif
}
