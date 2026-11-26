/**
 * @file    super_cap.c
 * @brief   超级电容通信与控制(CAN)
 * @layer   MODULE
 * @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */

/*
 * @Descripttion:
 * @version:
 * @Author: Chenfu
 * @Date: 2022-12-02 21:32:47
 * @LastEditTime: 2022-12-05 15:29:49
 */
#include "super_cap.h"
#include "memory.h"
#include "stdlib.h"

static SuperCapInstance *super_cap_instance = NULL; // 可以由app保存此指针

static void SuperCapRxCallback(CANInstance *_instance)
{
    uint8_t *rxbuff;
    SuperCap_Msg_s *Msg;
    rxbuff = _instance->rx_buff;
    Msg = &super_cap_instance->cap_msg;
    Msg->cap_vol = (uint16_t)(rxbuff[0] << 8 | rxbuff[1])/1000.0;
    Msg->power_in = (uint16_t)(rxbuff[2] << 8 | rxbuff[3])/1000.0;
    Msg->power_chassis = (uint16_t)(rxbuff[4] << 8 | rxbuff[5])/1000.0;
}

SuperCapInstance *SuperCapInit(SuperCap_Init_Config_s *supercap_config)
{
    super_cap_instance = (SuperCapInstance *)malloc(sizeof(SuperCapInstance));
    memset(super_cap_instance, 0, sizeof(SuperCapInstance));
    
    supercap_config->can_config.can_module_callback = SuperCapRxCallback;
    super_cap_instance->can_ins = CANRegister(&supercap_config->can_config);
    return super_cap_instance;
}

void SuperCapSend(SuperCapInstance *instance, uint8_t *data)
{
    memcpy(instance->can_ins->tx_buff, data, 8);
    CANTransmit(instance->can_ins,8);
}

SuperCap_Msg_s SuperCapGet(SuperCapInstance *instance)
{
    return instance->cap_msg;
}