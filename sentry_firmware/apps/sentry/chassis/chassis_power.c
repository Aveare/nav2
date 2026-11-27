/**
 * @file    chassis_power.c
 * @brief   哨兵底盘功率控制:电机模型预测功率 → 电流指令衰减系数
 * @layer   APP
 * @author  小猫大王
 * @note    模型/控制律移植自步兵(DEV-20261126-12/31/33),参数在
 *          config/sentry_2026/config_chassis.h,首次哨兵台架需按裁判功率复核。
 */
#include "chassis_power.h"
#include "config.h"

#include <math.h>

static float s_scale_lp = 1.0f; // 衰减系数低通状态,0~1

void chassis_power_reset(void)
{
    s_scale_lp = 1.0f;
}

static void motor_power_coeffs(float i_raw, float w_dps, float *c0, float *c1, float *c2)
{
    const float i = fabsf(i_raw) / CHASSIS_POWER_CURRENT_DIV;
    const float w = fabsf(w_dps) / 6.0f; // deg/s → rpm
    *c0 = CHASSIS_POWER_MODEL_K0 + CHASSIS_POWER_MODEL_K2 * w + CHASSIS_POWER_MODEL_K5 * w * w;
    *c1 = (CHASSIS_POWER_MODEL_K1 + CHASSIS_POWER_MODEL_K3 * w) * i;
    *c2 = CHASSIS_POWER_MODEL_K4 * i * i;
}

float chassis_power_scale(const float current_cmd[4], const float w_dps[4], float limit_w,
                          float min_scale, float *p_pred_out, float *p_post_out)
{
    if (!(limit_w > 1.0f) || limit_w > 200.0f)
        limit_w = CHASSIS_POWER_LIMIT_DEFAULT;
    if (p_pred_out != 0)
        *p_pred_out = 0.0f;
    if (p_post_out != 0)
        *p_post_out = 0.0f;
    if (!(min_scale >= 0.0f))
        min_scale = 0.0f;
    if (min_scale > 1.0f)
        min_scale = 1.0f;

    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
    for (uint8_t k = 0u; k < 4u; k++)
    {
        float c0, c1, c2;
        motor_power_coeffs(current_cmd[k], w_dps[k], &c0, &c1, &c2);
        s0 += c0;
        s1 += c1;
        s2 += c2;
    }

    s0 = CHASSIS_POWER_MODEL_SCALE * s0 + CHASSIS_POWER_MODEL_BIAS;
    s1 *= CHASSIS_POWER_MODEL_SCALE;
    s2 *= CHASSIS_POWER_MODEL_SCALE;

    const float p_full = s0 + s1 + s2;
    if (p_pred_out != 0)
        *p_pred_out = (p_full > 0.0f) ? p_full : 0.0f;

    float s_raw = 1.0f;
    if (p_full > limit_w)
    {
        const float a = s2;
        const float b = s1;
        const float c = s0 - limit_w;

        if (fabsf(a) < 1e-9f)
        {
            if (fabsf(b) < 1e-9f)
            {
                s_raw = (c <= 0.0f) ? 1.0f : 0.0f;
            }
            else
            {
                const float k = -c / b;
                s_raw = (k <= 0.0f) ? 0.0f : ((k >= 1.0f) ? 1.0f : k);
            }
        }
        else
        {
            const float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f)
            {
                s_raw = 0.0f;
            }
            else
            {
                const float sq = sqrtf(disc);
                const float k1 = (-b - sq) / (2.0f * a);
                const float k2 = (-b + sq) / (2.0f * a);
                float best = -1.0f;
                if (k1 >= 0.0f && k1 <= 1.0f)
                    best = k1;
                if (k2 >= 0.0f && k2 <= 1.0f && k2 > best)
                    best = k2;
                s_raw = (best >= 0.0f) ? best : 0.0f;
            }
        }
    }

    if (s_raw > 1.0f)
        s_raw = 1.0f;
    if (s_raw < 0.0f)
        s_raw = 0.0f;

    /* 快降慢升:功率超限同拍降额,恢复方向低通。 */
    if (s_raw < s_scale_lp)
        s_scale_lp = s_raw;
    else
        s_scale_lp += CHASSIS_POWER_SCALE_LP * (s_raw - s_scale_lp);
    if (s_scale_lp > 1.0f)
        s_scale_lp = 1.0f;
    const float floor_s = fmaxf(CHASSIS_POWER_SCALE_MIN, min_scale);
    if (s_scale_lp < floor_s)
        s_scale_lp = floor_s;

    if (p_post_out != 0)
        *p_post_out = s0 + s1 * s_scale_lp + s2 * s_scale_lp * s_scale_lp;
    return s_scale_lp;
}
