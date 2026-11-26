/**
 * @file    dm_motor.h
 * @brief   达妙电机接口(预留)
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef DMMOTOR_H
#define DMMOTOR_H
#include <stdint.h>
#include "can_port.h"
#include "controller.h"
#include "motor_def.h"

// 以下宏定义在第一次使用DM框架时请务必根据实际情况更改

#define DM_MOTOR_CNT 6 // 达妙电机注册数量最大值
#define DM_TASK_CYCLE                                                                              \
    2 // 达妙任务运行周期，决定控制频率，PID计算频率以及电机反馈频率，必须为整数，单位为ms，推荐值为2

// 以下为位置，速度，力矩的映射范围，电机第一次使用时一定要配合实际情况以及DM调试助手修改
/* 2026-11-23 哨兵适配:DM-J4310 驱动器参数 PMAX=12.5 rad / VMAX=30 rad/s / TMAX=10 N·m。
 * 本工程只有哨兵使用 DM,故直接改全局映射;若将来接入 6006/8006/8009,
 * 必须把这组范围改为每实例配置,否则编码/解码比例会静默错位。 */
#define DM_P_MIN (-12.5f)
#define DM_P_MAX 12.5f
#define DM_V_MIN (-30.0f)
#define DM_V_MAX 30.0f
#define DM_T_MIN (-10.0f)
#define DM_T_MAX 10.0f

// 这两个映射范围是固定的，请不要修改
#define DM_KP_MIN 0.0f
#define DM_KP_MAX 500.0f
#define DM_KD_MIN 0.0f
#define DM_KD_MAX 5.0f

typedef enum
{
    DM_STATE_DISABLE = 0x0,
    DM_STATE_ENABLE = 0x1,
    DM_STATE_OVER_VOLTAGE = 0x8,
    DM_STATE_UNDER_VOLTAGE = 0x9,
    DM_STATE_OVER_CURRENT = 0xA,
    DM_STATE_MOS_OVER_TEMP = 0xB,
    DM_STATE_MOTOR_COIL_OVER_TEMP = 0xC,
    DM_STATE_COMM_LOST = 0xD,
    DM_STATE_OVERLOAD = 0xE,
} dm_motor_err_state_e; // 电机反馈的状态位枚举

typedef struct
{
    uint8_t state;
    float velocity;
    float last_position;
    float position;
    float total_position;
    float torque;
    float T_Mos;
    float T_Rotor;
    int total_round;
} dm_motor_measure_t; // 存储电机反馈值的结构体

typedef struct
{
    uint16_t position_des;
    uint16_t velocity_des;
    uint16_t torque_des;
    uint16_t Kp;
    uint16_t Kd;
} dm_motor_send_t;

typedef enum
{
    NORMAL_MODE = 0,
    MIT_MODE
} motor_control_type_e; // DM电机封装的两个模式

typedef struct
{
    dm_motor_measure_t measure;

    // 普通模式配置的PID相关设置
    dm_motor_control_setting_t motor_settings;
    pid_instance_t speed_PID;
    pid_instance_t angle_PID;

    // 普通模式配置的反馈与前馈指针
    float *other_angle_feedback_ptr;
    float *other_speed_feedback_ptr;
    float *speed_feedforward_ptr;
    float *force_feedforward_ptr;
    float pid_ref;

    // MIT模式配置的参数
    float MIT_ref_angle;
    float MIT_ref_speed;
    float MIT_ref_torque;
    float MIT_Kp;
    float MIT_Kd;

    int rx_refresh_dt; // 用于通过实时观测变量来监测总线通信质量不参与通信超时的判断（看门狗依赖DM内置的timeout）
    // 如果观察该变量大于等于2，则说明通信质量低，请检查布线是否规范，总线负载是否过高
    int rx_init_flag; // 第一次收到反馈标志位，用于防止多圈角度计数错误
    uint64_t rx_sample_us; // CAN 反馈接收时刻,MCU tick 域 us,分辨率 1ms

    motor_working_type_e stop_flag;    // 软件关闭电机标志位
    can_t *motor_can_instance;   // 电机对应的CAN通信实例
    motor_control_type_e working_type; // 工作模式
} dm_motor_t;                     // DM电机实例

typedef enum
{
    DM_CMD_MOTOR_MODE = 0xfc,    // 使能,会响应指令
    DM_CMD_RESET_MODE = 0xfd,    // 停止
    DM_CMD_ZERO_POSITION = 0xfe, // 将当前的位置设置为编码器零位
    DM_CMD_CLEAR_ERROR = 0xfb    // 清除电机过热错误
} dm_motor_mode_e;                // DM电机特殊控制帧对应的枚举

/**
 * @brief DM普通模式注册函数
 * @param config
 * 电机初始化结构体，包含了电机控制设置，电机PID参数设置，电机类型以及电机挂载的CAN设置
 * @return 返回一个DM电机实例
 */
dm_motor_t *dm_motor_init(dm_motor_init_config_t *config);

/**
 * @brief DM普通模式下设置最外环参考值，内环参考值请通过前馈指针设置
 * @param motor 对应的电机实例
 * @param ref 设置的参考值
 */
void dm_motor_set_ref(dm_motor_t *motor, float ref);

/**
 * @brief DM普通模式下设置最外环的闭环类型
 * @param motor 对应的电机实例
 * @param closeloop_type 闭环类型
 */
void dm_motor_outer_loop(dm_motor_t *motor, motor_closeloop_type_e closeloop_type);

/**
 * @brief DM普通模式下切换反馈类型
 * @param motor 对应的电机实例
 * @param loop 需要切换反馈类型的闭环
 * @param type 反馈类型
 */
void dm_motor_change_feed(dm_motor_t *motor, motor_closeloop_type_e loop, motor_feedback_source_e type);

/**
 * @brief MIT模式注册函数
 * @param config 电机初始化结构体，包含了MIT参数设置，电机反转标志位设置，电机挂载的CAN设置
 * @return 返回一个DM电机实例
 */
dm_motor_t *dm_motor_mit_init(dm_motor_mit_init_config_t *config);

/**
 * @brief MIT模式下设置位置参考值
 * @param motor 对应的电机实例
 * @param angle 设置的参考值
 */
void dm_motor_set_mit_angle(dm_motor_t *motor, float angle);

/**
 * @brief MIT模式下设置速度参考值
 * @param motor 对应的电机实例
 * @param speed 设置的参考值
 */
void dm_motor_set_mit_speed(dm_motor_t *motor, float speed);

/**
 * @brief MIT模式下设置力矩参考值
 * @param motor 对应的电机实例
 * @param torque 设置的参考值
 */
void dm_motor_set_mit_torque(dm_motor_t *motor, float torque);

/**
 * @brief 软件开启电机，电机初始化后默认就是开启状态
 * @param motor 对应的电机实例
 */
void dm_motor_enable(dm_motor_t *motor);

/**
 * @brief 软件关闭电机，使电机输出为0并清空PID参数，请调用该接口来对电机进行急停而不是直接发送失能帧
 * @param motor 对应的电机实例
 */
void dm_motor_stop(dm_motor_t *motor);

/**
 * @brief 将电机当前位置设置为电机的零点，不推荐调用
 * @param motor 对应的电机实例
 */
void dm_motor_cali_encoder(dm_motor_t *motor);

/**
 * @brief DM电机任务函数，应该将其放置在motor_control_task();中以一定频率运行
 */
void dm_motor_task();

#endif // !DMMOTOR
