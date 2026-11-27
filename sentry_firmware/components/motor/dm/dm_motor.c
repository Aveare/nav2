/**
 * @file    dm_motor.c
 * @brief   达妙 MIT 协议与 C 板电机实例适配
 * @author  小猫大王
 * @layer   COMPONENT
 * @note    协议移植自用户提供的 cod-h7-template-main.zip，
 *          Components/Device/Src/Motor.c，原作者 GrassFan Wang。
 *          保留跃鹿实例/PID调用接口（原作者 MSC1309），以 can_transmit 替换 FDCAN 发送。
 */

#include "dm_motor.h"
#include "kernel.h"
#include "memory.h"
#include "string.h"
#include "log_port.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>

static uint8_t idx_dm = 0;//用于记录注册的电机数量
static dm_motor_t dm_motor_instance[DM_MOTOR_CNT];//提前定义好空白电机实例
static uint8_t dm_cycle_cnt = 0;//用于分频计数，控制DM任务周期

/* 两个用于将uint值和float值进行映射的函数,在设定发送值和解析反馈值时使用 */
static uint16_t dm_motor_float_to_uint(float x, float x_min, float x_max, uint8_t bits)
{
    if (!isfinite(x))
        x = 0.0f;
    x = fminf(x_max, fmaxf(x_min, x));
    float span = x_max - x_min;
    float offset = x_min;
    return (uint16_t)((x - offset) * ((float)((1 << bits) - 1)) / span);
}
static float dm_motor_uint_to_float(int x_int, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return ((float)x_int) * span / ((float)((1 << bits) - 1)) + offset;
}

//用于给电机发送设置性质的指令，包含使能，失能，重设零点，清除错误
static void dm_motor_set_mode(dm_motor_mode_e cmd, dm_motor_t *motor)
{
    memset(motor->motor_can_instance->tx_buff, 0xff, 7);  // 发送电机指令的时候前面7bytes都是0xff
    motor->motor_can_instance->tx_buff[7] = (uint8_t)cmd; // 最后一位是命令id
    can_transmit((bus_tx_frame_t *)motor->motor_can_instance, 8);
}

/* 移植 dm_motor_decode：CAN ID 由 BSP 分发，协议内 ID 再核对一次。 */
static void dm_motor_decode(can_t *motor_can)
{
    const uint8_t *rxbuff = motor_can->rx_buff;
    dm_motor_t *motor = (dm_motor_t *)motor_can->id;
    dm_motor_measure_t *measure = &(motor->measure); // 将can实例中保存的id转换成电机实例的指针

    if (motor_can->rx_len != 8u || (rxbuff[0] & 0x0fu) != motor_can->tx_id)
        return;

    motor->rx_refresh_dt = 0;//一旦接收到反馈重置计数
    motor->rx_sample_us = (uint64_t)kernel_tick_ms() * 1000u; // CAN 帧进入 MCU 的采样时刻

    measure->state = rxbuff[0] >> 4;

    measure->last_position = measure->position;//保存上一次的角度，用于多圈计算

    measure->position_raw = (uint16_t)((rxbuff[1] << 8) | rxbuff[2]);
    measure->position = dm_motor_uint_to_float(measure->position_raw, DM_P_MIN, DM_P_MAX, 16);

    if (motor->rx_init_flag == 0){
        measure->last_position = measure->position;//防止因为第一次接受到数据时measure->last_position默认为0导致计数错误
        motor->rx_init_flag = 1;
    }

    measure->velocity_raw = (uint16_t)((rxbuff[3] << 4) | (rxbuff[4] >> 4));
    measure->velocity = dm_motor_uint_to_float(measure->velocity_raw, DM_V_MIN, DM_V_MAX, 12);

    measure->torque_raw = (uint16_t)(((rxbuff[4] & 0x0f) << 8) | rxbuff[5]);
    measure->torque = dm_motor_uint_to_float(measure->torque_raw, motor->torque_min,
                                    motor->torque_max, 12);

    measure->T_Mos = (float)rxbuff[6];
    measure->T_Rotor = (float)rxbuff[7];//解析电机温度

    // 多圈角度计数算法，前提是两次采样周期间隔内电机转动的角度不超过DM_P_MAX
    if (motor->measure.position - motor->measure.last_position > DM_P_MAX)
        motor->measure.total_round -= 1;
    else if (motor->measure.position - motor->measure.last_position < -DM_P_MAX)
        motor->measure.total_round += 1;
    motor->measure.total_position = motor->measure.position + motor->measure.total_round * 2 * DM_P_MAX;
}

