/**
 * @file    ax650_uart1.h
 * @brief   AX650 链路接口与状态查询
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    ins_state 任务发送姿态，串口任务解析下行命令，CMD 任务只读取命令快照。
 */
#ifndef AX650_UART1_H
#define AX650_UART1_H

#include "ax650_pose.h"
#include <stdint.h>

typedef struct
{
    ax650_command_t command;   // 最近一帧通过校验的 0x0503 控制指令
    uint32_t command_rx_ms; // 最近有效命令接收时刻，ms
    uint32_t command_count; // 有效命令累计帧数
    uint8_t command_valid;  // 已收到命令且未超过 AX650_COMMAND_TIMEOUT_MS
} ax650_uart1_state_t;

/**
 * @brief  配置独占的 USART1 为 921600 8E1，并按 AX650_UART1_RX_ENABLE 注册接收
 * @return 成功或已经初始化返回 1；失败返回 0，后续姿态更新会重试。
 * @note   在 ins_state 初始化后调用；失败不阻塞任务，也不复位目标。
 */
uint8_t ax650_uart1_init(void *uart_handle);

/**
 * @brief  按配置周期发送最新真实姿态，忙时跳过本次而不排队
 * @param  q_wxyz  最新机体到导航四元数 [w,x,y,z]，无量纲
 * @return 成功启动本帧 IT 发送返回 1；未到周期、忙或数据非法返回 0。
 * @note   先调用初始化请求；仅由 ins_state 任务写静态缓冲区，IT 完成前不改写。
 */
uint8_t ax650_uart1_send_quaternion(const float q_wxyz[4]);

/**
 * @brief 更新姿态帧携带的裁判系统实测弹速
 * @param speed_mps 裁判系统 0x0207 弹速，m/s
 * @note 非有限值或非正值发送 0，表示尚无实测弹速；正值按原值发送。
 */
void ax650_uart1_set_projectile_speed(float speed_mps);

/**
 * @brief 更新姿态帧携带的裁判系统本车 ID
 * @param robot_id 裁判系统 0x0201 robot_id；0 表示尚未收到
 * @note AX650 用该值自动取反得到敌方颜色。
 */
void ax650_uart1_set_robot_id(uint8_t robot_id);

/**
 * @brief 设置姿态上报中的 AX650 ProgramMode
 * @param mode AX650 原始模式值；左中自瞄为 AX650_AUTO_AIM_MODE
 * @note uint8_t 在 Cortex-M4 上原子读写，由 CMD 任务写、ins_state 任务读。
 */
void ax650_uart1_set_mode(uint8_t mode);

/**
 * @brief 获取最近一帧下行控制指令及新鲜度
 * @param out 非空快照地址
 * @note 超时仅把快照标为无效，不擦除最近角度，控制层可保持上一目标。
 */
void ax650_uart1_get_state(ax650_uart1_state_t *out);

#endif
