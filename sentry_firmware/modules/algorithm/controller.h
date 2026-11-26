/**
 ******************************************************************************
 * @file	 controller.h
 * @author  Wang Hongxi
 * @author  modified by JIT CR (增加D项限幅字段)
 * @version V1.1.3
 * @date    2021/7/3
 * @brief   PID控制器
 ******************************************************************************
 */
#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include "main.h"
#include "stdint.h"
#include "memory.h"
#include "stdlib.h"
#include "bsp_dwt.h"
#include "arm_math.h"
#include <math.h>

#ifndef abs
#define abs(x) ((x > 0) ? x : -x)
#endif

// PID 优化环节使能标志位,按位或启用,按位与判断
typedef enum
{
    PID_IMPROVE_NONE = 0b00000000,                // 0000 0000
    PID_Integral_Limit = 0b00000001,              // 0000 0001 积分限幅
    PID_Derivative_On_Measurement = 0b00000010,   // 0000 0010 微分先行(作用于测量值,目标阶跃无微分冲击)
    PID_Trapezoid_Intergral = 0b00000100,         // 0000 0100 梯形积分
    PID_Proportional_On_Measurement = 0b00001000, // 0000 1000
    PID_OutputFilter = 0b00010000,                // 0001 0000 输出低通
    PID_ChangingIntegrationRate = 0b00100000,     // 0010 0000 变速积分
    PID_DerivativeFilter = 0b01000000,            // 0100 0000 微分滤波
    PID_ErrorHandle = 0b10000000,                 // 1000 0000 堵转检测
} PID_Improvement_e;

/* PID 报错类型 */
typedef enum errorType_e
{
    PID_ERROR_NONE = 0x00U,
    PID_MOTOR_BLOCKED_ERROR = 0x01U
} ErrorType_e;

typedef struct
{
    uint64_t ERRORCount;
    ErrorType_e ERRORType;
} PID_ErrorHandler_t;

/* PID实例结构体 */
typedef struct
{
    //---------------------------------- init config block
    // config parameter
    float Kp;
    float Ki;
    float Kd;
    float MaxOut;
    float DeadBand;

    // improve parameter
    PID_Improvement_e Improve;
    float IntegralLimit;     // 积分限幅
    float CoefA;             // 变速积分 For Changing Integral
    float CoefB;             // 变速积分 ITerm = Err*((A-abs(err)+B)/A)  when B<|err|<A+B
    float Output_LPF_RC;     // 输出滤波器 RC = 1/omegac
    float Derivative_LPF_RC; // 微分滤波器系数
    /* D 项限幅(<=0 不限幅)。注意:此字段必须与 PID_Init_Config_s 的同名字段
     * 处于相同的相对位置——PIDInit 用 memcpy 复制 config,错位会整体错乱。 */
    float DerivativeLimit;

    //-----------------------------------
    // for calculating
    float Measure;
    float Last_Measure;
    float Err;
    float Last_Err;
    float Last_ITerm;

    float Pout;
    float Iout;
    float Dout;
    float ITerm;

    float Output;
    float Last_Output;
    float Last_Dout;

    float Ref;

    uint32_t DWT_CNT;
    float dt;

    PID_ErrorHandler_t ERRORHandler;
} PIDInstance;

/* 用于PID初始化的结构体(字段顺序必须与 PIDInstance 的 config block 一致) */
typedef struct
{
    // basic parameter
    float Kp;
    float Ki;
    float Kd;
    float MaxOut;   // 输出限幅
    float DeadBand; // 死区

    // improve parameter
    PID_Improvement_e Improve;
    float IntegralLimit; // 积分限幅
    float CoefA;         // AB为变速积分参数
    float CoefB;         // ITerm = Err*((A-abs(err)+B)/A)  when B<|err|<A+B
    float Output_LPF_RC; // RC = 1/omegac
    float Derivative_LPF_RC;
    float DerivativeLimit; // D项限幅,<=0 不限幅
} PID_Init_Config_s;

/**
 * @brief 初始化PID实例
 * @param pid    PID实例指针
 * @param config PID初始化配置
 */
void PIDInit(PIDInstance *pid, PID_Init_Config_s *config);

/**
 * @brief 计算PID输出
 *
 * @param pid     PID实例指针
 * @param measure 反馈值
 * @param ref     设定值
 * @return float  PID计算输出
 */
float PIDCalculate(PIDInstance *pid, float measure, float ref);

/**
 * @brief  PID清零
 * @param  pid: 指向PID结构体的指针
 */
void PIDClear(PIDInstance *pid);

#endif
