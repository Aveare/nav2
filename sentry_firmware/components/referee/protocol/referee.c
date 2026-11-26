/**
 * @file    referee.c
 * @brief   裁判系统协议解析:0x0XXX 分发与数据缓存
 * @layer   COMPONENT
 * @author  kidneygood
 * @author  modified by 小猫大王
* @version 0.1
* @date 2022-11-18
* @copyright Copyright (c) 2022
 */
#include "referee.h"
#include "string.h"
#include "referee_crc.h"
#include "usart_port.h"
#include "daemon.h"
#include "log_port.h"
#include "kernel.h"

#define REFEREE_RX_BUFFER_SIZE 255u // 裁判系统接收缓冲区大小

static usart_t *referee_usart_instance; // 裁判系统串口实例
static daemon_t *referee_daemon;        // 裁判系统守护进程
static referee_info_t referee_info;           // 裁判系统数据

/**
 * @brief  读取裁判数据(任务上下文,由 usart_process_all 消费驱动)
 * @param  buff: 装配缓冲中的连续字节
 * @param  buff_len: 可用字节数
 * @retval 无
 * @note   DEV-20261121-04:ORE 丢字节后帧边界偏移,SOF 可能不在包首 —— 向后扫描下一个
 *         0xA5 重新对齐,残帧跳过。
 * @note   DEV-20261121-08:**改为迭代实现**。此前用递归做错位自愈,裁判缓冲一旦被噪声
 *         填成"满是 0xA5"的形状,递归深度可达数百层、每层数十字节栈 → 任务栈溢出 →
 *         HardFault → 死等看门狗(实车症状:射击瞬间失控、遥控无效、2s 后蜂鸣三响)。
 *         迭代版栈占用恒定,且对垃圾数据只做线性扫描。
 */
static uint16_t referee_judge_read(uint8_t *buff, uint16_t buff_len)
{
    if (buff == NULL)
        return 0;

    const uint16_t total = buff_len; // 保存入口长度,返回值=已消费字节数

    while (buff_len >= LEN_HEADER)
    {
        uint16_t judge_length = 0; // 当前帧总长,字节;0 = 尚未取得合法长度
        uint8_t crc16_ok = 0;      // 当前帧 CRC16 是否通过

        /* 对齐:当前位置不是帧头时,向后扫描下一个 0xA5(迭代,不递归) */
        if (buff[SOF] != REFEREE_SOF)
        {
            uint16_t i = 1;
            while ((uint16_t)(i + LEN_HEADER) <= buff_len && buff[i] != REFEREE_SOF)
                i++;
            if ((uint16_t)(i + LEN_HEADER) > buff_len)
                return (uint16_t)(total - buff_len); // 剩余字节凑不出帧头,留给下一次消费拼接
            buff += i;
            buff_len = (uint16_t)(buff_len - i);
            continue;
        }

        // 写入帧头数据(5-byte),用于判断是否开始存储裁判数据
        memcpy(&referee_info.FrameHeader, buff, LEN_HEADER);

        // 帧头CRC8校验(上方已保证 buff[0]=0xA5)
        if (referee_verify_crc8_check_sum(buff, LEN_HEADER) == true)
        {
            // 统计一帧数据长度(byte),用于CR16校验
            judge_length = buff[DATA_LENGTH] + LEN_HEADER + LEN_CMDID + LEN_TAIL;

            // 长度防御:DataLength 损坏时 CRC16 会读到接收缓冲之外
            if (judge_length > buff_len)
                return (uint16_t)(total - buff_len);

            // 帧尾CRC16校验
            if (referee_verify_crc16_check_sum(buff, judge_length) == true)
            {
                crc16_ok = 1;
                // 2个8位拼成16位int
                referee_info.CmdID = (buff[6] << 8 | buff[5]);
                // 解析数据命令码,将数据拷贝到相应结构体中(注意拷贝数据的长度)
                // 第8个字节开始才是数据 data=7
                switch (referee_info.CmdID)
                {
                case ID_game_state: // 0x0001
                    memcpy(&referee_info.GameState, (buff + DATA_Offset), LEN_game_state);
                    break;
                case ID_game_result: // 0x0002
                    memcpy(&referee_info.GameResult, (buff + DATA_Offset), LEN_game_result);
                    break;
                case ID_game_robot_survivors: // 0x0003
                    memcpy(&referee_info.GameRobotHP, (buff + DATA_Offset), LEN_game_robot_HP);
                    break;
                case ID_event_data: // 0x0101
                    memcpy(&referee_info.EventData, (buff + DATA_Offset), LEN_event_data);
                    break;
                case ID_referee_warning_t: // 0x104
                    memcpy(&referee_info.WarnData, (buff + DATA_Offset), LEN_referee_warning_t);
                    break;
                case ID_dart_info_t: // 0x105
                    memcpy(&referee_info.DartData, (buff + DATA_Offset), LEN_dart_info_t);
                    break;
                case ID_game_robot_state: // 0x0201
                    memcpy(&referee_info.GameRobotState, (buff + DATA_Offset),
                           LEN_game_robot_state);
                    break;
                case ID_power_heat_data: // 0x0202
                    memcpy(&referee_info.PowerHeatData, (buff + DATA_Offset), LEN_power_heat_data);
                    referee_info.PowerHeatTick++; // 帧号:供上层判"新帧",见 rm_referee.h
                    break;
                case ID_game_robot_pos: // 0x0203
                    memcpy(&referee_info.GameRobotPos, (buff + DATA_Offset), LEN_game_robot_pos);
                    break;
                case ID_buff_musk: // 0x0204
                    memcpy(&referee_info.BuffMusk, (buff + DATA_Offset), LEN_buff_musk);
                    break;
                case ID_robot_hurt: // 0x0206
                    memcpy(&referee_info.RobotHurt, (buff + DATA_Offset), LEN_robot_hurt);
                    break;
                case ID_shoot_data: // 0x0207
                    memcpy(&referee_info.ShootData, (buff + DATA_Offset), LEN_shoot_data);
                    break;
                case ID_projectile_allowance_t: // 0x208
                    memcpy(&referee_info.AllowanceData, (buff + DATA_Offset),
                           LEN_projectile_allowance_t);
                    break;
                case ID_rfid_status_t: // 0x209
                    memcpy(&referee_info.RFIDdata, (buff + DATA_Offset), LEN_rfid_status_t);
                    break;
                case ID_student_interactive: // 0x0301   syhtodo接收代码未测试
                    memcpy(&referee_info.ReceiveData, (buff + DATA_Offset), LEN_receive_data);
                    break;
                case ID_map_interact_data: // 0x303
                    memcpy(&referee_info.MapData, (buff + DATA_Offset), LEN_map_interact_data);
                    break;
                }
            }
        }

        /* 前进:CRC16 通过则跳过整帧(多帧继续下一轮),否则只跳 1 字节重新找帧头。
         * 迭代推进保证栈占用恒定 —— 见函数头 DEV-20261121-08 说明。 */
        {
            uint16_t step =
                (crc16_ok && judge_length >= (uint16_t)(LEN_HEADER + LEN_CMDID + LEN_TAIL))
                    ? judge_length
                    : 1u;
            buff += step;
            buff_len = (uint16_t)(buff_len - step);
        }
    }

    return (uint16_t)(total - buff_len);
}

