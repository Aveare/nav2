/**
 * @file    config_gimbal.h
 * @brief   哨兵三轴云台参数:大 yaw(DM MIT)/ 小 yaw(GM6020)/ pitch(DM MIT)
 * @layer   CONFIG
 * @author  小猫大王
 * @note    控制分配:**小 yaw 先吃掉世界目标误差,大 yaw 只在小 yaw 相对角饱和后
 *          承担剩余误差**;pitch 直接跟踪世界俯仰目标。
 * @note    DM 控制力矩单位为 N·m,位置/速度为电机轴域;变速比按输出轴/电机轴定义。
 * @note    零位/方向/限位全部 [待实测];首次上电必须架空、逐轴确认符号。
 */
#pragma once

/* ---------------- 标定回退值(calib_enable=false 时使用) ---------------- */

#define YAW_CHASSIS_ALIGN_ECD 0        /* [待实测] 大 yaw 正对底盘前方时的编码器值(仅占位) */
#define PITCH_HORIZON_ECD 0            /* [待实测] pitch 水平位转子单圈编码器值(仅占位) */
#define PITCH_REDUCTION_RATIO 1.0f     /* [配置] DM-J4310 输出轴/电机轴 = 1:1 */
#define PITCH_ECD_SIGN (+1.0f)         /* [待实测] pitch 编码器方向系数 */

/* ---------------- 电机 ID / 总线 ---------------- */

#define SENTRY_BIG_YAW_DM_ID 5         /* [实测-参考] 大 yaw DM 控制 ID,CAN1 */
#define SENTRY_BIG_YAW_DM_MASTER_ID 0  /* [实测-参考] 大 yaw 反馈 Master ID */
#define SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE 1 /* [配置] 1=使能 DM 回传,但强制零 MIT 增益/力矩且禁用 PID */
#define SENTRY_PITCH_DM_ID 5           /* [实测-参考] pitch DM 控制 ID,CAN2 */
#define SENTRY_PITCH_DM_MASTER_ID 0    /* [实测-参考] pitch 反馈 Master ID */
#define SENTRY_SMALL_YAW_ESC_ID 1      /* [实测-参考] GM6020 拨码 ID,CAN2 */
#define SENTRY_SMALL_YAW_CURRENT_MODE 0 /* [配置] 0=0x1FF 电压帧;1=0x1FE 电流帧 */

/* ---------------- 方向 / 变速比 / 零位 ---------------- */

#define SENTRY_BIG_YAW_DIRECTION (+1.0f)      /* [待实测] 电机正力矩 → 世界 yaw 正方向 */
#define SENTRY_PITCH_DIRECTION (+1.0f)        /* [实测 2026-11-23] 编码器/关节方向:抬枪 total_position 增大 */
#define SENTRY_PITCH_TORQUE_SIGN (-1.0f)      /* [实测 2026-11-23] 达妙正力矩实际使枪口下压,输出取反 */
#define SENTRY_SMALL_YAW_DIRECTION (+1.0f)    /* [待实测] GM6020 正输出 → 相对角正方向 */
#define SENTRY_BIG_YAW_TRANSMISSION_RATIO 0.96f /* [实测 2026-11-23] 手转上部斜率法 0.974/0.945,两次取中 0.96;齿数未数 */
#define SENTRY_PITCH_TRANSMISSION_RATIO 1.0f   /* [待实测] 输出轴/电机轴 */
#define SENTRY_BIG_YAW_ZERO_RAD (-0.470f)      /* [实测 2026-11-23] 机械正前时 total_position;比值≠1,断电整圈歧义约 15° */
#define SENTRY_PITCH_ZERO_RAD (-1.935f)        /* [实测 2026-11-23] 枪管水平时 pitch 电机 total_position,rad(阶段0采集) */
#define SENTRY_SMALL_YAW_ZERO_ECD 27           /* [实测-参考 shaobin 2026-11-22 21:13] 小 yaw 机械中位;离回绕点 8192 仅 27 计数,解缠是硬要求 */
#define SENTRY_IMU_AFTER_SMALL_YAW 0
/* [待确认] 1=IMU 装在小 yaw 之后的枪体上(IMU 世界 yaw 包含小 yaw 相对角);
 * 0=IMU 装在大 yaw 结构上(IMU 世界 yaw 只含大 yaw)。两种安装的控制分配公式不同,
 * 首次上电必须按实际安装确认;错了会表现为大 yaw/小 yaw 互相抢或静止时缓慢漂移。 */

/* ---------------- 大 yaw(DM-J4310,MIT 力矩) ---------------- */

