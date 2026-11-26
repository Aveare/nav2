/**
 * @file    config.h
 * @brief   步兵 2026 本车配置伞头:全工程唯一参数入口
 * @layer   CONFIG
 * @author  小猫大王
 */
#pragma once

#include "config_board.h"
#include "config_chassis.h"
#include "config_gimbal.h"
#include "config_ax650.h"
#include "config_shoot.h"
#include "config_heat.h"
#include "config_kbm.h"
#include "config_remote.h"
#include "config_protect.h"

/* ---------------- 跨域一致性守卫 ----------------
 * 子头只定义自己域内的值;需要读两个域的检查集中在这里,避免子头互相 include。 */

/* USART1 只有一个使用者,由编译期宏二选一:
 *   AX650_UART1_ENABLE=1, CHASSIS_VOFA_UART1_ENABLE=0 -> 自瞄
 *   AX650_UART1_ENABLE=0, CHASSIS_VOFA_UART1_ENABLE=1 -> VOFA 调试
 * 不允许两者同时开;USART6 固定给裁判系统,不能作为调试口。 */
#if defined(CHASSIS_VOFA_UART1_ENABLE) && CHASSIS_VOFA_UART1_ENABLE && AX650_UART1_ENABLE
#error "UART1 owner conflict: AX650 and debug VOFA must be mutually exclusive"
#endif

/* 下行接收不能脱离链路总开关。 */
#if AX650_UART1_RX_ENABLE && !AX650_UART1_ENABLE
#error "AX650_UART1_RX_ENABLE=1 需要先开启 AX650_UART1_ENABLE"
#endif