/*裁判系统串口接收回调函数,解析数据 */
static void referee_rx_callback(void)
{
    daemon_reload(referee_daemon);
    /* usart 契约:回调把 recv_len 改小 = 已消费字节数,余量由 usart_process_all 保留拼接。
     * 原来不回写会把半帧一起丢掉,0x0201/0x0202 可能长时间拼不齐(DEV-20261124-17)。 */
    const uint16_t consumed =
        referee_judge_read(referee_usart_instance->recv_buff, referee_usart_instance->recv_len);
    referee_usart_instance->recv_len = consumed;
}
// 裁判系统丢失回调函数,重新初始化裁判系统串口
static void referee_lost_callback(void *arg)
{
    usart_service_init(referee_usart_instance);
    LOG_WARN("[rm_ref] lost referee data");
}

/* 裁判系统通信初始化 */
referee_info_t *referee_init(UART_HandleTypeDef *referee_usart_handle)
{
    usart_init_config_t conf = {0}; // 必须清零:tx_only 未初始化会随机关掉接收(DEV-20261121-12)
    conf.module_callback = referee_rx_callback;
    conf.usart_handle = referee_usart_handle;
    conf.recv_buff_size = REFEREE_RX_BUFFER_SIZE; // mx 255(u8)
    referee_usart_instance = usart_register(&conf);

    daemon_init_config_t daemon_conf = {
        .callback = referee_lost_callback,
        .owner_id = referee_usart_instance,
        .reload_count = 30, // 0.3s没有收到数据,则认为丢失,重启串口接收
    };
    referee_daemon = daemon_register(&daemon_conf);

    return &referee_info;
}

/**
 * @brief 发送 UI 绘制数据或车间通信数据
 * @param send   发送数据首地址
 * @param tx_len 发送长度,字节
 * @note  **用中断发送而不是 DMA**(DEV-20261121-12):CubeMX 只为 USART6 配了 RX DMA,
 *        hdmatx 为空,`HAL_UART_Transmit_DMA` 会直接返回 HAL_ERROR —— 表现为"客户端 UI
 *        一个图元都没有,也不报错"。裁判单包 ≤128B,115200 下 IT 发送约 11ms,
 *        下面的 34ms 延时足够覆盖。
 * @note  函数内含 RTOS 延时(裁判接收上限约 10Hz),**禁止在 200Hz 控制周期内调用**。
 */
void referee_send(uint8_t *send, uint16_t tx_len)
{
    usart_send(referee_usart_instance, send, tx_len, USART_TRANSFER_IT);
    kernel_task_delay(34);
}
