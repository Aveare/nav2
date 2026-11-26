/**
 * @file    can_comm.h
 * @brief   双板通信接口(休眠支)
 * @layer   COMPONENT
 * @author  Neo neozng1@hnu.edu.cn
 * @author  modified by 小猫大王
* @version 0.1
* @date 2022-11-27
* @copyright Copyright (c) 2022 HNUYueLu EC all rights reserved
 */
#ifndef CAN_COMM_H
#define CAN_COMM_H

#include "can_port.h"
#include "daemon.h"

#define CAN_COMM_MAX_INSTANCE 4 // 注意均衡负载,一条总线上不要挂载过多的外设

#define CAN_COMM_MAX_BUFFSIZE 60  // 最大发送/接收字节数,如果不够可以增加此数值
#define CAN_COMM_HEADER       's' // 帧头
#define CAN_COMM_TAIL         'e' // 帧尾
#define CAN_COMM_OFFSET_BYTES 4   // 's'+ datalen + 'e' + crc8

#pragma pack(1)
/* CAN comm 结构体, 拥有CAN comm的app应该包含一个CAN comm指针 */
typedef struct
{
    can_t *can_ins;
    /* 发送部分 */
    uint8_t send_data_len; // 发送数据长度
    uint8_t send_buf_len;  // 发送缓冲区长度,为发送数据长度+帧头单包数据长度帧尾以及校验和(4)
    uint8_t raw_sendbuf[CAN_COMM_MAX_BUFFSIZE +
                        CAN_COMM_OFFSET_BYTES]; // 额外4个bytes保存帧头帧尾和校验和
    /* 接收部分 */
    uint8_t recv_data_len; // 接收数据长度
    uint8_t recv_buf_len;  // 接收缓冲区长度,为接收数据长度+帧头单包数据长度帧尾以及校验和(4)
    uint8_t raw_recvbuf[CAN_COMM_MAX_BUFFSIZE +
                        CAN_COMM_OFFSET_BYTES]; // 额外4个bytes保存帧头帧尾和校验和
    uint8_t unpacked_recv_data
        [CAN_COMM_MAX_BUFFSIZE]; // 解包后的数据,调用can_comm_get()后cast成对应的类型通过指针读取即可
    /* 接收和更新标志位*/
    uint8_t recv_state;   // 接收状态,
    uint8_t cur_recv_len; // 当前已经接收到的数据长度(包括帧头帧尾datalen和校验和)
    uint8_t
        update_flag; // 数据更新标志位,当接收到新数据时,会将此标志位置1,调用can_comm_get()后会将此标志位置0

    daemon_t *comm_daemon;
} can_comm_t;
#pragma pack()

/* CAN comm 初始化结构体 */
typedef struct
{
    can_init_config_t can_config; // CAN初始化结构体
    uint8_t send_data_len;        // 发送数据长度
    uint8_t recv_data_len;        // 接收数据长度

    uint16_t daemon_count; // 守护进程计数,用于初始化守护进程
} can_comm_init_config_t;

/**
 * @brief 初始化CANComm
 *
 * @param config CANComm初始化结构体
 * @return can_comm_t*
 */
can_comm_t *can_comm_init(can_comm_init_config_t *comm_config);

/**
 * @brief 通过CANComm发送数据
 *
 * @param instance cancomm实例
 * @param data 注意此地址的有效数据长度需要和初始化时传入的datalen相同
 */
void can_comm_send(can_comm_t *instance, uint8_t *data);

/**
 * @brief 获取CANComm接收的数据,需要自己使用强制类型转换将返回的void指针转换成指定类型
 *
 * @return void* 返回的数据指针
 * @attention 注意如果希望直接通过转换指针访问数据,如果数据是union或struct,要检查是否使用了pack(n)
 *            CANComm接收到的数据可以看作是pack(1)之后的,是连续存放的.
 *            如果使用了pack(n)可能会导致数据错乱,并且无法使用强制类型转换通过memcpy直接访问,转而需要手动解包.
 *            强烈建议通过CANComm传输的数据使用pack(1)
 */
void *can_comm_get(can_comm_t *instance);

/**
 * @brief 检查CANComm是否在线
 *
 * @param instance
 * @return uint8_t
 */
uint8_t can_comm_is_online(can_comm_t *instance);

#endif // !CAN_COMM_H
