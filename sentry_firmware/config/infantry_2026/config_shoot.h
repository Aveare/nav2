/**
 * @file    config_shoot.h
 * @brief   发射参数:摩擦轮/拨盘/射速
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/robots/infantry/params/调参结论.md。
 */
#pragma once

/* ---------------- 发射 ---------------- */

#define SHOOT_NUM_PER_CIRCLE 7
/* [实测] 拨盘一圈装弹量,实机数槽得 7。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_LOADER_REDUCTION 36.0f
/* [规格] M2006 + C610 拨盘减速比,官方规格值。 */
#define SHOOT_ONE_BULLET_DELTA_ANGLE (360.0f / SHOOT_NUM_PER_CIRCLE)
/* [实测 换算] 拨出一发弹丸时拨盘输出轴的转角,输出轴 deg(不是电机轴)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_ANGLE_TOL_DEG 6.0f
/* [理论] 单发到位容差,输出轴 deg(约 1/8 槽距)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_SINGLE_TIMEOUT_MS 1000.0f
/* [理论] 单发起步后的到位超时,ms。超时中止当前队列并告警。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_RATE_DEFAULT 6.0f
/* [换算 待实测 2026-11-21] 连发射速,发/s。换算:拨盘 RPM = 射速 × 1852/6 ≈ 1852RPM@6发/s。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* [安全 2026-11-21] 连发射速绝对上限,发/s。仅作护栏:合法路径上射速已被热量守卫
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_RATE_MAX 20.0f

/* [战术 2026-11-21] 三档请求射速,发/s(DEV-20261121-11)。档位由键盘 X 循环切换,
 * 实际放行仍由热量守卫裁决 —— 爆发档不等于能打满,只是"请求得更多"。 */
#define SHOOT_RATE_ECO (14.0f / 10.0f)
/* 省热档 = 散热平衡(冷却 14 点/s ÷ 每发 10 点),稳态热量不涨,适合压制/骚扰。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_RATE_BURST 12.0f
/* 爆发档:近距离换血/拆基地用。守卫在 230/14 下会把有效射速压到 ~7 发/s(恒定摊平)
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* 拨轮扳机式,原始刻度域 [-660, 660]:
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_DIAL_TRIGGER_ZONE (-240)
/* [实测-部分] 扳机区入口,原始刻度。dial ≤ 此值进入扳机判定。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_DIAL_TRIGGER_RELEASE (-220)
/* [理论] 扳机区退出阈值,原始刻度,比入口高 20 构成迟滞。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_DIAL_SINGLE_HOLD_MS 250.0f
/* [实测-部分] 按住判别时长,ms。按住 ≥ 此值转连发,否则按短拨处理(松手沿登记单发)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_DIAL_FRICTION_KEEP_MS 2500.0f
/* [理论] 遥控拨轮松开后的摩擦轮保持时间,ms。默认关闭摩擦轮;下压拨轮时启动,
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_FRICTION_TARGET_RPM 6900.0f
/* [实测 2026-11-23] 摩擦轮目标转速,RPM(电机转子)。用户确认 6900 RPM 用于将
 * 实际弹速控制在约 24 m/s；更换摩擦轮、弹丸或压紧量后必须重新用裁判系统复测。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_FRICTION_READY_TOL_RPM 500.0f
/* [理论] 双轮到速容差,RPM。两轮都进入 ±此值才允许拨弹(互锁)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* 拨盘堵转保护:参考非零 + 转速≈0 + 电流超阈,持续超时 → 停盘锁存,拨轮回中解除。 */
#define LOADER_JAM_CURRENT 1800.0f
/* [待实测] 堵转电流阈值,C610 原始单位(满量程 10000)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_JAM_SPEED_RPM 30.0f
/* [待实测] 判"堵住"的转速门限,RPM。低于此值且电流超阈才计入堵转累计。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_JAM_HOLD_MS 500.0f
/* [待实测] 堵转持续判定时长,ms。超时才锁存,避免瞬时冲击误判。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_FRICTION_RPM_OFFSET 0.0f
/* [待实测] 摩擦轮转速微调,RPM,叠加在 SHOOT_FRICTION_TARGET_RPM 上(弹速标定 P2 后按挡位填)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_BACKOFF_EN 0
/* [配置] 拨盘自动退弹开关(发射计划 P4-2)。0 = 现状(堵转锁存等人工解除);
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define SHOOT_CALIB_LOG 1
/* [配置] 发射标定日志开关:每 0.5s 一行 [shootcal] fl/fr/ldr/cur(双轮RPM带符号/拨盘RPM/拨盘电流)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_WAIT_TIMEOUT_MS 2000.0f
/* [理论] 单发/三发"等待起步"的最长时间,ms。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define LOADER_FRICTION_OFF_GRACE_MS 100.0f
/* [理论] 摩擦轮关闭到丢弃待发队列的宽限期,ms。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 发射电机 PID 与 ID ---------------- */
#define SHOOT_FRICTION_ID_L 1
/* [配置] 左摩擦轮电调 ID(CAN2);右轮为 2,拨盘为 3(与 README 拨码一致)。 */
#define SHOOT_FRICTION_ID_R 2
#define LOADER_ID 3

