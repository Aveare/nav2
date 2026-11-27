/**
 * @file    config_remote.h
 * @brief   哨兵遥控参数:通道刻度/死区
 * @layer   CONFIG
 * @author  小猫大王
 * @note    遥控仅作调试/人工接管;比赛自动运行时由 cmd 仲裁切到自主输入。
 */
#pragma once

#define RC_YAW_MAX_RATE 200.0f   /* [待实测] 左横杆满杆 → 世界 yaw 目标角速度,deg/s */
#define RC_PITCH_MAX_RATE 60.0f  /* [待实测] 左纵杆满杆 → pitch 目标角速度,deg/s */
#define RC_STICK_DEADBAND 10     /* [实测-沿用] 摇杆回中/接管死区,原始刻度 */
#define RC_STICK_FULL_SCALE 660.0f /* [规格] DBUS 摇杆满量程,原始刻度 */
