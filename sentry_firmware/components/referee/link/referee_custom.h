/**
 * @file    referee_custom.h
 * @brief   自定义交互接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef REFEREE_CUSTOM_H
#define REFEREE_CUSTOM_H
#include "usart.h"
#include "referee_protocol.h"
#include "usart_port.h"

#pragma pack(1)

typedef struct
{
    referee_frame_header_t FrameHeader;
    uint16_t CmdID;
    robot2custom_data_t Controller_Data;
    uint16_t frametail;
} referee_custom_refresh_t; // 自定义控制器数据

#pragma pack()

void referee_custom_data_refresh(robot2custom_data_t Robot_Data);

#endif
