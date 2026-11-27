/**
 * @file    navigation_test.c
 * @brief   USB 分包/校验/时效/单位转换主机测试
 * @layer   TESTS
 * @author  小猫大王
 */
#include "autonomy.h"
#include "ax650_pose.h"
#include "config.h"
#include "nav_protocol.h"
#include "quaternion_ekf.h"
#include "referee_task.h"
#include "usb_port.h"
#include "sentry_def.h"
#include "msg.h"
#include "vision_link.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

ekf_state_t ekf_ins;
static uint32_t now_ms;
static uint8_t usb_buffer[64], tx_buffer[64];
static USBCallback receive_callback;
static sentry_gimbal_feed_t feed;
static unsigned tx_count;
static uint16_t tx_length;
static uint8_t usb_busy;

referee_info_t *referee_get_info(void) { return NULL; }

uint32_t kernel_tick_ms(void) { return now_ms; }
uint32_t kernel_enter_critical(void) { return 0; }
void kernel_exit_critical(uint32_t saved) { (void)saved; }
uint8_t *usb_init(usb_init_config_t config) { receive_callback = config.rx_cbk; return usb_buffer; }
uint8_t usb_is_transmitting(void) { return usb_busy; }
uint8_t usb_try_transmit(uint8_t *buffer, uint16_t length)
{
    assert(length <= sizeof(tx_buffer));
    memcpy(tx_buffer, buffer, length);
    tx_length = length;
    usb_busy = 1u;
    ++tx_count;
    return 0;
}
msg_sub_t msg_sub(const char *name, size_t size) { (void)name; (void)size; return &feed; }
int msg_recv(msg_sub_t sub, void *output) { (void)sub; memcpy(output, &feed, sizeof(feed)); return 1; }

static void navigation_deliver(const uint8_t *bytes, size_t size)
{
    assert(size <= sizeof(usb_buffer));
    memcpy(usb_buffer, bytes, size);
    receive_callback((uint16_t)size);
}

static void navigation_complete_tx(void)
{
    usb_busy = 0u;
    tx_length = 0u;
}

static void write_float_le(uint8_t *output, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    output[0] = (uint8_t)bits;
    output[1] = (uint8_t)(bits >> 8);
    output[2] = (uint8_t)(bits >> 16);
    output[3] = (uint8_t)(bits >> 24);
}

static void make_target_frame(uint8_t output[39])
{
    const float values[7] = {30.0f, -5.0f, 0.25f, -0.125f, 1.0f, -2.0f, 4.5f};
    memset(output, 0, 39u);
    output[0] = 0xA5u;
    output[1] = 30u;
    output[3] = 3u;
    output[4] = ax650_pose_crc8(output, 4u);
    output[5] = 0x03u;
    output[6] = 0x05u;
    for (size_t i = 0; i < 7u; ++i)
        write_float_le(output + 7u + i * 4u, values[i]);
    output[35] = 1u;
    output[36] = 3u;
    const uint16_t crc = ax650_pose_crc16(output, 37u);
    output[37] = (uint8_t)crc;
    output[38] = (uint8_t)(crc >> 8);
}

static void print_hex(const uint8_t *bytes, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        printf("%02x", bytes[i]);
    puts("");
}

