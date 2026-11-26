/**
 * @file    cmd_watchdog.h
 * @brief   哨兵命令断流看门狗:纯逻辑,主机可测
 * @layer   APP
 * @author  小猫大王
 * @note    与步兵 cmd_watchdog 同语义;哨兵只使用 200Hz 任务调用。
 */
#ifndef SENTRY_CMD_WATCHDOG_H
#define SENTRY_CMD_WATCHDOG_H

#include "stdint.h"

typedef struct
{
    float last_ms; // 最近一次收到命令的时刻,ms
    uint8_t fed;   // 是否收到过命令;上电尚未收到时按断流处理
} cmd_watchdog_t;

static inline void cmd_watchdog_feed(cmd_watchdog_t *w, float now_ms)
{
    w->last_ms = now_ms;
    w->fed = 1;
}

static inline uint8_t cmd_watchdog_expired(const cmd_watchdog_t *w, float now_ms, float timeout_ms)
{
    float dt;
    if (!w->fed)
        return 1;
    dt = now_ms - w->last_ms;
    if (dt < 0.0f)
        return 0;
    return (dt > timeout_ms) ? 1 : 0;
}

#endif // SENTRY_CMD_WATCHDOG_H
