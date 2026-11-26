/**
 * @file    referee_vt.c
 * @brief   图传链路裁判发送:自定义/自建客户端双路径
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "referee_vt.h"
#include "string.h"
#include "referee_crc.h"
#include "usart_port.h"
#include "daemon.h"
#include "log_port.h"
#include "kernel.h"
#define REFEREE_VT_BUFFER_SIZE 255U // 计算的其实60U就够装两个信息了，怕不够先定100

static usart_t *referee_vt_usart_instance;
static daemon_t *referee_vt_daemon;
static referee_vt_info_t
    referee_vt_info; // 这三个基本是模仿rm_referee.c,常规链路和图传链路的信息分开
static void
JudgeVtData(uint8_t *vt_buff) // 都static了就不考虑复用性了，还是得考虑单板的车同时跑两个链路
{
    uint16_t judge_length; // 统计一帧数据长度
    if (vt_buff == NULL)   // 空数据包，则不作任何处理
    {
        return;
    }
    /* 迭代而非递归(2026-11-23):递归版在连续 0xA5 噪声时栈深不可控,与 referee.c
     * 同款缺陷(DEV-20261121-08 已修 referee.c),本文件随同修复;保持原判帧语义,
     * 不做逐字节重扫,仅消除递归并加帧长边界检查。 */
    while (1)
    {
        // 写入帧头数据(5-byte),用于判断是否开始存储裁判数据
        memcpy(&referee_vt_info.FrameHeader, vt_buff, LEN_HEADER);

        // 判断帧头数据(0)是否为0xA5:非 0xA5 直接结束
        if (vt_buff[SOF] != REFEREE_SOF)
        {
            return;
        }
        // 帧头CRC8校验
        if (referee_verify_crc8_check_sum(vt_buff, LEN_HEADER) == true)
        {
            // 统计一帧数据长度(byte),用于CR16校验
            judge_length = vt_buff[DATA_LENGTH] + LEN_HEADER + LEN_CMDID + LEN_TAIL;
            // 帧尾CRC16校验
            if (referee_verify_crc16_check_sum(vt_buff, judge_length) == true)
            {
                // 2个8位拼成16位int
                referee_vt_info.CmdID = (vt_buff[6] << 8 | vt_buff[5]);
                // 解析数据命令码,将数据拷贝到相应结构体中(注意拷贝数据的长度)
                // 第8个字节开始才是数据 data=7
                switch (
                    referee_vt_info
                        .CmdID) // todo:加入自定义客户端相关的解析，但考虑新赛季裁判系统可能大改先暂时放置
                {
                case ID_custom2robot: // 0x0302
                    memcpy(&referee_vt_info.Custom2Robot, (vt_buff + DATA_Offset),
                           LEN_custom2robot_data);
                    break;
                case ID_selfclient2robot: // 0x0311
                    memcpy(&referee_vt_info.SelfClient2Robot, (vt_buff + DATA_Offset),
                           LEN_selfclient2robot);
                    break;
                }
            }
        }
        // 当前帧总长;DataLength 异常(超出接收缓冲)直接丢弃,防止推进指针越界读
        uint32_t frame_total = sizeof(referee_frame_header_t) + LEN_CMDID +
                               referee_vt_info.FrameHeader.DataLength + LEN_TAIL;
        if (frame_total > REFEREE_VT_BUFFER_SIZE)
        {
            return;
        }
        // 首地址加帧长度,指向CRC16下一字节,用来判断是否为0xA5,从而判断一个数据包是否有多帧数据
        if (*(vt_buff + frame_total) != 0xA5)
        {
            return;
        }
        vt_buff += frame_total; // 多帧:推进到下一帧继续解析(替代原递归调用)
    }
}
// 图传链路需要发送一些东西
/**
 * @brief 裁判系统图传链路数据发送函数
 * @param
 */
void referee_vt_send_self_client(uint8_t *send, uint16_t tx_len)
{
    usart_send(referee_vt_usart_instance, send, tx_len, USART_TRANSFER_DMA);
    kernel_task_delay(20);
}

void referee_vt_send_custom(uint8_t *send, uint16_t tx_len)
{
    usart_send(referee_vt_usart_instance, send, tx_len, USART_TRANSFER_DMA);
    kernel_task_delay(100);
}

static void referee_vt_callback()
{
    daemon_reload(referee_vt_daemon);
    JudgeVtData(referee_vt_usart_instance->recv_buff);
}
// 裁判系统丢失处理
static void referee_vt_lost(void *arg)
{
    usart_service_init(referee_vt_usart_instance);
    LOG_WARN("referee vt lost, re-init usart");
}

referee_vt_info_t *referee_vt_init(UART_HandleTypeDef *referee_vt_usart_handle)
{
    usart_init_config_t conf = {0}; // tx_only 必须为 0:接收口(DEV-20261121-06)
    conf.module_callback = referee_vt_callback;
    conf.usart_handle = referee_vt_usart_handle;
    conf.recv_buff_size = REFEREE_VT_BUFFER_SIZE;
    referee_vt_usart_instance = usart_register(&conf);
    daemon_init_config_t daemon_conf = {
        .callback = referee_vt_lost,
        .owner_id = referee_vt_usart_instance,
        .reload_count = 30,
    };
    referee_vt_daemon = daemon_register(&daemon_conf);
    return &referee_vt_info;
}
