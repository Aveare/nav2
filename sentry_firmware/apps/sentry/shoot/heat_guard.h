/**
 * @file    heat_guard.h
 * @brief   哨兵热量守卫:放行/射速/记账与降级状态
 * @layer   APP
 * @author  小猫大王
 * @note    三原则:①裁判反馈只校正本地估计;②记账点在放行瞬间,不等裁判确认;
 *          ③误差只允许往安全方向(高估热量),向下校正必须连续多帧。
 * @note    裁判从未上线 = OFF(台架直通);曾上线后断流 = 降级限速,二者不混用。
 */
#ifndef SENTRY_HEAT_GUARD_H
#define SENTRY_HEAT_GUARD_H

#include <stdint.h>

typedef enum
{
    HEAT_GUARD_OFF = 0, // 裁判从未上线:全放行(台架)
    HEAT_GUARD_NORMAL,  // 正常:连发按余热预算给射速
    HEAT_GUARD_SINGLE,  // 只放单发
    HEAT_GUARD_HOLD,    // 禁发
} heat_guard_state_e;

void heat_guard_init(void);
void heat_guard_update(void);
uint8_t heat_guard_allow_burst(float demand, float *rate);
uint8_t heat_guard_allow_single(void);
void heat_guard_commit_shot(void);
heat_guard_state_e heat_guard_get_state(void);
float heat_guard_get_heat(void);
void heat_guard_get_snapshot(float *q, float *limit, float *cool);
uint8_t heat_guard_is_degraded(void);

#endif // SENTRY_HEAT_GUARD_H
