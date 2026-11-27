/**
 * @file    config_gimbal.h
 * @brief   哨兵三轴云台参数:大 yaw(DM MIT)/ 小 yaw(GM6020)/ pitch(DM MIT)
 * @layer   CONFIG
 * @author  小猫大王
 * @note    三轴独立控制；大 yaw 普通回正，小陀螺仅世界速度补偿；总输出开关仍为0。
 * @note    DM 控制力矩单位为 N·m,大yaw/pitch控制器保留源rad、rad/s、Nm单位；比值按电机输出轴/关节轴定义。
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
#define SENTRY_BIG_YAW_DM_MASTER_ID 0x000u /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_DM_ID 5           /* [实测-参考] pitch DM 控制 ID,CAN2 */
#define SENTRY_PITCH_DM_MASTER_ID 0x000u /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_SMALL_YAW_ESC_ID 1u /* [实测 2026-11-26] GM6020 ID1(0x205) */
#define SENTRY_SMALL_YAW_CURRENT_MODE 0u /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */

/* ---------------- 方向 / 变速比 / 零位 ---------------- */

#define SENTRY_BIG_YAW_DIRECTION 1.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_DIRECTION (-1.0f) /* [实测 2026-11-27] 关节与枪口方向相反:上抬时 total_position 减小
                                        * (下限位 -112.2°,上限位 -181.7°);原 +1 是错的 */
#define SENTRY_PITCH_TORQUE_SIGN (+1.0f)
/* [推导 2026-11-28,待台架首推确认] 编码器闭环的**稳定性符号只跟电机走,与机构方向无关**:
 * 同款大 yaw DM4310 已台架验证"MIT 正力矩 → total_position 增大",闭环需 正输出 → 正力矩,
 * 故取 +1。原 -1 是 11-23 世界俯仰(IMU)闭环时按"正力矩枪口下压"取的反号;11-26 迁移改成
 * 编码器闭环后没有重新推导,保留 -1 等于正反馈:任何一点杆量都会被放大到冲限位
 * (用户现象"动一下摇杆直接上抬到限位")。台架若首推即加速离开目标:急停,写
 * sentry_rt_pitch_torque_sign = -1 并回头改这里。 */
#define SENTRY_SMALL_YAW_DIRECTION (+1.0f)    /* [待实测] GM6020 正输出 → 相对角正方向 */
#define SENTRY_BIG_YAW_TRANSMISSION_RATIO 1.0f /* [实测 2026-11-27] 复核:est_wz 低估 ~25%,实际电机/关节 192/188≈1.02 */
#define SENTRY_PITCH_TRANSMISSION_RATIO 1.0f
/* [实测 2026-11-27] 传动比 R = 电机轴角度 / 枪管俯仰角(与大 yaw 同口径)。手推硬限位
 * -112.2°~-181.7° 共 69.5°,对应枪管 下压 35.7°~上抬 33.8° 共 69.5° → 1:1。
 * 摇杆命令、软限位、爬升率都是枪管度,固件按 /R 换算到电机轴;台架量法:断电手推枪管
 * 10°(手机水平仪),R = Δsentry_pitch_position_rad(换算成度) / 10;运行期 sentry_rt_pitch_ratio 可试。 */
#define SENTRY_BIG_YAW_ZERO_RAD SENTRY_BIG_YAW_FRONT_ALIGN_RAD
#define SENTRY_PITCH_ZERO_RAD (-2.58163f)     /* [实测 2026-11-27] 枪管水平时 total_position,rad
                                               * (-147.917°;11-23 的 -1.935 已被拆装改变) */
#define SENTRY_SMALL_YAW_ZERO_ECD SENTRY_SMALL_YAW_CENTER_ECD
#define SENTRY_IMU_AFTER_SMALL_YAW 0
/* [待确认] 1=IMU 装在小 yaw 之后的枪体上(IMU 世界 yaw 包含小 yaw 相对角);
 * 0=IMU 装在大 yaw 结构上(IMU 世界 yaw 只含大 yaw)。两种安装的控制分配公式不同,
 * 首次上电必须按实际安装确认;错了会表现为大 yaw/小 yaw 互相抢或静止时缓慢漂移。 */

/* ---------------- 大 yaw(DM-J4310,MIT 力矩) ---------------- */

