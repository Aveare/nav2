/**
 * @file    fault_port.c
 * @brief   致命故障留痕:备份寄存器写入与导出
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note   2026-11-21 建立(DEV-20261121-08)。为什么不写 Flash:故障现场(栈溢出/硬件异常)
*         里调用 Flash 驱动是不安全的(擦写期间取指阻塞、可能二次故障);RTC 备份寄存器
*         是几字节的寄存器写入,任何上下文都能安全完成。
* @note   为什么立即 NVIC_SystemReset 而不等 IWDG:HardFault 的默认处理是死循环,
*         行为上等于"整车僵死 2 秒"。记录死因后立刻复位,RTO 控制在几百 ms 内恢复,
*         且下次开机可通过蜂鸣器听到死因。
 */
#include "fault_port.h"
#include "dwt_port.h" // dwt_delay/dwt_get_timeline_ms:中断关闭期间也不依赖 SysTick
#include "main.h"

/* USART1 句柄由 CubeMX 在 usart.c 定义(本工程作 VOFA/调试口) */
extern UART_HandleTypeDef huart1;

#define FAULT_MAGIC 0xFA7A0000u // 高 16 位魔数:区分"真实记录"与备份域初值

static float fault_hold_until_ms; // 故障文本打印后的串口独占截止时刻,ms

void fault_init(void)
{
    /* 备份域访问:不开 DBP 则写 RTC->BKPxR 无效 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
}

void fault_record_and_reset(uint32_t code)
{
    fault_record_detail(code, 0u, 0u, 0u, 0u, 0u, 0u);
}

void fault_record_detail(uint32_t code, uint32_t cfsr, uint32_t bfar, uint32_t pc, uint32_t hfsr,
                       uint32_t lr, uint32_t sp)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP0R = FAULT_MAGIC | (code & 0xFFu);
    RTC->BKP1R = cfsr; // SCB->CFSR:故障类别(取指/数据访问/压栈失败/未定义指令…)
    RTC->BKP2R = bfar; // SCB->BFAR/MMFAR:出错访问地址
    RTC->BKP3R = pc;   // 故障现场的 PC(可 addr2line 定位到代码行)
    RTC->BKP4R = hfsr; // SCB->HFSR
    RTC->BKP5R = lr;   // 返回地址:定位"是哪一句日志/哪个调用"触发的故障
    RTC->BKP6R = sp;   // 故障时栈指针:与栈初值比较可判定栈溢出
    /* 异常压栈帧的 R0~R3:对"格式串/参数错位"类故障是决定性证据 ——
     * 其中会直接包含 %s 收到的坏指针,以及罗列在 .rodata 的格式串地址
     * (后者可用 objdump 反查,精确定位到是哪一条 LOG_PRINT)。 */
    if (sp != 0u)
    {
        const uint32_t *frame = (const uint32_t *)sp;
        RTC->BKP7R = frame[0];  // R0
        RTC->BKP8R = frame[1];  // R1
        RTC->BKP9R = frame[2];  // R2
        RTC->BKP10R = frame[3]; // R3
    }

    /* 立即复位:不再返回故障现场(栈可能已损坏,返回无意义) */
    NVIC_SystemReset();
    while (1) // REDFLAG-EXEMPT: 复位指令之后不可达防御环;不进 T02 台账,G4 redflags
              // 已豁免(B0a-②,2026-11-22)
        ;
}

/**
 * @brief 开机把故障详情打成 ASCII 到 USART1
 * @note  会被重复打印 3 次(间隔 120ms),并用 DWT 阻塞延时 —— 此刻 SysTick 中断尚未
 *        使能(robot_init 处于 __disable_irq 内),HAL_Delay 会死等。
 * @note  打印后进入"串口独占期"(fault_hold_active 为真),VOFA 数据流暂停 1.5s,
 *        避免文本行被 200Hz 的 JustFloat 二进制淹没。
 */
void fault_dump_to_uart(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    uint32_t raw = RTC->BKP0R;
    if ((raw & 0xFFFF0000u) != FAULT_MAGIC)
        return;

    char buf[320]; // 多行 ASCII 文本(含 R0~R3),256 不够
    int n = __builtin_snprintf(
        buf, sizeof(buf),
        "\r\n========== [FAULT] ==========\r\n"
        "code=%02lX\r\nCFSR=%08lX\r\nBFAR=%08lX\r\nPC=%08lX\r\nHFSR=%08lX\r\n"
        "LR=%08lX\r\nSP=%08lX\r\nR0=%08lX\r\nR1=%08lX\r\nR2=%08lX\r\nR3=%08lX\r\n"
        "=============================\r\n",
        (unsigned long)(raw & 0xFFu), (unsigned long)RTC->BKP1R, (unsigned long)RTC->BKP2R,
        (unsigned long)RTC->BKP3R, (unsigned long)RTC->BKP4R, (unsigned long)RTC->BKP5R,
        (unsigned long)RTC->BKP6R, (unsigned long)RTC->BKP7R, (unsigned long)RTC->BKP8R,
        (unsigned long)RTC->BKP9R, (unsigned long)RTC->BKP10R);
    if (n > 0)
    {
        for (uint8_t i = 0; i < 3u; ++i) // 重复 3 次:串口助手漏不掉
        {
            (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 100);
            dwt_delay(0.12f); // 不依赖 SysTick(此时中断还关着)
        }
    }

    fault_hold_until_ms = dwt_get_timeline_ms() + 1500.0f; // 独占 1.5s

    /* 注意:这里**不清** BKP0R —— 由随后执行的 fault_consume() 统一清除。
     * 否则开机鸣叫码会退化成"两短(软件复位)",丢失"四短=HardFault/五短=栈溢出"的信息。 */
}

uint8_t fault_hold_active(void)
{
    return dwt_get_timeline_ms() < fault_hold_until_ms;
}

uint32_t fault_consume(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    uint32_t raw = RTC->BKP0R;
    if ((raw & 0xFFFF0000u) != FAULT_MAGIC)
        return 0u;

    RTC->BKP0R = 0u; // 一次消费
    return raw & 0xFFu;
}

/* FreeRTOS 栈溢出钩子(需 FreeRTOSConfig 打开 configCHECK_FOR_STACK_OVERFLOW) */
#include "FreeRTOS.h"
#include "task.h"

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    fault_record_and_reset(FAULT_CODE_STACKOVF);
}
