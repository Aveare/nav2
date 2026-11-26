/**
 * @file motor_def.h
 * @author neozng
 * @author modified by JIT CR (增加GM6020电压/电流控制模式)
 * @brief  电机通用的数据结构定义
 * @version beta
 * @date 2022-11-01
 *
 * @copyright Copyright (c) 2022 HNU YueLu EC all rights reserved
 */

#ifndef MOTOR_DEF_H
#define MOTOR_DEF_H

#include "controller.h"
#include "stdint.h"

#define LIMIT_MIN_MAX(x, min, max) (x) = (((x) <= (min)) ? (min) : (((x) >= (max)) ? (max) : (x)))

/**
 * @brief 闭环类型,多环用或运算组合,如速度+电流: CURRENT_LOOP|SPEED_LOOP
 */
typedef enum
{
    OPEN_LOOP = 0b0000,
    CURRENT_LOOP = 0b0001,
    SPEED_LOOP = 0b0010,
    ANGLE_LOOP = 0b0100,

    // only for checking
    SPEED_AND_CURRENT_LOOP = 0b0011,
    ANGLE_AND_SPEED_LOOP = 0b0110,
    ALL_THREE_LOOP = 0b0111,
} Closeloop_Type_e;

typedef enum
{
    FEEDFORWARD_NONE = 0b000,
    CURRENT_FEEDFORWARD = 0b001,
    SPEED_FEEDFORWARD = 0b010,
    FORCE_FEEDFORWARD = 0b100,
    CURRENT_AND_SPEED_FEEDFORWARD = CURRENT_FEEDFORWARD | SPEED_FEEDFORWARD,
} Feedfoward_Type_e;

/* 反馈来源设定,设为OTHER_FEED时需要指定数据来源指针,详见Motor_Controller_s */
typedef enum
{
    MOTOR_FEED = 0,
    OTHER_FEED,
} Feedback_Source_e;

/* GM6020支持电压(0x1FF/0x2FF,±25000)与电流(0x1FE/0x2FE,±16384↔±3A)两种CAN控制帧。
 * 电流模式需电调固件支持且同分组内所有电机同模式;其余电机忽略此字段。 */
typedef enum
{
    MOTOR_CONTROL_VOLTAGE = 0,
    MOTOR_CONTROL_CURRENT,
} Motor_Control_Mode_e;

/* 电机正反转标志(作用于输出方向) */
typedef enum
{
    MOTOR_DIRECTION_NORMAL = 0,
    MOTOR_DIRECTION_REVERSE = 1
} Motor_Reverse_Flag_e;

/* 反馈量正反标志 */
typedef enum
{
    FEEDBACK_DIRECTION_NORMAL = 0,
    FEEDBACK_DIRECTION_REVERSE = 1
} Feedback_Reverse_Flag_e;

typedef enum
{
    MOTOR_STOP = 0,
    MOTOR_ENABLED = 1,
} Motor_Working_Type_e;

/* 电机控制设置:闭环类型、反转标志、反馈来源、前馈标志 */
typedef struct
{
    Closeloop_Type_e outer_loop_type;              // 最外层的闭环,未设置时默认为最高级的闭环
    Closeloop_Type_e close_loop_type;              // 使用几个闭环(串级)
    Motor_Reverse_Flag_e motor_reverse_flag;       // 是否反转
    Feedback_Reverse_Flag_e feedback_reverse_flag; // 反馈是否反向
    Feedback_Source_e angle_feedback_source;       // 角度反馈类型
    Feedback_Source_e speed_feedback_source;       // 速度反馈类型
    Feedfoward_Type_e feedforward_flag;            // 前馈标志
    Motor_Control_Mode_e control_mode;             // GM6020专用:电压/电流控制帧,其余电机保持默认
} Motor_Control_Setting_s;

typedef struct
{
    Closeloop_Type_e outer_loop_type;        // 最外层的闭环，必须设置
    Closeloop_Type_e close_loop_type;        // 使用几个闭环(串级)
    Motor_Reverse_Flag_e motor_reverse_flag; // 是否反转
    Feedback_Source_e angle_feedback_source; // 角度反馈类型
    Feedback_Source_e speed_feedback_source; // 速度反馈类型
    Feedfoward_Type_e feedforward_flag;      // 前馈标志
} DMMotor_Control_Setting_s;

/* 电机控制器:外部反馈指针、三环PID、串级中间变量 */
typedef struct
{
    float *other_angle_feedback_ptr; // 外部角度反馈数据指针
    float *other_speed_feedback_ptr; // 外部速度反馈数据指针
    float *speed_feedforward_ptr;    // 速度前馈数据指针
    float *current_feedforward_ptr;  // 电流(输出)前馈数据指针

    PIDInstance current_PID;
    PIDInstance speed_PID;
    PIDInstance angle_PID;

    float pid_ref; // 串级闭环逐环传递的中间参考值
} Motor_Controller_s;

/* 电机类型 */
typedef enum
{
    MOTOR_TYPE_NONE = 0,
    GM6020,
    M3508,
    M2006,
    LK9025,
    HT04,
} Motor_Type_e;

/**
 * @brief 电机控制器初始化结构体:三环PID配置+外部反馈/前馈指针
 *        不用某环可不配置;用外部反馈须同时设指针和 Feedback_Source_e
 */
typedef struct
{
    float *other_angle_feedback_ptr; // 外部角度反馈指针(电机用total_angle)
    float *other_speed_feedback_ptr; // 外部速度反馈指针,单位angle per sec

    float *speed_feedforward_ptr;   // 速度前馈指针
    float *current_feedforward_ptr; // 电流前馈指针

    PID_Init_Config_s current_PID;
    PID_Init_Config_s speed_PID;
    PID_Init_Config_s angle_PID;
} Motor_Controller_Init_s;

typedef struct
{
    float *other_angle_feedback_ptr; // 外部角度反馈指针
    float *other_speed_feedback_ptr; // 外部速度反馈指针

    float *speed_feedforward_ptr;  // 速度前馈指针
    float *force_feedforward_ptr;  // 力矩前馈指针

    PID_Init_Config_s speed_PID;
    PID_Init_Config_s angle_PID;
} DMMotor_Controller_Init_s;

/* CAN电机的通用初始化结构体 */
typedef struct
{
    Motor_Controller_Init_s controller_param_init_config;
    Motor_Control_Setting_s controller_setting_init_config;
    Motor_Type_e motor_type;
    CAN_Init_Config_s can_init_config;
} Motor_Init_Config_s;

typedef struct
{
    DMMotor_Controller_Init_s controller_param_init_config;
    DMMotor_Control_Setting_s controller_setting_init_config;
    CAN_Init_Config_s can_init_config;
} DMMotor_Init_Config_s; // DM普通模式初始化结构体

typedef enum
{
    MOTOR_INIT = 0,
    MOTOR_READY,
} Motor_state_e;

typedef struct
{
    float Kp;
    float Kd;
} MIT_Controller_Init_s;

typedef struct
{
    MIT_Controller_Init_s controller_param_init_config; // MIT参数
    Motor_Reverse_Flag_e motor_reverse_flag;            // 是否反转
    CAN_Init_Config_s can_init_config;                  // CAN实例
    float torque_min;                                    // MIT扭矩映射下限；0表示使用驱动默认值
    float torque_max;                                    // MIT扭矩映射上限；0表示使用驱动默认值
} MIT_Init_Config_s;                                    // MIT模式初始化结构体

#endif // !MOTOR_DEF_H
