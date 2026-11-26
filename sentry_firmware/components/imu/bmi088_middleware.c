/**
 * @file    bmi088_middleware.c
 * @brief   BMI088 SPI 中间件:片选与读写时序
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "bmi088_middleware.h"
#include "imu_port.h"

void BMI088_ACCEL_NS_L(void)
{
    imu_port_cs_accel(1u);
}
void BMI088_ACCEL_NS_H(void)
{
    imu_port_cs_accel(0u);
}

void BMI088_GYRO_NS_L(void)
{
    imu_port_cs_gyro(1u);
}
void BMI088_GYRO_NS_H(void)
{
    imu_port_cs_gyro(0u);
}

uint8_t bmi088_read_write_byte(uint8_t txdata)
{
    return imu_port_spi_transfer(txdata);
}
