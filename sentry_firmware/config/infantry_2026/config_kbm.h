/**
 * @file    config_kbm.h
 * @brief   键鼠参数:灵敏度/响应/触发区
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/spec/本车调参结论.md。
 */
#pragma once

/* ---------------- 键鼠控制 ---------------- */

/* 链路:电脑 MicroUSB 接 NDJ6/DT7 遥控器,键鼠字段随 DBUS 帧下发;无键鼠数据时恒 0,永不激活。
 * 键盘任意键 / 鼠标移动或点击即切入键鼠模式,超过 KBM_TIMEOUT_MS 无输入自动回落遥控。
 *
 * 映射模型(FPS 式线性 1:1,DEV-20261106-01,已取代位置域 + 加速曲线方案 DEV-20261102-03/04):
 * 帧计数 = 本帧(14ms)内的鼠标位移 → 换算为目标角速度 → 按控制周期积分,
 * 使"每计数恰好施加一次",灵敏度与 70Hz 帧率 / 200Hz 控制率解耦。
 * 固定增益、无加速度曲线(CSGO 式),保肌肉记忆。
 *
 * 灵敏度语义:KBM_*_SENS = deg / 帧计数,再乘运行时倍率 kbm_sens_scale
 * (V 降 / B 升 ×0.85,G 重置,范围 0.2~2.5,RTT 回显)。 */

#define KBM_YAW_SENS 0.06f
/* [实测-部分] 鼠标 x 计数 → yaw 转角,deg/帧计数。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_PITCH_SENS 0.04f
/* [实测-部分] 鼠标 y 计数 → pitch 转角,deg/帧计数。基线换算同上(0.15 × 2.86)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_DBUS_FRAME_S 0.014f
/* [规格] DBUS 帧周期,s(约 70Hz)。用于把"每帧计数"归一化成角速度,是灵敏度不随帧率漂移的关键。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_CMD_DT_S 0.005f
/* [规格] cmd 控制周期,s(200Hz)。角速度 → 目标角的积分步长,与任务调度频率绑定。 */
/* Shift 慢速(KBM_SLOW_RATIO)已于 DEV-20261124-03 删除:Shift 改为自瞄 toggle。 */
#define KBM_TIMEOUT_MS 3000.0f
/* [待实测] 键鼠无输入回落遥控的超时,ms。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_MOUSE_DEADZONE 4
/* [待实测] 鼠标静止计数死区,原始计数。用于滤掉静止时的抖动。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_INPUT_LP 0.6f
/* [实测-部分] 输入一阶低通系数。0.6 约 1.5 周期收敛,群延迟约 3ms(200Hz)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_YAW_MAX_RATE 450.0f
/* [实测-部分] yaw 甩枪角速度上限,deg/s。取值贴合速度环 Kp 调度的高速区。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_PITCH_MAX_RATE 150.0f
/* [理论] pitch 甩动角速度上限,deg/s。刻意低于 yaw,防甩过头翻枪。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* 灵敏度运行时调档:V 降档、B 升档(乘 STEP)、G 重置为 1.0。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define KBM_SENS_SCALE_MIN 0.2f
/* [待实测] 倍率下限。从 1.0 起按 0.85 步进约 10 次触底。 */
#define KBM_SENS_SCALE_STEP 0.85f
/* [待实测] 每档步进系数。大 → 调档手感跳跃;小 → 要按很多次才到位。 */
#define KBM_SENS_SCALE_MAX 2.5f
/* [待实测] 倍率上限。从 1.0 起约 6 次触顶。 */
/* 键鼠方向:本车 chassis 的 vx/vy 系数是整体取反标定的,故方向只能靠这两个符号裁决。 */
#define KBM_STRAFE_DIR (+1.0f)
/* [实测] 平移方向符号。A 左 / D 右;若左右反了改 -1,不改逻辑(DEV-20261102-05 实车裁决)。 */
#define KBM_PITCH_DIR (-1.0f)
/* [实测] 鼠标 pitch 符号。取反使"上滑 = 枪口上抬"(mouse.y 上滑为负);
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
