/**
 * @file    config_remote.h
 * @brief   遥控参数:通道刻度/死区
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 apps/infantry/cmd/ 的代码字面量收敛而来(R7),值未改。
* @note    这几个量与 YAW_REF_FF_MAX_RATE / PITCH_REF_FF_MAX_RATE 有硬耦合:
*          摇杆映射的角速度若超过前馈跳变阈值,前馈会被静默清零(见 config/gimbal.h)。
 */
#pragma once

#define RC_YAW_MAX_RATE 200.0f
/* [实测] 左横杆满杆时的 yaw 目标角速度,deg/s。200Hz 下按时间积分成目标角。 */
#define RC_PITCH_MAX_RATE 60.0f
/* [实测] 左纵杆满杆时的 pitch 目标角速度,deg/s。**必须 ≤ PITCH_REF_FF_MAX_RATE**,否则前馈被清零。 */
#define RC_STICK_DEADBAND 10
/* [实测] 摇杆回中/接管判死区(原始刻度)。低于此值视为"没在打杆"。 */
#define RC_STICK_FULL_SCALE 660.0f
/* [规格] DBUS 摇杆满量程(原始刻度 ±660)。 */
