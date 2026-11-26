/**
 * @file controller.c
 * @author wanghongxi
 * @author modified by neozng
 * @brief  PID控制器实现
 * @version beta
 * @date 2022-11-01
 *
 * @copyright Copyright (c) 2022 HNU YueLu EC all rights reserved
 */
#include "controller.h"
#include "memory.h"

/* ----------------------------PID优化环节---------------------------- */

// 梯形积分:(上底+下底)*高/2
static void f_Trapezoid_Intergral(PIDInstance *pid)
{
    pid->ITerm = pid->Ki * ((pid->Err + pid->Last_Err) / 2) * pid->dt;
}

// 变速积分(误差小时积分作用更强)
static void f_Changing_Integration_Rate(PIDInstance *pid)
{
    if (pid->Err * pid->Iout > 0)
    {
        // 积分呈累积趋势
        if (abs(pid->Err) <= pid->CoefB)
            return; // Full integral
        if (abs(pid->Err) <= (pid->CoefA + pid->CoefB))
            pid->ITerm *= (pid->CoefA - abs(pid->Err) + pid->CoefB) / pid->CoefA;
        else // 最大阈值,不使用积分
            pid->ITerm = 0;
    }
}

// 积分限幅:输出已饱和且积分仍在累积时清零当前ITerm,防windup
static void f_Integral_Limit(PIDInstance *pid)
{
    static float temp_Output, temp_Iout;
    temp_Iout = pid->Iout + pid->ITerm;
    temp_Output = pid->Pout + pid->Iout + pid->Dout;
    if (abs(temp_Output) > pid->MaxOut)
    {
        if (pid->Err * pid->Iout > 0) // 积分却还在累积
            pid->ITerm = 0;
    }

    if (temp_Iout > pid->IntegralLimit)
    {
        pid->ITerm = 0;
        pid->Iout = pid->IntegralLimit;
    }
    if (temp_Iout < -pid->IntegralLimit)
    {
        pid->ITerm = 0;
        pid->Iout = -pid->IntegralLimit;
    }
}

// 微分先行:只对测量值微分,目标阶跃不产生微分冲击
static void f_Derivative_On_Measurement(PIDInstance *pid)
{
    pid->Dout = pid->Kd * (pid->Last_Measure - pid->Measure) / pid->dt;
}

// 微分滤波:滤除高频噪声
static void f_Derivative_Filter(PIDInstance *pid)
{
    pid->Dout = pid->Dout * pid->dt / (pid->Derivative_LPF_RC + pid->dt) +
                pid->Last_Dout * pid->Derivative_LPF_RC / (pid->Derivative_LPF_RC + pid->dt);
}

// 输出滤波
static void f_Output_Filter(PIDInstance *pid)
{
    pid->Output = pid->Output * pid->dt / (pid->Output_LPF_RC + pid->dt) +
                  pid->Last_Output * pid->Output_LPF_RC / (pid->Output_LPF_RC + pid->dt);
}

// 输出限幅
static void f_Output_Limit(PIDInstance *pid)
{
    if (pid->Output > pid->MaxOut)
        pid->Output = pid->MaxOut;
    if (pid->Output < -(pid->MaxOut))
        pid->Output = -(pid->MaxOut);
}

// 电机堵转检测
static void f_PID_ErrorHandle(PIDInstance *pid)
{
    /*Motor Blocked Handle*/
    if (fabsf(pid->Output) < pid->MaxOut * 0.001f || fabsf(pid->Ref) < 0.0001f)
        return;

    if ((fabsf(pid->Ref - pid->Measure) / fabsf(pid->Ref)) > 0.95f)
        pid->ERRORHandler.ERRORCount++;
    else
        pid->ERRORHandler.ERRORCount = 0;

    if (pid->ERRORHandler.ERRORCount > 500)
        pid->ERRORHandler.ERRORType = PID_MOTOR_BLOCKED_ERROR;
}

/* ---------------------------外部算法接口--------------------------- */

/**
 * @brief 初始化PID,设置参数和启用的优化环节,将其他数据置零
 */
void PIDInit(PIDInstance *pid, PID_Init_Config_s *config)
{
    // config 与 PIDInstance 的 config block 布局一致,直接 memcpy 完成赋值
    // (两结构体同名字段的相对位置必须一致,新增字段两处同步)
    memset(pid, 0, sizeof(PIDInstance));
    memcpy(pid, config, sizeof(PID_Init_Config_s));
    DWT_GetDeltaT(&pid->DWT_CNT);
}

/**
 * @brief          PID计算
 * @param[in]      PID结构体
 * @param[in]      测量值
 * @param[in]      期望值
 */
float PIDCalculate(PIDInstance *pid, float measure, float ref)
{
    // 堵转检测
    if (pid->Improve & PID_ErrorHandle)
        f_PID_ErrorHandle(pid);

    pid->dt = DWT_GetDeltaT(&pid->DWT_CNT); // 两次计算的时间间隔

    pid->Measure = measure;
    pid->Ref = ref;
    pid->Err = pid->Ref - pid->Measure;

    // 死区外才计算PID
    if (abs(pid->Err) > pid->DeadBand)
    {
        // 基本位置式PID
        pid->Pout = pid->Kp * pid->Err;
        pid->ITerm = pid->Ki * pid->Err * pid->dt;
        pid->Dout = pid->Kd * (pid->Err - pid->Last_Err) / pid->dt;

        if (pid->Improve & PID_Trapezoid_Intergral)
            f_Trapezoid_Intergral(pid);
        if (pid->Improve & PID_ChangingIntegrationRate)
            f_Changing_Integration_Rate(pid);
        if (pid->Improve & PID_Derivative_On_Measurement)
            f_Derivative_On_Measurement(pid);
        if (pid->Improve & PID_DerivativeFilter)
            f_Derivative_Filter(pid);
        if (pid->Improve & PID_Integral_Limit)
            f_Integral_Limit(pid);

        /* D项限幅:Dout=Kdx角速度没有上限,快速运动时会把输出顶满("不跟手");
         * 低速段D远小于限值、阻尼照旧。<=0 表示不限幅。 */
        if (pid->DerivativeLimit > 0.0f)
        {
            if (pid->Dout > pid->DerivativeLimit)
                pid->Dout = pid->DerivativeLimit;
            else if (pid->Dout < -pid->DerivativeLimit)
                pid->Dout = -pid->DerivativeLimit;
        }

        pid->Iout += pid->ITerm;                         // 累加积分
        pid->Output = pid->Pout + pid->Iout + pid->Dout; // 总输出

        if (pid->Improve & PID_OutputFilter)
            f_Output_Filter(pid);

        f_Output_Limit(pid);
    }
    else // 死区内清空积分和输出
    {
        pid->Output = 0;
        pid->ITerm = 0;
    }

    // 保存当前数据供下周期使用
    pid->Last_Measure = pid->Measure;
    pid->Last_Output = pid->Output;
    pid->Last_Dout = pid->Dout;
    pid->Last_Err = pid->Err;
    pid->Last_ITerm = pid->ITerm;

    return pid->Output;
}

/**
 * @brief  PID清零
 */
void PIDClear(PIDInstance *pid)
{
    pid->ITerm = 0;
    pid->Last_Err = 0;
    pid->Last_Output = 0;
    pid->Last_Dout = 0;
    pid->Last_ITerm = 0;
    pid->Iout = 0;
    pid->Pout = 0;
    pid->Dout = 0;
    pid->Output = 0;
    pid->dt = DWT_GetDeltaT(&pid->DWT_CNT); // 必须更新时间戳,否则重新使能时I会突变
}
