/**
 * @file    config_gimbal.h
 * @brief   哨兵三轴云台参数:大 yaw(DM MIT)/小 yaw(GM6020)/pitch(DM MIT)
 * @layer   CONFIG
 * @author  小猫大王
 * @note    [SENTRY] 参数与遥控行为以本车 RM_shaobin2 工作树为准。
 * @note    IMU 已由用户确认安装在大 yaw 平台，不包含小 yaw 相对转角。
 */
#pragma once

/* ---------------- 电机 ID / 总线 ---------------- */

#define SENTRY_BIG_YAW_DM_ID 5
#define SENTRY_BIG_YAW_DM_MASTER_ID 0
#define SENTRY_PITCH_DM_ID 5
#define SENTRY_PITCH_DM_MASTER_ID 0
#define SENTRY_SMALL_YAW_ESC_ID 1
#define SENTRY_SMALL_YAW_CURRENT_MODE 0

/* calib 组件的通用回退合同；本车 DM 轴不读取这些 DJI 编码器占位值。 */
#define YAW_CHASSIS_ALIGN_ECD 0
#define PITCH_HORIZON_ECD 0
#define PITCH_REDUCTION_RATIO 1.0f
#define PITCH_ECD_SIGN (+1.0f)

/* ---------------- 大 yaw:机械正前方 + 小陀螺世界稳像 ---------------- */

#define SENTRY_BIG_YAW_OUTPUT_ENABLE 1
#define SENTRY_BIG_YAW_FRONT_ALIGN_RAD (-1.52914f)
/* [实测 2026-11-26] 用户将云台摆正后确认的达妙多圈位置。普通挡按最短路径锁定此位置。 */
#define SENTRY_BIG_YAW_TRANSMISSION_RATIO 1.0f
#define SENTRY_BIG_YAW_DIRECTION (+1.0f)
#define SENTRY_BIG_YAW_IMU_DIRECTION (+1.0f)
#define SENTRY_BIG_YAW_IMU_TIMEOUT_MS 20.0f

#define SENTRY_BIG_YAW_ANGLE_KP 20.0f
#define SENTRY_BIG_YAW_ANGLE_DEADBAND_DEG 0.02f
#define SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG 0.5f
#define SENTRY_BIG_YAW_TARGET_DEADBAND_DEG 2.0f
#define SENTRY_BIG_YAW_MAX_SPEED_DEG_S 90.0f
#define SENTRY_BIG_YAW_KD_NEAR 0.8f
#define SENTRY_BIG_YAW_KD_FAR 0.2f
#define SENTRY_BIG_YAW_KD_BLEND_LO_DEG 4.0f
#define SENTRY_BIG_YAW_KD_BLEND_HI_DEG 8.0f

#define SENTRY_BIG_YAW_SPEED_KP 5.2f
#define SENTRY_BIG_YAW_SPEED_KI 1.2f
#define SENTRY_BIG_YAW_INTEGRAL_NM 3.0f
/* [待实测] 本地工作树最新诊断值 3 N.m；烧录后必须监测线圈温度与驱动状态。 */
#define SENTRY_BIG_YAW_TORQUE_NM 10.0f
#define SENTRY_BIG_YAW_TORQUE_RANGE_NM 10.0f
#define SENTRY_BIG_YAW_TORQUE_LPF_RC_S 0.03f

/* ---------------- 小 yaw:相对中位位置保持 ---------------- */

#define SENTRY_SMALL_YAW_ENABLE 1
#define SENTRY_SMALL_YAW_CENTER_ECD 27u
#define SENTRY_SMALL_YAW_LIMIT_DEG 90.0f
#define SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG 10.0f
#define SENTRY_SMALL_YAW_RETURN_DEG_S 60.0f
#define SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE 1

#define SENTRY_SMALL_YAW_ANGLE_KP 25.0f
#define SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG 0.3f
#define SENTRY_SMALL_YAW_MAX_SPEED_DEG_S 180.0f
#define SENTRY_SMALL_YAW_STICK_RATE_DEG_S 120.0f
#define SENTRY_SMALL_YAW_STICK_DIR (-1.0f)
#define SENTRY_SMALL_YAW_SPEED_KP 50.0f
#define SENTRY_SMALL_YAW_SPEED_KI 0.0f
#define SENTRY_SMALL_YAW_INTEGRAL_LIMIT 2000
#define SENTRY_SMALL_YAW_MAX_OUT 16000
#define SENTRY_SMALL_YAW_DEADBAND_DEG_S 3.0f
#define SENTRY_SMALL_YAW_OUTPUT_RC_S 0.005f

/* ---------------- pitch:解锁位置为中心的相对行程 ---------------- */

#define SENTRY_PITCH_DIRECTION (+1.0f)
#define SENTRY_PITCH_TRAVEL_DEG 20.0f
#define SENTRY_PITCH_RATE_DEG_S 60.0f
#define SENTRY_PITCH_MOUSE_RATE_DEG_S 150.0f
#define SENTRY_PITCH_ANGLE_KP 8.0f
#define SENTRY_PITCH_DEADBAND_DEG 0.3f
#define SENTRY_PITCH_MAX_SPEED_DEG_S 90.0f
#define SENTRY_PITCH_SPEED_KP 0.6f
#define SENTRY_PITCH_SPEED_KI 0.2f
#define SENTRY_PITCH_INTEGRAL_NM 0.3f
#define SENTRY_PITCH_TORQUE_NM 1.0f
#define SENTRY_PITCH_TORQUE_RANGE_NM 10.0f

/* 大 yaw / pitch 共用的目标速度前馈与速度环输出滤波。 */
#define SENTRY_AXIS_OUTPUT_RC_S 0.012f
#define SENTRY_AXIS_D_LIMIT_DEG_S 30.0f
#define SENTRY_AXIS_FF_GAIN 1.0f
#define SENTRY_AXIS_FF_LPF 0.3f

/* ---------------- 安装 / 链路 / 安全 ---------------- */

#define SENTRY_IMU_AFTER_SMALL_YAW 0
/* [用户确认 2026-11-26] IMU 安装在大 yaw 平台，世界 yaw 只包含大 yaw。 */
#define SENTRY_GIMBAL_DM_LOST_MS 100.0f
#define SENTRY_GIMBAL_RECENTER_ENABLE 0
/* [配置] 大 yaw 自身锁机械正前方，不使用旧的“底盘追云台”回中流程。 */

/* ---------------- USART1 / VOFA 编码器观测 ---------------- */

#define GIMBAL_VOFA_UART1_ENABLE 1
/* [配置] USART1 以 VOFA JustFloat 输出三轴编码器位置；AX650 保持关闭。 */
#define GIMBAL_VOFA_SEND_DIVIDER 1u
