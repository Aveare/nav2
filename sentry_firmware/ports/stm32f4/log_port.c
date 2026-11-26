/**
 * @file    log_port.c
 * @brief   RTT 日志 sink:彩色/浮点转换/运行期拦截
 * @layer   PORTS(stm32f4)
 * @author  modified by 小猫大王
* @note    接口说明见 log_port.h。
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "log_port.h"
#include "general_def.h"

#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h> // strchr:格式串浮点占位符扫描(DEV-20261121-10)

/* SEGGER RTT 控制块(由 SEGGER_RTT.c 定义),此处用于做指针健全性检查 */
extern SEGGER_RTT_CB _SEGGER_RTT;

void log_init()
{
    SEGGER_RTT_Init();
}

/**
 * @brief  RTT 上缓冲区控制块健全性检查
 * @return 1 = 三项字段(pBuffer/SizeOfBuffer/WrOff)都合理
 * @note   DEV-20261121-09 实车故障:CFSR=PRECISERR、BFAR=0x40240000、PC 在 vprintf。
 *         只要 pBuffer、SizeOfBuffer、WrOff 任一项是垃圾,`pBuffer + WrOff` 就会算出非法地址。
 */
static uint8_t log_rtt_buffer_sane(void)
{
    SEGGER_RTT_BUFFER_UP *p = &_SEGGER_RTT.aUp[LOG_BUFFER_INDEX];
    uint32_t base = (uint32_t)p->pBuffer;
    if (base < 0x20000000u || base >= 0x20020000u) // 必须在 SRAM 内
        return 0;
    if (p->SizeOfBuffer == 0u || p->SizeOfBuffer > 4096u) // 必须与配置的 1024B 同量级
        return 0;
    if (p->WrOff >= p->SizeOfBuffer) // 写偏移必须落在缓冲内
        return 0;
    return 1;
}

static volatile uint8_t s_log_enabled = 1; // 根因已修(日志 %f 参数错位),恢复输出(DEV-20261121-10)

void log_set_enabled(uint8_t on)
{
    s_log_enabled = on;
}

/**
 * @brief 统一日志入口(所有 LOGxxx 宏最终都到这里)
 * @note  两道闸:①日志总开关(运行期);②RTT 控制块健全性。任一不过 → 静默返回。
 *        实车证据:HardFault 的 PC 落在 SEGGER_RTT_vprintf 的 %s 循环、BFAR=0x40240000
 *        —— 格式串参数被写坏时,进 vprintf 就是往非法地址读写。日志可以丢,车不能崩。
 */
/**
 * @brief 检查格式串中是否含浮点占位符(%f/%e/%g/%a 及其大写形式)
 * @return 1 = 含浮点占位符(必须拒绝打印)
 * @note  DEV-20261121-10:SEGGER RTT 的 printf 不支持浮点。写了 %f 时该占位符既不打印
 *        也不消费参数,后续 %s 会把浮点值当 char* 解引用 → HardFault(实车 BFAR=0x40240000)。
 *        这里做**运行期最后一道拦截**:宁可丢一条日志,也不能崩车。
 * @note  只做最小的规范解析:%% 跳过;依次跳过 flags/width/precision/长度修饰符;
 *        命中 f/F/e/E/g/G/a/A 立即返回。
 */
static uint8_t log_fmt_has_float(const char *fmt)
{
    const char *p = fmt;
    while (*p != '\0')
    {
        if (*p != '%')
        {
            ++p;
            continue;
        }
        ++p;
        if (*p == '%') // "%%" 是转义的百分号
        {
            ++p;
            continue;
        }
        while (*p != '\0' && strchr("-+ #0", *p) != NULL) // flags
            ++p;
        while (*p >= '0' && *p <= '9') // width
            ++p;
        if (*p == '.') // precision
        {
            ++p;
            while (*p >= '0' && *p <= '9')
                ++p;
        }
        while (*p != '\0' && strchr("hljztL", *p) != NULL) // length
            ++p;
        if (*p == '\0')
            break;
        if (strchr("fFeEgGaA", *p) != NULL)
            return 1;
    }
    return 0;
}

int log_printf(const char *fmt, ...)
{
    if (!s_log_enabled)
        return 0;

    /* 硬规则 1 的运行期兜底:格式串含浮点占位符 → 拒绝打印,并提示一次 */
    if (log_fmt_has_float(fmt))
    {
        static uint8_t warned;
        if (!warned)
        {
            warned = 1;
            SEGGER_RTT_WriteString(
                LOG_BUFFER_INDEX, "\r\nE:[bsp_log] float specifier in LOG format is FORBIDDEN "
                              "(use log_float_to_str or (int)+%d). Log dropped to avoid HardFault.\r\n");
        }
        return 0;
    }

    if (!log_rtt_buffer_sane())
    {
        SEGGER_RTT_Init(); // 控制块被写坏:重建后本次丢弃
        return 0;
    }

    va_list args;
    va_start(args, fmt);
    int n = SEGGER_RTT_vprintf(LOG_BUFFER_INDEX, fmt, &args);
    va_end(args);
    return n;
}

int log_print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = SEGGER_RTT_vprintf(LOG_BUFFER_INDEX, fmt,
                               &args); // 一次可以开启多个buffer(多个终端),我们只用一个
    va_end(args);
    return n;
}

/**
 * @brief 浮点转字符串(供不支持 %f 的 RTT 日志使用)
 * @note  DEV-20261121-10:原实现无边界保护 —— 输入 NaN/Inf 或超大值时,
 *        `(int)va` 得到 INT_MIN,sprintf 会输出 "-2147483648.-2147483648"(23 字节),
 *        直接溢出 16 字节栈缓冲、破坏调用者栈。实车"射击时 HardFault"的一个可疑源。
 *        现改为:非有限值/超范围先钳位,输出最坏 15 字节,永不溢出。
 */
void log_float_to_str(char *str, float va)
{
    if (str == NULL)
        return;

    /* NaN 判定用自比较(避免引入 isfinite 的库依赖);Inf/超大值按上限钳位 */
    if (va != va)
    {
        str[0] = 'n';
        str[1] = 'a';
        str[2] = 'n';
        str[3] = '\0';
        return;
    }
    if (va > 99999.0f)
        va = 99999.0f;
    if (va < -99999.0f)
        va = -99999.0f;

    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - (float)head) * 1000);
    head = MATH_ABS(head);
    point = MATH_ABS(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point); // 最坏 "-99999.999" 共 10 字节
    else
        sprintf(str, "%d.%d", head, point);
}
