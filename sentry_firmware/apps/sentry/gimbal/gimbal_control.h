/**
 * @file    gimbal_control.h
 * @brief   哨兵达妙云台轴 SI 单位串级 PID
 * @layer   APP
 * @author  modified by 小猫大王
 */
#ifndef SENTRY_GIMBAL_CONTROL_H
#define SENTRY_GIMBAL_CONTROL_H

#include "controller.h"
#include <stdbool.h>

typedef struct
{
    pid_init_config_t angle; /* rad -> rad/s */
    pid_init_config_t speed; /* rad/s -> N.m */
    float kd_near;
    float kd_far;
    float blend_lo_rad;
    float blend_hi_rad;
    float feedforward_gain;
    float feedforward_lpf;
} gimbal_axis_config_t;

typedef struct
{
    pid_instance_t angle;
    pid_instance_t speed;
    gimbal_axis_config_t config;
    float last_target_rad;
    float last_ms;
    float rate_feedforward;
    float speed_target_rad_s;
    float torque_nm;
    bool initialized;
} gimbal_axis_controller_t;

void gimbal_axis_init(gimbal_axis_controller_t *axis, const gimbal_axis_config_t *config);
void gimbal_axis_reset(gimbal_axis_controller_t *axis);
bool gimbal_axis_update(gimbal_axis_controller_t *axis, float angle_rad, float speed_rad_s,
                        float target_rad, float now_ms);
bool gimbal_axis_update_speed(gimbal_axis_controller_t *axis, float speed_rad_s,
                              float target_rad_s, float now_ms);

#endif /* SENTRY_GIMBAL_CONTROL_H */
