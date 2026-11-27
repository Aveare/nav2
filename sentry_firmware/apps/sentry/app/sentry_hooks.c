/**
 * @file    sentry_hooks.c
 * @brief   哨兵姿态任务启动前装配钩子
 * @layer   APP
 * @author  小猫大王
 * @note    NUC 固定走 USB CDC；本钩子不占 USART1，UART1 由 gimbal_init 注册给 VOFA。
 */
#include "sentry_hooks.h"

void sentry_ins_init(void)
{
}
