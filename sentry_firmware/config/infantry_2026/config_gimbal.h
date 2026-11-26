/**
 * @file    config_gimbal.h
 * @brief   云台参数:零位/限位/调度/前馈
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/spec/本车调参结论.md。
 */
#pragma once

/* ---------------- 云台零位与限位 ---------------- */

#define YAW_CHASSIS_ALIGN_ECD 1835
/* [实测 2026-10-28] yaw 对准底盘正前方时的编码器值,原始刻度(0~8191)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_HORIZON_ECD 47
/* [实测 2026-10-28] pitch 处于水平时的编码器值,**转子单圈刻度**(0~8191)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_REDUCTION_RATIO 19.0f
/* [理论] pitch 轴减速比:M3508 转子度 / 输出轴度(C620 内置 19:1)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_ECD_SIGN (+1.0f)
/* [待实测] 编码器方向系数:使 sign*total_angle/ratio 与 IMU Pitch 同向(+1 或 -1)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define GIMBAL_CALIB_TRIGGER_ROCKER_L1 600
/* [待实测] 标定触发条件之一:左纵杆推满(满量程 ±660)保持 GIMBAL_CALIB_TRIGGER_HOLD_MS。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define GIMBAL_CALIB_TRIGGER_HOLD_MS 3000u
/* [待实测] 标定触发保持时间,ms。太短易误触发,太长手感差。 */
#define PITCH_MAX_ANGLE 25.0f
/* [实测] 俯仰上限,deg(IMU 反馈域)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_MIN_ANGLE (-22.0f)
/* [实测] 俯仰下限,deg(IMU 反馈域)。与上限同源,经机械限位实测。 */
/* ---------------- 云台解锁回中 ---------------- */
#define GIMBAL_RECENTER_ENABLE 1
/* [配置] 解锁回中功能开关。关掉后 F 键回正与解锁自动回中均失效,云台保持当前角度。 */
#define GIMBAL_RECENTER_SPEED 90.0f
/* [已废弃-仅存档] 旧"转云台"式回中的速度,deg/s。2026-11-17 回中改为"只动底盘"后
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* [理论] 回中角速度上限,deg/s。180° 约 2s,压住"甩"的观感。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_ALIGN_SPEED 300.0f
/* [实测] **回中时底盘旋转速度**,deg/s。2026-11-17 新增;同日 120 → 180 → **300**(用户两次反馈不够快)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_ALIGN_SOFT 8.0f
/* [实测-派生] **回中末端线性收敛区**,deg。|offset| 小于此值后按比例降速(线性到 0)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define GIMBAL_RECENTER_DEADBAND 2.0f
/* [理论] 回中完成判据,deg。|offset| 小于此值即视为已回中。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* GIMBAL_RECENTER_DIR 已于 2026-11-17 删除:回中不再改云台目标,该方向符号失去意义。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define GIMBAL_RECENTER_TIMEOUT 600
/* [理论] 回中超时,单位控制周期。200Hz × 600 = 3s。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- yaw 角度环 Kd 按 |err| 调度 ---------------- */
/* 远离目标用低 Kd 保跟手,接近目标用全 Kd 防松杆回弹,区间内线性过渡。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_KD_NEAR (5.8f)
/* [实测] 近目标区阻尼增益。|err| ≤ BLEND_LO(即 YAW_KD_BLEND_LO)时取此值(全阻尼)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_KD_FAR (1.5f)
/* [实测] 远目标区阻尼增益。|err| ≥ BLEND_HI(即 YAW_KD_BLEND_HI)时取此值(低阻尼)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_KD_BLEND_LO (4.0f)
/* [理论] 调度下界,deg。|err| 低于此值进入全阻尼区。 */
#define YAW_KD_BLEND_HI (8.0f)
/* [理论] 调度上界,deg。|err| 高于此值进入低阻尼区,两者之间线性过渡,保证增益连续。 */
/* ---------------- yaw 速度环 Kp 按相对转速调度 ---------------- */
/* 小陀螺推杆时相对转速可达 -460 deg/s,反电动势侵蚀可用扭矩,故高速区线性提高 Kp 补扭矩(DEV-116)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_SPEED_KP_LOW 20.0f
/* [实测] 低速区速度环 Kp。全部历史稳定数据均在此时段取得。 */
#define YAW_SPEED_KP_HIGH (YAW_ESC_CURRENT_MODE ? 40.0f : 40.0f)
/* [2026-11-17 第二次上调:30 → 40] 承接下面 20→30 的记录(那条的收尾一句就是"若仍软脚再上 40")。
 * [战术决策 2026-11-23] 两分支统一为 40 并维持:"电流模式回落 20"系推演未经实测,搁置;
 * 0x2FE 复验通过后凭实测波形再议(DEC-015)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* [理论→实测修正] 高速区速度环满增益,随命令帧类型派生。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_SPEED_KP_BLEND_LO 300.0f
/* [实测-部分] 调度起点,相对转速 deg/s,取稳态自转保持点附近。 */
#define YAW_SPEED_KP_BLEND_HI 450.0f
/* [实测-部分] 调度终点,相对转速 deg/s,此处达到满增益。区间内线性过渡。 */
#define YAW_SPEED_KP_WORLD_LO 60.0f
/* [实测-派生] 第二路调度的下界,云台**世界**角速度 deg/s。**2026-11-17 新增**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_SPEED_KP_WORLD_HI 150.0f
/* [实测-派生] 第二路上界,世界角速度 deg/s。60~150 之间线性过渡,与第一路取 max。 */
#define YAW_SPEED_KP_LP 0.30f
/* [理论] 速度环 Kp 调度一阶低通系数(每拍,200Hz)。0.30 对应时间常数约 12ms、
 * 截止约 13.6Hz:只抑制变速时 rel 跨 300 拐点、以及底盘 roll/pitch 耦合让 world
 * 短暂越阈值造成的 Kp 快速抖动,不改变稳态调度值。1=不滤波(旧行为),0=冻结(勿用)。 */
