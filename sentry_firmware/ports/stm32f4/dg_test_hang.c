/**
 * @file    dg_test_hang.c
 * @brief   G9 看门狗注入钩子(仅 -DDG_TEST_HANG 生效)
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note    验收链路:上电 → 延时 DG_TEST_HANG_DELAY_MS → diag_watch_simulate_hang
*          挂死指定必活任务 → IWDG 停喂 → 复位。判据:复位 ≤3.0s(2.46s 硬件 +
*          检测余量)+ [boot] RST cause IWDG=1 + 复位后 ROBOT_STATUS_STOP 需解锁。
*          任务名/延时由构建定义传入: DG_TEST_HANG_TASK="ins_state" DG_TEST_HANG_DELAY_MS=10000
*          (未传默认挂 ins_state/10s)。RTT 手动命令路径二期。
 */
#if defined(DG_TEST_HANG)

#include "diag.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "log_port.h"

#ifndef DG_TEST_HANG_TASK
#define DG_TEST_HANG_TASK "INS"
#endif
#ifndef DG_TEST_HANG_DELAY_MS
#define DG_TEST_HANG_DELAY_MS 10000u
#endif

void dg_test_hang_init(void);

static void dg_hang_task(void const *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(DG_TEST_HANG_DELAY_MS));
    LOG_WARN("[G9] injecting hang on task [%s], expect IWDG reset <=3.0s", DG_TEST_HANG_TASK);
    diag_watch_simulate_hang(DG_TEST_HANG_TASK);
    vTaskDelete(NULL);
}

void dg_test_hang_init(void)
{
    osThreadDef(dghang, dg_hang_task, osPriorityBelowNormal, 0, 128);
    osThreadCreate(osThread(dghang), NULL);
}

typedef int dg_test_hang_dummy_t; /* 防 -Wempty-translation-unit(无定义构建) */

#endif /* DG_TEST_HANG */
