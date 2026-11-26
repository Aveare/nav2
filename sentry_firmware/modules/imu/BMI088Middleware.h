/**
 * @file    BMI088Middleware.h
 * @brief   BMI088 与 SPI 总线之间的读写适配层
 * @layer   MODULE
 * @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */

#ifndef BMI088MIDDLEWARE_H
#define BMI088MIDDLEWARE_H

#include "main.h"

#define BMI088_USE_SPI
//#define BMI088_USE_IIC

/*
#define CS1_ACCEL_GPIO_Port ACCEL_NSS_GPIO_Port
#define CS1_ACCEL_Pin ACCEL_NSS_Pin
#define CS1_GYRO_GPIO_Port GYRO_NSS_GPIO_Port
#define CS1_GYRO_Pin GYRO_NSS_Pin
*/


#if defined(BMI088_USE_SPI)
extern void BMI088_ACCEL_NS_L(void);
extern void BMI088_ACCEL_NS_H(void);

extern void BMI088_GYRO_NS_L(void);
extern void BMI088_GYRO_NS_H(void);

extern uint8_t BMI088_read_write_byte(uint8_t reg);

extern SPI_HandleTypeDef *BMI088_SPI;

#elif defined(BMI088_USE_IIC)

#endif

#endif