/* ---------------- yaw 控制域与前馈 ---------------- */
#define YAW_ESC_CURRENT_MODE 1
/* [待实测] yaw GM6020 的 CAN 命令帧类型开关。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_ESC_OUT_MAX (YAW_ESC_CURRENT_MODE ? 16384.0f : 25000.0f)
/* [规格] yaw 速度环输出限幅 = 对应命令帧的协议满量程(电流 16384 / 电压 25000),单位=电调原始码。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_CURRENT_DOMAIN_SCALE (YAW_ESC_CURRENT_MODE ? 0.6554f : 1.0f)
/* [换算] 输出域换算系数 = 16384/25000。电压模式下 PID 输出直接是电压码,
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_REF_FF_GAIN 1.0f
/* [理论] yaw 目标角速度前馈增益。前馈量取"本周期 ref 增量 / dt",
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_REF_FF_MAX_RATE 450.0f
/* [实测] 前馈认可的最大目标角速度,deg/s(左横杆与键鼠甩枪共用)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_REF_FF_MAX_DELTA (YAW_REF_FF_MAX_RATE * 0.005f * 1.1f)
/* [实测 派生] 单周期合法增量上限,deg。0.005s 为 200Hz 控制周期,1.1 为余量。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* yaw 惯量前馈 τ = Jα:α 取 Gyro[2] 差分后低通,加速时助力、减速时刹车,稳态归零(DEV-101)。 */
#define YAW_INERTIA_FF_GAIN 3.5f
/* [实测] 惯量前馈增益,电流码/(deg/s²)(乘 YAW_CURRENT_DOMAIN_SCALE 后生效)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_INERTIA_FF_LP 0.2f
/* [理论] α 低通系数。200Hz 下截止约 3.5Hz,滤掉差分噪声,避免前馈抖动。 */
#define YAW_INERTIA_FF_ALPHA_MAX 800.0f
/* [实测] α 限幅,deg/s²。**2026-11-17 由 1000 降到 800**,与增益 3.0→3.5 配套(DEV-105 的成熟组合)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_INERTIA_FF_SRC 0
/* [理论] 惯量前馈的 α 来源。0 = IMU 世界系实测差分(现行,已实车验证);1 = 参考轨迹 α_ref。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_VREF_LP 0.3f
/* [理论] 仅 `YAW_INERTIA_FF_SRC=1` 时使用:参考速度副本的一阶低通系数。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_FF_CALIB_LOG 0
/* [配置] P2 标定专用日志开关(0.5s 一行)。关掉后无任何输出,也不影响控制。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* yaw 摩擦/反电动势前馈:ff = B × 相对转速,补偿"世界系反馈看不见相对转速"造成的稳态残差(DEV-125)。 */
/* ---------------- pitch 目标角速度前馈 ---------------- */
/* 治纯 P 恒速跟踪滞后:满杆 60 deg/s 时滞后 7.5°(= 速度 / Kp),松杆后还会再滑一下。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_REF_FF_GAIN 1.0f
/* [实测-部分] pitch 目标角速度前馈增益。环路稳定后重新引入(DEV-20261030-11),
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_REF_FF_MAX_RATE 60.0f
/* [规格] 前馈认可的最大目标角速度,deg/s。与 cmd 层杆量映射一致(满杆 60 deg/s)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_REF_FF_MAX_DELTA (PITCH_REF_FF_MAX_RATE * 0.014f * 3.0f)
/* [实测 派生] 单帧合法增量上限,deg。0.014s 为 DBUS 帧周期,×3.0 容纳帧间抖动,
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_REF_FF_LP 0.3f
/* [理论] 前馈一阶低通系数。200Hz 下截止约 11Hz,抹掉 70Hz 帧率阶跃纹波。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- pitch 重力前馈(新结构必做,见 .Doc/detail/pitch轴优化计划.md) ---------------- */
/* 模型:I_ff(θ) = GAIN × (A·cos(θ − φ) + B),θ 取 **IMU 欧拉 Pitch**(重力是世界系量;
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_GRAVITY_FF_GAIN 1.0f
/* [实测] 重力前馈总增益。0 = 关闭(默认,行为与整定前完全一致)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_GRAVITY_A 1927.0f
/* [实测] 最大重力矩等效电流,单位 C620 原始码,**带符号**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_GRAVITY_PHASE -92.0f
/* [实测] 质心相位 φ,deg。理论 0(质心在俯仰轴正上方或正下方);偏离说明质心不在轴线上。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_GRAVITY_OFFSET 3371.0f
/* [实测] 常值偏置 B,C620 码。吸收线缆扭力/静摩擦那类与角度无关的恒值项。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define PITCH_GRAVITY_FF_LIMIT (1.2f * ((PITCH_GRAVITY_A < 0.0f ? -PITCH_GRAVITY_A : PITCH_GRAVITY_A) + \
                                        (PITCH_GRAVITY_OFFSET < 0.0f ? -PITCH_GRAVITY_OFFSET : PITCH_GRAVITY_OFFSET)))
/* [理论] 前馈自身限幅,码,由 A/B 自动派生(|A|+|B| 再留 20% 余量),A=B=0 时恰为 0。
 * **为什么必须限幅**:`CURRENT_FEEDFORWARD` 在 `dji_motor_control` 里是在速度环输出**之后**叠加的,
 * 前面没有任何钳位 → `MaxOut=4000` 管不到前馈,总电流可达 4000+FF。
 * 拟合偏差或相位写错时,这层限幅是唯一的物理上限。 */

