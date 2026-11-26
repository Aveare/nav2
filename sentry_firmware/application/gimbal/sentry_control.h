/**
 * @file    sentry_control.h
 * @brief   哨兵云台串级 PID 力矩计算
 * @layer   APP
 */
#ifndef SENTRY_CONTROL_H
#define SENTRY_CONTROL_H

#include "controller.h"
#include <stdbool.h>

typedef struct {
    PID_Init_Config_s angle; // rad -> rad/s
    PID_Init_Config_s speed; // rad/s -> Nm
    float kd_near, kd_far;
    float blend_lo_rad, blend_hi_rad;
    float feedforward_gain, feedforward_lpf;
} SentryAxisConfig;

typedef struct {
    PIDInstance angle, speed;
    SentryAxisConfig config;
    float last_target_rad, last_ms, rate_feedforward;
    float speed_target_rad_s, torque_nm;
    bool initialized;
} SentryAxisController;

/** 初始化串级环；参数单位为 rad、rad/s、Nm。单写者为云台任务。 */
void SentryAxisInit(SentryAxisController *axis, const SentryAxisConfig *config);
/** 停止时清积分、微分历史与前馈；下次更新从实测位置建立历史。 */
void SentryAxisReset(SentryAxisController *axis);
/**
 * @brief  角度环、限幅速度前馈、速度 PI，输出对应控制轴的力矩
 * @param  angle_rad 实测角，rad；yaw为关节角或世界航向（随模式），pitch为4310输出轴角
 * @param  speed_rad_s 对应角速度，rad/s
 * @param  target_rad 目标角，rad
 * @param  now_ms 同一任务使用的当前时间，ms
 * @return 输入或周期异常时 false 并清输出；首次有效调用零力建立历史
 */
bool SentryAxisUpdate(SentryAxisController *axis, float angle_rad, float speed_rad_s,
                      float target_rad, float now_ms);

/**
 * @brief  仅速度PI，旁路角度环，不记忆或追赶航向；由调用方在模式切换时复位
 * @param  speed_rad_s 实测世界角速度，rad/s
 * @param  target_rad_s 目标世界角速度，rad/s；稳速补偿时为0
 * @param  now_ms 当前时间，ms
 * @return 首次调用零力建立历史；输入或周期异常时false并清输出
 */
bool SentryAxisUpdateSpeed(SentryAxisController *axis, float speed_rad_s,
                          float target_rad_s, float now_ms);

#endif
