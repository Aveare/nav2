/**
 * @file    sentry_autoaim_test.c
 * @brief   自瞄目标解算主机测试:有效性/保持窗/视觉 fire/pitch 限幅
 * @layer   TESTS
 * @author  小猫大王
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "robot_def.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "vision_link.h"
#include "dwt_port.h"

remote_rc_ctrl_t *rc_data;
robot_status_e robot_state;
chassis_ctrl_cmd_t chassis_cmd;
sentry_gimbal_cmd_t gimbal_cmd;
shoot_ctrl_cmd_t shoot_cmd;
sentry_gimbal_feed_t gimbal_fb_recv;
sentry_chassis_feed_t chassis_fb_recv;
sentry_shoot_feed_t shoot_fb_recv;
float ax650_raw_yaw_target;
uint8_t yaw_recentering;
uint16_t recenter_cnt;
uint8_t auto_aim_active;
uint8_t auto_aim_fire_request;
kbm_input_source_e input_source;

static remote_rc_ctrl_t rc;
static vision_state_t vision_ut;
static float fake_ms = 1000.0f;

uint32_t dwt_get_timeline_ms(void)
{
    return (uint32_t)fake_ms;
}

uint8_t remote_control_is_online(void)
{
    return 1u;
}

void vision_link_get_state(vision_state_t *out)
{
    *out = vision_ut;
    out->valid = (vision_ut.command_count != 0u &&
                  (uint32_t)(fake_ms - vision_ut.rx_ms) <= 100u)
                     ? 1u
                     : 0u;
}

static void reset_all(void)
{
    memset(&rc, 0, sizeof(rc));
    memset(&vision_ut, 0, sizeof(vision_ut));
    memset(&gimbal_cmd, 0, sizeof(gimbal_cmd));
    memset(&gimbal_fb_recv, 0, sizeof(gimbal_fb_recv));
    rc_data = &rc;
    rc.rc.switch_right = REMOTE_RC_SW_MID;
    robot_state = ROBOT_STATUS_READY;
    gimbal_fb_recv.big_yaw_world_deg = 0.0f;
    fake_ms = 1000.0f;
}

static void give_target(float yaw, float pitch, uint8_t target_id, uint8_t shoot)
{
    vision_ut.command.yaw = yaw;
    vision_ut.command.pitch = pitch;
    vision_ut.command.yaw_speed = 0.0f;
    vision_ut.command.shoot = shoot;
    vision_ut.command.target_id = target_id;
    vision_ut.command_count++;
    vision_ut.rx_ms = (uint32_t)fake_ms;
}

int main(void)
{
    reset_all();
    assert(cmd_auto_aim_set() == 0);
    assert(gimbal_cmd.aim_valid == 0u);

    /* valid target: first tick initializes smooth at current world, second moves */
    give_target(30.0f, 5.0f, 1u, 1u);
    assert(cmd_auto_aim_set() == 1);
    assert(gimbal_cmd.aim_valid == 1u);
    assert(auto_aim_fire_request == 1u);
    assert(fabsf(gimbal_cmd.pitch_abs - 5.0f) < 1e-3f);
    fake_ms += 20.0f;
    assert(cmd_auto_aim_set() == 1);
    assert(gimbal_cmd.big_yaw > 0.0f && gimbal_cmd.big_yaw <= 30.0f);

    /* target_id=0 -> hold window, then release */
    vision_ut.command.target_id = 0u;
    vision_ut.command_count++;
    vision_ut.rx_ms = (uint32_t)fake_ms;
    assert(cmd_auto_aim_set() == 1);   /* within 300 ms hold */
    assert(gimbal_cmd.aim_valid == 1u);
    assert(auto_aim_fire_request == 0u);
    fake_ms += 400.0f;
    assert(cmd_auto_aim_set() == 0);
    assert(gimbal_cmd.aim_valid == 0u);

    /* right switch UP (kill) invalidates immediately */
    reset_all();
    give_target(10.0f, 0.0f, 1u, 0u);
    rc.rc.switch_right = REMOTE_RC_SW_UP;
    assert(cmd_auto_aim_set() == 0);
    assert(gimbal_cmd.aim_valid == 0u);

    /* pitch clamp */
    reset_all();
    give_target(0.0f, 90.0f, 1u, 0u);
    assert(cmd_auto_aim_set() == 1);
    assert(gimbal_cmd.pitch_abs <= SENTRY_AUTO_AIM_PITCH_MAX_DEG + 1e-3f);

    printf("sentry autoaim tests passed\n");
    return 0;
}
