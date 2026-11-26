/**
 * @file    config_ax650.h
 * @brief   哨兵 AX650 自瞄链路:USB CDC / 目标整形 / 自瞄辅助参数
 * @layer   CONFIG
 * @author  小猫大王
 * @note    复用 components/ax650 的 0x0503 协议;视觉端输出世界系 yaw/pitch 目标。
 *          USB CDC 与导航共用，由 autonomy 作为唯一端口所有者统一分流。
 */
#pragma once

#define AX650_USB_ENABLE   1
#define AX650_UART1_ENABLE 0
/* [配置] 自瞄改走 USB CDC；USART1 释放给调试，禁止两个 AX650 传输同时启用。 */
#ifndef AX650_UART1_RX_ENABLE
#define AX650_UART1_RX_ENABLE 0
#endif
#define AX650_UART1_BAUD 921600u      /* [协议] USART1 波特率,8E1 */
#define AX650_POSE_PERIOD_MS 10u      /* [协议] 姿态上报周期,ms(100Hz) */
#define AX650_COMMAND_TIMEOUT_MS 100u /* [待实测] 0x0503 下行命令有效期,ms */
#define AX650_TX_SPEED_MPS 0.0f       /* [协议] 弹速占位,m/s */
#define AX650_TX_MODE 0u              /* [协议] 标定模式占位 */
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
