/**
 * @file    kernel_freertos.c
 * @brief   kernel 的 FreeRTOS 后端(唯一 RTOS 落点)
 * @layer   PORTS(freertos)
 * @author  小猫大王
* @note    **本目录是 framework 里 RTOS include 的唯一合法落点**(T01/§3.2 闭包扫描)。
*          映射纪律:xTaskCreate 直用,不再包一层 CMSIS-OS;优先级数值直接用
*          kernel.h 钉死的 3/4/5(迁移前 CMSIS osPriorityNormal→3 的隐式契约)。
*          kernel_time_us 读 DWT_CYCCNT(Cortex-M3/M4 通用,归 port 不归 framework)。
*          fw_assert_fail = configASSERT 改造出口(方案 §4):FAILFAST 分投递 +
*          受控复位(行为变化点,已随 B1 批准;停机挂死路径见 §4 err 节)。
 */
#include "FreeRTOS.h"
#include "task.h"
#include "kernel.h"
#include "err.h"
#include "main.h" /* SystemCoreClock/DWT 时钟源接线的 HAL 面仅此一处 */

/* ---- 任务 ---- */
kernel_task_t kernel_task_create(const kernel_task_cfg_t *cfg)
{
    TaskHandle_t handle = NULL;
    if (cfg == NULL || cfg->fn == NULL || cfg->name == NULL)
        return NULL;
    if (xTaskCreate((TaskFunction_t)cfg->fn, cfg->name, cfg->stack_words, cfg->arg, cfg->priority,
                    &handle) != pdPASS)
        return NULL;
    return (kernel_task_t)handle;
}

void kernel_task_delete(kernel_task_t task)
{
    vTaskDelete((TaskHandle_t)task);
}

void kernel_task_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void kernel_task_delay_until(uint32_t *last_wake_ms, uint32_t period_ms)
{
    vTaskDelayUntil((TickType_t *)last_wake_ms, pdMS_TO_TICKS(period_ms));
}

void kernel_task_yield(void)
{
    taskYIELD();
}

/* ---- 临界区(BASEPRI;ISR 内用 FROM_ISR 变体) ---- */
uint32_t kernel_enter_critical(void)
{
    return (uint32_t)taskENTER_CRITICAL_FROM_ISR();
}

void kernel_exit_critical(uint32_t saved)
{
    taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)saved);
}

int kernel_is_in_isr(void)
{
    return (int)__get_IPSR();
}

const char *kernel_current_task(void)
{
    return pcTaskGetName(NULL);
}

/* ---- 时间:tick 来自调度器;us 来自 DWT(Cortex-M 通用调试外设,归 port) ---- */
uint32_t kernel_tick_ms(void)
{
    const TickType_t tick =
        kernel_is_in_isr() ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
    return (uint32_t)(tick * portTICK_PERIOD_MS);
}

uint64_t kernel_time_us(void)
{
    uint32_t cycles = DWT->CYCCNT;
    return (uint64_t)cycles / (SystemCoreClock / 1000000u);
}

/* ---- 启动 ---- */
void kernel_start(void)
{
    vTaskStartScheduler();
}

/* ---- configASSERT 出口(方案 §4 改造;行为变化点,随 B1 批准) ---- */
void fw_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line;
    err_report(ERR_CODE_KERNEL_ASSERT, ERR_POLICY_FAILFAST_BOOT);
    /* FAILFAST 未接 FaultRecord 前的兜底:关中断 + 受控复位(B3 换 err 接线) */
    taskDISABLE_INTERRUPTS();
    NVIC_SystemReset();
}

/* err.c 的弱 tick 源:与调度 tick 同源(仅 LOG_PRINT 时间戳用途) */
uint32_t err_log_drain_tick(void)
{
    return kernel_tick_ms();
}
