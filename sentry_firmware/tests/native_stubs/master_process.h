/**
 * @file    master_process.h
 * @brief   主机测试桩:替代 master_process.h 的最小实现,仅供 PC 端编译测试
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
// 枚举成员与真实master_process.h对齐,保证shoot.c等引用方可在host环境语法检查
typedef enum { BULLET_SPEED_NONE = 0, SMALL_AMU_15, SMALL_AMU_18, SMALL_AMU_30 } Bullet_Speed_e;
typedef enum { COLOR_NONE = 0, COLOR_BLUE = 0, COLOR_RED = 1 } Enemy_Color_e;
