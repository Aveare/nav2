#pragma once
#include <stdint.h>
typedef void (*USBCallback)(uint16_t);
typedef struct { USBCallback tx_cbk; USBCallback rx_cbk; } USB_Init_Config_s;
uint8_t *USBInit(USB_Init_Config_s config);
uint8_t USBIsTransmitting(void);
uint8_t USBTryTransmit(uint8_t *buffer, uint16_t length);