/* 移植 dm_motor_send_mit 的 MIT 分支，所有模式共用同一套位域打包。 */
static void dm_motor_send_mit(dm_motor_t *motor, float position, float velocity,
                                  float kp, float kd, float torque)
{
    const uint16_t p_int = dm_motor_float_to_uint(position, DM_P_MIN, DM_P_MAX, 16);
    const uint16_t v_int = dm_motor_float_to_uint(velocity, DM_V_MIN, DM_V_MAX, 12);
    const uint16_t t_int = dm_motor_float_to_uint(torque, motor->torque_min,
                                         motor->torque_max, 12);
    const uint16_t kp_int = dm_motor_float_to_uint(kp, DM_KP_MIN, DM_KP_MAX, 12);
    const uint16_t kd_int = dm_motor_float_to_uint(kd, DM_KD_MIN, DM_KD_MAX, 12);
    uint8_t *data = motor->motor_can_instance->tx_buff;

    data[0] = (uint8_t)(p_int >> 8);
    data[1] = (uint8_t)p_int;
    data[2] = (uint8_t)(v_int >> 4);
    data[3] = (uint8_t)(((v_int & 0x0fu) << 4) | (kp_int >> 8));
    data[4] = (uint8_t)kp_int;
    data[5] = (uint8_t)(kd_int >> 4);
    data[6] = (uint8_t)(((kd_int & 0x0fu) << 4) | (t_int >> 8));
    data[7] = (uint8_t)t_int;
    can_transmit((bus_tx_frame_t *)motor->motor_can_instance, 8);
}

//用于自动恢复通信（清除timeout错误），自动使能意外失能的电机
static bool dm_auto_recover_comm(dm_motor_t *motor)
{
    if (motor->recovery_wait_ms < DM_DRIVER_ENABLE_RETRY_MS)
        return false;
    motor->recovery_wait_ms = 0;
    if (motor->rx_init_flag && motor->measure.state == DM_STATE_COMM_LOST)
    {
        dm_motor_set_mode(DM_CMD_CLEAR_ERROR, motor);
        return true;
    }
    if (!motor->rx_init_flag || motor->measure.state == DM_STATE_DISABLE ||
        (motor->rx_refresh_dt >= (int)(DM_DRIVER_ENABLE_RETRY_MS / DM_TASK_CYCLE) &&
         motor->measure.state == DM_STATE_ENABLE))
    {
        dm_motor_set_mode(DM_CMD_MOTOR_MODE, motor);
        return true;
    }
    return false;
}

//普通模式初始化函数
dm_motor_t *dm_motor_init(dm_motor_init_config_t *config)
{
    if (idx_dm >= DM_MOTOR_CNT){//注册电机数量超过最大值，报错
        LOG_ERROR("[dmmotor] Too many motors!");
        return NULL;
    }

    if(config->can_init_config.tx_id > 0x0F){//电机的txid大于0xF，会使电机状态位解析值不可预测，报错
        LOG_ERROR("[dmmotor] txID is greater than 0x0F.");
        return NULL;
    }

    dm_motor_t *motor = &dm_motor_instance[idx_dm];
    idx_dm++;
    motor->torque_min = DM_T_MIN;
    motor->torque_max = DM_T_MAX;

    //将注册信息填入实例中
    motor->motor_settings = config->controller_setting_init_config;
    pid_init(&motor->speed_PID, &config->controller_param_init_config.speed_PID);
    pid_init(&motor->angle_PID, &config->controller_param_init_config.angle_PID);
    motor->other_angle_feedback_ptr = config->controller_param_init_config.other_angle_feedback_ptr;
    motor->other_speed_feedback_ptr = config->controller_param_init_config.other_speed_feedback_ptr;
    motor->speed_feedforward_ptr = config->controller_param_init_config.speed_feedforward_ptr;
    motor->force_feedforward_ptr = config->controller_param_init_config.force_feedforward_ptr;

    //为电机实例注册CAN总线
    config->can_init_config.can_module_callback = dm_motor_decode;
    config->can_init_config.id = motor;
    motor->motor_can_instance = can_register(&config->can_init_config);

    //模式为普通模式
    motor->working_type = NORMAL_MODE;

    //开启并使能电机
    dm_motor_enable(motor);
    motor->startup_wait_ms = DM_DRIVER_STARTUP_WAIT_MS;
    dm_motor_set_mode(DM_CMD_MOTOR_MODE, motor);

    return motor;
}

