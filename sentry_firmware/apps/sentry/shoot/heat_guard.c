/**
 * @file    heat_guard.c
 * @brief   哨兵热量守卫实现:260/30/10 规则下的本地估计与射速闸门
 * @layer   APP
 * @author  小猫大王
 * @note    200Hz 调用;内部只做估算与裁决,不碰电机。
 */
#include "heat_guard.h"
#include "config.h"
#include "referee_task.h"
#include "dwt_port.h"
#include "general_def.h"
#include "log_port.h"
#include <string.h>

typedef struct
{
    uint8_t initialized;
    uint8_t referee_seen;
    uint8_t allowance_seen;
    uint8_t degraded;
    uint32_t last_tick;
    uint32_t last_rx_ms;
    float q;
    float q_limit;
    float cooling;
    float last_ms;
    uint8_t down_count;
    float single_last_ms;
    heat_guard_state_e state;
} heat_guard_ctx_t;

static heat_guard_ctx_t heat;

static float heat_guard_margin_burst(void)
{
    float m = HEAT_GUARD_MARGIN_BURST;
    float cap = heat.q_limit * HEAT_GUARD_MARGIN_MAX_RATIO;
    return (m > cap) ? cap : m;
}

static float heat_guard_margin_single(void)
{
    float m = HEAT_GUARD_MARGIN_SINGLE;
    float cap = heat.q_limit * HEAT_GUARD_MARGIN_MAX_RATIO;
    return (m > cap) ? cap : m;
}

static uint8_t heat_guard_allowance_blocked(const referee_info_t *ref)
{
#if HEAT_GUARD_ALLOWANCE_ENABLE
    /* 0x0208 没有独立帧号;在首次看到"非零允许量"之前不能把 0 当成"额度耗尽",
     * 否则裁判刚上电/未发 0x0208 时会误锁枪。看到过非零后,再出现 0 才是真禁发。 */
    return (ref != NULL && ref->init_flag && heat.allowance_seen &&
            ref->AllowanceData.projectile_allowance_17mm < 1u)
               ? 1u
               : 0u;
#else
    (void)ref;
    return 0u;
#endif
}

void heat_guard_init(void)
{
    memset(&heat, 0, sizeof(heat));
    heat.q_limit = HEAT_GUARD_DEFAULT_HEAT_LIMIT;
    heat.cooling = HEAT_GUARD_DEFAULT_COOLING;
    heat.state = HEAT_GUARD_OFF;
    heat.initialized = 1u;
    heat.last_ms = 0.0f;
}

void heat_guard_update(void)
{
#if HEAT_GUARD_ENABLE
    if (!heat.initialized)
        heat_guard_init();

    const float now_ms = dwt_get_timeline_ms();
    float dt_s = (now_ms - heat.last_ms) * 0.001f;
    heat.last_ms = now_ms;
    if (dt_s < 0.0f || dt_s > 0.5f)
        dt_s = 0.005f;

    /* 1. 本拍冷却 */
    heat.q -= heat.cooling * dt_s;
    if (heat.q < 0.0f)
        heat.q = 0.0f;

    /* 2. 裁判同步 */
    referee_info_t *ref = referee_get_info();
    uint8_t new_heat_frame = 0;
    if (ref != NULL && ref->init_flag)
    {
        if (ref->AllowanceData.projectile_allowance_17mm > 0u)
            heat.allowance_seen = 1u;
        if (ref->PowerHeatTick != heat.last_tick)
        {
            heat.last_tick = ref->PowerHeatTick;
            heat.last_rx_ms = (uint32_t)now_ms;
            new_heat_frame = 1u;
            heat.referee_seen = 1u;
        }
    }

    if (new_heat_frame)
    {
        float q0 = (float)ref->GameRobotState.shooter_barrel_heat_limit;
        float cool = (float)ref->GameRobotState.shooter_barrel_cooling_value;
        if (q0 <= 0.0f)
            q0 = HEAT_GUARD_DEFAULT_HEAT_LIMIT;
        if (cool <= 0.0f)
            cool = HEAT_GUARD_DEFAULT_COOLING;
#if HEAT_GUARD_COOLING_USE_BUFF
        cool += (float)ref->BuffMusk.cooling_buff;
#endif
        heat.q_limit = q0;
        heat.cooling = cool;

        float qr = (float)ref->PowerHeatData.shooter_17mm_barrel_heat;
        if (qr > heat.q + HEAT_GUARD_DOWN_TOL)
        {
            heat.q = qr; // 向上校正:立即
            heat.down_count = 0u;
        }
        else if (qr < heat.q - HEAT_GUARD_DOWN_TOL)
        {
            if (++heat.down_count >= HEAT_GUARD_DOWN_FRAMES)
            {
                heat.q = qr; // 向下校正:连续多帧
                heat.down_count = 0u;
            }
        }
        else
        {
            heat.down_count = 0u;
            heat.q = qr;
        }
        heat.degraded = 0u;
    }
    else if (heat.referee_seen &&
             (now_ms - (float)heat.last_rx_ms) > HEAT_GUARD_REF_TIMEOUT_MS)
    {
        heat.degraded = 1u;
    }

    /* 3. 状态裁决 */
    if (!heat.referee_seen)
    {
        heat.state = HEAT_GUARD_OFF;
    }
    else if (heat_guard_allowance_blocked(ref))
    {
        heat.state = HEAT_GUARD_HOLD;
    }
    else
    {
        const float remaining = heat.q_limit - heat.q;
        const float margin_single = heat_guard_margin_single();
        const float margin_burst = heat_guard_margin_burst();
        const float hold_threshold = margin_single + HEAT_GUARD_HEAT_PER_BULLET;
        const float recover_threshold = hold_threshold + HEAT_GUARD_HYST;

        if (heat.state == HEAT_GUARD_HOLD)
        {
            if (remaining > recover_threshold)
                heat.state = HEAT_GUARD_NORMAL;
        }
        else if (remaining <= hold_threshold)
        {
            heat.state = HEAT_GUARD_HOLD;
        }
        else if (remaining <= margin_burst + HEAT_GUARD_HEAT_PER_BULLET)
        {
            heat.state = HEAT_GUARD_SINGLE;
        }
        else
        {
            heat.state = HEAT_GUARD_NORMAL;
        }
        (void)margin_single;
    }

#if HEAT_GUARD_LOG
    {
        static heat_guard_state_e last_state = HEAT_GUARD_OFF;
        static float last_log_ms = 0.0f;
        if (heat.state != last_state)
        {
            LOG_WARN("[heat] state %d -> %d (q=%d/%d c=%d)", (int)last_state, (int)heat.state,
                     (int)heat.q, (int)heat.q_limit, (int)heat.cooling);
            last_state = heat.state;
        }
        if (now_ms - last_log_ms >= 1000.0f)
        {
            last_log_ms = now_ms;
            LOG_INFO("[heat] q=%d/%d cool=%d state=%d deg=%u", (int)heat.q, (int)heat.q_limit,
                     (int)heat.cooling, (int)heat.state, (unsigned)heat.degraded);
        }
    }
#endif
#else
    heat.state = HEAT_GUARD_OFF;
#endif
}

