/**
 * @file    config_ax650.h
 * @brief   哨兵 USB RMCV 0x0503/0x1021 协议 + 目标整形参数
 * @layer   CONFIG
 * @author  小猫大王
 * @note    USB CDC 同流承载 NV v1 与 RMCV 0x0503/0x1021;USART1 专用于三轴编码器 VOFA。
 *          协议核心以 Avear 实测版为准;本分支 0x0503 接入云台/发射(自瞄),
 *          0x1021 发实测弹速/模式/ID。
 * @note    0x1021 姿态只含大 yaw 平台 IMU,不是完整枪管姿态;0x0503 的 pitch 语义
 *          与平台俯仰换算见 config_gimbal 的自瞄段。
 */
#pragma once

#define AX650_UART1_ENABLE 0
/* [配置] 哨兵不再使用 AX650;保持 0,释放 USART1 给云台 VOFA。 */
#ifndef AX650_UART1_RX_ENABLE
#define AX650_UART1_RX_ENABLE 0 /* RM_shaobin2 current configuration */
#endif
#define AX650_UART1_BAUD 921600u      /* [协议] USART1 波特率,8E1 */
#define AX650_POSE_PERIOD_MS 10u      /* [协议] 姿态上报周期,ms(100Hz) */

#define AX650_USB_ENABLE 1
/* [配置] 1=NUC 自瞄/导航共用 USB CDC:RMCV 0x1021 姿态上行 + 0x0503 目标下行。
 * USB 唯一所有者仍是 autonomy 任务;本开关只控制 RMCV 收发,不影响 NV v1。 */
#define AX650_USB_POSE_PERIOD_MS 10u /* [协议] 0x1021 周期,ms(100Hz) */
#define AX650_COMMAND_TIMEOUT_MS 100u /* [待实测] 0x0503 下行命令有效期,ms */
#define AX650_TX_SPEED_MPS 0.0f       /* [协议] 弹速默认占位,运行时由 referee 覆盖 */
#define AX650_TX_MODE 0u              /* [协议] 模式默认占位,运行时由 auto_aim_active 覆盖 */
#define AX650_TX_ROBOT_ID 0u          /* [协议] 本车 ID 默认占位,运行时由 referee 覆盖 */
#define AX650_AUTO_AIM_MODE 1u        /* [协议] 左中挡上报的自瞄模式 */

/* ---------------- 目标整形(世界 yaw) ---------------- */

#define YAW_AUTO_KF_ENABLE 1
#define YAW_AUTO_KF_ANGLE_NOISE_STD 0.40f  /* [待实测] 角度测量标准差,deg */
#define YAW_AUTO_KF_RATE_NOISE_STD 100.0f  /* [待实测] 角速度测量标准差,deg/s */
#define YAW_AUTO_KF_ACCEL_NOISE_STD 1200.0f /* [待实测] 加速度噪声,deg/s² */
#define YAW_AUTO_KF_RATE_LIMIT 500.0f      /* [理论] 角速度测量限幅,deg/s */
#define YAW_AUTO_KF_RESET_ANGLE 20.0f      /* [理论] 创新超限直接重置,deg */
#define YAW_AUTO_SMOOTH_ENABLE 1
#define YAW_AUTO_SMOOTH_POSITION_GAIN 20.0f /* [待实测] 目标整形位置增益,1/s */
#define YAW_AUTO_SMOOTH_MAX_RATE 300.0f     /* [待实测] 目标整形最大角速度,deg/s */
#define YAW_AUTO_SMOOTH_MAX_ACCEL 3000.0f   /* [待实测] 目标整形最大角加速度,deg/s² */
#define YAW_AUTO_VOFA_USART6_ENABLE 0       /* [配置] USART6 固定给裁判系统 */
