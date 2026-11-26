/**
 * @file    imu_port.h
 * @brief   BMI088 SPI/片选/加热器平台接口
 * @layer   PORTS(stm32f4)
 * @author  JIT CR
 * @date    2026-11-23
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目。
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void imu_port_cs_accel(uint8_t select);
void imu_port_cs_gyro(uint8_t select);
uint8_t imu_port_spi_transfer(uint8_t txdata);
uint8_t imu_port_heater_start(void);
void imu_port_heater_set(uint16_t compare);

#ifdef __cplusplus
}
#endif