//MIT模式初始化函数
dm_motor_t *dm_motor_mit_init(dm_motor_mit_init_config_t *config)
{
    if (idx_dm >= DM_MOTOR_CNT) {//注册电机数量超过最大值，报错
        LOG_ERROR("[dmmotor] Too many motors!");
        return NULL;
    }

    if(config->can_init_config.tx_id > 0x0F){//电机的txid大于0xF，会使电机状态位解析值不可预测，报错
        LOG_ERROR("[dmmotor] txID is greater than 0x0F.");
        return NULL;
    }

    dm_motor_t *motor = &dm_motor_instance[idx_dm];
    idx_dm++;

    const bool custom_torque_range = isfinite(config->torque_min) &&
        isfinite(config->torque_max) && config->torque_min < 0.0f &&
        config->torque_max > 0.0f && config->torque_min < config->torque_max;
    motor->torque_min = custom_torque_range ? config->torque_min : DM_T_MIN;
    motor->torque_max = custom_torque_range ? config->torque_max : DM_T_MAX;

    //将注册信息填入实例中
    motor->motor_settings.motor_reverse_flag = config->motor_reverse_flag;

    motor->MIT_Kp = config->controller_param_init_config.Kp;
    motor->MIT_Kd = config->controller_param_init_config.Kd;

    //为电机实例注册CAN总线
    config->can_init_config.can_module_callback = dm_motor_decode;
    config->can_init_config.id = motor;
    motor->motor_can_instance = can_register(&config->can_init_config);

    //模式为MIT模式
    motor->working_type = MIT_MODE;

    //开启并使能电机
    dm_motor_enable(motor);
    motor->startup_wait_ms = DM_DRIVER_STARTUP_WAIT_MS;
    dm_motor_set_mode(DM_CMD_MOTOR_MODE, motor);

    return motor;
}

/// @brief 两种模式下均可使用的通用函数
//开启电机（注意不是使能）
void dm_motor_enable(dm_motor_t *motor)
{
    motor->stop_flag = MOTOR_ENABLED;
}

//关闭电机，使电机的输出为0，清空PID（注意不是失能）
void dm_motor_stop(dm_motor_t *motor)
{
    motor->stop_flag = MOTOR_STOP;
}

//用于重设零点
void dm_motor_cali_encoder(dm_motor_t *motor)
{
    dm_motor_set_mode(DM_CMD_ZERO_POSITION, motor);
    dwt_delay(0.01);//重设零点会使电机短时间内无法使用，需要延时一段时间
}

/// @brief 普通模式下设置各种参数
//切换电机最外层闭环
void dm_motor_outer_loop(dm_motor_t *motor, motor_closeloop_type_e type)
{
    motor->motor_settings.outer_loop_type = type;
}

//切换电机反馈模式
void dm_motor_change_feed(dm_motor_t *motor, motor_closeloop_type_e loop, motor_feedback_source_e type)
{
    if (loop == ANGLE_LOOP)
        motor->motor_settings.angle_feedback_source = type;
    else if (loop == SPEED_LOOP)
        motor->motor_settings.speed_feedback_source = type;
}

//设置最外环参考值，内环参考值请通过前馈指针设置
void dm_motor_set_ref(dm_motor_t *motor, float ref)
{
    motor->pid_ref = ref;
}

/// @brief MIT模式下设置各种参数
void dm_motor_set_mit_angle(dm_motor_t *motor, float angle)
{
    motor->MIT_ref_angle = angle;
}

void dm_motor_set_mit_speed(dm_motor_t *motor, float speed)
{
    motor->MIT_ref_speed = speed;
}

void dm_motor_set_mit_torque(dm_motor_t *motor, float torque)
{
    motor->MIT_ref_torque = torque;
}

