#include "nav_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_gcc.h"

uint32_t nav_tick_ms(void)
{
    const TickType_t ticks = (__get_IPSR() != 0u) ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
    return (uint32_t)(ticks * portTICK_PERIOD_MS);
}

uint32_t nav_enter_critical(void)
{
    return (uint32_t)taskENTER_CRITICAL_FROM_ISR();
}

void nav_exit_critical(uint32_t saved)
{
    taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)saved);
}
