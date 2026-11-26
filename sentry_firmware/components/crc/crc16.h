/**
 * @file    crc16.h
 * @brief   CRC16 接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef __CRC16_H
#define __CRC16_H
#include "main.h"

#define CRC_START_16     0xFFFF
#define CRC_START_MODBUS 0xFFFF
#define CRC_POLY_16      0xA001

uint16_t crc16_calc(const uint8_t *input_str, uint16_t num_bytes);
uint16_t crc16_modbus(const uint8_t *input_str, uint16_t num_bytes);
uint16_t crc16_update(uint16_t crc, uint8_t c);
void crc16_init_table(void);

#endif
