/**
 * @file    dmmotor.h
 * @brief   达妙电机驱动接口
 * @layer   MODULE
 * @note    协议部分移植自 cod-h7-template-main 的 Components/Device/Motor，适配经典 CAN。
 */

#ifndef DMMOTOR_H
#define DMMOTOR_H
#include <stdint.h>
#include "bsp_can.h"
#include "controller.h"
#include "motor_def.h"

//以下宏定义在第一次使用DM框架时请务必根据实际情况更改

#define DM_MOTOR_CNT 6 //达妙电机注册数量最大值
#define DM_TASK_CYCLE 2u // 达妙驱动周期，ms；MotorControlTask 每 1ms 调用一次

//以下为位置，速度，力矩的映射范围，电机第一次使用时一定要配合实际情况以及DM调试助手修改
#define DM_P_MIN (-12.5f)
#define DM_P_MAX 12.5f
#define DM_V_MIN (-30.0f)
#define DM_V_MAX 30.0f
#define DM_T_MIN (-10.0f)
#define DM_T_MAX 10.0f

//这两个映射范围是固定的，请不要修改
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f

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
} DMMotor_Err_state_e;//电机反馈的状态位枚举

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
    uint16_t position_raw; // CAN 反馈 POS 的原始 16 位协议值
    uint16_t velocity_raw;
    uint16_t torque_raw;
} DM_Motor_Measure_s;//存储电机反馈值的结构体

typedef struct
{
    uint16_t position_des;
    uint16_t velocity_des;
    uint16_t torque_des;
    uint16_t Kp;
    uint16_t Kd;
} DMMotor_Send_s;

typedef enum
{
    NORMAL_MODE = 0,
    MIT_MODE
} Motor_Control_Type_e;//DM电机封装的两个模式

typedef struct
{
    DM_Motor_Measure_s measure;

    //普通模式配置的PID相关设置
    DMMotor_Control_Setting_s motor_settings;
    PIDInstance speed_PID;
    PIDInstance angle_PID;

    //普通模式配置的反馈与前馈指针
    float *other_angle_feedback_ptr;
    float *other_speed_feedback_ptr;
    float *speed_feedforward_ptr;
    float *force_feedforward_ptr;
    float pid_ref;

    //MIT模式配置的参数
    float MIT_ref_angle;
    float MIT_ref_speed;
    float MIT_ref_torque;
    float MIT_Kp;
    float MIT_Kd;
    float torque_min; // 当前电机MIT扭矩映射下限，与驱动器TMAX参数匹配
    float torque_max; // 当前电机MIT扭矩映射上限，与驱动器TMAX参数匹配

    volatile int rx_refresh_dt; // 距离上次有效反馈的驱动周期数，达到 INT_MAX 后饱和
    volatile int rx_init_flag; // 第一帧有效反馈标志
    volatile uint64_t rx_sample_us; // CAN反馈接收的MCU tick时间(us)
    uint16_t startup_wait_ms;
    uint16_t recovery_wait_ms;

    Motor_Working_Type_e stop_flag;//软件关闭电机标志位
    CANInstance *motor_can_instance;//电机对应的CAN通信实例
    Motor_Control_Type_e working_type;//工作模式
} DMMotorInstance;//DM电机实例

typedef enum
{
    DM_CMD_MOTOR_MODE = 0xfc,    // 使能,会响应指令
    DM_CMD_RESET_MODE = 0xfd,    // 停止
    DM_CMD_ZERO_POSITION = 0xfe, // 将当前的位置设置为编码器零位
    DM_CMD_CLEAR_ERROR = 0xfb    // 清除电机过热错误
} DMMotor_Mode_e;// DM电机特殊控制帧对应的枚举

/**
 * @brief DM普通模式注册函数
 * @param config 电机初始化结构体，包含了电机控制设置，电机PID参数设置，电机类型以及电机挂载的CAN设置
 * @return 返回一个DM电机实例
 */
DMMotorInstance *DMMotorInit(DMMotor_Init_Config_s *config);

/**
 * @brief DM普通模式下设置最外环参考值，内环参考值请通过前馈指针设置
 * @param motor 对应的电机实例
 * @param ref 设置的参考值
 */
void DMMotorSetRef(DMMotorInstance *motor, float ref);

/**
 * @brief DM普通模式下设置最外环的闭环类型
 * @param motor 对应的电机实例
 * @param closeloop_type 闭环类型
 */
void DMMotorOuterLoop(DMMotorInstance *motor, Closeloop_Type_e closeloop_type);

/**
 * @brief DM普通模式下切换反馈类型
 * @param motor 对应的电机实例
 * @param loop 需要切换反馈类型的闭环
 * @param type 反馈类型
 */
void DMMotorChangeFeed(DMMotorInstance *motor, Closeloop_Type_e loop, Feedback_Source_e type);

/**
 * @brief MIT模式注册函数
 * @param config 电机初始化结构体，包含了MIT参数设置，电机反转标志位设置，电机挂载的CAN设置
 * @return 返回一个DM电机实例
 */
DMMotorInstance *MITMotorInit(MIT_Init_Config_s *config);

/**
 * @brief MIT模式下设置位置参考值
 * @param motor 对应的电机实例
 * @param angle 设置的参考值
 */
void DMMotorSetMITAngle(DMMotorInstance *motor, float angle);

/**
 * @brief MIT模式下设置速度参考值
 * @param motor 对应的电机实例
 * @param speed 设置的参考值
 */
void DMMotorSetMITSpeed(DMMotorInstance *motor, float speed);

/**
 * @brief MIT模式下设置力矩参考值
 * @param motor 对应的电机实例
 * @param torque 设置的参考值
 */
void DMMotorSetMITTorque(DMMotorInstance *motor, float torque);

/**
 * @brief 软件开启电机，电机初始化后默认就是开启状态
 * @param motor 对应的电机实例
 */
void DMMotorEnable(DMMotorInstance *motor);

/**
 * @brief 软件关闭电机，使电机输出为0并清空PID参数，请调用该接口来对电机进行急停而不是直接发送失能帧
 * @param motor 对应的电机实例
 */
void DMMotorStop(DMMotorInstance *motor);

/**
 * @brief 将电机当前位置设置为电机的零点，不推荐调用
 * @param motor 对应的电机实例
 */
void DMMotorCaliEncoder(DMMotorInstance *motor);

/**
 * @brief DM电机任务函数，应该将其放置在MotorControlTask();中以一定频率运行
 */
void DMMotorTask();

#endif // !DMMOTOR
