/**
 * @file    kernel_posix.c
 * @brief   kernel 的主机桩(fw_kernel_test 用;方案 §13 "kernel POSIX stub")
 * @author  小猫大王
 * @layer   TESTS
 *
 * @note    临界区 = stdatomic 自旋锁(双线程用例可真验互斥);is_in_isr 恒 0;
 *          时间 = 测试可控单调钟(确定性);task 只登记不下真线程。
 *          线程用 Win32 CreateThread:**本机主机编译器为 MinGW.org 6.3,无 pthread.h**
 *          (方案原文"pthread 双线程用例"的落地偏差,双线程语义不变,记 DEV-07)。
 */
#include "kernel.h"
#include "err.h"
#include "kernel_posix.h"
#include <stdatomic.h>
#include <string.h>
#include <windows.h>

static atomic_flag crit_flag = ATOMIC_FLAG_INIT;
static volatile uint64_t fake_us;

static struct
{
    kernel_task_fn fn;
    void *arg;
    const char *name;
    uint8_t prio;
} tasks[8];
static unsigned ntasks;

kernel_task_t kernel_task_create(const kernel_task_cfg_t *cfg)
{
    if (cfg == NULL || ntasks >= 8)
        return NULL;
    tasks[ntasks].fn = cfg->fn;
    tasks[ntasks].arg = cfg->arg;
    tasks[ntasks].name = cfg->name;
    tasks[ntasks].prio = cfg->priority;
    return (kernel_task_t)(uintptr_t)(++ntasks);
}
void kernel_task_delete(kernel_task_t task)
{
    (void)task;
}
void kernel_task_delay(uint32_t ms)
{
    fake_us += (uint64_t)ms * 1000u;
}
void kernel_task_delay_until(uint32_t *last, uint32_t period)
{
    *last += period;
}
void kernel_task_yield(void)
{
}

uint32_t kernel_enter_critical(void)
{
    while (atomic_flag_test_and_set_explicit(&crit_flag, memory_order_acquire))
        ;
    return 1u; /* 单层自旋锁,保存值仅占位 */
}
void kernel_exit_critical(uint32_t saved)
{
    (void)saved;
    atomic_flag_clear_explicit(&crit_flag, memory_order_release);
}
int kernel_is_in_isr(void)
{
    return 0;
}
const char *kernel_current_task(void)
{
    return ntasks ? tasks[ntasks - 1].name : "main";
}

uint32_t kernel_tick_ms(void)
{
    return (uint32_t)(fake_us / 1000u);
}
uint64_t kernel_time_us(void)
{
    return fake_us;
}
void kernel_start(void)
{
} /* 主机由测试驱动 */

void fw_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line;
    err_report(ERR_CODE_KERNEL_ASSERT, ERR_POLICY_FAILFAST_BOOT);
}
void kernel_posix_advance_us(uint64_t us)
{
    fake_us += us;
}
unsigned kernel_posix_task_count(void)
{
    return ntasks;
}
uint32_t err_log_drain_tick(void)
{
    return kernel_tick_ms();
}

/* ---- 线程辅助(Win32;主机专用) ---- */
unsigned kernel_posix_spawn(kernel_posix_thread_fn fn, void *arg)
{
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, NULL);
    return (unsigned)(uintptr_t)h;
}
void kernel_posix_join(unsigned handle)
{
    WaitForSingleObject((HANDLE)(uintptr_t)handle, INFINITE);
    CloseHandle((HANDLE)(uintptr_t)handle);
}