#define SHOOT_FRICTION_SPEED_PID_KP 2.4f
/* [实车] 摩擦轮速度环 Kp(反馈 deg/s)。 */
#define SHOOT_FRICTION_SPEED_PID_KI 0.5f
/* [实车] 摩擦轮速度环 Ki。 */
#define SHOOT_FRICTION_SPEED_PID_IL 1500
/* [理论] 摩擦轮速度环积分限幅。 */
#define SHOOT_FRICTION_SPEED_PID_MAXOUT 6000
/* [理论] 摩擦轮速度环输出限幅(C620 码)。 */
#define LOADER_SPEED_PID_KP 6.0f
/* [实车] 拨盘速度环 Kp(反馈 deg/s)。 */
#define LOADER_SPEED_PID_KI 0.5f
/* [实车] 拨盘速度环 Ki。 */
#define LOADER_SPEED_PID_IL 800
/* [理论] 拨盘速度环积分限幅。 */
#define LOADER_SPEED_PID_MAXOUT 8000
/* [实车] 拨盘输出限幅(40% → 80% 载弹量上调,见 DEVLOG)。 */

/* 单发/三发角度环(DEV-20261124-04):拨盘必须闭环到目标角才停,不能靠速度环满速冲。
 * 开关=0 回退到原来的"速度环满速 + 软件轮询到位"行为,便于实车 A/B 对比;
 * 数值参考哨兵工程(未标定),步兵实车需重标 —— 详见参数证据表同名节。 */
#define LOADER_ANGLE_LOOP_EN 1
/* [配置] 单发/三发真角度环开关。0=回退速度环满速冲(已知每发过冲 1~2 发);1=启用角度环。 */
#define LOADER_ANGLE_PID_KP 8.0f
/* [待实测] 拨盘角度环 Kp(误差电机轴 deg → 目标转速 deg/s)。
 * DEV-20261125-07:原 2.5 时误差 1851deg 只给 4627deg/s,连 MAXOUT 都用不满,
 * 一发要 0.5s 以上;提到 8 使大误差段能吃满转速上限。 */
#define LOADER_ANGLE_PID_KI 0.2f
/* [待实测] 拨盘角度环 Ki,只用于消除到位静差,过大会在目标角附近来回振荡。 */
#define LOADER_ANGLE_PID_MAXOUT 15000.0f
/* [理论] 角度环输出上限(目标转速,电机轴 deg/s)≈2500 RPM 转子。
 * DEV-20261125-07:原 6000(≈1000RPM)是单发延迟 0.5~1s 的主因 ——
 * 一发 1851deg 匀速就要 0.31s。代价是拨盘冲击更大,首测需观察是否顶到堵转判据。 */
#define LOADER_STOP_AFTER_STEP_EN 1
/* [配置] 每发到位后立即切速度环 + 清 PID + ref=0。0=回退旧行为(队列未空时继续满速)。 */
