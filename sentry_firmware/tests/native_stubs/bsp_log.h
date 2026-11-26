/**
 * @file    bsp_log.h
 * @brief   主机测试桩:替代 bsp_log.h 的最小实现,仅供 PC 端编译测试
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
#define LOGINFO(...) ((void)0)
#define LOGWARNING(...) ((void)0)
#define LOGERROR(...) ((void)0)
// 与真实bsp_log.h保持一致的浮点转换接口声明(telemetry等模块引用)
void Float2Str(char *str, float va);
