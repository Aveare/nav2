/**
 * @file    servo_motor.h
 * @brief   舵机接口(预留)
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H

#include "main.h"
#include "tim.h"
#include <stdint-gcc.h>
#include "pwm_port.h"
#include "usart_port.h"

#define SERVO_MOTOR_CNT    7
#define SERVO_MOTOR_FRAME_FIRST  0x55
#define SERVO_MOTOR_FRAME_SECOND 0x55
#define SERVO_MOTOR_MAX_BUFF     10
#define SERVO_MOTOR_CMD_MOVE     0x03
#define SERVO_MOTOR_CMD_UNLOAD   0x14
#define SERVO_MOTOR_CMD_POS_READ 0x15
typedef enum
{
    Servo_None_Type = 0,
    Bus_Servo = 1,
    PWM_Servo = 2,
} servo_motor_type_e;

/* 用于初始化不同舵机的结构体,各类舵机通用 */
typedef struct
{
    pwm_init_config_t pwm_init_config;
    servo_motor_type_e servo_type;
    UART_HandleTypeDef *_handle;
    uint8_t servo_id;
} servo_motor_init_config_t;
typedef struct
{
    uint8_t servo_id;
    float angle;
    uint16_t recv_angle;
    pwm_t *pwm_instance;
    usart_t *usart_instance;
    servo_motor_type_e servo_type;
} servo_motor_t;

servo_motor_t *servo_init(servo_motor_init_config_t *Servo_Init_Config);
void servo_set_angle(servo_motor_t *servo, float angle);
#endif // SERVO_MOTOR_H
