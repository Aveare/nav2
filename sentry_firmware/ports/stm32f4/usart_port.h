/**
 * @file    usart_port.h
 * @brief   USART 接口
 * @layer   PORTS(stm32f4)
 * @author  neozng
 * @author  modified by JIT CR (2026-10-05 增加 recv_len;2026-11-21 重构为循环 DMA 架构)
 * @author  modified by 小猫大王
* @note    2026-11-21 架构重构(DEV-20261121-06)。旧架构 = DMA_NORMAL + 每次 IDLE/收满后
*          软件重启接收,实车证明有两条死亡路径:①re-arm 返回 HAL_BUSY 被静默吞掉 → 停摆;
*          ②ORE 标志不清 → 错误中断风暴打满 CPU → 全车失控。
*          新架构 = DMA_CIRCULAR(硬件连续写入,永不停止)+ 任务侧消费:
*          ① 正常数据路径**没有任何重启动作**,"每帧重武装"的死亡窗口从设计上消失;
*          ② 错误 = 丢字节 + 计数(usart_get_err_count),DMA 继续跑,由解析器的帧重同步兜住;
*          ③ 解析全部在任务上下文(中断不做 CRC),与跃鹿哨兵/官方 RoboRTS 的 FIFO 方案同构;
*          ④ 消费由 200Hz robot_task 轮询驱动,**不依赖 IDLE/TC 中断**——中断丢事件只影响及时性。
* @note    环形缓冲回绕在本层消化:消费时把连续增量拷进线性工作缓冲 recv_buff,
*          上层回调看到的永远是首尾连续的字节。
 */
#ifndef BSP_RC_H
#define BSP_RC_H

#include <stdint.h>
#include "main.h"

#define USART_DEVICE_CNT   3   // C板至多分配3个串口
/* B4b 行为白名单(方案 §5):256→1024。921600bps 下 256B 仅约 2.8ms 深度,低于 5ms
 * 消费周期,属于丢帧行为变化;1024B ≈ 11ms 深度,覆盖消费抖动。登记于 §5 白名单 B4b。 */
#define USART_RX_RING_SIZE 1024 // 循环 DMA 环形缓冲,字节(=1024;消费周期 5ms)
#define USART_RX_ASSEMBLY_SIZE                                                                     \
    512 // 线性装配缓冲,字节(容纳裁判最大帧 + 残留;实例在 FreeRTOS 堆上,需控总量)

// 模块回调函数,用于解析协议(在任务上下文被调用,不在中断里)
typedef void (*usart_module_callback)();

/* 发送模式枚举 */
typedef enum
{
    USART_TRANSFER_NONE = 0,
    USART_TRANSFER_BLOCKING,
    USART_TRANSFER_IT,
    USART_TRANSFER_DMA,
} usart_transfer_mode_t;

// 串口实例结构体,每个module都要包含一个实例.
typedef struct
{
    uint8_t ring[USART_RX_RING_SIZE];          // DMA 循环写入目标(CIRCULAR,硬件连续写,永不停止)
    uint8_t recv_buff[USART_RX_ASSEMBLY_SIZE]; // 线性装配缓冲:跨拍拼帧(一次消费可能是半帧)
    uint16_t read_pos;                         // ring 的消费位置,0~USART_RX_RING_SIZE(仅任务侧推进)
    uint16_t assemble_len;                     // 装配缓冲中待解析的字节数(帧未收全时跨拍累积)
    uint8_t recv_buff_size;                    // 注册时的包大小提示(环形缓冲固定 LIMIT)
    uint16_t recv_len;                // 进回调 = 装配缓冲可用长度;出回调 = 模块实际消费的字节数
    UART_HandleTypeDef *usart_handle; // 实例对应的 usart_handle
    usart_module_callback module_callback; // 解析数据的回调(任务上下文)
    uint8_t slot;                          // 注册槽位(内部使用)
    uint8_t rx_enable;                     // 0 = 只发不收(VOFA 等),不启动接收
    volatile uint32_t err_cnt;             // ORE/FE/NE 错误累计(诊断;错误不再重启 DMA)
    volatile uint32_t overrun_cnt;         // B4b 增补:消费追不上、DMA 覆盖未读数据次数(方案 §4)
} usart_t;