#define SENTRY_BIG_YAW_OUTPUT_ENABLE 1       /* [用户确认 2026-11-26] 过温故障已解决，启用回正/小陀螺世界速度补偿 */
#define SENTRY_BIG_YAW_MAX_TORQUE_NM 10.0f  /* [规格] 必须 ≤ 驱动器 TMAX;J4310 参考值 10 */
#define SENTRY_BIG_YAW_ANGLE_KP 20.0f /* [实测 2026-11-26] 同步带修好后沿用;若虚位复现降到 12/8 */
#define SENTRY_BIG_YAW_ANGLE_DEADBAND SENTRY_BIG_YAW_DEADBAND_DEG /* 兼容旧观察工具，deg */
#define SENTRY_BIG_YAW_MAX_SPEED_DEG_S 90.0f /* [移植/待实测] RM_shaobin2,控制器使用 rad/rad/s/Nm */
#define SENTRY_BIG_YAW_ANGLE_IL 0.0f        /* [理论] 角度环不积分(速度环负责) */
#define SENTRY_BIG_YAW_SPEED_KP 4.2f /* [实测 2026-11-26] 小陀螺稳像速度环,Kp 5.2→4.2 降左右振荡 */
#define SENTRY_BIG_YAW_SPEED_KI 2.0f /* [实测 2026-11-27] 0.8→2.0:进入小陀螺拖拽 163→81°/9s,无过冲 */
#define SENTRY_BIG_YAW_SPEED_IL SENTRY_BIG_YAW_INTEGRAL_NM
#define SENTRY_BIG_YAW_SPEED_MAXOUT SENTRY_BIG_YAW_TORQUE_NM
#define SENTRY_BIG_YAW_OUTPUT_LPF_RC SENTRY_BIG_YAW_TORQUE_LPF_RC_S /* 兼容参数名，s */

#define SENTRY_BIG_YAW_FRICTION_FF_NM 0.0f /* [配置 2026-11-28] 小陀螺摩擦前馈,N·m;0=关(默认)。
        * 按关节速度软符号反向注入,抵消同步带/轴承拖拽:稳态负载实测 3.2~3.4,
        * PI 从零积分起步要 ~9s 填满(进入拖拽 81°/9s)。台架用 sentry_rt_big_yaw_fric_ff_nm 从 2.0 起调 */
#define SENTRY_BIG_YAW_FRICTION_FF_RAMP_RAD_S 1.0f /* [理论] 软符号斜坡宽度,rad/s;|关节速度|达此值前馈到全量 */

/* ---------------- 小 yaw(GM6020,相对精瞄) ---------------- */

#define SENTRY_SMALL_YAW_ENABLE 1u /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_SMALL_YAW_LIMIT_DEG 125.0f /* [实测 2026-11-23] 手转机械行程 ±136°,留 11° 余量 */
#define SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG 10.0f /* [理论] 控制目标再内缩 10° */
#define SENTRY_SMALL_YAW_FINE_DEG 10.0f /* 仅历史分配算法测试使用，正常控制不再分配世界目标 */
#define SENTRY_SMALL_YAW_ANGLE_KP 10.0f /* [实测 2026-11-27] 25→10:半度误差放大成 ±12°/s 指令导致高频抖 */
#define SENTRY_SMALL_YAW_ANGLE_DEADBAND 0.8f    /* [实测 2026-11-27] 0.3→0.8:不追亚度级抖动 */
#define SENTRY_SMALL_YAW_MAX_SPEED_DEG_S 180.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_SMALL_YAW_ANGLE_IL 100.0f        /* [理论] 角度环积分限幅 */
#define SENTRY_SMALL_YAW_SPEED_KP 50.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_SMALL_YAW_SPEED_KI 0.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_SMALL_YAW_SPEED_IL 2000          /* [理论] 速度环积分限幅 */
#define SENTRY_SMALL_YAW_SPEED_MAXOUT 16000     /* [待实测] 输出限幅(电压帧 25000 的 64%) */
#define SENTRY_SMALL_YAW_OUTPUT_RC 0.02f        /* [待实测] 输出低通 RC,s */

/* ---------------- pitch(DM-J4310,MIT 力矩) ---------------- */

