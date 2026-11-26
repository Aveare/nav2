/**
 * @file    profile_compile_sentry.c
 * @brief   B11 三骨架编译矩阵:sentry profile 锚点检查(sim 目标)
 * @author  小猫大王
 * @layer   TESTS
 */
#include <stdio.h>

#include "profile.h"

#ifndef PROFILE_ROBOT_SENTRY
#error "sentry profile.h 未定义 PROFILE_ROBOT_SENTRY"
#endif
#if PROFILE_ROBOT_SENTRY != 1
#error "PROFILE_ROBOT_SENTRY 必须为 1"
#endif
#ifndef PROFILE_AUTO_OPERATION
#error "sentry profile.h 缺自动运行能力锚点"
#endif

int main(void)
{
    if (SENTRY_BIG_YAW_OUTPUT_ENABLE != 1)
    {
        puts("FAIL: migrated sentry big-yaw output must be enabled");
        return 1;
    }
    if (SENTRY_BIG_YAW_TORQUE_NM > SENTRY_BIG_YAW_TORQUE_RANGE_NM ||
        SENTRY_BIG_YAW_INTEGRAL_NM > SENTRY_BIG_YAW_TORQUE_NM ||
        SENTRY_BIG_YAW_SPEED_KP <= 0.0f || SENTRY_BIG_YAW_SPEED_KI < 0.0f)
    {
        puts("FAIL: sentry big-yaw torque limits");
        return 1;
    }
    puts("PASS: profile sentry anchors (auto, migrated big-yaw enabled, torque limits)");
    return 0;
}
