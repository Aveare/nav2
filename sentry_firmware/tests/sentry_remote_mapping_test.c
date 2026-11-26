/**
 * @file    sentry_remote_mapping_test.c
 * @brief   哨兵本车遥控映射主机测试
 * @layer   TESTS
 * @author  modified by 小猫大王
 */
#include "robot_cmd_internal.h"
#include "robot_cmd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static remote_rc_ctrl_t test_rc[2];
static uint32_t test_ms;

kbm_input_source_e input_source;
remote_rc_ctrl_t *rc_data = test_rc;
robot_status_e robot_state = ROBOT_STATUS_READY;
chassis_ctrl_cmd_t chassis_cmd;
sentry_gimbal_cmd_t gimbal_cmd;
shoot_ctrl_cmd_t shoot_cmd;
sentry_gimbal_feed_t gimbal_fb_recv;
sentry_chassis_feed_t chassis_fb_recv;
sentry_shoot_feed_t shoot_fb_recv;
float ax650_raw_yaw_target;
uint8_t yaw_recentering;
uint16_t recenter_cnt;

uint8_t remote_control_is_online(void)
{
    return 1u;
}

uint32_t dwt_get_timeline_ms(void)
{
    return test_ms;
}

fire_tier_e cmd_fire_tier_get(void)
{
    return FIRE_TIER_NORMAL;
}

float cmd_fire_tier_rate(void)
{
    return SHOOT_RATE_DEFAULT;
}

static void sentry_test_reset(void)
{
    memset(test_rc, 0, sizeof(test_rc));
    memset(&chassis_cmd, 0, sizeof(chassis_cmd));
    memset(&gimbal_cmd, 0, sizeof(gimbal_cmd));
    memset(&shoot_cmd, 0, sizeof(shoot_cmd));
    test_rc[RC_SLOT_CURRENT].rc.switch_left = REMOTE_RC_SW_MID;
    test_rc[RC_SLOT_CURRENT].rc.switch_right = REMOTE_RC_SW_MID;
}

int main(void)
{
    sentry_test_reset();
    test_rc[RC_SLOT_CURRENT].rc.rocker_r1 = 660;
    cmd_remote_set();
    assert(chassis_cmd.chassis_mode == CHASSIS_RC_CONTROL);
    assert(fabsf(chassis_cmd.vx - CHASSIS_MAX_SPEED) < 0.01f);
    assert(fabsf(chassis_cmd.vy) < 0.01f);

    test_rc[RC_SLOT_CURRENT].rc.rocker_l_ = 660;
    cmd_remote_set();
    assert(fabsf(chassis_cmd.wz + CHASSIS_ROTATE_SPEED) < 0.01f);
    assert(gimbal_cmd.small_yaw_rate_deg_s == 0.0f);

    test_rc[RC_SLOT_CURRENT].rc.switch_left = REMOTE_RC_SW_DOWN;
    cmd_remote_set();
    assert(chassis_cmd.wz == 0.0f);
    assert(fabsf(gimbal_cmd.small_yaw_rate_deg_s - SENTRY_SMALL_YAW_STICK_RATE_DEG_S) <
           0.01f);
    assert(shoot_cmd.shoot_mode == SHOOT_ON);
    assert(shoot_cmd.friction_mode == FRICTION_ON);
    assert(shoot_cmd.load_mode == LOADER_BURST);

    test_rc[RC_SLOT_CURRENT].rc.switch_left = REMOTE_RC_SW_MID;
    cmd_remote_set();
    assert(shoot_cmd.friction_mode == FRICTION_OFF);
    assert(shoot_cmd.load_mode == LOADER_STOP);

    test_rc[RC_SLOT_CURRENT].rc.switch_right = REMOTE_RC_SW_DOWN;
    cmd_remote_set();
    assert(chassis_cmd.chassis_mode == CHASSIS_ROTATE);

    sentry_test_reset();
    test_rc[RC_SLOT_CURRENT].rc.rocker_l1 = 660;
    cmd_remote_set();
    assert(gimbal_cmd.pitch > 0.0f);

    puts("PASS: sentry RC movement, left-down fire, small-yaw split and spin");
    return 0;
}
