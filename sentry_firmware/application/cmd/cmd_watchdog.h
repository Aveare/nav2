/**
 * @file    cmd_watchdog.h
 * @brief   CMD 命令断流看门狗:消费端判断"多久没收到新命令",超时由调用方切安全态
 * @layer   APP
 * @note    纯逻辑实现,不依赖 BSP / RTOS / 消息层头文件,可在主机单测
 *          (tests/cmd_watchdog_test.c)。
 * @note    2026-10-27 建立,作者 JIT CR。
 */
#ifndef CMD_WATCHDOG_H
#define CMD_WATCHDOG_H

#include "stdint.h"

typedef struct
{
    float last_ms; // 最近一次收到命令的时刻,ms
    uint8_t fed;   // 是否收到过命令;上电尚未收到时按断流处理
} CmdWatchdog_t;

/**
 * @brief  收到一条新命令时调用,刷新时刻并置已喂狗标志
 */
static inline void CmdWatchdogFeed(CmdWatchdog_t *w, float now_ms)
{
    w->last_ms = now_ms;
    w->fed = 1;
}

/**
 * @brief  判断命令是否已断流
 * @param  timeout_ms 允许的最大静默时长,ms
 * @return 1 = 已断流(调用方应切安全态),0 = 正常
 * @note   从未收到过命令(fed = 0)即返回断流:上电期按安全侧处理。
 * @note   时间回退(now < last)按"未超时"处理,避免时间源异常时误切零力。
 * @note   严格大于 timeout_ms 才判定断流,边界行为可预测。
 */
static inline uint8_t CmdWatchdogExpired(const CmdWatchdog_t *w, float now_ms, float timeout_ms)
{
    float dt;
    if (!w->fed)
        return 1;
    dt = now_ms - w->last_ms;
    if (dt < 0.0f)
        return 0;
    return (dt > timeout_ms) ? 1 : 0;
}

#endif // !CMD_WATCHDOG_H
