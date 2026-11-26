/**
 * @file    robot_cmd_internal.h
 * @brief   哨兵 cmd 内部共享状态与跨文件接口
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_ROBOT_CMD_INTERNAL_H
#define SENTRY_ROBOT_CMD_INTERNAL_H

#include "robot_def.h"
#include "remote_control.h"
#include "sentry_def.h"

/* 输入源:遥控杆 / 图传键鼠 */
typedef enum
{
    INPUT_RC = 0,
    INPUT_KBM,
} kbm_input_source_e;

extern kbm_input_source_e input_source;

/* ---------------- 跨簇共享状态(定义在 robot_cmd.c) ---------------- */

extern remote_rc_ctrl_t *rc_data;
extern robot_status_e robot_state;
extern chassis_ctrl_cmd_t chassis_cmd;
extern sentry_gimbal_cmd_t gimbal_cmd;
extern shoot_ctrl_cmd_t shoot_cmd;
extern sentry_gimbal_feed_t gimbal_fb_recv;
extern sentry_chassis_feed_t chassis_fb_recv;
extern sentry_shoot_feed_t shoot_fb_recv;
extern float ax650_raw_yaw_target;
extern uint8_t yaw_recentering;
extern uint16_t recenter_cnt;

/* ---------------- 跨 TU 函数 ---------------- */

void cmd_update_input_source(void);
void cmd_keyboard_mouse_set(void);
void cmd_remote_set(void);
void cmd_authority_arbitrate(void);

void cmd_fire_tier_cycle(void);
fire_tier_e cmd_fire_tier_get(void);
float cmd_fire_tier_rate(void);

#if YAW_AUTO_KF_ENABLE
void cmd_yaw_auto_kalman_reset(void);
float cmd_yaw_auto_kalman_update(float measured_angle, float measured_rate, uint8_t target_id,
                                 uint32_t command_count, float now_ms);
#endif
#if YAW_AUTO_SMOOTH_ENABLE
void cmd_yaw_auto_smooth_reset(void);
float cmd_yaw_auto_smooth_update(float target, float initial_angle, float now_ms);
#endif

#endif // SENTRY_ROBOT_CMD_INTERNAL_H