#define SENTRY_PITCH_MAX_ANGLE SENTRY_PITCH_MAX_DEG
#define SENTRY_PITCH_MIN_ANGLE SENTRY_PITCH_MIN_DEG
#define SENTRY_PITCH_ZERO_AT_BOOT 1u
/* [用户要求 2026-11-28] 1=上电校准 pitch:首个有效反馈锁定为 0 位,软限位从上电位置起算。
 * 上电前把枪管放到行程中间(水平附近):放在行程端点会有一侧顶硬限位。
 * 11-27 曾改 0(固定水平零位)是因为"上电在下限位会把下限位当水平",按本条操作要求即可避免。 */
#define SENTRY_PITCH_MIN_DEG (-15.0f)
#define SENTRY_PITCH_MAX_DEG 15.0f
/* [配置 2026-11-28] 相对上电零位的软限位,单位是**枪管俯仰角**(负=下压,正=上抬),
 * 固件按 SENTRY_PITCH_DIRECTION 换算到电机轴。11-27 手推硬限位:水平位上抬 33.8°/下压 35.7°
 * (电机轴 69.5° 行程,与枪管 1:1),枪管居中时 ±15° 每侧留 ~18° 余量;
 * 台架确认后用 sentry_rt_pitch_min/max_deg 放宽再回填。 */
#define SENTRY_PITCH_LIMIT_MARGIN_DEG 2.0f
/* [安全] 实测角越过软限位该值立即断力(11-23 事故教训)。 */
#define SENTRY_PITCH_MAX_TORQUE_NM 1.5f     /* [安全] 连杆已断,行程未复核前限 1.5 N·m */
#define SENTRY_PITCH_ANGLE_KP 8.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_ANGLE_DEADBAND 0.5f    /* [待实测] 角度死区,deg */
#define SENTRY_PITCH_MAX_SPEED_DEG_S 90.0f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_ANGLE_IL 30.0f         /* [理论] 角度环积分限幅(当前控制链未引用:角度环无积分) */
#define SENTRY_PITCH_SPEED_KP 0.6f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_SPEED_KI 0.2f /* [移植/待实测] RM_shaobin2，控制器使用 rad/rad/s/Nm */
#define SENTRY_PITCH_SPEED_IL 1.0f          /* [待实测] 速度环积分限幅,Nm(当前控制链未引用:生效的是 SENTRY_PITCH_INTEGRAL_NM) */
#define SENTRY_PITCH_SPEED_MAXOUT 10.0f     /* [规格] 速度环输出限幅,Nm(当前控制链未引用:生效的是 SENTRY_PITCH_TORQUE_NM) */
#define SENTRY_PITCH_OUTPUT_LPF_RC 0.02f    /* [待实测] 力矩输出低通 RC,s(当前控制链未引用:生效的是 SENTRY_AXIS_OUTPUT_RC_S) */

/* 重力前馈:τ_ff = A·cos(θ − φ) + B,N·m。A/B=0 时完全关闭。 */
#define SENTRY_PITCH_GRAVITY_A 0.0f         /* [待实测] 重力矩幅值,Nm */
#define SENTRY_PITCH_GRAVITY_B 0.0f         /* [待实测] 常值偏置,Nm */
#define SENTRY_PITCH_GRAVITY_PHASE_DEG 0.0f /* [待实测] 质心相位,deg */
#define SENTRY_PITCH_GRAVITY_LIMIT_NM 3.0f  /* [理论] 重力前馈自身限幅,Nm */

/* ---------------- 链路 / 安全 ---------------- */

#define SENTRY_GIMBAL_DM_LOST_MS 100.0f     /* [理论] DM 反馈超时判据,ms(约 50 个 DM 周期) */
#define SENTRY_GIMBAL_RECENTER_ENABLE 0 /* 独立转向不触发底盘回中 */

/* ---------------- UART1 / VOFA 编码器观测 ---------------- */

#define GIMBAL_VOFA_UART1_ENABLE 1
/* [配置] 1=USART1 以 VOFA JustFloat 输出三轴编码器位置,同时必须关闭 AX650。 */
#define GIMBAL_VOFA_SEND_DIVIDER 1u
/* [配置] 200 Hz `gimbal_task` 分频;1=200 Hz,2=100 Hz。 */

#define SENTRY_BIG_YAW_CONTROL_ENABLE 1u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_CONTROL_ENABLE 1u
/* [安全 2026-11-27] 上电校准零位 + 相对软限位(默认 0~15°) + 实测角越界断力就位后恢复使能;
 * 首次台架必须架空空转、手扶枪管,确认首拍不动、上行限幅正确后再放行。 */

