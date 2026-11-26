/**
 * @file    buzzer.c
 * @brief   蜂鸣器:实例注册与鸣叫控制(复位原因播报)
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "pwm_port.h"
#include "buzzer.h"
#include "dwt_port.h"
#include "string.h"

static pwm_t *buzzer;
static buzzer_t *buzzer_list[BUZZER_DEVICE_CNT] = {0};
static buzzer_t *buzzer_default_instance; // 开机复位原因播报用(DEV-20261121-07)

/**
 * @brief 蜂鸣器初始化
 *
 */
void buzzer_init(void *pwm_handle, uint32_t channel)
{
    pwm_init_config_t buzzer_config = {
        .htim = pwm_handle,
        .channel = channel,
        .dutyratio = 0,
        .period = 0.001,
    };
    buzzer = pwm_register(&buzzer_config);

    /* 注册一个默认实例供开机鸣叫码使用:此前本工程没有注册任何实例,
     * buzzer_task 遍历到 NULL 时靠"地址 0 在 F4 映射到 Flash 可读"侥幸没崩,
     * 属未定义行为,一并消除(DEV-20261121-07)。 */
    buzzer_config_t def = {
        .alarm_level = ALARM_LEVEL_LOW,
        .octave = OCTAVE_4,
        .loudness = 0.5f,
    };
    buzzer_default_instance = buzzer_register(&def);
}

buzzer_t *buzzer_get_default(void)
{
    return buzzer_default_instance;
}

buzzer_t *buzzer_register(buzzer_config_t *config)
{
    if (config == NULL || config->alarm_level >= BUZZER_DEVICE_CNT)
    {
        LOG_ERROR("[buzzer] invalid alarm level %u",
                  (unsigned)(config == NULL ? 0u : config->alarm_level));
        return NULL;
    }
    buzzer_t *buzzer_new = (buzzer_t *)malloc(sizeof(buzzer_t));
    if (buzzer_new == NULL)
        return NULL;
    memset(buzzer_new, 0, sizeof(buzzer_t));

    buzzer_new->alarm_level = config->alarm_level;
    buzzer_new->loudness = config->loudness;
    buzzer_new->octave = config->octave;
    buzzer_new->alarm_state = ALARM_OFF;

    buzzer_list[config->alarm_level] = buzzer_new;
    return buzzer_new;
}

void buzzer_set_alarm_status(buzzer_t *buzzer, buzzer_alarm_state_e state)
{
    buzzer->alarm_state = state;
}

void buzzer_task()
{
    buzzer_t *buzz;
    for (size_t i = 0; i < BUZZER_DEVICE_CNT; ++i)
    {
        buzz = buzzer_list[i];
        if (buzz == NULL) // 未注册的槽位必须跳过:否则解引用 NULL(F4 地址 0 可读,侥幸不崩但属 UB)
            continue;
        if (buzz->alarm_level > ALARM_LEVEL_LOW)
        {
            continue;
        }
        if (buzz->alarm_state == ALARM_OFF)
        {
            pwm_set_duty_ratio(buzzer, 0);
        }
        else
        {
            pwm_set_duty_ratio(buzzer, buzz->loudness);
            switch (buzz->octave)
            {
            case OCTAVE_1:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_C5);
                break;
            case OCTAVE_2:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_D5);
                break;
            case OCTAVE_3:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_E5);
                break;
            case OCTAVE_4:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_F5);
                break;
            case OCTAVE_5:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_G5);
                break;
            case OCTAVE_6:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_A5);
                break;
            case OCTAVE_7:
                pwm_set_period(buzzer, (float)1 / BUZZER_NOTE_B5);
                break;
            default:
                break;
            }
            break;
        }
    }
}
