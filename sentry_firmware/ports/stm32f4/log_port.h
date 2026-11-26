/**
 * @file    log_port.h
 * @brief   日志接口
 * @layer   PORTS(stm32f4)
 * @author  modified by 小猫大王
* @note  不占用串口;LOG_ERROR 用于错误路径,禁止在中断内高频调用。
* @note  源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
*
* @note   规则 1 有运行期兜底:log_printf 会扫描格式串,命中浮点占位符就**拒绝打印**
*         (丢一条日志,绝不冒崩车的风险),并打印一次安全提示。
* @note   规则 1 的构建期检查:tools/check_log_format.ps1,提交前跑一次。
 */
#ifndef _BSP_LOG_H
#define _BSP_LOG_H

#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#include <stdint.h>
#include <stdio.h>

#define LOG_BUFFER_INDEX 0

/* [临时诊断开关 2026-11-21] 置 1 = 关闭全部 LOGxxx(编译成空语句)。
 * 背景:实车在"启动射击"时稳定复现 HardFault,故障 PC 落在 SEGGER_RTT_vprintf 的
 * `%s` 字符串循环里、BFAR=0x40240000(float 2.5625 的位模式)。为二分定位,
 * 先把日志整体摘掉:若崩溃消失,即可确认问题在日志调用路径(log_float_to_str 溢出/坏指针);
 * 若仍崩溃,则说明是别处的野写,与日志无关。
 * 关闭日志不影响蜂鸣码与 [FAULT] 串口输出。定位完成后置回 0。 */
#ifndef LOG_BSP_OFF
#define LOG_BSP_OFF 0
#endif
#if LOG_BSP_OFF && !defined(LOG_DISABLE_SYSTEM)
#define LOG_DISABLE_SYSTEM 1
#endif

/**
 * @brief 日志系统初始化
 *
 */
void log_init();

/**
 * @brief 日志功能原型,供下面的LOGI,LOGW,LOGE等使用
 * @note  统一走 log_printf 而不是直接调 SEGGER_RTT_printf(DEV-20261121-10):
 *        这样可以在**运行期**整体静音日志,用于二分定位射击时的 HardFault;
 *        参数依旧被引用,不会产生"未使用变量"警告。
 */
#define LOG_PROTO(type, color, format, ...)                                                        \
    log_printf("  %s%s" format "\r\n%s", color, type, ##__VA_ARGS__, RTT_CTRL_RESET)

/*----------------------------------------下面是日志输出的接口-------------------------------------------------*/

/* 清屏 */
#define LOG_CLEAR() SEGGER_RTT_WriteString(0, "  " RTT_CTRL_CLEAR)

/* 无颜色日志输出 */
#define LOG_PRINT(format, ...) LOG_PROTO("", "", format, ##__VA_ARGS__)

/**
 *  有颜色格式日志输出,建议使用这些宏来输出日志
 *  @attention 注意这些接口不支持浮点格式化输出,若有需要,请使用log_float_to_str()函数进行转换后再打印
 *  @note 在release版本上车使用时,与makefile中添加的宏LOG_DISABLE_SYSTEM一起使用,可以关闭日志系统
 */
#if LOG_DISABLE_SYSTEM
#define LOG_INFO(format, ...)
#define LOG_WARN(format, ...)
#define LOG_ERROR(format, ...)
#else
// information level
#define LOG_INFO(format, ...)    LOG_PROTO("I:", RTT_CTRL_TEXT_BRIGHT_GREEN, format, ##__VA_ARGS__)
// warning level
#define LOG_WARN(format, ...) LOG_PROTO("W:", RTT_CTRL_TEXT_BRIGHT_YELLOW, format, ##__VA_ARGS__)
// error level
#define LOG_ERROR(format, ...)   LOG_PROTO("E:", RTT_CTRL_TEXT_BRIGHT_RED, format, ##__VA_ARGS__)
#endif //  LOG_DISABLE_SYSTEM

/**
 * @brief 通过segger RTT打印日志,支持格式化输出,格式化输出的实现参考printf.
 * @attention !! 此函数不支持浮点格式化,若有需要,请使用log_float_to_str()函数进行转换后再打印 !!
 *
 * @param fmt 格式字符串
 * @param ... 参数列表
 * @return int 打印的log字符数
 */
int log_print(const char *fmt, ...);

/**
 * @brief  所有 LOGxxx 实际调用的统一入口
 * @note   DEV-20261121-10:内部先检查"日志总开关 + RTT 控制块健全性"。
 *         两者任一不满足就静默丢弃,绝不进 SEGGER_RTT_vprintf —— 实车 HardFault
 *         的 PC 就在 vprintf 的 %s 循环里(读到 float 位模式 0x40240000)。
 * @param  fmt 格式字符串
 * @return 实际写出的字符数;被拦截时返回 0
 */
int log_printf(const char *fmt, ...);

/**
 * @brief 打开/关闭日志输出(运行期)
 * @param on 1 = 打开
 */
void log_set_enabled(uint8_t on);

/**
 * @brief 利用sprintf(),将float转换为字符串进行打印
 * @attention 浮点数需要转换为字符串后才能通过RTT打印
 *
 * @param str 转换后的字符串
 * @param va 待转换的float
 */
void log_float_to_str(char *str, float va);

#endif