#define SENTRY_BIG_YAW_CAN_BUS 1u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_DM_CAN_ID 5u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_FRONT_ALIGN_VALID 1u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_FRONT_ALIGN_RAD (-1.52914f) /* [实测 2026-11-26] 人工摆正云台后的多圈位置 */

#define SENTRY_BIG_YAW_FRONT_ALIGN_AT_BOOT 1u
/* [配置 2026-11-27] 1=上电校准车头:首个有效反馈的关节角锁定为前方参考,
 * 忽略下面的编译常量;连续旋转会让 total_position 跨 25 rad 窗口,常量不可靠。
 * 要求上电前把大 yaw 摆到车头标记。0=退回编译常量(旧行为)。 */

#define SENTRY_BIG_YAW_IMU_DIRECTION 1.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_IMU_TIMEOUT_MS 20.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG 0.5f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_TORQUE_LPF_RC_S 0.03f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_KD_NEAR 0.8f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_KD_FAR 0.2f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_KD_BLEND_LO_DEG 4.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_KD_BLEND_HI_DEG 8.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_INTEGRAL_NM 6.0f /* [实验/待实测 2026-11-27] 3.0→6.0:稳态负载 3.2~3.4 N·m 顶死限幅致 +3°/s 漂移 */

#define SENTRY_BIG_YAW_TORQUE_RANGE_NM 10.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_TORQUE_NM 10.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_CHASSIS_MOUSE_RATE_DEG_S 60.0f /* [移植/待实测] RM_shaobin2 */

#define DM_STARTUP_WAIT_MS 30u /* [移植/待实测] RM_shaobin2 */

#define DM_ENABLE_RETRY_MS 100u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_CAN_BUS 2u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_MODEL "GM6020" /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG 0.8f /* [实测 2026-11-27] 0.3→0.8:不追亚度级抖动 */

#define SENTRY_SMALL_YAW_STICK_RATE_DEG_S 120.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_STICK_DIR (-1.0f) /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_INTEGRAL_LIMIT 2000 /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_MAX_OUT 16000 /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_DEADBAND_DEG_S 3.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_OUTPUT_RC_S 0.005f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_CARRIER_INJ 0.0f /* [配置 2026-11-28] 母体世界角速度注入系数,(deg/s)/(deg/s);0=关(默认)。
        * 载体(大yaw座)晃动时小yaw反向补偿做枪口世界稳像:大yaw世界环管低频,小yaw直驱补高频。
        * 仅 IMU 在线时生效;方向不对把 sentry_rt_small_yaw_carrier_inj 取负。台架从 0.3 起调 */

#define SENTRY_SMALL_YAW_CENTER_ECD 27u /* [实测 2026-11-26] 小 yaw 机械中位 ECD */

#define SENTRY_SMALL_YAW_RETURN_DEG_S 60.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE 1u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_CAN_BUS 2u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_DM_CAN_ID 5u /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_INTEGRAL_NM 0.3f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_TORQUE_NM 1.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_TORQUE_RANGE_NM 10.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_DEADBAND_DEG 0.02f /* [实测 2026-11-26] 同步带修好后沿用;虚位复现改 1~2° */

#define SENTRY_BIG_YAW_TARGET_DEADBAND_DEG 2.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_BIG_YAW_REARM_HEALTHY_CYCLES 20u /* [配置 2026-11-28] 重锚锁存自动解除所需连续健康拍数(200Hz 下 100ms)。
        * 单帧任务超时/反馈掉帧不再永久卸力;硬故障(超温/失联/DM 掉使能)期间 healthy=false 不解除 */

#define SENTRY_PITCH_DEADBAND_DEG 0.3f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_AXIS_OUTPUT_RC_S 0.012f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_AXIS_D_LIMIT_DEG_S 30.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_AXIS_FF_GAIN 1.0f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_AXIS_FF_LPF 0.3f /* [移植/待实测] RM_shaobin2 */

#define SENTRY_PITCH_TRAVEL_DEG 20.0f /* [移植/待实测] RM_shaobin2(当前控制链未引用:限位用 SENTRY_PITCH_MIN/MAX_DEG) */

#define SENTRY_PITCH_RATE_DEG_S 60.0f /* [移植/待实测] RM_shaobin2,gimbal 内目标爬升限速,deg/s */

