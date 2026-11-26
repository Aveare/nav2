/**
 * @file    fault_port.h
 * @brief   故障留痕接口
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note    2026-11-21 建立(DEV-20261121-08)。实车出现"射击瞬间失控 → 遥控无效 →
*          2s 后看门狗复位",无法判断是任务卡死、栈溢出还是硬件异常。探针不可用后,
*          改为用 RTC 备份寄存器记录死因:HardFault/栈溢出在**复位前**写入,开机读取后
*          转成蜂鸣器鸣叫码 —— 纯听觉得到答案,零外部接触。
 */
#ifndef FAULT_PORT_H
#define FAULT_PORT_H

#include <stdint.h>

/* 死因编码(写入 RTC->BKP0R 的低 8 位,高 24 位为魔数防误读) */
#define FAULT_CODE_HARDFAULT 0xE1u // 硬件异常(HardFault)
#define FAULT_CODE_STACKOVF  0x51u // FreeRTOS 栈溢出

/**
 * @brief 使能备份域访问(开机尽早调用一次,之后才能读写 RTC 备份寄存器)
 */
void fault_init(void);

/**
 * @brief 记录死因并立即复位(在故障处理函数内调用,不得返回)
 * @param code FAULT_CODE_xxx
 */
void fault_record_and_reset(uint32_t code);

/**
 * @brief 记录故障详情并立即复位
 * @param code  FAULT_CODE_xxx
 * @param cfsr  SCB->CFSR:故障类别
 * @param bfar  SCB->BFAR:出错访问地址
 * @param pc    故障指令地址(可 addr2line)
 * @param hfsr  SCB->HFSR
 * @param lr    故障时的返回地址:能定位到"是哪一句日志/哪个函数调用"触发的故障
 * @param sp    故障时的栈指针:与任务栈底比较可判断栈溢出
 * @note  详情写入 RTC 备份寄存器,开机由 fault_dump_to_uart() 以 ASCII 打到 USART1,
 *        普通 USB 串口即可读取 —— 不需要调试器(DEV-20261121-09)。
 */
void fault_record_detail(uint32_t code, uint32_t cfsr, uint32_t bfar, uint32_t pc, uint32_t hfsr,
                       uint32_t lr, uint32_t sp);

/**
 * @brief 若有故障详情,开机用 USART1 打印 ASCII(921600)并进入 1.5s 串口独占期
 * @note  非侵入通道:USART1 是普通串口,不打探针、不暂停内核。
 */
void fault_dump_to_uart(void);

/**
 * @brief 是否处于故障文本的串口独占期(为真时上层应暂停 VOFA 数据流)
 * @return 1 = 独占中
 */
uint8_t fault_hold_active(void);

/**
 * @brief 读取并清除上次记录的死因
 * @return 0 = 无记录;否则为 FAULT_CODE_xxx
 */
uint32_t fault_consume(void);

#endif
