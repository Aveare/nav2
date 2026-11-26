/**
 * @file    can_port.h
 * @brief   CAN 总线接口
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note    每个 CAN 句柄对应一条总线,注册数受 CAN_MX_REGISTER_CNT 限制。
* @note    源自跃鹿 basic_framework(逐函数搬运,符号 1:1);B4a 增补:丢帧计数/
*          限流、filter bank 预算断言、ConfigFilter 返回值必查、bus_tx_frame 伪实例。
 */
#ifndef CAN_PORT_H
#define CAN_PORT_H

#include <stdint.h>
#include "can.h"

// 最多能够支持的CAN设备数
#define CAN_MX_REGISTER_CNT 16       // 这个数量取决于CAN总线的负载
#define CAN_FILTER_MAX_CNT   (2 * 14) // 最多可以使用的CAN过滤器数量,目前远不会用到这么多
#define CAN_DEVICE_CNT      2        // 根据板子设定,F407IG有CAN1,CAN2,因此为2;F334只有一个,则设为1
#define CAN_SEND_FIFO_SIZE  16       // 发送消息队列的长度
// 如果只有1个CAN,还需要把bsp_can.c中所有的hcan2变量改为hcan1(别担心,主要是总线和FIFO的负载均衡,不影响功能)

/* can instance typedef, every module registered to CAN should have this variable */
#pragma pack(1)
typedef struct _
{
    CAN_HandleTypeDef *can_handle; // can句柄
    uint32_t tx_id;                // 发送id
    uint32_t tx_mailbox;           // CAN消息填入的邮箱号
    uint8_t tx_buff[8];            // 发送缓存,最大为8
    uint8_t rx_buff[8];            // 接收缓存,最大消息长度为8
    uint32_t rx_id;                // 接收id
    uint8_t rx_len;                // 接收长度,可能为0-8
    // 接收的回调函数,用于解析接收到的数据
    void (*can_module_callback)(
        struct _ *); // callback needs an instance to tell among registered ones
    void *id;        // 使用can外设的模块指针(即id指向的模块拥有此can实例,是父子关系)
} can_t;
#pragma pack()

/* B4a 伪实例(方案 §4):发送分组只需 tx_id+payload+硬件面,RX 字段不进发送帧;
 * dji_motor.c 的 sender_assignment[6] 改用本类型;tx_id 晚绑定不变量(GM6020 模式切换) */
typedef struct
{
    CAN_HandleTypeDef *can_handle; // can句柄(总线判定)
    uint32_t tx_id;                // 发送id(帧基址 0x1ff/0x200/0x2fe)
    uint32_t tx_mailbox;           // CAN消息填入的邮箱号
    uint8_t tx_buff[8];            // 发送缓存
} bus_tx_frame_t;

/* CAN实例初始化结构体,将此结构体指针传入注册函数 */
typedef struct
{
    CAN_HandleTypeDef *can_handle;              // can句柄
    uint32_t tx_id;                             // 发送id
    uint32_t rx_id;                             // 接收id
    void (*can_module_callback)(can_t *); // 处理接收数据的回调函数
    void *id; // 拥有can实例的模块地址,用于区分不同的模块(如果有需要的话),如果不需要可以不传入
} can_init_config_t;

typedef struct
{
    uint8_t data_buff[8];         // 数据包
    uint8_t DLC_length;           // 数据长度
    bus_tx_frame_t *can_instance; // 对应的发送分组帧指针(B4a:伪实例)
} can_fifo_data_t;
typedef struct
{
    can_fifo_data_t queue[CAN_SEND_FIFO_SIZE]; // 队列
    uint8_t front_idx;                         // 队列头索引
    uint8_t back_idx;                          // 队列尾索引
    uint8_t current_size;                      // 当前队列长度
} can_send_fifo_t;

/**
 * @brief Register a module to CAN service,remember to call this before using a CAN device
 *        注册(初始化)一个can实例,需要传入初始化配置的指针.
 * @param config init config
 * @return can_t* can instance owned by module
 */
can_t *can_register(can_init_config_t *config);

/**
 * @brief transmit mesg through CAN device,通过can实例发送消息
 *        发送前需要向CAN实例的tx_buff写入发送数据
 *
 * @param length 要发送的一帧数据的长度，最大为8
 * @param _instance* can instance owned by module
 */
void can_transmit(bus_tx_frame_t *_instance, uint8_t length);

/* B4a 增补:TX FIFO 满丢最老计数(方案 §4"满丢最老必须计数");bus: 0=can1 1=can2 */
unsigned CAN_GetTxFifoDropCount(uint8_t bus);

/* B5 bus_id 白名单(方案 §5):业务经本抽象判定总线,不直触 &hcan1/&hcan2 符号。
 * 等价表:bus_of(h)==0 ⇔ h==&hcan1(恒等映射,分组/日志行为不变,DEV-20261122-15)。 */
int can_port_bus_of(const CAN_HandleTypeDef *h);

#endif
