/**
 * @file    sentry_usb_protocol_test.c
 * @brief   哨兵 USB CDC 六类消息同流主机测试
 * @layer   TESTS
 * @author  小猫大王
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "autonomy.h"
#include "ax650_pose.h"
#include "config.h"
#include "kernel.h"
#include "msg.h"
#include "nav_protocol.h"
#include "quaternion_ekf.h"
#include "sentry_def.h"
#include "usb_port.h"
#include "vision_link.h"

ekf_state_t ekf_ins;
static uint32_t fake_ms = 1000u;
static USBCallback rx_callback;
static uint8_t rx_mirror[64];
static sentry_gimbal_feed_t fake_gimbal_feed;
static unsigned seen_feedback;
static unsigned seen_sync_response;
static unsigned seen_pose;

uint32_t kernel_tick_ms(void)
{
    return fake_ms;
}

uint32_t kernel_enter_critical(void)
{
    return 0u;
}

void kernel_exit_critical(uint32_t saved)
{
    (void)saved;
}

uint8_t *usb_init(usb_init_config_t config)
{
    rx_callback = config.rx_cbk;
    return rx_mirror;
}

uint8_t usb_try_transmit(uint8_t *buffer, uint16_t length)
{
    if (length == SENTRY_NAV_PROTOCOL_FRAME_SIZE && buffer[0] == 'N' && buffer[1] == 'V')
    {
        if (buffer[3] == SENTRY_NAV_PROTOCOL_MSG_FEEDBACK)
            ++seen_feedback;
        else if (buffer[3] == SENTRY_NAV_PROTOCOL_MSG_SYNC_RESPONSE)
            ++seen_sync_response;
    }
    else if ((length == AX650_POSE_FRAME_SIZE && buffer[0] == 0xA5u &&
              buffer[5] == 0x21u && buffer[6] == 0x10u) ||
             (length == AX650_FULL_POSE_FRAME_SIZE && buffer[0] == 0xA5u &&
              buffer[5] == 0x22u && buffer[6] == 0x10u))
    {
        ++seen_pose;
    }
    return 0u;
}

uint8_t usb_is_transmitting(void)
{
    return 0u;
}

msg_sub_t msg_sub(const char *name, size_t size)
{
    (void)name;
    (void)size;
    return (msg_sub_t)1;
}

int msg_recv(msg_sub_t sub, void *output)
{
    (void)sub;
    memcpy(output, &fake_gimbal_feed, sizeof(fake_gimbal_feed));
    return 1;
}

static void put_u16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static void put_float(uint8_t *output, float value)
{
    memcpy(output, &value, sizeof(value));
}

static size_t build_vision_command(uint8_t *output, float yaw, float pitch,
                                   float distance, uint8_t shoot, uint8_t target_id)
{
    const uint16_t payload_length = 30u;
    const size_t frame_length = payload_length + 9u;
    memset(output, 0, frame_length);
    output[0] = 0xA5u;
    put_u16(output + 1u, payload_length);
    output[3] = 7u;
    output[4] = ax650_pose_crc8(output, 4u);
    put_u16(output + 5u, 0x0503u);
    put_float(output + 7u, yaw);
    put_float(output + 11u, pitch);
    put_float(output + 31u, distance);
    output[35] = shoot;
    output[36] = target_id;
    put_u16(output + 37u, ax650_pose_crc16(output, frame_length - 2u));
    return frame_length;
}

static void feed_usb(const uint8_t *data, uint16_t length)
{
    assert(length <= sizeof(rx_mirror));
    memcpy(rx_mirror, data, length);
    rx_callback(length);
}

static void test_vision_link_unit(void)
{
    uint8_t frame[64];
    vision_state_t state;
    vision_link_init();
    memset(&ekf_ins, 0, sizeof(ekf_ins));

    const size_t length = build_vision_command(frame, 37.5f, 8.0f, 3.2f, 1u, 6u);
    vision_link_feed(frame, 11u, fake_ms);
    vision_link_get_state(&state);
    assert(state.valid == 0u && state.command_count == 0u);
    vision_link_feed(frame + 11u, length - 11u, fake_ms);
    vision_link_get_state(&state);
    assert(state.valid == 1u && state.command_count == 1u);
    assert(fabsf(state.command.yaw - 37.5f) < 1e-4f);
    assert(state.command.shoot == 1u && state.command.target_id == 6u);

    const size_t bad_length = build_vision_command(frame, 0.0f, 0.0f, -1.0f, 0u, 1u);
    vision_link_feed(frame, bad_length, fake_ms + 1u);
    vision_link_get_state(&state);
    assert(state.dropped == 1u && state.command_count == 1u);
    fake_ms += AX650_COMMAND_TIMEOUT_MS + 1u;
    vision_link_get_state(&state);
    assert(state.valid == 0u);

    ekf_ins.Initialized = 1u;
    ekf_ins.UpdateCount = 1u;
    ekf_ins.q[0] = 1.0f;
    const size_t pose_length = vision_link_build_pose(frame, sizeof(frame), fake_ms);
    assert(pose_length == AX650_POSE_FRAME_SIZE);
    assert(frame[0] == 0xA5u && frame[5] == 0x21u && frame[6] == 0x10u);
    assert(frame[23] == AX650_TX_MODE && frame[28] == AX650_TX_ROBOT_ID);
    fake_ms += AX650_POSE_PERIOD_MS;
    assert(vision_link_build_pose(frame, sizeof(frame), fake_ms) == 0u);
    ++ekf_ins.UpdateCount;
    assert(vision_link_build_pose(frame, sizeof(frame), fake_ms) == AX650_POSE_FRAME_SIZE);
}

static void test_full_usb_pipeline(void)
{
    uint8_t frame[64];
    autonomy_init();
    memset(&fake_gimbal_feed, 0, sizeof(fake_gimbal_feed));
    fake_gimbal_feed.big_yaw_rel_deg = 30.0f;
    fake_gimbal_feed.big_yaw_sample_us = 1234000u;
    fake_gimbal_feed.big_yaw_online = 1u;
    memset(&ekf_ins, 0, sizeof(ekf_ins));
    ekf_ins.Initialized = 1u;
    ekf_ins.UpdateCount = 1u;
    ekf_ins.q[0] = 1.0f;

    const nav_frame_t command = {
        .type = SENTRY_NAV_PROTOCOL_MSG_COMMAND,
        .sequence = 1u,
        .sample_us = 1000000u,
        .value = {0.25f, -0.125f, 0.5f},
        .flags = SENTRY_NAV_PROTOCOL_FLAG_VALID,
    };
    nav_protocol_encode(&command, frame);
    feed_usb(frame, SENTRY_NAV_PROTOCOL_FRAME_SIZE);

    const nav_frame_t sync_request = {
        .type = SENTRY_NAV_PROTOCOL_MSG_SYNC_REQUEST,
        .sequence = 9u,
        .sample_us = 1000000u,
    };
    nav_protocol_encode(&sync_request, frame);
    feed_usb(frame, SENTRY_NAV_PROTOCOL_FRAME_SIZE);

    const size_t command_length = build_vision_command(frame, 33.0f, 6.0f, 2.0f, 1u, 3u);
    feed_usb(frame, 13u);
    feed_usb(frame + 13u, (uint16_t)(command_length - 13u));
    autonomy_task();

    sentry_auto_cmd_t auto_command;
    assert(autonomy_get_command(&auto_command) == 1);
    assert(fabsf(auto_command.vx - 250.0f) < 1e-3f);
    assert(fabsf(auto_command.vy - 125.0f) < 1e-3f);
    assert(fabsf(auto_command.wz - 28.6479f) < 1e-2f);

    vision_state_t vision;
    vision_link_get_state(&vision);
    assert(vision.valid == 1u && vision.command_count == 1u);
    assert(vision.command.yaw == 33.0f && vision.command.target_id == 3u);
    assert(seen_feedback == 1u);
    assert(seen_sync_response == 1u);
    assert(seen_pose == 1u);
}

int main(void)
{
    test_vision_link_unit();
    test_full_usb_pipeline();
    puts("PASS: sentry USB NV/RMCV six-message protocol");
    return 0;
}
