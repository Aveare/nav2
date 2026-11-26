/**
 * @file    config_protect.h
 * @brief   哨兵保护参数:看门狗/解锁死区/拨轮急停
 * @layer   CONFIG
 * @author  小猫大王
 */
#pragma once

#define CMD_WATCHDOG_TIMEOUT_MS 200.0f /* [理论] 命令断流看门狗超时,ms */
#define SHOOT_DIAL_STOP_THRESHOLD 300  /* [实测-沿用] 拨轮急停阈值,原始刻度 */
#define PROTECT_AUTH_NEUTRAL_DEADBAND 10 /* [实测-沿用] 解锁全回中死区,原始刻度 */
#define SENTRY_AUTH_RC_LOST_STOP 1
/* [配置] 1=遥控失联立即整机零力(手动调试默认);0=保留自主输入(自主模式启用后改 0)。 */
