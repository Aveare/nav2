/**
 * @file    gimbal_control.c
 * @brief   哨兵达妙云台轴 SI 单位串级 PID
 * @layer   APP
 * @author  modified by 小猫大王
 * @note    从本车 RM_shaobin2 哨兵控制器移植，角度/速度/力矩分别使用 rad、rad/s、N.m。
 */
#include "gimbal_control.h"
#include <math.h>
#include <string.h>

static float gimbal_axis_clamp(float value, float limit)
{
    return fminf(limit, fmaxf(-limit, value));
}

void gimbal_axis_init(gimbal_axis_controller_t *axis, const gimbal_axis_config_t *config)
{
    memset(axis, 0, sizeof(*axis));
    axis->config = *config;
    pid_init(&axis->angle, &axis->config.angle);
    pid_init(&axis->speed, &axis->config.speed);
}

void gimbal_axis_reset(gimbal_axis_controller_t *axis)
{
    pid_clear(&axis->angle);
    pid_clear(&axis->speed);
    axis->rate_feedforward = 0.0f;
    axis->speed_target_rad_s = 0.0f;
    axis->torque_nm = 0.0f;
    axis->initialized = false;
}

bool gimbal_axis_update(gimbal_axis_controller_t *axis, float angle_rad, float speed_rad_s,
                        float target_rad, float now_ms)
{
    if (!isfinite(angle_rad) || !isfinite(speed_rad_s) || !isfinite(target_rad) ||
        !isfinite(now_ms))
    {
        gimbal_axis_reset(axis);
        return false;
    }
    if (!axis->initialized)
    {
        gimbal_axis_reset(axis);
        axis->angle.Last_Measure = angle_rad;
        axis->speed.Last_Measure = speed_rad_s;
        axis->last_target_rad = target_rad;
        axis->last_ms = now_ms;
        axis->initialized = true;
        return true;
    }

    const float dt = (now_ms - axis->last_ms) * 0.001f;
    if (dt <= 0.0f || dt > 0.02f)
    {
        gimbal_axis_reset(axis);
        return false;
    }

    const float error = fabsf(target_rad - angle_rad);
    const float blend_range = axis->config.blend_hi_rad - axis->config.blend_lo_rad;
    const float blend = (blend_range > 0.0f)
                            ? fminf(1.0f, fmaxf(0.0f, (error - axis->config.blend_lo_rad) /
                                                              blend_range))
                            : 0.0f;
    axis->angle.Kd = axis->config.kd_near + blend * (axis->config.kd_far - axis->config.kd_near);

    const float delta = target_rad - axis->last_target_rad;
    const float rate = fabsf(delta) <= axis->angle.MaxOut * dt * 1.1f
                           ? delta / dt * axis->config.feedforward_gain
                           : 0.0f;
    axis->rate_feedforward +=
        axis->config.feedforward_lpf * (rate - axis->rate_feedforward);
    const float angle_output = pid_calculate(&axis->angle, angle_rad, target_rad);
    axis->speed_target_rad_s =
        gimbal_axis_clamp(angle_output + axis->rate_feedforward, axis->angle.MaxOut);
    axis->torque_nm = pid_calculate(&axis->speed, speed_rad_s, axis->speed_target_rad_s);
    axis->last_target_rad = target_rad;
    axis->last_ms = now_ms;
    if (!isfinite(axis->torque_nm))
    {
        gimbal_axis_reset(axis);
        return false;
    }
    return true;
}

bool gimbal_axis_update_speed(gimbal_axis_controller_t *axis, float speed_rad_s,
                              float target_rad_s, float now_ms)
{
    if (!isfinite(speed_rad_s) || !isfinite(target_rad_s) || !isfinite(now_ms))
    {
        gimbal_axis_reset(axis);
        return false;
    }
    if (!axis->initialized)
    {
        gimbal_axis_reset(axis);
        axis->speed.Last_Measure = speed_rad_s;
        axis->last_ms = now_ms;
        axis->initialized = true;
        return true;
    }

    const float dt = (now_ms - axis->last_ms) * 0.001f;
    if (dt <= 0.0f || dt > 0.02f)
    {
        gimbal_axis_reset(axis);
        return false;
    }
    pid_clear(&axis->angle);
    axis->rate_feedforward = 0.0f;
    axis->speed_target_rad_s = gimbal_axis_clamp(target_rad_s, axis->angle.MaxOut);
    axis->torque_nm = pid_calculate(&axis->speed, speed_rad_s, axis->speed_target_rad_s);
    axis->last_ms = now_ms;
    if (!isfinite(axis->torque_nm))
    {
        gimbal_axis_reset(axis);
        return false;
    }
    return true;
}
