/**
 * @file    profile_compile_heavy.c
 * @brief   B11 三骨架编译矩阵:heavy profile 锚点检查(sim 目标)
 * @author  小猫大王
 * @layer   TESTS
 */
#include <stdio.h>

#include "profile.h"

#ifndef PROFILE_ROBOT_HEAVY
#error "heavy profile.h 未定义 PROFILE_ROBOT_HEAVY"
#endif
#if PROFILE_ROBOT_HEAVY != 1
#error "PROFILE_ROBOT_HEAVY 必须为 1"
#endif
#ifndef PROFILE_HAS_42MM_SHOOT
#error "heavy profile.h 缺 42mm 能力锚点"
#endif

int main(void)
{
    puts("PASS: profile heavy anchors (42mm)");
    return 0;
}
