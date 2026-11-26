/**
 * @file    usb_port.h
 * @brief   USB 接口
 * @layer   PORTS(stm32f4)
 * @author  your name
 * @author  modified by 小猫大王
* @version 0.1
* @date 2023-02-09
* @copyright Copyright (c) 2023
 */
#pragma once
#include "usb_device.h"
#include "usbd_cdc.h"
#include "usbd_conf.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

typedef struct
{
    USBCallback tx_cbk;
    USBCallback rx_cbk;
} usb_init_config_t;

/* @note 虚拟串口的波特率/校验位/数据位等动态可变,取决于上位机的设定 */
/* 使用时不需要关心这些设置(作为从机) */

uint8_t *usb_init(usb_init_config_t usb_conf); // bsp初始化时调用会重新枚举设备

void usb_transmit(uint8_t *buffer, uint16_t len); // 通过usb发送数据

/** 非阻塞发送;0=已受理,非0=未枚举/忙。缓冲区保持到 TX 完成回调。 */
uint8_t usb_try_transmit(uint8_t *buffer, uint16_t len);
/** 仅查询发送缓冲是否仍被 USB 持有;未枚举时返回 0。 */
uint8_t usb_is_transmitting(void);