uint8_t heat_guard_allow_burst(float demand, float *rate)
{
#if HEAT_GUARD_ENABLE
    if (rate == NULL)
        return 0;
    if (heat.state == HEAT_GUARD_OFF)
    {
        *rate = demand;
        return (demand > 0.0f) ? 1u : 0u;
    }
    if (heat.state != HEAT_GUARD_NORMAL || demand <= 0.0f)
        return 0;

    float remaining = heat.q_limit - heat.q - heat_guard_margin_burst();
    if (remaining <= 0.0f)
        return 0;

#if HEAT_GUARD_RATE_MODE
    float allowed = (remaining / HEAT_GUARD_FLAT_WINDOW_S + heat.cooling) /
                    HEAT_GUARD_HEAT_PER_BULLET;
#else
    float allowed = (remaining / HEAT_GUARD_SPEND_TIME_S + heat.cooling) /
                    HEAT_GUARD_HEAT_PER_BULLET;
#endif
    if (allowed > demand)
        allowed = demand;
    if (allowed <= 0.0f)
        return 0;
    *rate = allowed;
    return 1u;
#else
    if (rate != NULL)
        *rate = demand;
    return (demand > 0.0f) ? 1u : 0u;
#endif
}

uint8_t heat_guard_allow_single(void)
{
#if HEAT_GUARD_ENABLE
    const float now_ms = dwt_get_timeline_ms();
    if (heat.state == HEAT_GUARD_OFF)
        return 1u;
    if (heat.state == HEAT_GUARD_HOLD)
        return 0;

    if (heat.q + HEAT_GUARD_HEAT_PER_BULLET + heat_guard_margin_single() > heat.q_limit)
        return 0;

    if (heat.state == HEAT_GUARD_SINGLE && heat.cooling > 0.1f)
    {
        float gap_ms = HEAT_GUARD_HEAT_PER_BULLET / heat.cooling * 1000.0f *
                       HEAT_GUARD_SINGLE_GAP_BULLETS;
        if (now_ms - heat.single_last_ms < gap_ms)
            return 0;
    }

    heat.q += HEAT_GUARD_HEAT_PER_BULLET;
    heat.single_last_ms = now_ms;
    return 1u;
#else
    return 1u;
#endif
}

void heat_guard_commit_shot(void)
{
#if HEAT_GUARD_ENABLE
    heat.q += HEAT_GUARD_HEAT_PER_BULLET;
    if (heat.q > heat.q_limit)
        heat.q = heat.q_limit;
#endif
}

heat_guard_state_e heat_guard_get_state(void)
{
    return heat.state;
}

float heat_guard_get_heat(void)
{
    return heat.q;
}

void heat_guard_get_snapshot(float *q, float *limit, float *cool)
{
    if (q != NULL)
        *q = heat.q;
    if (limit != NULL)
        *limit = heat.q_limit;
    if (cool != NULL)
        *cool = heat.cooling;
}

uint8_t heat_guard_is_degraded(void)
{
    return heat.degraded;
}
