/**
 * @file    dwt_port.c
 * @brief   DWT 时基:周期计数/延时/时间线
 * @layer   PORTS(stm32f4)
 * @author  Wang Hongxi
 * @author  modified by Neo with annotation
 * @author  modified by 小猫大王
* @version V1.1.0
* @date    2022/3/8
 */
#include "dwt_port.h"
/* 注:原先 include cmsis_os.h 但全文件未使用任何 CMSIS 符号,2026-11-23 删除。 */

static dwt_time_t dwt_systime;
static uint32_t CPU_FREQ_Hz, CPU_FREQ_Hz_ms, CPU_FREQ_Hz_us;
static uint32_t dwt_cyccnt_round_count;
static uint32_t dwt_cyccnt_last;
static uint64_t dwt_cyccnt64;

/**
 * @brief 私有函数,用于检查DWT CYCCNT寄存器是否溢出,并更新dwt_cyccnt_round_count
 * @attention 此函数假设两次调用之间的时间间隔不超过一次溢出
 *
 * @todo 更好的方案是为dwt的时间更新单独设置一个任务?
 *       不过,使用dwt的初衷是定时不被中断/任务等因素影响,因此该实现仍然有其存在的意义
 *
 */
static void dwt_cnt_update(void)
{
    static volatile uint8_t bit_locker = 0;
    if (!bit_locker)
    {
        bit_locker = 1;
        volatile uint32_t cnt_now = DWT->CYCCNT;
        if (cnt_now < dwt_cyccnt_last)
            dwt_cyccnt_round_count++;

        dwt_cyccnt_last = DWT->CYCCNT;
        bit_locker = 0;
    }
}

void dwt_init(uint32_t CPU_Freq_mHz)
{
    /* 使能DWT外设 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* DWT CYCCNT寄存器计数清0 */
    DWT->CYCCNT = (uint32_t)0u;

    /* 使能Cortex-M DWT CYCCNT寄存器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    CPU_FREQ_Hz = CPU_Freq_mHz * 1000000;
    CPU_FREQ_Hz_ms = CPU_FREQ_Hz / 1000;
    CPU_FREQ_Hz_us = CPU_FREQ_Hz / 1000000;
    dwt_cyccnt_round_count = 0;

    dwt_cnt_update();
}

float dwt_get_delta_t(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    float dt = ((uint32_t)(cnt_now - *cnt_last)) / ((float)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    dwt_cnt_update();

    return dt;
}

double dwt_get_delta_t64(uint32_t *cnt_last)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    double dt = ((uint32_t)(cnt_now - *cnt_last)) / ((double)(CPU_FREQ_Hz));
    *cnt_last = cnt_now;

    dwt_cnt_update();

    return dt;
}

void dwt_systime_update(void)
{
    volatile uint32_t cnt_now = DWT->CYCCNT;
    static uint64_t CNT_TEMP1, CNT_TEMP2, CNT_TEMP3;

    dwt_cnt_update();

    dwt_cyccnt64 = (uint64_t)dwt_cyccnt_round_count * (uint64_t)UINT32_MAX + (uint64_t)cnt_now;
    CNT_TEMP1 = dwt_cyccnt64 / CPU_FREQ_Hz;
    CNT_TEMP2 = dwt_cyccnt64 - CNT_TEMP1 * CPU_FREQ_Hz;
    dwt_systime.s = CNT_TEMP1;
    dwt_systime.ms = CNT_TEMP2 / CPU_FREQ_Hz_ms;
    CNT_TEMP3 = CNT_TEMP2 - dwt_systime.ms * CPU_FREQ_Hz_ms;
    dwt_systime.us = CNT_TEMP3 / CPU_FREQ_Hz_us;
}

float dwt_get_timeline_s(void)
{
    dwt_systime_update();

    float DWT_Timelinef32 = dwt_systime.s + dwt_systime.ms * 0.001f + dwt_systime.us * 0.000001f;

    return DWT_Timelinef32;
}

float dwt_get_timeline_ms(void)
{
    dwt_systime_update();

    float DWT_Timelinef32 = dwt_systime.s * 1000 + dwt_systime.ms + dwt_systime.us * 0.001f;

    return DWT_Timelinef32;
}

uint64_t dwt_get_timeline_us(void)
{
    dwt_systime_update();

    uint64_t DWT_Timelinef32 = dwt_systime.s * 1000000 + dwt_systime.ms * 1000 + dwt_systime.us;

    return DWT_Timelinef32;
}

void dwt_delay(float Delay)
{
    uint32_t tickstart = DWT->CYCCNT;
    float wait = Delay;

    while ((DWT->CYCCNT - tickstart) < wait * (float)CPU_FREQ_Hz)
        ;
}
