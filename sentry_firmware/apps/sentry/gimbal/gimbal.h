/**
 * @file    gimbal.h
 * @brief   哨兵三轴云台应用接口:初始化与任务入口
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_GIMBAL_H
#define SENTRY_GIMBAL_H

/**
 * @brief 三轴云台初始化:注册大 yaw/pitch 达妙 MIT、小 yaw GM6020,订阅命令
 * @note 须在 RTOS 启动前调用,由 assembly 统一调度;只能调用一次。
 */
void gimbal_init(void);

/**
 * @brief 三轴云台任务:大 yaw 回正/纯速度补偿 + pitch 轴增量 + 小 yaw 独立限位
 * @note 由 robot_task() 以 200Hz 调用;内部持有 PID/反馈状态,不可并发调用。
 */
void gimbal_task(void);

/**
 * @brief 当前 pitch 电机输出轴角,deg(供标定/调试读取)
 */
float gimbal_get_pitch_deg(void);

#endif // SENTRY_GIMBAL_H
