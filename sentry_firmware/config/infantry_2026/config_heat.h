/**
 * @file    config_heat.h
 * @brief   热量守卫参数:预算/阈值/射速律
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/robots/infantry/params/调参结论.md。
 */
#pragma once

/* ---------------- 枪口热量守卫 ---------------- */

#define HEAT_GUARD_ENABLE 1
/* [配置] 热量守卫总开关。0 = 完全不读裁判系统、不记热量、全放行(仅用于排除故障对照)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_ALLOWANCE_ENABLE 0
/* [配置] 2026 允许发弹量(0x0208)硬闸门开关。1 = 17mm 额度 <1 发即禁发(单发+连发都挡),
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_HEAT_PER_BULLET 10.0f
/* [规则] 17mm 每发增加的热量,热量点。来自 2026 规则 3.3.1.1(与图片一致)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_MARGIN_BURST 30.0f
/* [参考 待实测] 连发保留余量,热量点。这段热量不允许使用,用来买"停扣后仍在膛内的弹"。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_MARGIN_SINGLE 10.0f
/* [战术决策 2026-11-21] 单发保留余量,热量点。**用户定为 10:打到热量剩 10 点即完全禁发**
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_MARGIN_MAX_RATIO 0.25f
/* [理论] 余量占 Q0 的比例上限,无量纲。低等级 Q0 很小时(如 50 点),
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_HYST 5.0f
/* [理论] 状态回差,热量点。进入用阈值,退出用阈值减本值。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_SPEND_TIME_S 3.0f
/* [待实测] 余热预算的摊开时间,s(仅 HEAT_GUARD_RATE_MODE=0 指数衰减模式生效)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_RATE_MODE 1
/* [战术 2026-11-21] 射速律选择:
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_FLAT_WINDOW_S 4.0f
/* [战术 2026-11-21] 恒定摊平的窗口,s(仅 RATE_MODE=1 生效)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_REF_TIMEOUT_MS 300.0f
/* [理论] 判定热量数据断流的超时,ms。0x0202 名义 10Hz,丢 3 帧即降级。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_DOWN_TOL 5.0f
/* [理论] 允许向下校正的差值门限,热量点。裁判值比本方估计低超过本值才考虑下调。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_DOWN_FRAMES 3
/* [理论] 向下校正需要连续满足的帧数,帧(100ms/帧)。3 帧 = 300ms,长于在途时延。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_SINGLE_GAP_BULLETS 2.0f
/* [理论] 单发模式的最小间隔,单位"平衡间隔的倍数"(无量纲)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_COOLING_USE_BUFF 1
/* [待实测] 是否把 0x0204 的冷却增益(直接值,点/s)加到基础冷却上。1 = 相加。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_DEFAULT_HEAT_LIMIT 230.0f
/* [实测 2026-11-21] 兜底热量上限,热量点。仅在"曾上线但一个 0x0202 都没收到"时使用,
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_DEFAULT_COOLING 14.0f
/* [实测 2026-11-21] 兜底冷却速率,热量点/s。使用条件同上。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define HEAT_GUARD_LOG 1
/* [配置] 热量守卫日志:状态迁移必打一条 WARNING(不受本宏控制),另有 1Hz INFO 摘要。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
