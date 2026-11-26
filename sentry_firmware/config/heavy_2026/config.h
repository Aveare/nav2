/**
 * @file    config.h
 * @brief   重装 2026 骨架配置(编译期占位,勿烧录)
 * @layer   CONFIG
 * @author  小猫大王
* @note    本文件**不包含任何实车标定值**。重装 42mm 的几何/发射/PID 参数
*          必须按 M5 实车重新测量与整定,写进本目录后再移除 SKELETON 标记。
* @note    目前 assembly.c 的 init/run 为空,只跑公共板级/IMU/守护任务;
*          calib_enable=false,不会读取 Flash 零位。
 */
#pragma once

#pragma message "heavy_2026 is a compile-only skeleton; do NOT flash"

/* 板型:骨架阶段固定单板 */
#define ONE_BOARD

/* AX650 视觉链路:重装骨架未接 */
#define AX650_UART1_ENABLE 0
#define AX650_UART1_RX_ENABLE 0
#define AX650_UART1_BAUD 921600u
#define AX650_POSE_PERIOD_MS 10u
#define AX650_COMMAND_TIMEOUT_MS 100u
#define AX650_TX_SPEED_MPS 0.0f
#define AX650_TX_MODE 0u

/* USART6:裁判骨架默认;调试时仅用于自瞄 yaw VOFA。 */
#define YAW_AUTO_VOFA_USART6_ENABLE 0

/* calib.c 编译占位:calib_enable=false,不读 Flash;实车必须重标 */
#define CALIB_FLASH_ADDR 0x080E0000u
#define CALIB_FLASH_SECTOR_COUNT 1u
#define CALIB_MAGIC 0x31424C43u
#define CALIB_VERSION 1u
#define DJI_ECD_TO_DEG 0.043945f
#define PITCH_ECD_SIGN 1
#define PITCH_HORIZON_ECD 0
#define PITCH_REDUCTION_RATIO 1.0f
#define YAW_CHASSIS_ALIGN_ECD 0
