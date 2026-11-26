/**
 * @file    heat_guard.h
 * @brief   热量守卫接口:快照/放行/记账与降级状态
 * @layer   APP
 * @author  小猫大王
* @note    设计要点见 .Doc/spec/热量管理设计.md,三条不可交换的规矩:
*          ① 反馈打在估计器上,前馈打在执行器上 —— 裁判 ΔQ 只校正 Q̂,不直接改射速;
*          ② 记账点前移到"放行"瞬间,不等裁判确认(确认迟到 100~200ms);
*          ③ 误差只允许往安全方向(高估热量),向下校正必须连续多帧。
* @note    本模块不碰电机、不碰硬件句柄,只输出"能否发/发多快",由 shoot_task 执行。
* @note    裁判数据缺失时分两种:从未上线 = 直通(台架);曾上线但断流 = 降级限速。
*          二者不可混为一谈,否则台架不响或赛场盲打。
* @note    2026 允许发弹量(0x0208)是比热量更硬的一级闸门:额度 <1 发即禁发,
*          由 HEAT_GUARD_ALLOWANCE_ENABLE 控制(台架恒 0 额度,默认关;比赛置 1)。
 */
#ifndef HEAT_GUARD_H
#define HEAT_GUARD_H

#include <stdint.h>

/* 发射闸门状态。热量单调上升时按 NORMAL → SINGLE → HOLD 逐级收紧,
 * 下降时各自带 HEAT_GUARD_HYST 回差,避免在阈值上抖动。 */
typedef enum
{
    HEAT_GUARD_OFF = 0, // 未上线:从未收到有效 0x0201(无裁判系统/台架),全放行
    HEAT_GUARD_NORMAL,  // 正常:连发按余热预算给射速
    HEAT_GUARD_SINGLE,  // 只放单发,且强制最小间隔(单发在途弹确定,余量可收窄)
    HEAT_GUARD_HOLD,    // 禁发:保留余量已耗尽,等冷却回落
} heat_guard_state_e;

/**
 * @brief 初始化热量守卫(在 shoot_init 中调用一次)
 * @note  仅清状态,不读裁判系统:裁判系统此时可能尚未初始化。
 */
void heat_guard_init(void);

/**
 * @brief 热量守卫周期更新(200Hz,shoot_task 每周期首行调用)
 * @note  内部完成:冷却阶梯扣减、裁判帧硬校正、状态迁移与分级告警。
 */
void heat_guard_update(void);

/**
 * @brief 申请连发射速
 * @param demand 操作手请求的射速,发/s(来自 cmd,已限幅)
 * @param rate   输出:允许的射速,发/s;返回 0 时该值无意义
 * @return 1 = 允许连发,0 = 禁止(状态为 SINGLE/HOLD 或算得射速为 0)
 */
uint8_t heat_guard_allow_burst(float demand, float *rate);

/**
 * @brief 申请放行一发(单发/三发逐发调用)
 * @return 1 = 现在可以推进这一发,0 = 被闸门挡住
 * @note  放行即记账(内部加一发热量),调用方不需再调 heat_guard_commit_shot。
 *        被挡住时不消耗最小间隔计时,下一周期可再次申请。
 */
uint8_t heat_guard_allow_single(void);

/**
 * @brief 记一发已推入摩擦轮(仅连发用:槽沿过一发角时调用)
 * @note  单发/三发已在 heat_guard_allow_single 内部记账,重复调用会多记。
 */
void heat_guard_commit_shot(void);

/**
 * @brief 当前闸门状态
 */
heat_guard_state_e heat_guard_get_state(void);

/**
 * @brief 本方热量估计值 Q̂,热量点(1 点 = 1,与裁判同量纲)
 */
float heat_guard_get_heat(void);

/**
 * @brief  取守卫当前快照,供 UI 显示
 * @param  q     当前热量估计,热量点;可为 NULL
 * @param  limit 热量上限(0x0201 下发),热量点;可为 NULL
 * @param  cool  冷却速率(0x0201 下发),热量点/s;可为 NULL
 */
void heat_guard_get_snapshot(float *q, float *limit, float *cool);

/**
 * @brief 裁判热量数据是否已断流(曾上线但超时未更新)
 * @return 1 = 断流降级中
 */
uint8_t heat_guard_is_degraded(void);

#endif // !HEAT_GUARD_H
