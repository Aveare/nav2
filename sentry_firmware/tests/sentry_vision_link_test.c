/**
 * @file    sentry_vision_link_test.c
 * @brief   vision_link 主机测试:0x0503 分片/校验/超时 + 0x1021 打包
 * @layer   TESTS
 * @author  小猫大王
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ax650_pose.h"
#include "ins_task.h"
#include "vision_link.h"

ekf_state_t ekf_ins;
static uint32_t fake_ms = 1000u;

uint32_t kernel_tick_ms(void)
{
    return fake_ms;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_f32(uint8_t *p, float v)
{
    memcpy(p, &v, sizeof(v));
}

static size_t build_command(uint8_t *out, float yaw, float pitch, float distance,
                            uint8_t shoot, uint8_t target_id)
{
    const uint16_t payload_len = 30u;
    const size_t frame_len = 9u + payload_len;
    memset(out, 0, frame_len);
    out[0] = 0xA5u;
    put_u16(out + 1, payload_len);
    out[3] = 1u;
    out[4] = ax650_pose_crc8(out, 4u);
    put_u16(out + 5, 0x0503u);
    put_f32(out + 7, yaw);
    put_f32(out + 11, pitch);
    put_f32(out + 15, 0.0f);
    put_f32(out + 19, 0.0f);
    put_f32(out + 23, 0.0f);
    put_f32(out + 27, 0.0f);
    put_f32(out + 31, distance);
    out[35] = shoot;
    out[36] = target_id;
    put_u16(out + 37, ax650_pose_crc16(out, frame_len - 2u));
    return frame_len;
}

int main(void)
{
    uint8_t frame[64];
    vision_state_t state;
    vision_link_init();
    memset(&ekf_ins, 0, sizeof(ekf_ins));

    const size_t len = build_command(frame, 37.5f, 8.0f, 3.2f, 1u, 6u);
    assert(len == 39u);
    /* fragment in two chunks */
    vision_link_feed(frame, 11u, 1000u);
    vision_link_get_state(&state);
    assert(!state.valid && state.command_count == 0u);
    vision_link_feed(frame + 11u, len - 11u, 1000u);
    vision_link_get_state(&state);
    assert(state.valid && state.command_count == 1u);
    assert(fabsf(state.command.yaw - 37.5f) < 1e-4f);
    assert(fabsf(state.command.pitch - 8.0f) < 1e-4f);
    assert(state.command.target_id == 6u && state.command.shoot == 1u);

    /* negative distance is rejected */
    const size_t bad = build_command(frame, 0.0f, 0.0f, -1.0f, 0u, 1u);
    vision_link_feed(frame, bad, 1100u);
    vision_link_get_state(&state);
    assert(state.dropped == 1u && state.command_count == 1u);

    /* timeout */
    fake_ms = 1201u;
    vision_link_get_state(&state);
    assert(!state.valid);

    /* 0x1021 pose packing */
    ekf_ins.Initialized = 1u;
    ekf_ins.UpdateCount = 1u;
    ekf_ins.q[0] = 1.0f;
    vision_link_set_mode(1u);
    vision_link_set_projectile_speed(24.0f);
    vision_link_set_robot_id(7u);
    fake_ms = 2000u;
    const size_t pose_len = vision_link_build_pose(frame, sizeof(frame), fake_ms);
    assert(pose_len == 31u);
    assert(frame[0] == 0xA5u && frame[5] == 0x21u && frame[6] == 0x10u);
    assert(frame[23] == 1u);       /* program_mode */
    assert(frame[28] == 7u);       /* robot_id */

    /* same millisecond: no duplicate frame */
    assert(vision_link_build_pose(frame, sizeof(frame), fake_ms) == 0u);
    /* 新帧要求 INS 有新样本(实测版语义):同时推进 tick 与 UpdateCount */
    ekf_ins.UpdateCount = 2u;
    fake_ms += 10u;
    assert(vision_link_build_pose(frame, sizeof(frame), fake_ms) == 31u);

    printf("sentry vision_link tests passed\n");
    return 0;
}
