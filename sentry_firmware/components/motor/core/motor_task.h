/**
 * @file    motor_task.h
 * @brief   电机调度接口
 * @layer   COMPONENT
 * @author  neozng
 * @author  modified by 小猫大王
* @version beta
* @date 2022-11-01
* @copyright Copyright (c) 2022
 */
#ifndef MOTOR_TASK_H
#define MOTOR_TASK_H

/**
 * @brief 电机控制闭环任务,在RTOS中应该设定为1Khz运行
 *        舵机控制任务的频率设定为20Hz或更低
 *
 * @note 好无语,就一个函数罢了,干脆全部放到头文件里好了.
 *
 */
void motor_control_task();

#endif // !MOTOR_TASK_H
