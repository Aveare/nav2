/**
 * @file    iwdg_port.c
 * @brief   独立看门狗:启动与喂狗
 * @layer   PORTS(stm32f4)
 * @author  modified by 小猫大王
* @note   2026-11-21 建立(DEV-20261121-06)。选 2s 而不是 1s:robot_init 完成到
*         robot_task 首次运行之间有空档,2s 对僵死检测仍足够快、对启动更宽容。
* @note   为什么直接写寄存器而不用 HAL:本工程 HAL 树已裁掉 stm32f4xx_hal_iwdg.c/h;
*         且 IWDG 的寄存器协议只有 5 条(解锁/写分频/写重载/等更新/喂狗),封装无收益。
* @note   为什么不用窗口看门狗(WWDG):WWDG 挂 APB 时钟,主时钟异常时同样失效;
*         IWDG 用独立 LSI,是时钟树故障下仍可信的唯一看门狗。
* @note   调试可用性:置 DBGMCU 的 IWDG_STOP 冻结位,调试器暂停核心时 IWDG 停止计数
*         (否则断点一停 2s 就复位,无法调试)。
 */
#include "iwdg_port.h"
#include "main.h"

void iwdg_init(void)
{
    /* LSI 是 IWDG 的唯一时钟源:未起振则计不到数 */
    RCC->CSR |= RCC_CSR_LSION;
    uint32_t t0 = HAL_GetTick();
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0u)
    {
        if (HAL_GetTick() - t0 > 100u) // LSI 起振典型 <10ms;100ms 未就绪视为异常
            return;                    // 宁可没有看门狗,也不做成复位循环
    }

    IWDG->KR = 0xCCCCu; // 启动 IWDG(不可停止)
    IWDG->KR = 0x5555u; // 允许写 PR/RLR
    IWDG->PR = 0x3u;    // 预分频 /64:32kHz → 500Hz,1 计数 = 2ms
    IWDG->RLR = 1000u;  // 1000 × 2ms = 2s
    while (IWDG->SR != 0u)
        ;               // 等 PR/RLR 生效(PVU/RVU 清零)
    IWDG->KR = 0xAAAAu; // 首次喂狗

    /* 调试暂停时冻结 IWDG(F4:DBGMCU_APB1_FZ bit12 = DBG_IWDG_STOP) */
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
}

void iwdg_feed(void)
{
    IWDG->KR = 0xAAAAu;
}