#define SENTRY_BIG_YAW_MAX_TORQUE_NM 10.0f  /* [规格] 必须 ≤ 驱动器 TMAX;J4310 参考值 10 */
#define SENTRY_BIG_YAW_ANGLE_KP 12.0f       /* [待实测] 角度环 Kp,1/s;虚位下偏软会顿,12 起调 */
#define SENTRY_BIG_YAW_ANGLE_DEADBAND 1.0f  /* [待实测] 角度死区,deg;2° 偏大会像"不动",1° 起调 */
#define SENTRY_BIG_YAW_MAX_SPEED_DEG_S 240.0f /* [待实测] 角度环输出限幅,deg/s */
#define SENTRY_BIG_YAW_ANGLE_IL 0.0f        /* [理论] 角度环不积分(速度环负责) */
#define SENTRY_BIG_YAW_SPEED_KP 0.063f      /* [待实测] 速度环 Kp,Nm/(deg/s)≈3.6 Nm/(rad/s) */
#define SENTRY_BIG_YAW_SPEED_KI 0.0105f     /* [待实测] 速度环 Ki,Nm/deg≈0.6 Nm/rad */
#define SENTRY_BIG_YAW_SPEED_IL 1.2f        /* [待实测] 速度环积分限幅,Nm */
#define SENTRY_BIG_YAW_SPEED_MAXOUT 10.0f   /* [规格] 速度环输出限幅,Nm */
#define SENTRY_BIG_YAW_OUTPUT_LPF_RC 0.03f  /* [待实测] 力矩输出低通 RC,s */

/* ---------------- 小 yaw(GM6020,相对精瞄) ---------------- */

#define SENTRY_SMALL_YAW_LIMIT_DEG 135.0f       /* [实测 2026-11-23] 电机端行程 ±136°,取 135 留 1° 余量 */
#define SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG 10.0f  /* [理论] 目标角夹在 ±125°,留惯性余量 */
#define SENTRY_SMALL_YAW_FINE_DEG 10.0f          /* [设计] 粗精分离:小 yaw 只吃 ±10°,大 yaw 承担其余 */
#define SENTRY_SMALL_YAW_ANGLE_KP 25.0f         /* [待实测] 角度环 Kp,1/s */
#define SENTRY_SMALL_YAW_ANGLE_DEADBAND 0.3f    /* [待实测] 角度死区,deg */
#define SENTRY_SMALL_YAW_MAX_SPEED_DEG_S 180.0f /* [待实测] 角度环输出限幅,deg/s */
#define SENTRY_SMALL_YAW_ANGLE_IL 100.0f        /* [理论] 角度环积分限幅 */
#define SENTRY_SMALL_YAW_SPEED_KP 50.0f         /* [待实测] 速度环 Kp,电调码/(deg/s) */
#define SENTRY_SMALL_YAW_SPEED_KI 0.0f          /* [理论] 速度环不加积分,避免双积分 */
#define SENTRY_SMALL_YAW_SPEED_IL 2000          /* [理论] 速度环积分限幅 */
#define SENTRY_SMALL_YAW_SPEED_MAXOUT 16000     /* [待实测] 输出限幅(电压帧 25000 的 64%) */
#define SENTRY_SMALL_YAW_OUTPUT_RC 0.02f        /* [待实测] 输出低通 RC,s */

/* ---------------- pitch(DM-J4310,MIT 力矩) ---------------- */

#define SENTRY_PITCH_MAX_ANGLE 42.0f        /* [实测 2026-11-23 手推 +52.7°,取 42 留 10° 余量] 关节域 */
#define SENTRY_PITCH_MIN_ANGLE 6.0f         /* [实测 2026-11-23 手推 +7.8°,取 6(向上留余量);下限在水平附近,禁止向下顶] */
#define SENTRY_PITCH_MAX_TORQUE_NM 1.5f     /* [安全] 连杆已断,行程未复核前限 1.5 N·m */
#define SENTRY_PITCH_ANGLE_KP 10.0f         /* [待实测] 角度环 Kp,1/s */
#define SENTRY_PITCH_ANGLE_DEADBAND 0.5f    /* [待实测] 角度死区,deg */
#define SENTRY_PITCH_MAX_SPEED_DEG_S 90.0f  /* [安全] 角度环输出限幅,deg/s */
#define SENTRY_PITCH_ANGLE_IL 30.0f         /* [理论] 角度环积分限幅 */
#define SENTRY_PITCH_SPEED_KP 0.05f         /* [待实测] 速度环 Kp,Nm/(deg/s) */
#define SENTRY_PITCH_SPEED_KI 0.005f        /* [待实测] 速度环 Ki,Nm/deg */
#define SENTRY_PITCH_SPEED_IL 1.0f          /* [待实测] 速度环积分限幅,Nm */
#define SENTRY_PITCH_SPEED_MAXOUT 10.0f     /* [规格] 速度环输出限幅,Nm */
#define SENTRY_PITCH_OUTPUT_LPF_RC 0.02f    /* [待实测] 力矩输出低通 RC,s */

/* 重力前馈:τ_ff = A·cos(θ − φ) + B,N·m。A/B=0 时完全关闭。 */
#define SENTRY_PITCH_GRAVITY_A 0.0f         /* [待实测] 重力矩幅值,Nm */
#define SENTRY_PITCH_GRAVITY_B 0.0f         /* [待实测] 常值偏置,Nm */
#define SENTRY_PITCH_GRAVITY_PHASE_DEG 0.0f /* [待实测] 质心相位,deg */
#define SENTRY_PITCH_GRAVITY_LIMIT_NM 3.0f  /* [理论] 重力前馈自身限幅,Nm */

/* ---------------- 链路 / 安全 ---------------- */

#define SENTRY_GIMBAL_DM_LOST_MS 100.0f     /* [理论] DM 反馈超时判据,ms(约 50 个 DM 周期) */
#define SENTRY_GIMBAL_RECENTER_ENABLE 1     /* [配置] 解锁时大 yaw 目标重锚定 + 小 yaw 回中 */
