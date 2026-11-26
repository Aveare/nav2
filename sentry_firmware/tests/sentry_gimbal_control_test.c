/**
 * @file    sentry_gimbal_control_test.c
 * @brief   哨兵 SI 单位串级 PID 主机测试
 * @layer   TESTS
 * @author  modified by 小猫大王
 */
#include "gimbal_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint32_t test_us = 1000000u;

float dwt_get_delta_t(uint32_t *last)
{
    const float dt = (test_us - *last) * 0.000001f;
    *last = test_us;
    return dt;
}

uint32_t dwt_get_timeline_ms(void)
{
    return test_us / 1000u;
}

static bool sentry_test_step(gimbal_axis_controller_t *axis, float angle, float speed,
                             float target)
{
    test_us += 5000u;
    return gimbal_axis_update(axis, angle, speed, target, test_us * 0.001f);
}

int main(void)
{
    const gimbal_axis_config_t config = {
        .angle =
            {
                .Kp = 12.0f,
                .MaxOut = 1.5f,
                .DeadBand = 0.005f,
                .Improve = PID_Derivative_On_Measurement,
                .DerivativeLimit = 0.5f,
            },
        .speed =
            {
                .Kp = 0.6f,
                .Ki = 0.1f,
                .MaxOut = 0.8f,
                .IntegralLimit = 0.1f,
                .Improve = PID_Integral_Limit | PID_Trapezoid_Intergral | PID_OutputFilter,
                .Output_LPF_RC = 0.012f,
            },
        .kd_near = 0.4f,
        .kd_far = 0.1f,
        .blend_lo_rad = 0.07f,
        .blend_hi_rad = 0.14f,
        .feedforward_gain = 1.0f,
        .feedforward_lpf = 0.3f,
    };
    gimbal_axis_controller_t axis;
    gimbal_axis_init(&axis, &config);
    assert(sentry_test_step(&axis, 2.0f, 0.0f, 2.0f) && axis.torque_nm == 0.0f);
    assert(sentry_test_step(&axis, 2.0f, 0.0f, 2.1f) && axis.torque_nm > 0.0f);
    assert(axis.rate_feedforward == 0.0f);
    for (unsigned int i = 0; i < 3000u; ++i)
    {
        assert(sentry_test_step(&axis, 2.0f, 0.0f, 3.0f));
        assert(fabsf(axis.torque_nm) <= 0.80001f);
        assert(fabsf(axis.speed_target_rad_s) <= 1.50001f);
        assert(fabsf(axis.speed.Iout) <= 0.10001f);
    }
    gimbal_axis_reset(&axis);
    assert(!axis.initialized && axis.torque_nm == 0.0f && axis.speed.Iout == 0.0f);

    assert(gimbal_axis_update_speed(&axis, 0.0f, 0.0f, test_us * 0.001f));
    test_us += 5000u;
    assert(gimbal_axis_update_speed(&axis, 0.5f, 0.0f, test_us * 0.001f));
    assert(axis.torque_nm < 0.0f && fabsf(axis.torque_nm) <= 0.80001f);
    assert(!gimbal_axis_update_speed(&axis, NAN, 0.0f, test_us * 0.001f));

    puts("PASS: sentry cascade SI torque/rate/integral limits and speed-only damping");
    return 0;
}
