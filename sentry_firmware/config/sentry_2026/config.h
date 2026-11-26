/**
 * @file    config.h
 * @brief   哨兵 2026 本车配置伞头:全工程唯一参数入口
 * @layer   CONFIG
 * @author  小猫大王
 * @note    业务代码只 #include "config.h";子头只定义本域参数,跨域检查集中在伞头。
 * @note    本配置为**实车调试起点**:三轴零位/方向/限位/PID 均带 [待实测] 标记,
 *          首次上电必须按 .Doc/spec/参数证据表.md 的标定流程逐项确认后再闭环。
 * @note    CAN 映射(与参考工程 RM_shaobin 一致):
 *          CAN1: 4×M3508 底盘 ESC 1-4 + DM-J4310 大 yaw(ID5/Master0)
 *          CAN2: DM-J4310 pitch(ID5/Master0) + GM6020 小 yaw(ESC1)
 *                + 2×M3508 摩擦轮(ESC1/2) + M2006 拨盘(ESC3)
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
#include "config_autonomy.h"

/* ---------------- 跨域一致性守卫 ----------------
 * 子头只定义自己域内的值;需要读两个域的检查集中在这里,避免子头互相 include。 */

/* USART1 只有一个使用者:哨兵云台 VOFA、底盘 VOFA 与旧 AX650 链路互斥。 */
#if GIMBAL_VOFA_UART1_ENABLE && CHASSIS_VOFA_UART1_ENABLE
#error "UART1 owner conflict: gimbal VOFA and chassis VOFA must be mutually exclusive"
#endif
#if AX650_UART1_ENABLE && (GIMBAL_VOFA_UART1_ENABLE || CHASSIS_VOFA_UART1_ENABLE)
#error "UART1 owner conflict: AX650 and debug VOFA must be mutually exclusive"
#endif

/* 下行接收不能脱离链路总开关。 */
#if AX650_USB_ENABLE && AX650_UART1_ENABLE
#error "AX650 USB CDC and USART1 transports must be mutually exclusive"
#endif

#if AX650_UART1_RX_ENABLE && !AX650_UART1_ENABLE
#error "AX650_UART1_RX_ENABLE=1 需要先开启 AX650_UART1_ENABLE"
#endif

/* 小 yaw 电流模式与电压模式的输出限幅必须匹配协议满量程。 */
#if (SENTRY_SMALL_YAW_ENABLE != 0) && (SENTRY_SMALL_YAW_ENABLE != 1)
#error "SENTRY_SMALL_YAW_ENABLE 只能为 0 或 1"
#endif
#if SENTRY_SMALL_YAW_CURRENT_MODE && (SENTRY_SMALL_YAW_SPEED_MAXOUT > 16384)
#error "小 yaw 电流模式输出限幅不得超过 16384(0x1FE/0x2FE 协议满量程)"
#endif
#if !SENTRY_SMALL_YAW_CURRENT_MODE && (SENTRY_SMALL_YAW_SPEED_MAXOUT > 25000)
#error "小 yaw 电压模式输出限幅不得超过 25000(0x1FF/0x2FF 协议满量程)"
#endif
