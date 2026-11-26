/**
 * @file    crc8.h
 * @brief   CRC8 接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef __CRC8_H
#define __CRC8_H

#include "main.h"

#define CRC_START_8 0x00

uint8_t crc8_calc(const uint8_t *input_str, uint16_t num_bytes);
uint8_t crc8_update(uint8_t crc, uint8_t val);

#endif
