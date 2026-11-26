/**
 * @file    referee_crc.h
 * @brief   裁判 CRC 接口
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef REFEREE_CRC_H
#define REFEREE_CRC_H

// 裁判系统官方 CRC 校验;通用 CRC 实现在 components/crc(勿与此处 LUT 混用)

#include <stdint.h>
#include <stdbool.h>

// CRC8
void referee_crc8_append(uint8_t *pchMessage, uint16_t dwLength);
uint32_t referee_verify_crc8_check_sum(uint8_t *pchMessage, uint16_t dwLength);
uint8_t referee_get_crc8_check_sum(uint8_t *pchMessage, uint16_t dwLength, uint8_t ucCRC8);

// CRC16
void referee_crc16_append(uint8_t *pchMessage, uint32_t dwLength);
uint32_t referee_verify_crc16_check_sum(uint8_t *pchMessage, uint32_t dwLength);
uint16_t referee_get_crc16_check_sum(uint8_t *pchMessage, uint32_t dwLength, uint16_t wCRC);

#endif
