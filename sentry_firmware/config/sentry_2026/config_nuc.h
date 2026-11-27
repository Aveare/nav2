/**
 * @file    config_nuc.h
 * @brief   哨兵 NUC USB 自瞄通信与目标整形参数
 * @layer   CONFIG
 * @author  小猫大王
 * @note    NUC 通过 USB CDC 同流承载 NV v1 与 RMCV 0x0503/0x1022；
 *          USART1 专用于三轴编码器 VOFA，哨兵不装配 AX650 串口链路。
 * @note    0x1022 上送大 yaw IMU 四元数、三轴关节角、独立采样时间和有效位；
 *          旧 0x1021 打包接口仅保留兼容。
 */
#pragma once

#define SENTRY_NUC_RMCV_ENABLE 1
/* [配置] 1=NUC 自瞄/导航共用 USB CDC：RMCV 0x1022 姿态上行 + 0x0503 目标下行。
 * USB 唯一所有者仍是 autonomy 任务；本开关只控制 RMCV 收发，不影响 NV v1。 */
#define SENTRY_NUC_RMCV_POSE_PERIOD_MS 10u    /* [协议] 0x1022 周期，ms（100 Hz） */
#define SENTRY_NUC_RMCV_COMMAND_TIMEOUT_MS 100u /* [待实测] 0x0503 下行命令有效期，ms */
#define SENTRY_NUC_RMCV_TX_SPEED_MPS 0.0f     /* [协议] 弹速默认占位，运行时由 referee 覆盖 */
#define SENTRY_NUC_RMCV_TX_MODE 0u            /* [协议] 模式默认占位，运行时由 auto_aim_active 覆盖 */
#define SENTRY_NUC_RMCV_TX_ROBOT_ID 0u        /* [协议] 本车 ID 默认占位，运行时由 referee 覆盖 */

/* 共享层仍编译历史 AX650 USART1 组件；以下仅提供关闭态编译兼容，不注册串口、不产生运行期链路。
 * RMCV 帧编解码器沿用 ax650_pose 符号是协议兼容实现，不代表哨兵使用 AX650 硬件。 */
#define AX650_UART1_RX_ENABLE 0
#define AX650_UART1_BAUD 921600u
#define AX650_POSE_PERIOD_MS SENTRY_NUC_RMCV_POSE_PERIOD_MS
#define AX650_COMMAND_TIMEOUT_MS SENTRY_NUC_RMCV_COMMAND_TIMEOUT_MS
#define AX650_TX_SPEED_MPS SENTRY_NUC_RMCV_TX_SPEED_MPS
#define AX650_TX_MODE SENTRY_NUC_RMCV_TX_MODE

/* ---------------- 目标整形（世界 yaw） ---------------- */

#define YAW_AUTO_KF_ENABLE 1
#define YAW_AUTO_KF_ANGLE_NOISE_STD 0.40f  /* [待实测] 角度测量标准差，deg */
#define YAW_AUTO_KF_RATE_NOISE_STD 100.0f  /* [待实测] 角速度测量标准差，deg/s */
#define YAW_AUTO_KF_ACCEL_NOISE_STD 1200.0f /* [待实测] 加速度噪声，deg/s² */
#define YAW_AUTO_KF_RATE_LIMIT 500.0f      /* [理论] 角速度测量限幅，deg/s */
#define YAW_AUTO_KF_RESET_ANGLE 20.0f      /* [理论] 创新超限直接重置，deg */
#define YAW_AUTO_SMOOTH_ENABLE 1
#define YAW_AUTO_SMOOTH_POSITION_GAIN 20.0f /* [待实测] 目标整形位置增益，1/s */
#define YAW_AUTO_SMOOTH_MAX_RATE 300.0f     /* [待实测] 目标整形最大角速度，deg/s */
#define YAW_AUTO_SMOOTH_MAX_ACCEL 3000.0f   /* [待实测] 目标整形最大角加速度，deg/s² */
#define YAW_AUTO_VOFA_USART6_ENABLE 0       /* [配置] USART6 固定给裁判系统 */
