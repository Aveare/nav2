/**
 * @file    usb_port.h
 * @brief   哨兵 USB 双协议主机测试端口桩
 * @layer   TESTS
 * @author  小猫大王
 */
#pragma once

#include <stdint.h>

typedef void (*USBCallback)(uint16_t length);
typedef struct
{
    USBCallback tx_cbk;
    USBCallback rx_cbk;
} usb_init_config_t;

uint8_t *usb_init(usb_init_config_t config);
uint8_t usb_try_transmit(uint8_t *buffer, uint16_t length);
uint8_t usb_is_transmitting(void);