/* usart 初始化配置结构体 */
typedef struct
{
    uint8_t recv_buff_size;                // 模块期望的一包数据大小(提示用)
    UART_HandleTypeDef *usart_handle;      // 实例对应的 usart_handle
    usart_module_callback module_callback; // 解析数据的回调(任务上下文)
    uint8_t tx_only;                       // **仅 =1 时关闭接收**;0/其它值一律启动接收
    /* 为什么用"仅等于 1 才关"而不是"等于 1 才开":工程里存在未清零的
     * `usart_init_config_t conf;` 声明(HC05/servo 等),字段是栈垃圾。
     * 反向语义下垃圾值不会意外关掉接收口,只有显式 .tx_only=1 才关。 */
} usart_init_config_t;

/**
 * @brief 注册一个串口实例,返回串口实例指针
 * @note  默认启动循环 DMA 接收;只有 config.tx_only==1(纯发送口,如 VOFA)才不启动
 * @param init_config 传入串口初始化结构体
 */
usart_t *usart_register(usart_init_config_t *init_config);

/**
 * @brief 启动/重启串口接收(循环 DMA)
 * @note  只在初始化与 daemon 失联恢复时调用。**正常数据路径不需要调用它** ——
 *        DMA_CIRCULAR 永不停止,不存在"每帧重启"的死亡窗口。
 * @param _instance 模块拥有的串口实例
 */
void usart_service_init(usart_t *_instance);

/**
 * @brief 消费所有实例的待处理数据并调用模块回调
 * @note  **由 200Hz robot_task 每拍调用一次**(任务上下文)。不依赖任何中断:
 *        IDLE/TC 事件丢失只会让数据延迟到下一拍消费,不会丢。
 * @note  装配协议:新字节追加到 recv_buff 尾部(跨拍拼帧 —— 5ms 轮询会把 1.56ms 的
 *        DBUS 帧切成两半,必须支持半帧累积);回调入口 recv_len = 可用总长,
 *        **回调可把它改小表示实际消费了多少字节**,剩余留在缓冲里等下一次拼帧;
 *        回调不改 recv_len 则视为全部消费(裁判/视觉等自带重同步的流式解析器)。
 */
void usart_process_all(void);

/**
 * @brief 读取该实例的接收错误累计次数(ORE/FE/NE)
 * @note  错误只计数、不重启 DMA —— 丢的字节由解析器的帧重同步兜住(DBUS 定长帧除外,
 *        见 remote_control.c 的注释)。
 * @param _instance 串口实例
 * @return 累计错误次数
 */
uint32_t usart_get_err_count(usart_t *_instance);
uint32_t usart_get_overrun_count(usart_t *_instance); /* B4b 增补:追平覆盖计数 */

/**
 * @brief 通过调用该函数可以发送一帧数据,需要传入一个usart实例,发送buff以及这一帧的长度
 * @note 在短时间内连续调用此接口,若采用IT/DMA会导致上一次的发送未完成而新的发送取消.
 * @note
 * 若希望连续使用DMA/IT进行发送,请配合usart_is_ready()使用,或自行为你的module实现一个发送队列和任务.
 * @param _instance 串口实例
 * @param send_buf 待发送的数据buffer
 * @param send_size 发送字节数
 * @param mode 发送模式
 */
void usart_send(usart_t *_instance, uint8_t *send_buf, uint16_t send_size,
               usart_transfer_mode_t mode);

/**
 * @brief 判断串口是否准备好,用于连续或异步的IT/DMA发送
 *
 * @param _instance 要判断的串口实例
 * @return uint8_t ready 1, busy 0
 */
uint8_t usart_is_ready(usart_t *_instance);

/**
 * @brief  将空闲串口配置为 8 位有效数据、偶校验、1 停止位
 * @param  handle  已初始化且尚未启用收发的 HAL 句柄，无量纲
 * @param  baud  波特率，bit/s
 * @return HAL 初始化成功返回 1；忙、空指针或波特率为零返回 0。
 * @note   在 usart_register 前调用；HAL 的 9B 加偶校验对应线上 8E1。
 */
uint8_t usart_configure8_e1(UART_HandleTypeDef *handle, uint32_t baud);

/**
 * @brief  尝试启动非阻塞 IT 发送并返回实际启动结果
 * @param  instance  独占串口实例，无量纲
 * @param  buffer  帧字节数组，无量纲；发送完成前必须保持有效且不变
 * @param  length  发送字节数
 * @return HAL 成功启动发送返回 1；忙、输入非法或 HAL 失败返回 0。
 */
uint8_t usart_try_send_it(usart_t *instance, uint8_t *buffer, uint16_t length);

#endif
