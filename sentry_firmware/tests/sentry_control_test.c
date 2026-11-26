/* 真实串级PID：验证单位、饱和、复位和异常时间处理。 */
#include "sentry_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint32_t test_us = 1000000u;
float DWT_GetDeltaT(uint32_t *last)
{
    float dt = (test_us - *last) * 0.000001f;
    *last = test_us;
    return dt;
}
uint32_t DWT_GetTimeline_ms(void) { return test_us / 1000u; }

static bool Step(SentryAxisController *axis, float angle, float speed, float target)
{
    test_us += 5000u;
    return SentryAxisUpdate(axis, angle, speed, target, test_us * 0.001f);
}

int main(void)
{
    SentryAxisConfig config = {
        .angle = {.Kp = 12, .MaxOut = 1.5f, .DeadBand = 0.005f,
                  .Improve = PID_Derivative_On_Measurement, .DerivativeLimit = 0.5f},
        .speed = {.Kp = 0.6f, .Ki = 0.1f, .MaxOut = 0.8f, .IntegralLimit = 0.1f,
                  .Improve = PID_Integral_Limit | PID_Trapezoid_Intergral | PID_OutputFilter,
                  .Output_LPF_RC = 0.012f},
        .kd_near = 0.4f, .kd_far = 0.1f,
        .blend_lo_rad = 0.07f, .blend_hi_rad = 0.14f,
        .feedforward_gain = 1, .feedforward_lpf = 0.3f,
    };
    SentryAxisController axis;
    SentryAxisInit(&axis, &config);
    assert(Step(&axis, 2, 0, 2) && axis.torque_nm == 0); // 非零角开环历史不产生D冲击
    assert(Step(&axis, 2, 0, 2) && axis.torque_nm == 0);
    assert(Step(&axis, 2, 0, 2.1f) && axis.torque_nm > 0);
    assert(axis.rate_feedforward == 0); // 目标跳变不注入速度脉冲
    for (unsigned i=0; i<3000; ++i) {
        assert(Step(&axis, 2, 0, 3));
        assert(fabsf(axis.torque_nm) <= 0.80001f);
        assert(fabsf(axis.speed_target_rad_s) <= 1.50001f);
        assert(fabsf(axis.speed.Iout) <= 0.10001f);
    }
    assert(axis.torque_nm > 0.7f); // 上限仍是Nm，而非DJI原始码
    SentryAxisReset(&axis);
    assert(!axis.initialized && axis.torque_nm == 0 && axis.speed.Iout == 0);
    assert(Step(&axis, 2, 0, 2));
    assert(Step(&axis, 2, 0, 1.9f) && axis.torque_nm < 0);
    SentryAxisReset(&axis);
    assert(Step(&axis, 0, 0, 0));
    assert(Step(&axis, 0.001f, 0.2f, 0.001f));
    assert(axis.rate_feedforward > 0 && axis.speed_target_rad_s > 0); // 恒速目标前馈
    assert(!Step(&axis, NAN, 0, 0) && axis.torque_nm == 0);
    assert(Step(&axis, 0, 0, 0));
    test_us += 100000u;
    assert(!SentryAxisUpdate(&axis, 0, 0, 1, test_us * 0.001f));
    assert(axis.torque_nm == 0 && !axis.initialized);

    /* 纯速度模式：零角速度目标、正负阻尼、限矩、异常周期与非有限输入。 */
    SentryAxisReset(&axis);
    assert(SentryAxisUpdateSpeed(&axis, 0, 0, test_us * 0.001f));
    for (unsigned i=0; i<100; ++i) {
        test_us += 5000u;
        assert(SentryAxisUpdateSpeed(&axis, 0.5f, 0, test_us * 0.001f));
        assert(axis.speed_target_rad_s == 0 && axis.torque_nm < 0);
        assert(fabsf(axis.torque_nm) <= 0.80001f);
        assert(fabsf(axis.speed.Iout) <= 0.10001f);
        assert(axis.angle.Output == 0 && axis.rate_feedforward == 0);
    }
    SentryAxisReset(&axis);
    assert(SentryAxisUpdateSpeed(&axis, -0.5f, 0, test_us * 0.001f));
    test_us += 5000u;
    assert(SentryAxisUpdateSpeed(&axis, -0.5f, 0, test_us * 0.001f));
    assert(axis.torque_nm > 0);
    assert(!SentryAxisUpdateSpeed(&axis, 0, 0, test_us * 0.001f)); // 同一时刻重复调用
    assert(!axis.initialized && axis.torque_nm == 0);
    assert(SentryAxisUpdateSpeed(&axis, 0, 0, test_us * 0.001f));
    test_us += 100000u;
    assert(!SentryAxisUpdateSpeed(&axis, 0, 0, test_us * 0.001f));
    assert(!SentryAxisUpdateSpeed(&axis, NAN, 0, test_us * 0.001f));
    assert(!SentryAxisUpdateSpeed(&axis, 0, INFINITY, test_us * 0.001f));
    assert(!SentryAxisUpdateSpeed(&axis, 0, 0, NAN));

    puts("PASS: cascade SI torque/rate/integral limits, feedback-D seed, feedforward, reset, NaN/timing");
    return 0;
}
