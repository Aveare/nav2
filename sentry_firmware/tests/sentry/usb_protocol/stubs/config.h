/**
 * @file    config.h
 * @brief   哨兵 USB 双协议主机测试配置桩
 * @layer   TESTS
 * @author  小猫大王
 */
#pragma once

#define AX650_USB_ENABLE 1
#define AX650_POSE_PERIOD_MS 10u
#define AX650_COMMAND_TIMEOUT_MS 100u
#define AX650_TX_SPEED_MPS 0.0f
#define AX650_TX_MODE 0u
#define AX650_TX_ROBOT_ID 0u
#define AX650_AUTO_AIM_MODE 1u

#define SENTRY_AUTONOMY_ENABLE 1
#define SENTRY_NAVIGATION_FEEDBACK_PERIOD_MS 20u
#define SENTRY_NAVIGATION_MAX_SPEED_M_S 0.35f
#define SENTRY_NAVIGATION_MAX_WZ_RAD_S 0.5f
#define SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS 300u
