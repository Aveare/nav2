/**
 * @file    sentry_gimbal_alloc_test.c
 * @brief   哨兵双 yaw 分配纯逻辑主机测试
 * @layer   TESTS
 * @author  小猫大王
 * @note    验证:小误差双 yaw 分配、禁用小 yaw 后大 yaw 独占、跨 ±180 正确折叠、
 *          以及自瞄慢配平 split(大 yaw 快环 + 小 yaw 慢配平)。
 */
#include <stdio.h>
#include <math.h>
#include "gimbal_alloc.h"

static int failures;

static void expect_near(const char *name, float got, float want, float tol)
{
    if (fabsf(got - want) > tol)
    {
        printf("FAIL %s: got=%.4f want=%.4f\n", name, got, want);
        failures++;
    }
    else
        printf("ok   %s: got=%.4f\n", name, got);
}

int main(void)
{
    float small, big;

    /* 1. 误差在限位内:小 yaw 全吃,大 yaw 目标 = 当前世界角(不动) */
    sentry_gimbal_allocate(30.0f, 20.0f, 60.0f, &small, &big);
    expect_near("small in-range", small, 10.0f, 1e-3f);
    expect_near("big hold", big, 20.0f, 1e-3f);

    /* 2. 正误差超限:小 yaw 饱和 +60,大 yaw 承担剩余 40 */
    sentry_gimbal_allocate(160.0f, 20.0f, 60.0f, &small, &big);
    expect_near("small saturated +", small, 60.0f, 1e-3f);
    expect_near("big takes remainder +", big, 100.0f, 1e-3f);

    /* 3. 负误差超限 */
    sentry_gimbal_allocate(-160.0f, 20.0f, 60.0f, &small, &big);
    expect_near("small saturated -", small, -60.0f, 1e-3f);
    expect_near("big takes remainder -", big, -100.0f, 1e-3f);

    /* 4. 跨 ±180:目标 -170,当前 +170 → 误差 +20(走近路) */
    sentry_gimbal_allocate(-170.0f, 170.0f, 60.0f, &small, &big);
    expect_near("wrap small", small, 20.0f, 1e-3f);
    expect_near("wrap big hold", big, 170.0f, 1e-3f);

    /* 5. 跨 ±180 且超限:目标 +170,当前 -170 → 误差 -20;大 yaw 目标展开到最近分支 -180 */
    sentry_gimbal_allocate(170.0f, -170.0f, 10.0f, &small, &big);
    expect_near("wrap saturated small", small, -10.0f, 1e-3f);
    expect_near("wrap big target", big, -180.0f, 1e-3f);

    /* 6. 小 yaw 失能:可用范围为 0,世界目标全部交给大 yaw */
    sentry_gimbal_allocate(30.0f, 20.0f, 0.0f, &small, &big);
    expect_near("small disabled", small, 0.0f, 1e-3f);
    expect_near("big takes full target", big, 30.0f, 1e-3f);

    /* 7. 自瞄慢配平:小 yaw 只拿给定配平量,大 yaw 承担世界目标剩余 */
    sentry_gimbal_split(30.0f, 20.0f, 10.0f, 60.0f, &small, &big);
    expect_near("split small", small, 10.0f, 1e-3f);
    expect_near("split big", big, 20.0f, 1e-3f);

    /* 8. 配平超限:小 yaw 夹到 60,大 yaw 承担 -30 */
    sentry_gimbal_split(30.0f, 20.0f, 100.0f, 60.0f, &small, &big);
    expect_near("split small clamp", small, 60.0f, 1e-3f);
    expect_near("split big remainder", big, -30.0f, 1e-3f);

    /* 9. 小 yaw 失能:split 把世界目标全部交给大 yaw */
    sentry_gimbal_split(30.0f, 20.0f, 0.0f, 0.0f, &small, &big);
    expect_near("split small disabled", small, 0.0f, 1e-3f);
    expect_near("split big full", big, 30.0f, 1e-3f);

    if (failures)
    {
        printf("RESULT: FAIL (%d)\n", failures);
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}
