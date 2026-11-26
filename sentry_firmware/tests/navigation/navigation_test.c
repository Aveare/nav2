/**
 * @file    navigation_test.c
 * @brief   USB 分包/校验/时效/单位转换主机测试
 * @layer   TESTS
 * @author  小猫大王
 */
#include "autonomy.h"
#include "nav_protocol.h"
#include "usb_port.h"
#include "sentry_def.h"
#include "msg.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t now_ms;
static uint8_t usb_buffer[64], tx_buffer[32];
static USBCallback receive_callback;
static sentry_gimbal_feed_t feed;
static unsigned tx_count;

uint32_t kernel_tick_ms(void) { return now_ms; }
uint32_t kernel_enter_critical(void) { return 0; }
void kernel_exit_critical(uint32_t saved) { (void)saved; }
uint8_t *usb_init(usb_init_config_t config) { receive_callback = config.rx_cbk; return usb_buffer; }
uint8_t usb_is_transmitting(void) { return 0; }
uint8_t usb_try_transmit(uint8_t *buffer, uint16_t length)
{
    assert(length == sizeof(tx_buffer));
    memcpy(tx_buffer, buffer, length);
    ++tx_count;
    return 0;
}
msg_sub_t msg_sub(const char *name, size_t size) { (void)name; (void)size; return &feed; }
int msg_recv(msg_sub_t sub, void *output) { (void)sub; memcpy(output, &feed, sizeof(feed)); return 1; }

static void navigation_deliver(const uint8_t *bytes, size_t size)
{
    assert(size <= sizeof(usb_buffer));
    memcpy(usb_buffer, bytes, size);
    receive_callback((uint32_t)size);
}

int main(void)
{
    nav_frame_t frame = {.type = NAV_MSG_COMMAND, .sequence = 7u, .sample_us = 123456789u,
                         .value = {0.25f, -0.125f, 0.5f}, .flags = NAV_FLAG_VALID};
    uint8_t bytes[NAV_FRAME_SIZE];
    nav_protocol_encode(&frame, bytes);
    /* 输出跨语言黄金向量,由 Python 测试逐字节核对。 */
    for (unsigned i = 0; i < sizeof(bytes); ++i) printf("%02x", bytes[i]);
    puts("");

    autonomy_init();
    now_ms = 100u;
    navigation_deliver(bytes, 9);
    autonomy_task();
    sentry_auto_cmd_t command;
    assert(!autonomy_get_command(&command));
    navigation_deliver(bytes + 9, sizeof(bytes) - 9);
    autonomy_task();
    assert(autonomy_get_command(&command));
    assert(command.vx == 250.0f && command.vy == 125.0f);
    assert(fabsf(command.wz - 28.64789f) < .0001f);
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
    assert(autonomy_get_command(&command));
    frame.sequence++;
    frame.flags = 0u;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.flags = NAV_FLAG_VALID;
    frame.sequence++;
    nav_protocol_encode(&frame, bytes);
    now_ms = 500u;
    navigation_deliver(bytes, sizeof(bytes));
    now_ms = 901u; /* 排队时间不能被伪装成新鲜数据。 */
    autonomy_task();
    assert(!autonomy_get_command(&command));

    frame.type = NAV_MSG_SYNC_REQUEST;
    nav_protocol_encode(&frame, bytes);
    navigation_deliver(bytes, sizeof(bytes));
    autonomy_task();
    nav_parser_t parser = {0};
    nav_frame_t decoded;
    int decoded_count = 0;
    for (unsigned i = 0; i < 32; ++i) decoded_count += nav_protocol_feed(&parser, tx_buffer[i], &decoded);
    assert(decoded_count == 1 && decoded.type == NAV_MSG_SYNC_RESPONSE && decoded.sample_us == 901000u);
    feed.big_yaw_rel_deg = 90.0f;
    feed.big_yaw_sample_us = 902000u;
    feed.big_yaw_online = 1u;
    now_ms = 920u;
    autonomy_task();
    memset(&parser, 0, sizeof(parser));
    for (unsigned i = 0; i < 32; ++i) (void)nav_protocol_feed(&parser, tx_buffer[i], &decoded);
    assert(decoded.type == NAV_MSG_FEEDBACK && decoded.sample_us == 902000u);
    assert(fabsf(decoded.value[0] - 1.5707963f) < 1e-6f);
    unsigned previous = tx_count;
    now_ms += 20u;
    autonomy_task();
    assert(tx_count == previous); /* 不给重复角度盖新时间。 */
    puts("firmware navigation tests passed");
    return 0;
}
