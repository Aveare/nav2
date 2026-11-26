/**
 * @file    motor_task.c
 * @brief   1kHz 电机集中调度任务
 * @layer   COMPONENT
 * @author  JIT CR
 * @author  modified by 小猫大王
* @date    2026-10-26
* @note    本车未接裁判/超电,PowerControl()暂不调用;其余与上游一致
 */
#include "motor_task.h"
#include "ht_motor.h"
#include "dji_motor.h"
#include "servo_motor.h"
#include "dm_motor.h"

void motor_control_task()
{
    dji_motor_control(); // DJI 电机三环闭环 + CAN 分组发送,1kHz

    /* 本车未启用的电机类型与接口(接入时请改用条件编译或按注册 idx 判断,
     * 不要靠注释掉调用来启停):
     * - LKMotorControl / StepMotorControl:LK9025 与步进电机本车未使用;
     * - HTMotorControl:HT04 反馈方式为"收到一帧即回传",按任务连续发送会导致总线拥塞,
     *   HTMotor 提供了 ht_motor_control_init() 的任务方式接口,接入时用它;
     * - PowerControl:本车未接裁判系统与超电,暂用普通 DJI 速度环。 */
    dm_motor_task();
}
