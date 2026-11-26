/**
 * @file    motor_task.c
 * @brief   电机控制任务:按频率调度各电机实例的闭环与 CAN 发送
 * @author  JIT CR
 * @date    2026-10-26
 * @note    本车未接裁判/超电,PowerControl()暂不调用;其余与上游一致
 */
#include "motor_task.h"
#include "LK9025.h"
#include "HT04.h"
#include "dji_motor.h"
#include "step_motor.h"
#include "servo_motor.h"
#include "power_control.h"
#include "dmmotor.h"

void MotorControlTask()
{
    DJIMotorControl(); // DJI 电机三环闭环 + CAN 分组发送,1kHz

    /* 本车未启用的电机类型与接口(接入时请改用条件编译或按注册 idx 判断,
     * 不要靠注释掉调用来启停):
     * - LKMotorControl / StepMotorControl:LK9025 与步进电机本车未使用;
     * - HTMotorControl:HT04 反馈方式为"收到一帧即回传",按任务连续发送会导致总线拥塞,
     *   HTMotor 提供了 HTMotorControlInit() 的任务方式接口,接入时用它;
     * - PowerControl:本车未接裁判系统与超电,暂用普通 DJI 速度环。 */
    DMMotorTask();
}
