/**
 * @file    chassis_power.c
 * @brief   底盘功率控制:电机模型预测功率 → 电流指令衰减系数
 * @layer   APP
 * @author  小猫大王
 * @note    背景(DEV-20261124-16):跟随转向瞬时功率可到 300W、小陀螺反转 110~150W。
 * @note    2026 通信协议 V1.2.0 把 0x0202 的 chassis_power 改成保留位,裁判不再回传
 *          功率,旧的"实测功率慢闭环"失去输入(实测恒 0,scale 恒 1,等于没开)。
 *          改为电机模型预测:输入只有电流指令与转子转速,不依赖裁判数据。
 * @note    模型与控制律参考天工大 304 PowerManager(MIT,2026 RMUL 实车验证):
 *          取电流/转速绝对值;超限时解二次方程求电流衰减系数,而不是线性比例。
 *          线性比例对 K4*i^2 项会过度/不足限制,二次解在模型内是精确的。
 * @note    2026-11-26(DEV-20261126-12):
 *          - 输入从"低通后的反馈电流"改为"速度环刚算出的指令电流"(`current_cmd`),
 *            少约 10ms 滞后(DJI_CURRENT_SMOOTH_COEF=0.9);
 *          - 电流/转速取绝对值,制动工况按正功率保守估计;
 *          - 衰减系数直接乘电机输出电流(`output_scale`),不再只压 MaxOut;
 *          - 加标定系数 SCALE/BIAS 与一阶低通。
 * @note    2026-11-27(DEV-20261127-200745):**制动轮不再计入功率预算**。
 *          裁判功率量的是电池输入,回馈制动的电功率为负,不会超限;
 *          旧实现取 |i||ω| 把 2000 mm/s 的满刹车判成约 450W,scale 压到 0.20,
 *          实车表现为松杆滑行、8000 mm/s² 的刹车斜坡基本不生效。
 *          现在 i 与 ω 反号的轮直接跳过求和,scale 只约束电动轮
 *          (配套 `chassis_limit_output` 对制动轮保持 output_scale=1)。
 */
#include "chassis_power.h"
#include "robot_def.h"

#include <math.h>

static float s_scale_lp = 1.0f; // 衰减系数低通状态,0~1

void chassis_power_reset(void)
{
    s_scale_lp = 1.0f;
}

/**
 * @brief 单轮功率多项式的三个系数(S0 常数项 / S1 一次项 / S2 二次项)
 * @param i_raw 未限幅电流指令,C620 原始码
 * @param w_dps 转子转速,deg/s
 * @note  模型输入 i=码/1000、w=rpm;取绝对值与 304 默认口径一致。
 *        调用方须先剔除制动轮(i 与 w 反号),本函数不做符号判断。
 */
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
        /* 制动轮(i 与 ω 反号)跳过:回馈制动的电功率为负,不占电池功率预算。
         * 静止(w=0)按电动处理,保证起步电流仍受模型约束。 */
        if (current_cmd[k] * w_dps[k] < 0.0f)
            continue;
        float c0, c1, c2;
        motor_power_coeffs(current_cmd[k], w_dps[k], &c0, &c1, &c2);
        s0 += c0;
        s1 += c1;
        s2 += c2;
    }

    /* 标定修正:P_corr = SCALE * P_model + BIAS。BIAS 是整车偏置,加到总常数项。 */
    s0 = CHASSIS_POWER_MODEL_SCALE * s0 + CHASSIS_POWER_MODEL_BIAS;
    s1 *= CHASSIS_POWER_MODEL_SCALE;
    s2 *= CHASSIS_POWER_MODEL_SCALE;

    const float p_full = s0 + s1 + s2; // 衰减系数为 1 时的预测功率
    if (p_pred_out != 0)
        *p_pred_out = (p_full > 0.0f) ? p_full : 0.0f;

    float s_raw = 1.0f;
    if (p_full > limit_w)
    {
        /* 解 S2*s^2 + S1*s + (S0 - limit) = 0,取 [0,1] 内较大根(304 limiter 口径)。
         * S2 正常恒正;退化情况逐级兜底,无解则给 0,由下限钳位接住。 */
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

    /* 快降慢升(DEV-20261126-31):功率超限必须同拍降额,否则切入/松杆的
     * 100~110W 尖峰会先跑 15ms 才被 scale 追上;恢复方向再低通,避免电流阶跃。
     * 原对称 0.6/拍连降额也过滤,保护延迟正好盖住尖峰第一段。 */
    if (s_raw < s_scale_lp)
        s_scale_lp = s_raw; // 降额:同拍生效
    else
        s_scale_lp += CHASSIS_POWER_SCALE_LP * (s_raw - s_scale_lp); // 恢复:慢升
    if (s_scale_lp > 1.0f)
        s_scale_lp = 1.0f;
    const float floor_s = fmaxf(CHASSIS_POWER_SCALE_MIN, min_scale);
    if (s_scale_lp < floor_s)
        s_scale_lp = floor_s;

    /* 缩放后的模型功率:验证“限幅后实际模型”用,避免只看限幅前的 p */
    if (p_post_out != 0)
        *p_post_out = s0 + s1 * s_scale_lp + s2 * s_scale_lp * s_scale_lp;
    return s_scale_lp;
}
