/**
 * @file    cmd_watchdog_test.c
 * @brief   主机测试:CMD 命令断流看门狗的边界与恢复行为(不依赖硬件)
 * @author  JIT CR
 * @date    2026-10-27
 *
 * 覆盖点:
 *  1. 上电未收到命令 → 视为断流(安全侧)
 *  2. 边界:dt == timeout 不判超时,dt > timeout 才判(严格大于)
 *  3. 收到新命令立即恢复
 *  4. 时间回退(复位/时间源异常)不误判为断流
 *  5. 模拟 200Hz 正常帧流 1s 不误触发;断流 200ms 后触发;恢复后立刻解除
 */
#include <stdio.h>
#include <stdint.h>

#include "cmd_watchdog.h"

static int g_fail = 0;

static void check(const char *name, int got, int want)
{
    if (got != want)
    {
        printf("  [FAIL] %-52s got=%d want=%d\n", name, got, want);
        g_fail++;
    }
    else
    {
        printf("  [ ok ] %-52s = %d\n", name, got);
    }
}

int main(void)
{
    CmdWatchdog_t wd = {0};
    const float T = 200.0f;

    printf("[1] 初始状态\n");
    check("从未收到命令 -> 断流", CmdWatchdogExpired(&wd, 0.0f, T), 1);
    check("从未收到命令(now 很大) -> 断流", CmdWatchdogExpired(&wd, 99999.0f, T), 1);

    printf("[2] 喂养后的边界\n");
    CmdWatchdogFeed(&wd, 1000.0f);
    check("dt=0   -> 正常", CmdWatchdogExpired(&wd, 1000.0f, T), 0);
    check("dt=199 -> 正常", CmdWatchdogExpired(&wd, 1199.0f, T), 0);
    check("dt=200 恰好等于阈值 -> 正常", CmdWatchdogExpired(&wd, 1200.0f, T), 0);
    check("dt=200.5 -> 断流", CmdWatchdogExpired(&wd, 1200.5f, T), 1);
    check("dt=1000 -> 断流", CmdWatchdogExpired(&wd, 2000.0f, T), 1);

    printf("[3] 恢复\n");
    CmdWatchdogFeed(&wd, 3000.0f);
    check("重新喂养后 dt=0 -> 正常", CmdWatchdogExpired(&wd, 3000.0f, T), 0);
    check("重新喂养后 dt=150 -> 正常", CmdWatchdogExpired(&wd, 3150.0f, T), 0);

    printf("[4] 时间回退\n");
    CmdWatchdogFeed(&wd, 5000.0f);
    check("now 回退到 4000 -> 不误判断流", CmdWatchdogExpired(&wd, 4000.0f, T), 0);

    printf("[5] 模拟 200Hz 帧流 1s(5ms/帧)\n");
    {
        CmdWatchdog_t sim = {0};
        int expired_cnt = 0;
        float t;
        for (t = 0.0f; t <= 1000.0f; t += 5.0f)
        {
            CmdWatchdogFeed(&sim, t); /* 每周期都能收到新命令 */
            if (CmdWatchdogExpired(&sim, t, T))
                expired_cnt++;
        }
        check("正常帧流期间断流次数 == 0", expired_cnt, 0);
    }

    printf("[6] 模拟断流:喂到 1000ms 后停发\n");
    {
        CmdWatchdog_t sim = {0};
        float t;
        CmdWatchdogFeed(&sim, 1000.0f);
        /* 停发后按 5ms 步进推进时间 */
        for (t = 1005.0f; t <= 1195.0f; t += 5.0f)
            ;
        check("停发 195ms -> 仍正常", CmdWatchdogExpired(&sim, 1195.0f, T), 0);
        check("停发 205ms -> 断流", CmdWatchdogExpired(&sim, 1205.0f, T), 1);
        CmdWatchdogFeed(&sim, 1210.0f);
        check("恢复帧流 -> 立刻正常", CmdWatchdogExpired(&sim, 1210.0f, T), 0);
    }

    printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASS", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
