/**
 * @file    referee_custom.c
 * @brief   裁判自定义交互:0x0302 客户端数据
 * @layer   COMPONENT
 * @author  LHXY (cjy719@hnu.edu.cn)
 * @author  modified by 小猫大王
* @version 0.1
* @date 2026-03-27
* @copyright Copyright (c) 2026
 */
#include "referee_vt.h"
#include "string.h"
#include "referee_crc.h"
#include "stdio.h"
#include "referee_custom.h"

static referee_custom_refresh_t referee_custom_refresh_data; // 机器人自定义控制器数据
int referee_vt_seq = 0;
/************************************************机器人推送数据*********************************/
void referee_custom_data_refresh(robot2custom_data_t Robot_Data)
{
    uint8_t temp_datalength = Robot_Custom_Data_LEN; // 自定义控制器数据长度

    referee_custom_refresh_data.FrameHeader.SOF = REFEREE_SOF;
    referee_custom_refresh_data.FrameHeader.DataLength = temp_datalength;
    referee_custom_refresh_data.FrameHeader.Seq = referee_vt_seq;
    referee_custom_refresh_data.FrameHeader.CRC8 =
        referee_get_crc8_check_sum((uint8_t *)&referee_custom_refresh_data, LEN_CRC8, 0xFF);

    referee_custom_refresh_data.CmdID = ID_robot2custom;

    referee_custom_refresh_data.Controller_Data = Robot_Data;

    referee_custom_refresh_data.frametail = referee_get_crc16_check_sum(
        (uint8_t *)&referee_custom_refresh_data, LEN_HEADER + LEN_CMDID + temp_datalength, 0xFFFF);

    referee_vt_send_custom((uint8_t *)&referee_custom_refresh_data,
                          LEN_HEADER + LEN_CMDID + temp_datalength + LEN_TAIL); // 发送
    referee_vt_seq++;                                                                   // 包序号+1
    if (referee_vt_seq > 255)
    {
        referee_vt_seq = 0;
    }
}
