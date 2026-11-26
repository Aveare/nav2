/**
 * @file    super_cap.c
 * @brief   超级电容通信(预留,0x301/0x302)
 * @layer   COMPONENT
 * @author  modified by 小猫大王
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

static super_cap_t *super_cap_instance = NULL; // 可以由app保存此指针

static void super_cap_rx_callback(can_t *_instance)
{
    uint8_t *rxbuff;
    super_cap_msg_t *Msg;
    rxbuff = _instance->rx_buff;
    Msg = &super_cap_instance->cap_msg;
    Msg->cap_vol = (uint16_t)(rxbuff[0] << 8 | rxbuff[1]) / 1000.0;
    Msg->power_in = (uint16_t)(rxbuff[2] << 8 | rxbuff[3]) / 1000.0;
    Msg->power_chassis = (uint16_t)(rxbuff[4] << 8 | rxbuff[5]) / 1000.0;
}

super_cap_t *super_cap_init(super_cap_init_config_t *supercap_config)
{
    super_cap_instance = (super_cap_t *)malloc(sizeof(super_cap_t));
    memset(super_cap_instance, 0, sizeof(super_cap_t));

    supercap_config->can_config.can_module_callback = super_cap_rx_callback;
    super_cap_instance->can_ins = can_register(&supercap_config->can_config);
    return super_cap_instance;
}

void super_cap_send(super_cap_t *instance, uint8_t *data)
{
    memcpy(instance->can_ins->tx_buff, data, 8);
    can_transmit((bus_tx_frame_t *)instance->can_ins, 8);
}

super_cap_msg_t super_cap_get(super_cap_t *instance)
{
    return instance->cap_msg;
}