int main(void)
{
    nav_frame_t frame = {.type = SENTRY_NAV_PROTOCOL_MSG_COMMAND, .sequence = 7u, .sample_us = 123456789u,
                         .value = {0.25f, -0.125f, 0.5f}, .flags = SENTRY_NAV_PROTOCOL_FLAG_VALID};
    uint8_t bytes[SENTRY_NAV_PROTOCOL_FRAME_SIZE];
    nav_protocol_encode(&frame, bytes);
    /* 输出跨语言黄金向量,由 Python 测试逐字节核对。 */
    print_hex(bytes, sizeof(bytes));

    autonomy_init();
    now_ms = 100u;
    navigation_deliver(bytes, 9);
    autonomy_task();
    sentry_auto_cmd_t command;
    assert(!autonomy_get_command(&command));
    navigation_deliver(bytes + 9, sizeof(bytes) - 9);
    autonomy_task();
#if SENTRY_AUTONOMY_ENABLE
    assert(autonomy_get_command(&command));
    assert(command.vx == 250.0f && command.vy == 125.0f);
    assert(fabsf(command.wz - 28.64789f) < .0001f);
#else
    assert(!autonomy_get_command(&command));
    assert(command.vx == 0.0f && command.vy == 0.0f && command.wz == 0.0f);
#endif
    now_ms = 300u;
    navigation_deliver(bytes, sizeof(bytes)); /* 重复序号不能刷新接收时刻。 */
    autonomy_task();
    now_ms = 401u;
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.sequence = 8u;
    frame.value[0] = NAN;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(!autonomy_get_command(&command));
    frame.value[0] = 1.0f;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.value[0] = .1f;
    nav_protocol_encode(&frame, bytes);
    bytes[20] ^= 1u;
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(!autonomy_get_command(&command));
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
#if SENTRY_AUTONOMY_ENABLE
    assert(autonomy_get_command(&command));
#else
    assert(!autonomy_get_command(&command));
    assert(command.vx == 0.0f && command.vy == 0.0f && command.wz == 0.0f);
#endif
    frame.sequence++;
    frame.flags = 0u;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.flags = SENTRY_NAV_PROTOCOL_FLAG_VALID;
    frame.sequence++;
    nav_protocol_encode(&frame, bytes);
    now_ms = 500u;
    navigation_deliver(bytes, sizeof(bytes));
    now_ms = 901u; /* 排队时间不能被伪装成新鲜数据。 */
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.type = SENTRY_NAV_PROTOCOL_MSG_SYNC_REQUEST;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(tx_length == SENTRY_NAV_PROTOCOL_FRAME_SIZE);
    nav_parser_t parser = {0};
    nav_frame_t decoded;
    int decoded_count = 0;
    for (unsigned i = 0; i < 32; ++i) decoded_count += nav_protocol_feed(&parser, tx_buffer[i], &decoded);
    assert(decoded_count == 1 && decoded.type == SENTRY_NAV_PROTOCOL_MSG_SYNC_RESPONSE && decoded.sample_us == 901000u);
    navigation_complete_tx();
    feed.big_yaw_rel_deg = 90.0f;
    feed.big_yaw_sample_us = 902000u;
    feed.big_yaw_online = 1u;
    now_ms = 920u;
    autonomy_task();
    assert(tx_length == SENTRY_NAV_PROTOCOL_FRAME_SIZE);
    memset(&parser, 0, sizeof(parser));
    for (unsigned i = 0; i < 32; ++i) (void)nav_protocol_feed(&parser, tx_buffer[i], &decoded);
    assert(decoded.type == SENTRY_NAV_PROTOCOL_MSG_FEEDBACK && decoded.sample_us == 902000u);
    assert(fabsf(decoded.value[0] - 1.5707963f) < 1e-6f);
    navigation_complete_tx();
    unsigned previous = tx_count;
    now_ms += 20u;
    autonomy_task();
    assert(tx_count == previous); /* 不给重复角度盖新时间。 */

    uint8_t target[39];
    make_target_frame(target);
    print_hex(target, sizeof(target));
    navigation_deliver(target, 13u);
    autonomy_task();
    vision_state_t vision;
    vision_link_get_state(&vision);
    assert(!vision.valid);
    navigation_deliver(target + 13u, sizeof(target) - 13u);
    autonomy_task();
    vision_link_get_state(&vision);
    assert(vision.valid && vision.command_count == 1u);
    assert(vision.command.yaw == 30.0f && vision.command.pitch == -5.0f);
    assert(vision.command.yaw_speed == 0.25f && vision.command.pitch_speed == -0.125f);
    assert(vision.command.distance == 4.5f && vision.command.shoot == 1u && vision.command.target_id == 3u);
    now_ms += AX650_COMMAND_TIMEOUT_MS + 1u;
    vision_link_get_state(&vision);
    assert(!vision.valid);

    ekf_ins.Initialized = 1u;
    ekf_ins.UpdateCount = 1u;
    ekf_ins.q[0] = 1.0f;
    ekf_ins.q[1] = ekf_ins.q[2] = ekf_ins.q[3] = 0.0f;
    feed.big_yaw_rel_deg = 12.0f;
    feed.small_yaw_rel_deg = -3.0f;
    feed.pitch_deg = 8.0f;
    feed.big_yaw_sample_us = 949000u;
    feed.small_yaw_sample_us = 948000u;
    feed.pitch_sample_us = 947000u;
    feed.big_yaw_online = feed.small_yaw_online = feed.pitch_online = 1u;
    now_ms = 950u;
    autonomy_task();
    assert(tx_length == SENTRY_NAV_PROTOCOL_FRAME_SIZE);
    navigation_complete_tx();
    now_ms = 960u;
    autonomy_task();
    assert(tx_length == AX650_FULL_POSE_FRAME_SIZE);
    assert(tx_buffer[5] == 0x22u && tx_buffer[6] == 0x10u);
    assert((tx_buffer[55] & 0x0Fu) == 0x0Fu);
    print_hex(tx_buffer, tx_length);
    puts("firmware navigation and auto-aim USB tests passed");
    return 0;
}
