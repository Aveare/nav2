/**
 * @file    err.c
 * @brief   错误四档分投递:LOG/安全态/重启/启动失败
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    LOG_ONLY 环:单生产者(SPSC)无锁 —— 写侧 ISR/任务均可,读侧 diag 任务;
*          满丢最旧并计溢出(bus 语义与 bus 模块一致:丢最老必须计数)。
*          SAFE_STATE/RESTART:volatile 原子标志(uint8 在 M4 单字写原子),
*          robot_task 首拍 err_safe_state_pending→consume 后执行注入回调(B3 接)。
*          FAILFAST_BOOT:调注册钩子;未注册只置 err_hard_flag(观察者不丢事实)。
*          本文件不依赖 kernel 后端(不取 is_in_isr —— 临界区禁 err_report 的
*          断言由 port 侧 err_assert/调用点保证),主机与 ARM 同源编译。
 */
#include "err.h"
#include <string.h>

#define ERR_LOG_CAP 32u

static err_log_entry_t log_ring[ERR_LOG_CAP];
static volatile uint16_t log_write; /* 只由 err_report 推进 */
static uint16_t log_read;           /* 只由 drain 推进(单消费者) */
static volatile unsigned log_overflow;

/* 计数表:16 位码全量索引(高字节=子系统)不能折叠到 256 槽 —— 0x0101/0x0201
 * 会撞槽(err.h 已登记码分属不同子系统);改 64 槽关联表,线性扫(已登记码 < 16,
 * ISR 路径开销可忽略)。 */
#define ERR_CODE_TABLE 64
static struct
{
    uint16_t code;
    volatile uint32_t count;
} code_table[ERR_CODE_TABLE];
static volatile unsigned code_table_n;
static volatile uint8_t safe_state_flag;
static volatile uint8_t restart_flag;
static volatile uint8_t hard_flag;
static err_failfast_fn failfast_hook;

void err_report(uint16_t code, err_policy_e policy)
{
    /* 计数:关联表;新码追加(表满则并入溢出事实,不再丢主流程) */
    unsigned k = code_table_n;
    for (; k > 0; --k)
        if (code_table[k - 1].code == code)
            break;
    if (k == 0)
    {
        if (code_table_n < ERR_CODE_TABLE)
        {
            code_table[code_table_n].code = code;
            code_table[code_table_n].count = 0;
            code_table_n++;
        }
    }
    for (unsigned i = 0; i < code_table_n; ++i)
        if (code_table[i].code == code)
            code_table[i].count++;

    err_log_entry_t *e = &log_ring[log_write % ERR_LOG_CAP];
    e->code = code;
    e->tick_ms = err_log_drain_tick();
    log_write++; /* 单写者,先填后推(与遥测环同纪律) */
    if ((uint16_t)(log_write - log_read) > ERR_LOG_CAP)
    {
        log_read++; /* 满丢最旧(bus 语义一致:丢最老必须计数) */
        log_overflow++;
    }

    switch (policy)
    {
    case ERR_POLICY_LOG_ONLY:
        break;
    case ERR_POLICY_SAFE_STATE:
        safe_state_flag = 1;
        break;
    case ERR_POLICY_RESTART:
        restart_flag = 1;
        break;
    case ERR_POLICY_FAILFAST_BOOT:
        hard_flag = 1;
        if (failfast_hook)
            failfast_hook(code);
        break;
    default:
        hard_flag = 1;
        break;
    }
}

void err_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line; /* 断言定位走后端(port 打印/留痕),本层只分投递 */
    err_report(ERR_CODE_KERNEL_ASSERT, ERR_POLICY_FAILFAST_BOOT);
}

uint32_t err_count(uint16_t code)
{
    for (unsigned i = 0; i < code_table_n; ++i)
        if (code_table[i].code == code)
            return code_table[i].count;
    return 0u;
}
int err_safe_state_pending(void)
{
    return safe_state_flag;
}
void err_safe_state_consume(void)
{
    safe_state_flag = 0;
}
int err_restart_pending(void)
{
    return restart_flag;
}
void err_restart_consume(void)
{
    restart_flag = 0;
}
void err_register_failfast(err_failfast_fn fn)
{
    failfast_hook = fn;
}
unsigned err_log_drain(err_log_entry_t *out, unsigned cap)
{
    unsigned n = 0;
    while (n < cap && log_read != log_write)
    {
        out[n++] = log_ring[log_read % ERR_LOG_CAP];
        log_read++;
    }
    return n;
}

unsigned err_log_overflow(void)
{
    return log_overflow;
}
uint8_t err_hard_flag(void)
{
    return hard_flag;
}

/* err_log_drain_tick 的桩来源:B1 阶段 tick 由 port 弱符号提供,避免 framework
 * 反向依赖 kernel 后端;ports/freertos 与 POSIX 桩各给强定义。 */
__attribute__((weak)) uint32_t err_log_drain_tick(void)
{
    return 0u;
}
