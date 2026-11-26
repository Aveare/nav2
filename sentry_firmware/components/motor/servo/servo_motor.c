/**
 * @file    servo_motor.c
 * @brief   舵机驱动(预留)
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "servo_motor.h"
#include "stdlib.h"
#include "memory.h"
#include "log_port.h"
uint8_t servo_angle_read[6] = {
    0x55, 0x55, 0x04, 0x15, 0x01, 0x01,
};
uint8_t servo_angle_write[16] = {
    0x55, 0x55, 0x08, 0x03, 0x01, 0xF4, 0x01, 0x01, 0x20, 0x03, 0x55, 0x55, 0x04, 0x15, 0x01, 0x01,
};
uint8_t servo_unload[6] = {0x55, 0x55, 0x04, 0x14, 0x01, 0x01};

/*第二版*/
static servo_motor_t *servo_motor_instance[SERVO_MOTOR_CNT];
static uint8_t servo_idx = 0; // register servo_idx,是该文件的全局舵机索引,在注册时使用
static void servo_motor_decode();
// 通过此函数注册一个舵机
servo_motor_t *servo_init(servo_motor_init_config_t *Servo_Init_Config)
{
    servo_motor_t *servo = (servo_motor_t *)malloc(sizeof(servo_motor_t));
    memset(servo, 0, sizeof(servo_motor_t));
    usart_init_config_t config = {0}; // tx_only 必须为 0:接收口(DEV-20261121-06)
    servo->servo_type = Servo_Init_Config->servo_type;
    switch (Servo_Init_Config->servo_type)
    {
    case Bus_Servo:
        config.module_callback = servo_motor_decode;
        config.recv_buff_size = SERVO_MOTOR_MAX_BUFF;
        config.usart_handle = Servo_Init_Config->_handle;
        servo->usart_instance = usart_register(&config);
        break;
    case PWM_Servo:
        servo->pwm_instance = pwm_register(&Servo_Init_Config->pwm_init_config);
        break;
    default:
        LOG_ERROR("Servo type error");
        break;
    }
    servo->servo_id = Servo_Init_Config->servo_id;
    servo_idx++;
    servo_motor_instance[servo->servo_id] = servo;

    return servo;
}
//@todo
// PWM舵机的角度设置需要根据相应定时器PWM等参数进行计算(是否需要规范定时器PWM的初始化参数，以便于计算)
void servo_set_angle(servo_motor_t *servo, float angle)
{
    switch (servo->servo_type)
    {
    case Bus_Servo:
        servo_angle_write[8] = (uint16_t)angle & 0xff;
        servo_angle_write[9] = (uint16_t)angle >> 8;
        usart_send(servo->usart_instance, servo_angle_write, 16, USART_TRANSFER_DMA);
        break;
    case PWM_Servo:
        servo->angle = angle;
        pwm_set_duty_ratio(servo->pwm_instance, angle);
        break;
    default:
        break;
    }
}

//@todo 只读取了角度 还有电压，动作是否完成等 且只支持一个串口
static void servo_motor_decode()
{
    for (uint8_t i = 0; i < servo_idx; i++)
    {
        if (servo_motor_instance[i]->servo_type == Bus_Servo)
        {
            if (servo_motor_instance[i]->usart_instance->recv_buff[0] == SERVO_MOTOR_FRAME_FIRST &&
                servo_motor_instance[i]->usart_instance->recv_buff[1] == SERVO_MOTOR_FRAME_SECOND)
            {
                if (servo_motor_instance[i]->usart_instance->recv_buff[3] == 21)
                {
                    servo_motor_instance[i]->recv_angle =
                        (servo_motor_instance[i]->usart_instance->recv_buff[7] << 8 |
                         servo_motor_instance[i]->usart_instance->recv_buff[6]);
                }
            }
        }
    }
}
