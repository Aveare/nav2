/**
 * @file    sentry_hooks.c
 * @brief   哨兵装配钩子实现:AX650 链路接入 USART1
 * @layer   APP
 * @author  小猫大王
 * @note    USART1 物理口由 config 的 AX650_UART1_ENABLE 独占;底盘 VOFA 必须先置 0。
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