#define PITCH_FF_CALIB_LOG 0
/* [配置] P1/P2 标定日志开关,每 0.5s 一行 `[pitchcal] ang/out/spd/cur`。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_FRICTION_FF_B 12.0f
/* [实测] yaw 摩擦/反电动势前馈系数。ff = B × 相对转速(乘 YAW_CURRENT_DOMAIN_SCALE 后生效)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_COULOMB_FF_TC 0.0f
/* [实测] 库仑摩擦前馈幅值,码。**实测为 0,故不启用**(仍保留宏位置以便将来换结构后复标)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 陀螺仪安装方向 (1 同向, -1 反向) ---------------- */
#define GIMBAL_GYRO_DIR_YAW 1
#define GIMBAL_GYRO_DIR_PITCH 1
#define GIMBAL_GYRO_DIR_ROLL 1
/* [不适用] 由 IMU 实际安装姿态决定,不是可调参数。改轴向/翻装后按现象逐个取反:
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 云台电机 PID 初值与限幅 ---------------- */
#define YAW_MOTOR_ID 5
/* [配置] GM6020 电调 ID(CAN1),yaw 轴。 */
#define PITCH_MOTOR_ID 4
/* [配置] M3508+C620 电调 ID(CAN2),pitch 轴。 */
#define YAW_ANGLE_PID_KP 24.0f
/* [实测] yaw 角度环 Kp(输入 deg,输出 deg/s,代码内乘 FW_DEG_TO_RAD)。实车整定值。 */
#define YAW_ANGLE_PID_D_LIMIT 8.0f
/* [实测] D 项限幅:快速运动时截断 Dout,防"转得越快刹得越狠"。 */
#define YAW_ANGLE_PID_DEADBAND 0.5f
/* [实测] 角度环死区(deg),兼作自瞄/手动切换的共用死区基准。 */
#define YAW_ANGLE_PID_IL 100.0f
/* [实测] 角度环积分限幅(deg)。 */
#define YAW_ANGLE_PID_MAXOUT 1200.0f
/* [实测] 角度环输出限幅 = 目标角速度上限(deg/s),代码内乘 FW_DEG_TO_RAD。 */
#define YAW_SPEED_PID_KI 2.0f
/* [实测] yaw 速度环 Ki(输出域,代码内除 FW_DEG_TO_RAD 并乘 YAW_CURRENT_DOMAIN_SCALE)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define YAW_SPEED_PID_IL 100.0f
/* [实测] yaw 速度环积分限幅。 */
#define PITCH_ANGLE_PID_KP 8.0f
/* [实测] pitch 角度环 Kp(deg 误差 → deg/s),代码内乘 FW_DEG_TO_RAD。 */
#define PITCH_ANGLE_PID_DEADBAND 0.6f
/* [实测] pitch 角度环死区(deg)。2026-11-17 由 0.3 提到 0.6(转 yaw 时 pitch 异响,见 DEVLOG)。 */
#define PITCH_ANGLE_PID_IL 100.0f
/* [实测] pitch 角度环积分限幅。 */
#define PITCH_ANGLE_PID_MAXOUT 180.0f
/* [实测] pitch 目标角速度上限(deg/s),代码内乘 FW_DEG_TO_RAD。 */
#define PITCH_SPEED_PID_KP 50.0f
/* [实测] pitch 速度环 Kp(反馈为 Gyro[0] 的 rad/s,代码内除 FW_DEG_TO_RAD)。 */
#define PITCH_SPEED_PID_KI 10.0f
/* [实测] pitch 速度环 Ki。20 有 70Hz 啸叫、2 恢复慢,10 为折中(DEV-08)。 */
#define PITCH_SPEED_PID_RC 0.012f
/* [待实测] 速度环输出低通 RC(s)。注释历史写 0.008,代码实际 0.012,以代码为准。 */
#define PITCH_SPEED_PID_IL 1500
/* [实测] 速度环积分限幅(C620 码)。 */
#define PITCH_SPEED_PID_MAXOUT 5000
/* [实测] 速度环输出限幅(C620 码)。 */
