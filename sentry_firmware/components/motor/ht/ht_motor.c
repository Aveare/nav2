/**
 * @file    ht_motor.c
 * @brief   海泰 HT04 电机驱动(预留)
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "ht_motor.h"
#include "memory.h"
#include "general_def.h"
#include "user_lib.h"
#include "kernel.h"
#include "string.h"
#include "daemon.h"
#include "stdlib.h"
#include "log_port.h"

static uint8_t idx;
static ht_motor_t *ht_motor_instance[HT_MOTOR_CNT];
static kernel_task_t ht_task_handle[HT_MOTOR_CNT];
static uint8_t zero_buff[6] = {0};

/**
 * @brief 设置电机模式,报文内容[0xff,0xff,0xff,0xff,0xff,0xff,0xff,cmd]
 *
 * @param cmd
 * @param motor
 */
static void ht_motor_set_mode(ht_motor_mode_e cmd, ht_motor_t *motor)
{
    memset(motor->motor_can_instace->tx_buff, 0xff, 7);  // 发送电机指令的时候前面7bytes都是0xff
    motor->motor_can_instace->tx_buff[7] = (uint8_t)cmd; // 最后一位是命令id
    can_transmit((bus_tx_frame_t *)motor->motor_can_instace, 8);
    memcpy(motor->motor_can_instace->tx_buff, zero_buff, 6);
}
/* 两个用于将uint值和float值进行映射的函数,在设定发送值和解析反馈值时使用 */
static uint16_t ht_motor_float_to_uint(float x, float x_min, float x_max, uint8_t bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return (uint16_t)((x - offset) * ((float)((1 << bits) - 1)) / span);
}
static float ht_motor_uint_to_float(int x_int, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

/**
 * @brief 解析电机反馈值
 *
 * @param motor_can 收到
 */
static void ht_motor_decode(can_t *motor_can)
{
    uint16_t tmp; // 用于暂存解析值,稍后转换成float数据,避免多次创建临时变量
    uint8_t const *rxbuff = motor_can->rx_buff;
    ht_motor_t *motor = (ht_motor_t *)motor_can->id;
    ht_motor_measure_t *measure = &(motor->measure); // 将can实例中保存的id转换成电机实例的指针

    daemon_reload(motor->motor_daemon);
    measure->feed_dt = dwt_get_delta_t(&measure->feed_cnt);

    measure->last_angle = measure->total_angle;
    tmp = (uint16_t)((rxbuff[1] << 8) | rxbuff[2]);
    measure->total_angle = ht_motor_uint_to_float(tmp, HT_MOTOR_P_MIN, HT_MOTOR_P_MAX, 16);

    tmp = (uint16_t)(rxbuff[3] << 4) | (rxbuff[4] >> 4);
    measure->speed_rads = math_average_filter((ht_motor_uint_to_float(tmp, HT_MOTOR_V_MIN, HT_MOTOR_V_MAX, 12) - HT_SPEED_BIAS),
                                        measure->speed_buff, HT_MOTOR_SPEED_BUFFER_SIZE);

    tmp = (uint16_t)(((rxbuff[4] & 0x0f) << 8) | rxbuff[5]);
    measure->real_current = HT_CURRENT_SMOOTH_COEF * ht_motor_uint_to_float(tmp, HT_MOTOR_T_MIN, HT_MOTOR_T_MAX, 12) +
                            (1 - HT_CURRENT_SMOOTH_COEF) * measure->real_current;
}

static void ht_motor_lost_callback(void *motor_ptr)
{
    ht_motor_t *motor = (ht_motor_t *)motor_ptr;
    LOG_WARN("[ht_motor] motor %d lost\n", motor->motor_can_instace->tx_id);
    if (++motor->lost_cnt % 10 != 0)
        ht_motor_set_mode(CMD_MOTOR_MODE, motor); // 尝试重新让电机进入控制模式
}

/**
 * @brief  编码器校准:单独发送标定帧,不走常规控制帧
 * @note   海泰协议的控制帧字段布局与常规 C620 不同,校准需按 p/v/kp/kd 重新组帧。
 */
void ht_motor_calib_encoder(ht_motor_t *motor)
{
    uint16_t p, v, kp, kd, t;
    p = ht_motor_float_to_uint(0, HT_MOTOR_P_MIN, HT_MOTOR_P_MAX, 16);
    v = ht_motor_float_to_uint(0, HT_MOTOR_V_MIN, HT_MOTOR_V_MAX, 12);
    kp = ht_motor_float_to_uint(0, HT_KP_MIN, HT_KP_MAX, 12);
    kd = ht_motor_float_to_uint(0, HT_KD_MIN, HT_KD_MAX, 12);
    t = ht_motor_float_to_uint(0, HT_MOTOR_T_MIN, HT_MOTOR_T_MAX, 12);

    uint8_t *buf = motor->motor_can_instace->tx_buff;
    buf[0] = p >> 8;
    buf[1] = p & 0xFF;
    buf[2] = v >> 4;
    buf[3] = ((v & 0xF) << 4) | (kp >> 8);
    buf[4] = kp & 0xFF;
    buf[5] = kd >> 4;
    buf[6] = ((kd & 0xF) << 4) | (t >> 8);
    buf[7] = t & 0xff;
    memcpy(zero_buff, buf,
           6); // 初始化的时候至少调用一次,故将其他指令为0时发送的报文保存一下,详见ht04电机说明
    can_transmit((bus_tx_frame_t *)motor->motor_can_instace, 8);
    dwt_delay(0.005);
    ht_motor_set_mode(CMD_ZERO_POSITION, motor); // sb 玩意校准完了编码器也不为0
    dwt_delay(0.005);
}

ht_motor_t *ht_motor_init(motor_init_config_t *config)
{
    ht_motor_t *motor = (ht_motor_t *)malloc(sizeof(ht_motor_t));
    memset(motor, 0, sizeof(ht_motor_t));

    motor->motor_settings = config->controller_setting_init_config;
    pid_init(&motor->current_PID, &config->controller_param_init_config.current_PID);
    pid_init(&motor->speed_PID, &config->controller_param_init_config.speed_PID);
    pid_init(&motor->angle_PID, &config->controller_param_init_config.angle_PID);
    motor->other_angle_feedback_ptr = config->controller_param_init_config.other_angle_feedback_ptr;
    motor->other_speed_feedback_ptr = config->controller_param_init_config.other_speed_feedback_ptr;

    config->can_init_config.can_module_callback = ht_motor_decode;
    config->can_init_config.id = motor;
    motor->motor_can_instace = can_register(&config->can_init_config);

    daemon_init_config_t conf = {
        .callback = ht_motor_lost_callback,
        .owner_id = motor,
        .reload_count = 5, // 50ms
    };
    motor->motor_daemon = daemon_register(&conf);

    ht_motor_enable(motor);
    ht_motor_set_mode(CMD_MOTOR_MODE, motor); // 确保电机已经上电并执行电机模式
    dwt_delay(0.05f);
    ht_motor_calib_encoder(motor); // 将当前编码器位置作为零位
    dwt_delay(0.05f); // 保证下一个电机发送时CAN是空闲的,注意应用在初始化模块的时候不应该进入中断
    ht_motor_instance[idx++] = motor;
    return motor;
}

void ht_motor_set_ref(ht_motor_t *motor, float ref)
{
    motor->pid_ref = ref;
}

/**
 * @brief 为了避免总线堵塞,为每个电机创建一个发送任务
 * @param argument 传入的电机指针
 */
__attribute__((noreturn)) void HTMotorTask(void *argument)
{
    float set, pid_measure, pid_ref;
    ht_motor_t *motor = (ht_motor_t *)argument;
    ht_motor_measure_t *measure = &motor->measure;
    motor_control_setting_t *setting = &motor->motor_settings;
    can_t *motor_can = motor->motor_can_instace;
    uint16_t tmp;

    while (1)
    {
        pid_ref = motor->pid_ref;
        if (setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            pid_ref *= -1;

        if ((setting->close_loop_type & ANGLE_LOOP) && setting->outer_loop_type == ANGLE_LOOP)
        {
            if (setting->angle_feedback_source == OTHER_FEED)
                pid_measure = *motor->other_angle_feedback_ptr;
            else
                pid_measure = measure->total_angle;
            // measure单位是rad,ref是角度,统一到angle下计算,方便建模
            pid_ref = pid_calculate(&motor->angle_PID, pid_measure * FW_RAD_TO_DEG, pid_ref);
        }

        if ((setting->close_loop_type & SPEED_LOOP) &&
            setting->outer_loop_type & (ANGLE_LOOP | SPEED_LOOP))
        {
            if (setting->feedforward_flag & SPEED_FEEDFORWARD)
                pid_ref += *motor->speed_feedforward_ptr;

            if (setting->angle_feedback_source == OTHER_FEED)
                pid_measure = *motor->other_speed_feedback_ptr;
            else
                pid_measure = measure->speed_rads;
            // measure单位是rad / s ,ref是angle per sec,统一到angle下计算
            pid_ref = pid_calculate(&motor->speed_PID, pid_measure * FW_RAD_TO_DEG, pid_ref);
        }

        if (setting->feedforward_flag & CURRENT_FEEDFORWARD)
            pid_ref += *motor->current_feedforward_ptr;
        if (setting->close_loop_type & CURRENT_LOOP)
        {
            pid_ref = pid_calculate(&motor->current_PID, measure->real_current, pid_ref);
        }

        set = pid_ref;

        MOTOR_LIMIT_MIN_MAX(set, HT_MOTOR_T_MIN, HT_MOTOR_T_MAX);
        tmp = ht_motor_float_to_uint(set, HT_MOTOR_T_MIN, HT_MOTOR_T_MAX, 12);
        if (motor->stop_flag == MOTOR_STOP)
            tmp = ht_motor_float_to_uint(0, HT_MOTOR_T_MIN, HT_MOTOR_T_MAX, 12);
        motor_can->tx_buff[6] = (tmp >> 8);
        motor_can->tx_buff[7] = tmp & 0xff;

        can_transmit((bus_tx_frame_t *)motor_can, 8);

        kernel_task_delay(1);
    }
}

void ht_motor_control_init()
{
    if (!idx)
        return;
    for (size_t i = 0; i < idx; i++)
    {
        kernel_task_cfg_t task_cfg = {
            .fn = HTMotorTask,
            .arg = ht_motor_instance[i],
            .name = "ht_motor",
            .stack_words = 128,
            .priority = KERNEL_PRIO_NORMAL,
        };
        ht_task_handle[i] = kernel_task_create(&task_cfg);
    }
}

void ht_motor_stop(ht_motor_t *motor)
{
    motor->stop_flag = MOTOR_STOP;
}

void ht_motor_enable(ht_motor_t *motor)
{
    motor->stop_flag = MOTOR_ENABLED;
}

void ht_motor_outer_loop(ht_motor_t *motor, motor_closeloop_type_e type)
{
    motor->motor_settings.outer_loop_type = type;
}
