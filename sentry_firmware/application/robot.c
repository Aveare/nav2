/**
 * @file    robot.c
 * @brief   整车装配:按板型初始化各应用并串联任务
 * @layer   APP
 * @note    2026-10-29 建立,作者 JIT CR。
 */
#include "bsp_init.h"
#include "robot.h"
#include "robot_def.h"
#include "robot_task.h"

// 编译warning,提醒开发者修改机器人参数
#ifndef ROBOT_DEF_PARAM_WARNING
#define ROBOT_DEF_PARAM_WARNING
#pragma message "check if you have configured the parameters in robot_def.h, IF NOT, please refer to the comments AND DO IT, otherwise the robot will have FATAL ERRORS!!!"
#endif // !ROBOT_DEF_PARAM_WARNING

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
#include "chassis.h"
#endif

#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
#include "gimbal.h"
#include "shoot.h"
#include "robot_cmd.h"
#include "autonomy.h"
#endif

/**
 * @brief  整车总初始化:先 BSP,再各应用 Init,最后创建任务
 * @note   初始化期间关闭中断;不要在中断使能前使用依赖中断的延时,必须延时只用 DWT_Delay()。
 * @note   各应用是否初始化由 config 的板型宏决定,换单板/双板配置无需改本文件。
 */
void RobotInit()
{
    __disable_irq();

    BSPInit();

#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    autonomy_init();
    RobotCMDInit();
#if !CHASSIS_TUNING_ONLY || SENTRY_BIG_YAW_CONTROL_ENABLE || SENTRY_PITCH_CONTROL_ENABLE || SENTRY_SMALL_YAW_ENABLE
    GimbalInit();
#endif
#if !CHASSIS_TUNING_ONLY || SENTRY_SHOOT_CONTROL_ENABLE
    ShootInit();
#endif
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    ChassisInit();
#endif

    OSTaskInit(); // 创建基础任务

    __enable_irq();
}

/**
 * @brief  应用任务入口,由 StartROBOTTASK 以 200Hz 调用
 * @note   调用顺序有意义:RobotCMDTask 先发布指令,后续执行单元在同一拍内消费,
 *         避免各应用读到上一周期的命令。
 */
void RobotTask()
{
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    autonomy_task();
    RobotCMDTask();
#if !CHASSIS_TUNING_ONLY || SENTRY_BIG_YAW_CONTROL_ENABLE || SENTRY_PITCH_CONTROL_ENABLE || SENTRY_SMALL_YAW_ENABLE
    GimbalTask();
#endif
#if !CHASSIS_TUNING_ONLY || SENTRY_SHOOT_CONTROL_ENABLE
    ShootTask();
#endif
#endif

#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    ChassisTask();
#endif
}