//DM任务函数，以一定频率执行（取决于DM_TASK_CYCLE），用于PID计算，发送控制帧，以及维护通信
void dm_motor_task()
{
    //用于控制函数执行周期
    if (idx_dm == 0)
        return;
    dm_cycle_cnt++;
    if (dm_cycle_cnt != DM_TASK_CYCLE)
        return;
    else
        dm_cycle_cnt = 0;

    dm_motor_t *motor;
    dm_motor_measure_t *measure;
    dm_motor_control_setting_t *motor_setting;

    for(int i = 0;i < idx_dm; i++){//遍历所有电机实例，分别执行任务

        motor = &dm_motor_instance[i];
        motor_setting = &motor->motor_settings;
        measure = &motor->measure;

        if (motor->rx_refresh_dt < INT_MAX)
            motor->rx_refresh_dt++;
        if (motor->recovery_wait_ms < DM_DRIVER_ENABLE_RETRY_MS)
            motor->recovery_wait_ms += DM_TASK_CYCLE;

        if (motor->startup_wait_ms > 0u)
        {
            motor->startup_wait_ms = motor->startup_wait_ms > DM_TASK_CYCLE
                                         ? motor->startup_wait_ms - DM_TASK_CYCLE : 0u;
            continue;
        }
        if (dm_auto_recover_comm(motor))
            continue;

        if (motor->working_type == MIT_MODE)//MIT模式需要发送的控制帧
        {
            const float direction = motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE
                                        ? -1.0f : 1.0f;
            const bool output_enabled = motor->stop_flag != MOTOR_STOP && motor->rx_init_flag &&
                                        motor->rx_refresh_dt < (int)(DM_DRIVER_ENABLE_RETRY_MS / DM_TASK_CYCLE) &&
                                        measure->state == DM_STATE_ENABLE;
            dm_motor_send_mit(motor, direction * motor->MIT_ref_angle,
                                  direction * motor->MIT_ref_speed,
                                  output_enabled ? motor->MIT_Kp : 0.0f,
                                  output_enabled ? motor->MIT_Kd : 0.0f,
                                  output_enabled ? direction * motor->MIT_ref_torque : 0.0f);
        }
        else if (motor->working_type == NORMAL_MODE)//普通模式需要发送的控制帧
        {
            float pid_ref, pid_measure, set;

            pid_ref = motor->pid_ref;

            if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)//如果设置反向，直接将参考值反转符号
                pid_ref *= -1;

            if ((motor_setting->close_loop_type & ANGLE_LOOP) && motor_setting->outer_loop_type == ANGLE_LOOP)//如果启用了角度环且最外层闭环为角度环，则计算角度环
            {
                if (motor_setting->angle_feedback_source == OTHER_FEED)//判断是否使用电机反馈
                    pid_measure = *motor->other_angle_feedback_ptr;
                else
                    pid_measure = measure->total_position;

                pid_ref = pid_calculate(&motor->angle_PID, pid_measure, pid_ref); //对总角度进行闭环，而不是单圈角度
            }

            if ((motor_setting->close_loop_type & SPEED_LOOP) && (motor_setting->outer_loop_type & (ANGLE_LOOP | SPEED_LOOP)))//如果启用了速度环且最外层闭环为角度环或速度环，则计算速度环
            {
                if (motor_setting->feedforward_flag & SPEED_FEEDFORWARD)
                    pid_ref += *motor->speed_feedforward_ptr; //速度前馈

                if (motor_setting->speed_feedback_source == OTHER_FEED)//判断是否使用电机反馈
                    pid_measure = *motor->other_speed_feedback_ptr;
                else
                    pid_measure = measure->velocity;

                pid_ref = pid_calculate(&motor->speed_PID, pid_measure, pid_ref);//对速度环闭环
            }


            if (motor_setting->feedforward_flag & FORCE_FEEDFORWARD)
                pid_ref += *motor->force_feedforward_ptr;//力矩前馈

            set = pid_ref;//得到最终的控制值

            MOTOR_LIMIT_MIN_MAX(set, motor->torque_min, motor->torque_max);//按该电机映射范围限幅

            //如果电机被关闭，则清空其PID以及力矩输出
            if (motor->stop_flag == MOTOR_STOP){
                set = 0.0f;
                pid_clear(&motor->angle_PID);
                pid_clear(&motor->speed_PID);
            }

            dm_motor_send_mit(motor, 0.0f, 0.0f, 0.0f, 0.0f, set);
        }
    }
}
