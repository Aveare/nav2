/**
 * @file    imu_port.c
 * @brief   BMI088 SPI/片选/加热器 STM32F4 实现
 * @layer   PORTS(stm32f4)
 * @author  JIT CR
 * @date    2026-11-23
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目。
 */
#include "imu_port.h"
#include "main.h"
#include "spi.h"
#include "tim.h"

void imu_port_cs_accel(uint8_t select)
{
    HAL_GPIO_WritePin(CS1_ACCEL_GPIO_Port, CS1_ACCEL_Pin,
                      select ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void imu_port_cs_gyro(uint8_t select)
{
    HAL_GPIO_WritePin(CS1_GYRO_GPIO_Port, CS1_GYRO_Pin,
                      select ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

uint8_t imu_port_spi_transfer(uint8_t txdata)
{
    uint8_t rxdata = 0u;
    (void)HAL_SPI_TransmitReceive(&hspi1, &txdata, &rxdata, 1u, 1000u);
    return rxdata;
}

uint8_t imu_port_heater_start(void)
{
    return (uint8_t)(HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1) == HAL_OK);
}

void imu_port_heater_set(uint16_t compare)
{
    __HAL_TIM_SetCompare(&htim10, TIM_CHANNEL_1, compare);
}
