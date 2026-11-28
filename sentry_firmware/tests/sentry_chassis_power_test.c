/**
 * @file    sentry_chassis_power_test.c
 * @brief   哨兵功率模型主机测试:衰减系数范围/下限/超限降额
 * @layer   TESTS
 * @author  小猫大王
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "chassis_power.h"
#include "config.h"

int main(void)
{
    const float zero_i[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float zero_w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float big_i[4] = {10000.0f, 10000.0f, 10000.0f, 10000.0f};
    const float spin_w[4] = {4000.0f, 4000.0f, 4000.0f, 4000.0f};
    float p_pred = 0.0f;
    float p_post = 0.0f;

    chassis_power_reset();
    float s = chassis_power_scale(zero_i, zero_w, 85.0f, CHASSIS_POWER_MIN_SCALE, &p_pred, &p_post);
    assert(s > 0.99f && s <= 1.0f);
    assert(p_post <= p_pred + 1e-3f);

    chassis_power_reset();
    s = chassis_power_scale(big_i, spin_w, 85.0f, CHASSIS_POWER_MIN_SCALE, &p_pred, &p_post);
    assert(isfinite(s));
    assert(s >= CHASSIS_POWER_MIN_SCALE - 1e-6f && s <= 1.0f);
    assert(p_pred > 85.0f);
    assert(p_post <= p_pred + 1e-3f);

    /* 非法上限回退默认值,不应产生 NaN/越界。 */
    chassis_power_reset();
    s = chassis_power_scale(big_i, spin_w, NAN, CHASSIS_POWER_MIN_SCALE, 0, 0);
    assert(isfinite(s) && s >= CHASSIS_POWER_MIN_SCALE - 1e-6f && s <= 1.0f);

    /* 制动轮(i 与 ω 反号)退出功率预算,与步兵 DEV-20261127-200745 同源:
     * 全制动 p_pred=0 且 scale=1;混合工况预测功率恰为全电动的一半。 */
    const float brake_cmd[4] = {-10000.0f, -10000.0f, -10000.0f, -10000.0f};
    chassis_power_reset();
    s = chassis_power_scale(brake_cmd, spin_w, 85.0f, CHASSIS_POWER_MIN_SCALE, &p_pred, &p_post);
    assert(s > 0.99f && s <= 1.0f);
    assert(p_pred == 0.0f && p_post == 0.0f);

    {
        /* 高上限(200W)保证 s=1,两种工况差应恰为制动轮的贡献(=0)。 */
        const float mix_cmd[4] = {10000.0f, -10000.0f, 10000.0f, -10000.0f};
        float p_all = 0.0f, p_mix = 0.0f;
        chassis_power_reset();
        (void)chassis_power_scale(big_i, spin_w, 200.0f, 0.0f, &p_all, 0);
        chassis_power_reset();
        (void)chassis_power_scale(mix_cmd, spin_w, 200.0f, 0.0f, &p_mix, 0);
        assert(p_all > 100.0f);
        assert(fabsf(p_mix * 2.0f - p_all) < 0.5f);
    }

    printf("sentry chassis power model tests passed\n");
    return 0;
}
