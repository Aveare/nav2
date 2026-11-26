/**
 * @file    ax650_uart1.h
 * @brief   USART1 的 AX650 周期姿态发送接口
 * @layer   MODULE
 * @note    唯一调用者为 INS 任务；不接管云台、底盘或发射指令。
 */
#ifndef AX650_UART1_H
#define AX650_UART1_H

#include <stdint.h>

/**
 * @brief  配置独占的 USART1 为 921600 8E1，并注册 BSP 实例
 * @return 成功或已经初始化返回 1；失败返回 0，后续姿态更新会重试。
 * @note   在 INS 初始化后调用；失败不阻塞任务，也不复位目标。
 */
uint8_t Ax650Uart1Init(void);

/**
 * @brief  按配置周期发送最新真实姿态，忙时跳过本次而不排队
 * @param  q_wxyz  最新机体到导航四元数 [w,x,y,z]，无量纲
 * @return 成功启动本帧 IT 发送返回 1；未到周期、忙或数据非法返回 0。
 * @note   先调用初始化请求；仅由 INS 任务写静态缓冲区，IT 完成前不改写。
 */
uint8_t Ax650Uart1SendQuaternion(const float q_wxyz[4]);

#endif
