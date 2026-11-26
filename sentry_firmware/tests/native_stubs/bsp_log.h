/**
 * @file    bsp_log.h
 * @brief   主机测试桩:替代 bsp_log.h 的最小实现,仅供 PC 端编译测试
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 */
#pragma once
/* 与真实 log_port.h 的发布默认一致:关闭日志,避免仅被 LOG_* 使用的静态函数
 * 在主机测试里变成 unused-function 告警。 */
#ifndef LOG_DISABLE_SYSTEM
#define LOG_DISABLE_SYSTEM 1
#endif
#define LOG_PRINT(...)   ((void)0)
#define LOG_INFO(...)    ((void)0)
#define LOG_WARN(...) ((void)0)
#define LOG_ERROR(...)   ((void)0)
// 与真实bsp_log.h保持一致的浮点转换接口声明(telemetry等模块引用)
void log_float_to_str(char *str, float va);
