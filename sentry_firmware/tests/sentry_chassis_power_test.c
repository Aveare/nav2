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

    printf("sentry chassis power model tests passed\n");
    return 0;
}
