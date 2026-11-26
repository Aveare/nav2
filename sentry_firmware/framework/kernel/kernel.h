/**
 * @file    kernel.h
 * @brief   kernel 平台无关原语:任务/延时/临界区/时间
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    本头是 kernel 的**唯一平台无关入口**:零 RTOS include、零 CMSIS include、
*          零 HAL include(G2/G6 闭包扫描:T01 判据 framework/ 0 个 RTOS include,
*          后端唯一落点 ports/freertos)。后端实现:
*          - ARM/FreeRTOS:ports/freertos/kernel_freertos.c(B1)
*          - 主机/POSIX:tests/fwk_posix/kernel_posix.c(B1,fw_kernel_test 用)
*
* @note    原语集(§4:15+3,必须含 is_in_isr/current_task/start):
*          任务 create/delete/delay/delay_until/yield;临界区 enter/exit;
*          is_in_isr/current_task/time_ms/time_us;start;version。
*          mutex/queue 推迟至有调用者(§T05,勿提前加)。
*
* @note    优先级数值**钉死**(方案 §4):NORMAL=3/HIGH=4/RT=5,
*          与迁移前 CMSIS osPriorityNormal→FreeRTOS 3 的隐式映射一致,
*          任何改动都要过 T05 验收"任务优先级表逐项等于迁移前"。
*          文件尾有静态断言,改值先过方案评审。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- 优先级数值钉死(迁移前隐式契约) ---- */
#define KERNEL_PRIO_NORMAL 3u
#define KERNEL_PRIO_HIGH   4u
#define KERNEL_PRIO_RT     5u

    typedef struct kernel_task *kernel_task_t;

    typedef void (*kernel_task_fn)(void *arg);

    typedef struct
    {
        kernel_task_fn fn; /* 任务体,不返回 */
        void *arg;
        const char *name;     /* 调试名(须为静态存储期字符串) */
        uint16_t stack_words; /* 栈深,单位:字(StackType_t) */
        uint8_t priority;     /* KERNEL_PRIO_* */
    } kernel_task_cfg_t;

    /* 任务 */
    kernel_task_t kernel_task_create(const kernel_task_cfg_t *cfg);
    void kernel_task_delete(kernel_task_t task);
    void kernel_task_delay(uint32_t ms);
    void kernel_task_delay_until(uint32_t *last_wake_ms, uint32_t period_ms);
    void kernel_task_yield(void);

    /* 临界区:任务上下文用 enter/exit;ISR 内 enter 返回要恢复的旧基线,exit 还原 */
    uint32_t kernel_enter_critical(void);
    void kernel_exit_critical(uint32_t saved);
    int kernel_is_in_isr(void);
    const char *kernel_current_task(void);

    /* 时间:ms 来自 tick;us 由 port 提供(DWT/主机单调钟),供 trace 与对拍 */
    uint32_t kernel_tick_ms(void);
    uint64_t kernel_time_us(void);

    /* 调度启动(port 接线;robot.c 启动序的最后一步) */
    void kernel_start(void);

    /* 锚点与版本(nm 可见,防 GLOB 空验;G2 断言 b 依赖) */
    const char *fw_version(void);
    extern const uint32_t fw_abi_anchor;

#ifdef __cplusplus
}
#endif

_Static_assert(KERNEL_PRIO_NORMAL == 3, "KERNEL_PRIO_NORMAL must stay 3 (T05 迁移前契约)");
_Static_assert(KERNEL_PRIO_HIGH == 4, "KERNEL_PRIO_HIGH must stay 4");
_Static_assert(KERNEL_PRIO_RT == 5, "KERNEL_PRIO_RT must stay 5");