#define SENTRY_PITCH_STICK_RATE_DEG_S 20.0f
/* [配置 2026-11-28] 遥控左纵杆满量程的 pitch 目标积分速率,deg/s。
 * 原与上面共用 60:半行程只有 15°,轻碰 0.25 s 就顶到限位;20 → 满杆 1.5 s 走完 ±15°。 */

#define SENTRY_PITCH_STICK_DEADBAND 40
/* [配置 2026-11-28] 遥控左纵杆死区,原始刻度(满量程 660)。原硬编码 10:
 * 回中偏置一旦超过 10 就会持续单向积分爬到限位;与底盘 CHASSIS_RC_DEADBAND=30 同量级。 */

#define SENTRY_PITCH_MOUSE_RATE_DEG_S 150.0f /* [移植/待实测] RM_shaobin2(当前控制链未引用:键鼠 pitch 用 config_kbm.h 的 KBM_PITCH_*) */

/* ---------------- 自瞄接入(左中挡 / 键鼠 Shift) ---------------- */

#define SENTRY_AUTO_AIM_ENABLE 1u
/* [配置] 1=左中挡或键鼠 Shift 进入自瞄;0=左中挡保持普通遥控(旧行为)。 */
#define SENTRY_AUTO_AIM_PITCH_ENABLE 0u
/* [安全 2026-11-28] 0=pitch 恒为手动。自瞄绝对角把"上电零位"当作相对平台 0° 俯仰,而上电零位
 * 是任意位置,未标定前视觉目标会把 pitch 直接推到软限位;大 yaw 自瞄不受影响。
 * 标定"上电零位对应的枪管世界 pitch"后再改回 1。 */
#define SENTRY_AUTO_AIM_FIRE_ENABLE 1u
/* [配置] 1=自瞄有效且视觉 shoot!=0 时按战术档位连发;0=只瞄不打。 */
#define SENTRY_AUTO_AIM_HOLD_MS 300.0f
/* [理论] 目标断帧后保持上一世界目标的时长,ms;超时退回普通回正。 */
#define SENTRY_AUTO_AIM_BIG_YAW_MAX_SPEED_DEG_S 150.0f
/* [理论] 自瞄时大 yaw 角度环输出上限,deg/s;普通模式仍用 90。 */
#define SENTRY_AUTO_AIM_SMALL_TRIM_LP 0.03f
/* [仿真] 小 yaw 慢速配平低通(每 5 ms)。大 yaw 承担快速世界环,小 yaw 只做慢配平,
 * 避免"小 yaw 吃掉全部误差、大 yaw 目标跟着自己走"在底盘持续旋转时失效。 */
#define SENTRY_AUTO_AIM_PITCH_MAX_SPEED_DEG_S 150.0f
/* [仿真] 自瞄 pitch 角度环输出上限,deg/s;普通模式 90。 */
#define SENTRY_AUTO_AIM_PITCH_TORQUE_NM 1.5f
/* [安全] 自瞄 pitch 力矩上限,N·m;与 SENTRY_PITCH_MAX_TORQUE_NM 一致。 */
#define SENTRY_AUTO_AIM_PITCH_RATE_DEG_S 120.0f
/* [仿真] 自瞄 pitch 目标爬升限速,deg/s;普通模式 60。 */
#define SENTRY_AUTO_AIM_YAW_OFFSET_DEG 0.0f
/* [标定] 视觉世界 yaw 与 IMU 世界 yaw 的固定偏置,deg。
 * 若 NUC 已在 0x1021 同世界系下发则为 0;不一致时用静态目标标一次。 */
#define SENTRY_AUTO_AIM_PITCH_WORLD_DIR SENTRY_PITCH_DIRECTION
/* [实测 2026-11-23] 复用抬枪 total_position 增大的方向;不再单独待标。 */
#define SENTRY_AUTO_AIM_PITCH_MIN_DEG (-30.0f)
#define SENTRY_AUTO_AIM_PITCH_MAX_DEG (30.0f)
/* [待实测] 视觉 pitch 目标的合理性限幅,deg;真正的关节限位仍由 ZERO_RAD±TRAVEL 夹住。 */
#define SENTRY_DM_TEMP_LIMIT_C 65.0f
/* [实测 2026-11-26] DM 0xC 过温出现在转子 59~67°C;软件阈值取 65°C,超限撤力需重新解锁。 */
