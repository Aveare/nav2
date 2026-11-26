/**
 * @file    iwdg_port.h
 * @brief   IWDG 接口
 * @layer   PORTS(stm32f4)
 * @author  modified by 小猫大王
* @note    2026-11-21 建立(DEV-20261121-06)。IWDG 由独立 LSI 时钟驱动,不依赖任何中断
*          或任务调度 —— 即使串口中断风暴把 CPU 打满、FreeRTOS 全部任务饿死,它照常计数,
*          超时即复位整车。这是"无论什么原因僵死,都不可能僵死到关不掉"的唯一硬件保证。
* @note    调试可用性:已开 DBGMCU 的 IWDG 冻结位,调试器暂停核心时 IWDG 暂停计数
*          (否则探针一 halt 超过超时就会复位,断点调试无法进行)。
 */
#ifndef IWDG_PORT_H
#define IWDG_PORT_H

#include <stdint.h>

/**
 * @brief 初始化并启动 IWDG(调用一次,启动后不可停止)
 * @note  超时 = LSI(≈32kHz) / 64 分频 × 1000 = 约 2s;必须在启动后 2s 内开始喂狗。
 *        由 robot_init 在中断使能后调用。
 */
void iwdg_init(void);

/**
 * @brief 喂狗(200Hz robot_task 每拍调用一次)
 * @note  只在主控制任务里喂:任何导致该任务停摆的故障(中断风暴/死循环/优先级反转)
 *        都会让 IWDG 在 2s 内复位,而不是僵死。
 */
void iwdg_feed(void);

#endif
