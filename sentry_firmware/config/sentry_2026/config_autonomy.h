/**
 * @file    config_autonomy.h
 * @brief   哨兵导航输入开关与待标定自主参数
 * @layer   CONFIG
 * @author  小猫大王
 * @note    2027 规则禁止多机通信;自主导航/决策由 USB CDC VCP 上位机下发状态量,
 *          下位机只保留输入接口与模式仲裁。默认 SENTRY_AUTONOMY_ENABLE=0。
 */
#pragma once

#define SENTRY_AUTONOMY_ENABLE 0          /* [配置] 0=只回传角度且导航命令硬清零;1=允许导航接管,待台架验证 */
#define SENTRY_NAVIGATION_FEEDBACK_PERIOD_MS 20u /* [配置] 大 yaw 新鲜 CAN 角度回传周期,ms(50 Hz) */
#define SENTRY_NAVIGATION_ACCEL_MM_S2 350.0f /* [待实测] 导航专用平移斜坡,不改手动参数 */
#define SENTRY_NAVIGATION_MAX_SPEED_M_S 0.35f /* [待实测] 每轴导航限速,与ROS低速配置一致 */
#define SENTRY_NAVIGATION_MAX_WZ_RAD_S 0.5f /* [待实测] 导航角速度限幅,不使用巡逻占位值 */
#define SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS 300.0f /* [待实测] 自主指令有效期,ms */
#define SENTRY_AUTONOMY_PATROL_SPEED 800.0f /* [待实测] 巡逻平移速度,mm/s */
#define SENTRY_AUTONOMY_ENGAGE_DISTANCE_MM 6000.0f /* [待实测] 交战距离阈值,mm */
#define SENTRY_AUTONOMY_RETREAT_HP 100    /* [待实测] 撤退血量阈值 */
#define SENTRY_AUTONOMY_MAX_WZ_DEG_S 180.0f /* [待实测] 自主转向角速度上限,deg/s */
