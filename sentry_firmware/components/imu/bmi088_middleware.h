/**
 * @file    bmi088_middleware.h
 * @brief   BMI088 中间件接口与总线选择
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef BMI088MIDDLEWARE_H
#define BMI088MIDDLEWARE_H

#include <stdint.h>

#define BMI088_USE_SPI
// #define BMI088_USE_IIC

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

extern uint8_t bmi088_read_write_byte(uint8_t reg);

#elif defined(BMI088_USE_IIC)

#endif

#endif
