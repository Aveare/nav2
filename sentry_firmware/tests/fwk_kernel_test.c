/**
 * @file    fwk_kernel_test.c
 * @brief   framework kernel/err/mem 主机用例(方案 §13 "kernel POSIX stub + pthread 双线程用例")
 * @author  小猫大王
 * @layer   TESTS
 *
 * @note    覆盖:①优先级钉死静态断言(编译期,间接由 include 保证);
 *          ②pthread 双线程临界区互斥(真并发,计数无丢失);
 *          ③任务登记;④err 四档分投递(LOG_ONLY 环顺序+回绕+丢最旧计数、
 *          SAFE_STATE 取一次、FAILFAST 钩子计数);⑤mem 池对齐/OOM;
 *          ⑥fw_version/fw_abi_anchor 锚点。
 *          期望值由本用例自身构造(纯框架层,无"迁移前代码"问题)。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kernel.h"
#include "err.h"
#include "mem.h"
#include "kernel_posix.h"

/* ---- ② 双线程临界区互斥(Win32 线程:本机 MinGW.org 无 pthread,记 DEV-07) ---- */
#define CONTEND_N 20000u
static volatile long shared_counter;
static unsigned contender(void *arg)
{
    (void)arg;
    for (unsigned i = 0; i < CONTEND_N; ++i)
    {
        kernel_enter_critical();
        shared_counter++; /* 临界区内读改写:无互斥则必丢更新 */
        kernel_exit_critical(0u);
    }
    return 0u;
}

static void test_critical_mutex(void)
{
    shared_counter = 0;
    unsigned a = kernel_posix_spawn(contender, NULL);
    unsigned b = kernel_posix_spawn(contender, NULL);
    kernel_posix_join(a);
    kernel_posix_join(b);
    assert(shared_counter == (long)(2 * CONTEND_N)); /* 2×20000 无丢失 */
    printf("PASS: critical section mutual exclusion (2 threads x %u)\n", CONTEND_N);
}

/* ---- ③ 任务登记 ---- */
static void dummy_task(void *arg)
{
    (void)arg;
}
static void test_task_registry(void)
{
    kernel_task_cfg_t cfg = {dummy_task, NULL, "fwk_test_task", 256, KERNEL_PRIO_NORMAL};
    assert(kernel_task_create(&cfg) != NULL);
    assert(kernel_posix_task_count() == 1u);
    printf("PASS: task registry (1 task, prio NORMAL=3)\n");
}

/* ---- ④ err 四档 ---- */
static unsigned failfast_calls;
static void failfast_hook(uint16_t code)
{
    (void)code;
    ++failfast_calls;
}
static void test_err_policies(void)
{
    err_register_failfast(failfast_hook);
    err_log_entry_t out[40];

    /* LOG_ONLY:40 次 > 容量 32 → 丢最旧 8,溢出计数 8 */
    for (int i = 0; i < 40; ++i)
        err_report((uint16_t)(0x100 + i), ERR_POLICY_LOG_ONLY);
    unsigned n = err_log_drain(out, 40);
    assert(n == 32u);                  /* 只剩最新 32 条 */
    assert(out[0].code == 0x100u + 8); /* 最旧 8 条(0x100..0x107)已丢 */
    assert(err_log_overflow() >= 8u);

    /* SAFE_STATE:置位 → 消费一次 → 清零 */
    assert(!err_safe_state_pending());
    err_report(ERR_CODE_MEM_OOM, ERR_POLICY_SAFE_STATE);
    assert(err_safe_state_pending());
    err_safe_state_consume();
    assert(!err_safe_state_pending());

    /* RESTART */
    err_report(ERR_CODE_MEM_OOM, ERR_POLICY_RESTART);
    assert(err_restart_pending());
    err_restart_consume();
    assert(!err_restart_pending());

    /* FAILFAST:钩子被调 + hard flag */
    err_report(ERR_CODE_KERNEL_ASSERT, ERR_POLICY_FAILFAST_BOOT);
    assert(failfast_calls == 1u);
    assert(err_hard_flag() == 1u);

    /* 计数器:MEM_OOM 码累计 = SAFE 1 + RESTART 1 = 2 */
    assert(err_count(ERR_CODE_MEM_OOM) == 2u);
    printf("PASS: err policies (ring 32/丢最旧计数/safe-state/restart/failfast)\n");
}

/* ---- ⑤ mem 池 ---- */
static void test_mem_pool(void)
{
    static uint8_t buf[256];
    mem_pool_init(buf, sizeof(buf));
    mem_pool_init(buf, sizeof(buf)); /* 幂等:第二次不重置 */
    assert(mem_capacity() == 256u);

    void *a = mem_alloc(8, 1);
    assert(a != NULL);
    void *b = mem_alloc(16, 16); /* 对齐 16 */
    assert(b != NULL && ((uintptr_t)b & 15u) == 0u);
    assert(mem_used() >= 24u);

    /* OOM:一次吃光剩余 → NULL + err 计数 */
    unsigned before = err_count(ERR_CODE_MEM_OOM);
    assert(mem_alloc(1024, 1) == NULL);
    assert(err_count(ERR_CODE_MEM_OOM) == before + 1u);
    printf("PASS: mem pool (align/OOM/err 计数)\n");
}

/* ---- ⑥ 锚点 ---- */
static void test_anchor(void)
{
    assert(fw_abi_anchor == 0xB10C0DEu);
    assert(fw_version()[0] == 'f');
    printf("PASS: fw anchor/version (nm G2 锚点)\n");
}

int main(void)
{
    test_critical_mutex();
    test_task_registry();
    test_err_policies();
    test_mem_pool();
    test_anchor();
    puts("PASS: fwk_kernel_test ALL");
    return 0;
}
