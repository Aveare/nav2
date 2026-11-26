/**
 * @file    config_heat.h
 * @brief   哨兵热量守卫参数:260 热 / 30 冷却 / 每发 10 热
 * @layer   CONFIG
 * @author  小猫大王
 * @note    参数来自 2026 哨兵规则锚点(400 HP / 100 W / 260 热 / 30 冷却 / 750 初始弹量)。
 * @note    裁判系统未上线时守卫进入 OFF(台架直通);曾上线后断流才降级限速。
 */
#pragma once

#define HEAT_GUARD_ENABLE 1
/* [配置] 热量守卫总开关。0=完全不读裁判、不记热量、全放行(仅故障对照)。 */
#define HEAT_GUARD_ALLOWANCE_ENABLE 1
/* [配置] 允许发弹量(0x0208)硬闸门。1=17mm 额度 <1 发即禁发;仅在裁判在线时生效。 */
#define HEAT_GUARD_HEAT_PER_BULLET 10.0f
/* [规则] 17mm 每发热量,热量点。 */
#define HEAT_GUARD_MARGIN_BURST 30.0f
/* [待实测] 连发保留余量,热量点。 */
#define HEAT_GUARD_MARGIN_SINGLE 10.0f
/* [待实测] 单发保留余量,热量点。 */
#define HEAT_GUARD_MARGIN_MAX_RATIO 0.25f
/* [理论] 余量占 Q0 的比例上限。 */
#define HEAT_GUARD_HYST 5.0f
/* [理论] 状态回差,热量点。 */
#define HEAT_GUARD_SPEND_TIME_S 3.0f
/* [待实测] 指数衰减模式下的余热摊开时间(仅 RATE_MODE=0)。 */
#define HEAT_GUARD_RATE_MODE 1
/* [配置] 1=恒定摊平窗口;0=按 SPEND_TIME 指数衰减。 */
#define HEAT_GUARD_FLAT_WINDOW_S 4.0f
/* [待实测] 恒定摊平窗口,s。 */
#define HEAT_GUARD_REF_TIMEOUT_MS 300.0f
/* [理论] 裁判热量帧断流判据,ms。 */
#define HEAT_GUARD_DOWN_TOL 5.0f
/* [理论] 允许向下校正的差值门限,热量点。 */
#define HEAT_GUARD_DOWN_FRAMES 3
/* [理论] 向下校正需要连续满足的帧数。 */
#define HEAT_GUARD_SINGLE_GAP_BULLETS 2.0f
/* [理论] 单发最小间隔(平衡间隔的倍数)。 */
#define HEAT_GUARD_COOLING_USE_BUFF 1
/* [待实测] 0x0204 冷却增益是否叠加到冷却速率。 */
#define HEAT_GUARD_DEFAULT_HEAT_LIMIT 260.0f
/* [规则] 兜底热量上限,热量点。 */
#define HEAT_GUARD_DEFAULT_COOLING 30.0f
/* [规则] 兜底冷却速率,热量点/s。 */
#define HEAT_GUARD_LOG 1
/* [配置] 状态迁移告警 + 1Hz 摘要日志。 */
