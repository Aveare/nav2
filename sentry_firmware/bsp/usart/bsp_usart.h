/**
 * @file    bsp_usart.h
 * @brief   串口 BSP 头文件:实例注册、回调与接收长度接口
 * @author  neozng
 * @author  modified by JIT CR (增加 recv_len 记录每次实际接收长度)
 */
#ifndef BSP_RC_H
#define BSP_RC_H

#include <stdint.h>
#include "main.h"

#define DEVICE_USART_CNT 3     // C板至多分配3个串口
#define USART_RXBUFF_LIMIT 256 // 如果协议需要更大的buff,请修改这里

// 模块回调函数,用于解析协议
typedef void (*usart_module_callback)();

/* 发送模式枚举 */
typedef enum
{
    USART_TRANSFER_NONE=0,
    USART_TRANSFER_BLOCKING,
    USART_TRANSFER_IT,
    USART_TRANSFER_DMA,
} USART_TRANSFER_MODE;

// 串口实例结构体,每个module都要包含一个实例.
// 由于串口是独占的点对点通信,所以不需要考虑多个module同时使用一个串口的情况,因此不用加入id;当然也可以选择加入,这样在bsp层可以访问到module的其他信息
typedef struct
{
    uint8_t recv_buff[USART_RXBUFF_LIMIT]; // 预先定义的最大buff大小,如果太小请修改USART_RXBUFF_LIMIT
    uint8_t recv_buff_size;                // 模块接收一包数据的大小
    uint16_t recv_len;                    // 本次DMA/IDLE实际接收长度,用于模块校验完整帧
    UART_HandleTypeDef *usart_handle;      // 实例对应的usart_handle
    usart_module_callback module_callback; // 解析收到的数据的回调函数
} USARTInstance;

/* usart 初始化配置结构体 */
typedef struct
{
    uint8_t recv_buff_size;                // 模块接收一包数据的大小
    UART_HandleTypeDef *usart_handle;      // 实例对应的usart_handle
    usart_module_callback module_callback; // 解析收到的数据的回调函数
} USART_Init_Config_s;

/**
 * @brief 注册一个串口实例,返回一个串口实例指针
 *
 * @param init_config 传入串口初始化结构体
 */
USARTInstance *USARTRegister(USART_Init_Config_s *init_config);

/**
 * @brief 启动串口服务,需要传入一个usart实例.一般用于lost callback的情况(使用串口的模块daemon)
 *
 * @param _instance
 */
void USARTServiceInit(USARTInstance *_instance);


/**
 * @brief 通过调用该函数可以发送一帧数据,需要传入一个usart实例,发送buff以及这一帧的长度
 * @note 在短时间内连续调用此接口,若采用IT/DMA会导致上一次的发送未完成而新的发送取消.
 * @note 若希望连续使用DMA/IT进行发送,请配合USARTIsReady()使用,或自行为你的module实现一个发送队列和任务.
 * @todo 是否考虑为USARTInstance增加发送队列以进行连续发送?
 * 
 * @param _instance 串口实例
 * @param send_buf 待发送数据的buffer
 * @param send_size how many bytes to send
 */
void USARTSend(USARTInstance *_instance, uint8_t *send_buf, uint16_t send_size,USART_TRANSFER_MODE mode);

/**
 * @brief 判断串口是否准备好,用于连续或异步的IT/DMA发送
 *
 * @param _instance 要判断的串口实例
 * @return uint8_t ready 1, busy 0
 */
uint8_t USARTIsReady(USARTInstance *_instance);

/**
 * @brief  将空闲串口配置为 8 位有效数据、偶校验、1 停止位
 * @param  handle  已初始化且尚未启用收发的 HAL 句柄，无量纲
 * @param  baud  波特率，bit/s
 * @return HAL 初始化成功返回 1；忙、空指针或波特率为零返回 0。
 * @note   在 USARTRegister 前调用；HAL 的 9B 加偶校验对应线上 8E1。
 */
uint8_t USARTConfigure8E1(UART_HandleTypeDef *handle, uint32_t baud);

/**
 * @brief  尝试启动非阻塞 IT 发送并返回实际启动结果
 * @param  instance  独占串口实例，无量纲
 * @param  buffer  帧字节数组，无量纲；发送完成前必须保持有效且不变
 * @param  length  发送字节数
 * @return HAL 成功启动发送返回 1；忙、输入非法或 HAL 失败返回 0。
 */
uint8_t USARTTrySendIT(USARTInstance *instance, uint8_t *buffer, uint16_t length);

#endif
