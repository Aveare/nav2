/**
 * @file    config_autonomy.h
 * @brief   哨兵 USB 导航通信与底盘接管参数
 * @layer   CONFIG
 * @author  小猫大王
 * @note    USB CDC VCP 使用 NV v1 固定帧。默认只收发联调数据,不把速度送入底盘。
 */
#pragma once

#define SENTRY_AUTONOMY_ENABLE 0 /* [安全] 1=双拨杆中位时允许导航接管;联调保持 0 */
#define SENTRY_NAVIGATION_FEEDBACK_PERIOD_MS 20u /* [配置] 大 yaw 新鲜 CAN 角度回传周期,ms */
#define SENTRY_NAVIGATION_ACCEL_MM_S2 350.0f /* [待实测] 导航专用平移斜坡 */
#define SENTRY_NAVIGATION_MAX_SPEED_M_S 0.35f /* [待实测] base_link 每轴导航限速 */
#define SENTRY_NAVIGATION_MAX_WZ_RAD_S 0.5f /* [待实测] base_link 导航角速度限幅 */
#define SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS 300u /* [安全] 自主指令有效期,ms */
#define SENTRY_AUTONOMY_PATROL_SPEED 800.0f /* [待实测] 巡逻平移速度,mm/s */
#define SENTRY_AUTONOMY_ENGAGE_DISTANCE_MM 6000.0f /* [待实测] 交战距离阈值,mm */
#define SENTRY_AUTONOMY_RETREAT_HP 100    /* [待实测] 撤退血量阈值 */
#define SENTRY_AUTONOMY_MAX_WZ_DEG_S 180.0f /* [待实测] 自主转向角速度上限,deg/s */
