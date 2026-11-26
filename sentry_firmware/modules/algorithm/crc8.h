/**
 * @file    crc8.h
 * @brief   CRC8 校验
 * @layer   MODULE
 * @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */

#ifndef __CRC8_H
#define __CRC8_H

#include "main.h"

#define CRC_START_8 0x00

uint8_t crc_8(const uint8_t *input_str, uint16_t num_bytes);
uint8_t update_crc_8(uint8_t crc, uint8_t val);

#endif
