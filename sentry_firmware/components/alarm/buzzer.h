/**
 * @file    buzzer.h
 * @brief   蜂鸣器接口:注册与鸣叫状态
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef BUZZER_H
#define BUZZER_H
#include "pwm_port.h"
#define BUZZER_DEVICE_CNT 5

#define BUZZER_NOTE_C5 523
#define BUZZER_NOTE_D5 587
#define BUZZER_NOTE_E5 659
#define BUZZER_NOTE_F5 698
#define BUZZER_NOTE_G5 784
#define BUZZER_NOTE_A5 880
#define BUZZER_NOTE_B5 988

typedef enum
{
    OCTAVE_1 = 0,
    OCTAVE_2,
    OCTAVE_3,
    OCTAVE_4,
    OCTAVE_5,
    OCTAVE_6,
    OCTAVE_7,
    OCTAVE_8,
} octave_e;

typedef enum
{
    ALARM_LEVEL_HIGH = 0,
    ALARM_LEVEL_ABOVE_MEDIUM,
    ALARM_LEVEL_MEDIUM,
    ALARM_LEVEL_BELOW_MEDIUM,
    ALARM_LEVEL_LOW,
} buzzer_alarm_level_e;

typedef enum
{
    ALARM_OFF = 0,
    ALARM_ON,
} buzzer_alarm_state_e;
typedef struct
{
    buzzer_alarm_level_e alarm_level;
    octave_e octave;
    float loudness;
} buzzer_config_t;

typedef struct
{
    float loudness;
    octave_e octave;
    buzzer_alarm_level_e alarm_level;
    buzzer_alarm_state_e alarm_state;
} buzzer_t;

void buzzer_init(void *pwm_handle, uint32_t channel);
void buzzer_task();
buzzer_t *buzzer_register(buzzer_config_t *config);
void buzzer_set_alarm_status(buzzer_t *buzzer, buzzer_alarm_state_e state);

/**
 * @brief  取开机鸣叫码用的默认实例(buzzer_init 内注册)
 * @return 实例指针;buzzer_init 尚未执行时为 NULL(调用方必须判空)
 */
buzzer_t *buzzer_get_default(void);
#endif // !BUZZER_H
