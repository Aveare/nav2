/**
 * @file    sentry_usb_pipeline_test.c
 * @brief   autonomy 双协议集成测试:NV COMMAND + 0x0503 同流解析 + 0x1022 上行
 * @layer   TESTS
 * @author  小猫大王
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "autonomy.h"
#include "ax650_pose.h"
#include "ins_task.h"
#include "kernel.h"
#include "msg.h"
#include "nav_protocol.h"
#include "referee_task.h"
#include "sentry_def.h"
#include "usb_port.h"
#include "vision_link.h"

ekf_state_t ekf_ins;
static uint32_t fake_ms = 1000u;
static USBCallback rx_callback;
static uint8_t rx_mirror[64];
static uint8_t last_tx[64];
static uint16_t last_tx_len;
static uint8_t tx_1022;
static referee_info_t referee_ut;
static uint8_t referee_online_ut;

uint32_t kernel_tick_ms(void) { return fake_ms; }
uint32_t kernel_enter_critical(void) { return 0u; }
void kernel_exit_critical(uint32_t saved) { (void)saved; }

uint8_t *usb_init(usb_init_config_t config)
{
    rx_callback = config.rx_cbk;
    return rx_mirror;
}

uint8_t usb_try_transmit(uint8_t *buffer, uint16_t length)
{
    if (length > sizeof(last_tx))
        return 1u;
    memcpy(last_tx, buffer, length);
    last_tx_len = length;
    if (length == 61u && buffer[0] == 0xA5u && buffer[5] == 0x22u)
        tx_1022 = 1u;
    return 0u;
}

uint8_t usb_is_transmitting(void) { return 0u; }

msg_sub_t msg_sub(const char *name, size_t size)
{
    (void)name;
    (void)size;
    return (msg_sub_t)1;
}

int msg_recv(msg_sub_t sub, void *output)
{
    (void)sub;
    if (output != NULL)
        memset(output, 0, sizeof(sentry_gimbal_feed_t));
    return 0;
}

referee_info_t *referee_get_info(void) { return referee_online_ut ? &referee_ut : 0; }

static void feed_usb(const uint8_t *data, uint16_t len)
{
    assert(len <= sizeof(rx_mirror));
    memcpy(rx_mirror, data, len);
    rx_callback(len);
}

static size_t build_command(uint8_t *out, float yaw, float pitch, float distance,
                            uint8_t shoot, uint8_t target_id)
{
    const uint16_t payload_len = 30u;
    const size_t frame_len = 9u + payload_len;
    memset(out, 0, frame_len);
    out[0] = 0xA5u;
    out[1] = (uint8_t)payload_len;
    out[2] = 0u;
    out[3] = 1u;
    out[4] = ax650_pose_crc8(out, 4u);
    out[5] = 0x03u;
    out[6] = 0x05u;
    memcpy(out + 7, &yaw, 4);
    memcpy(out + 11, &pitch, 4);
    memcpy(out + 31, &distance, 4);
    out[35] = shoot;
    out[36] = target_id;
    uint16_t crc = ax650_pose_crc16(out, frame_len - 2u);
    out[37] = (uint8_t)crc;
    out[38] = (uint8_t)(crc >> 8);
    return frame_len;
}

int main(void)
{
    uint8_t frame[64];
    autonomy_init();

    /* NV COMMAND: vx 0.25 m/s, ROS +y left -> -0.125 -> STM32 +125 mm/s, wz 0.5 rad/s */
    nav_frame_t cmd = {
        .type = SENTRY_NAV_PROTOCOL_MSG_COMMAND,
        .sequence = 1u,
        .sample_us = 1000000u,
        .value = {0.25f, -0.125f, 0.5f},
        .flags = SENTRY_NAV_PROTOCOL_FLAG_VALID,
    };
    nav_protocol_encode(&cmd, frame);
    feed_usb(frame, SENTRY_NAV_PROTOCOL_FRAME_SIZE);

    /* same USB slot carries 0x0503 */
    const size_t clen = build_command(frame, 33.0f, 6.0f, 2.0f, 1u, 3u);
    feed_usb(frame, (uint16_t)clen);

    autonomy_task();
    sentry_auto_cmd_t auto_cmd;
    assert(autonomy_get_command(&auto_cmd) == 1);
    assert(fabsf(auto_cmd.vx - 250.0f) < 1e-3f);
    assert(fabsf(auto_cmd.vy - 125.0f) < 1e-3f);
    assert(fabsf(auto_cmd.wz - 28.6479f) < 1e-2f);

    vision_state_t vision;
    vision_link_get_state(&vision);
    assert(vision.valid && vision.command.yaw == 33.0f && vision.command.target_id == 3u);

    /* 0x1022 uplink at the next 10 ms boundary */
    ekf_ins.Initialized = 1u;
    ekf_ins.UpdateCount = 1u;
    ekf_ins.q[0] = 1.0f;
    fake_ms += 20u;
    tx_1022 = 0u;
    autonomy_task();
    assert(tx_1022 == 1u);
    assert(last_tx_len == 61u);

    /* match-start detection: referee game_progress==4 and runtime force flag */
    sentry_rt_match_force_start = 0u;
    referee_online_ut = 0u;
    assert(autonomy_is_match_started() == 0u);
    referee_online_ut = 1u;
    referee_ut.init_flag = 1u;
    referee_ut.GameState.game_progress = 3u;
    assert(autonomy_is_match_started() == 0u);
    referee_ut.GameState.game_progress = 4u;
    assert(autonomy_is_match_started() == 1u);
    referee_ut.GameState.game_progress = 5u;
    assert(autonomy_is_match_started() == 0u);
    sentry_rt_match_force_start = 1u;
    assert(autonomy_is_match_started() == 1u);
    sentry_rt_match_force_start = 0u;

    printf("sentry USB pipeline tests passed\n");
    return 0;
}
