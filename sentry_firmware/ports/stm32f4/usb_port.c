/**
 * @file    usb_port.c
 * @brief   USB CDC 封装
 * @layer   PORTS(stm32f4)
 * @author  your name
 * @author  modified by 小猫大王
* @version 0.1
* @date 2023-02-09
* @copyright Copyright (c) 2023
 */
#include "usb_port.h"
#include "log_port.h"
#include "dwt_port.h"
#include "kernel.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static uint8_t *bsp_usb_rx_buffer; // 接收到的数据会被放在这里,buffer size为2048
// 注意usb单个数据包(Full speed模式下)最大为64byte,超出可能会出现丢包情况

uint8_t *usb_init(usb_init_config_t usb_conf)
{
    // usb的软件复位(模拟拔插)在usbd_conf.c中的HAL_PCD_MspInit()中
    bsp_usb_rx_buffer =
        CDCInitRxbufferNcallback(usb_conf.tx_cbk, usb_conf.rx_cbk); // 获取接收数据指针
    // usb的接收回调函数会在这里被设置,并将数据保存在bsp_usb_rx_buffer中
    LOG_INFO("USB init success");
    return bsp_usb_rx_buffer;
}

void usb_transmit(uint8_t *buffer, uint16_t len)
{
    CDC_Transmit_FS(buffer, len); // 发送
}

uint8_t usb_try_transmit(uint8_t *buffer, uint16_t len)
{
    uint32_t saved = kernel_enter_critical();
    uint8_t result = USBD_FAIL;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != NULL)
        result = CDC_Transmit_FS(buffer, len);
    kernel_exit_critical(saved);
    return result;
}

uint8_t usb_is_transmitting(void)
{
    uint32_t saved = kernel_enter_critical();
    uint8_t busy = 0u;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != NULL)
        busy = ((USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData)->TxState != 0u;
    kernel_exit_critical(saved);
    return busy;
}
