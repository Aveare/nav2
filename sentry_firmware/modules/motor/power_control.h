/**
 * @file    power_control.h
 * @brief   底盘功率控制:按功率模型分配四轮输出
 * @layer   MODULE
 * @note    待办:1) 为不同电机设置各自的低通惯性系数,而非统一宏;
 *          2) 为 M2006 / M3508 增加开环零位校准,初始化时按配置调用。
 * @note    源自跃鹿 basic_framework(作者 neozng,上游版本 0.2 / 2022-11-01,
 *          版权 HNU YueLu EC);本工程修正了原文件头误写为 dji_motor.h 的错误。
 */

#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H

#include "bsp_can.h"
#include "controller.h"
#include "motor_def.h"
#include "stdint.h"
#include "daemon.h"
#include "dji_motor.h"


DJIMotorInstance *PowerControlInit(Motor_Init_Config_s *config);

/**
 * @brief 电机功率控制,此时电机根据电机功率模型进行控制，不是直接的pid控制
 *
 * @param motor 电机实例指针
 * @param power 功率值
 */
void PowerControl(void);

/**
 * @brief 设置电机功率限制
 *
 * @param power_limit 功率限制值
 */
void SetPowerLimit(float power_limit);
#endif // !DJI_MOTOR_H
