/**
 * @file    sentry_control.c
 * @brief   轻云台串级 PID 力矩计算
 * @layer   APP
 * @note    结构参考 RM-JIT-CR-Sentinel；输出改为4310所需Nm，不使用旧DJI电调码值。
 */
#include "sentry_control.h"
#include <math.h>
#include <string.h>

static float Clamp(float value, float limit)
{
    return fminf(limit, fmaxf(-limit, value));
}

void SentryAxisInit(SentryAxisController *axis, const SentryAxisConfig *config)
{
    memset(axis, 0, sizeof(*axis));
    axis->config = *config;
    PIDInit(&axis->angle, &axis->config.angle);
    PIDInit(&axis->speed, &axis->config.speed);
}

void SentryAxisReset(SentryAxisController *axis)
{
    PIDClear(&axis->angle);
    PIDClear(&axis->speed);
    axis->rate_feedforward = 0.0f;
    axis->speed_target_rad_s = 0.0f;
    axis->torque_nm = 0.0f;
    axis->initialized = false;
}

bool SentryAxisUpdate(SentryAxisController *axis, float angle_rad, float speed_rad_s,
                      float target_rad, float now_ms)
{
    if (!isfinite(angle_rad) || !isfinite(speed_rad_s) ||
        !isfinite(target_rad) || !isfinite(now_ms))
    {
        SentryAxisReset(axis);
        return false;
    }
    if (!axis->initialized)
    {
        SentryAxisReset(axis);
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
        SentryAxisReset(axis);
        return false;
    }
    const float error = fabsf(target_rad - angle_rad);
    const float blend = fminf(1.0f, fmaxf(0.0f,
        (error - axis->config.blend_lo_rad) /
        (axis->config.blend_hi_rad - axis->config.blend_lo_rad)));
    axis->angle.Kd = axis->config.kd_near +
                     blend * (axis->config.kd_far - axis->config.kd_near);
    const float delta = target_rad - axis->last_target_rad;
    const float rate = fabsf(delta) <= axis->angle.MaxOut * dt * 1.1f
                           ? delta / dt * axis->config.feedforward_gain : 0.0f;
    axis->rate_feedforward += axis->config.feedforward_lpf * (rate - axis->rate_feedforward);
    const float angle_output = PIDCalculate(&axis->angle, angle_rad, target_rad);
    axis->speed_target_rad_s = Clamp(angle_output + axis->rate_feedforward, axis->angle.MaxOut);
    axis->torque_nm = PIDCalculate(&axis->speed, speed_rad_s, axis->speed_target_rad_s);
    axis->last_target_rad = target_rad;
    axis->last_ms = now_ms;
    if (!isfinite(axis->torque_nm))
    {
        SentryAxisReset(axis);
        return false;
    }
    return true;
}


/* 小陀螺只控制世界角速度，不恢复任何历史航向。 */
bool SentryAxisUpdateSpeed(SentryAxisController *axis, float speed_rad_s,
                          float target_rad_s, float now_ms)
{
    if (!isfinite(speed_rad_s) || !isfinite(target_rad_s) || !isfinite(now_ms))
    {
        SentryAxisReset(axis);
        return false;
    }
    if (!axis->initialized)
    {
        SentryAxisReset(axis);
        axis->speed.Last_Measure = speed_rad_s;
        axis->last_ms = now_ms;
        axis->initialized = true;
        return true;
    }
    const float dt = (now_ms - axis->last_ms) * 0.001f;
    if (dt <= 0.0f || dt > 0.02f)
    {
        SentryAxisReset(axis);
        return false;
    }
    PIDClear(&axis->angle);
    axis->rate_feedforward = 0.0f;
    axis->speed_target_rad_s = Clamp(target_rad_s, axis->angle.MaxOut);
    axis->torque_nm = PIDCalculate(&axis->speed, speed_rad_s, axis->speed_target_rad_s);
    axis->last_ms = now_ms;
    if (!isfinite(axis->torque_nm))
    {
        SentryAxisReset(axis);
        return false;
    }
    return true;
}
