#pragma once
#include <stdint.h>
typedef void (*USBCallback)(uint32_t);
typedef struct { USBCallback tx_cbk, rx_cbk; } usb_init_config_t;
uint8_t *usb_init(usb_init_config_t config);
uint8_t usb_try_transmit(uint8_t *buffer,uint16_t length);
uint8_t usb_is_transmitting(void);
