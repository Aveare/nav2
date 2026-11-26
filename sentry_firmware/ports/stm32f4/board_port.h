/**
 * @file    board_port.h
 * @brief   板级初始化:DWT + 日志(内联)
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note    dwt_init 由写死 168 改为 SystemCoreClock 派生(方案 §3.2);
*          168MHz 车况下数值等价(零行为)。只在 robot_init() 触发一次。
 */
#pragma once

#include "dwt_port.h"
#include "log_port.h"

static inline void board_init(void)
{
    dwt_init((uint32_t)(SystemCoreClock / 1000000u));
    log_init();
}
