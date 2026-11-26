/**
 * @file    autonomy.c
 * @brief   哨兵 USB 底盘导航输入与带采样时间的大 yaw 回传
 * @layer   APP
 * @author  小猫大王
 * @note    USB ISR 只复制数据;200Hz 应用任务解析,不产生云台/发射命令。
 */
#include "autonomy.h"
#include "config.h"
#include "nav_protocol.h"
#include "usb_port.h"
#include "kernel.h"
#include "msg.h"
#include "sentry_def.h"
#include <string.h>

/* USB FS 单次接收最多 64 字节;环满丢整包,不延长命令有效期。 */
typedef struct
{
    uint8_t bytes[64];
    uint16_t length;
    uint32_t received_ms;
} autonomy_rx_slot_t;

static autonomy_rx_slot_t rx_slots[8];
static volatile uint8_t rx_head, rx_tail;
static uint8_t *usb_rx;
static nav_parser_t parser;
static sentry_auto_cmd_t auto_cmd;
static uint32_t auto_last_ms, auto_sequence;
static uint8_t has_command;
static uint8_t tx_buffer[NAV_FRAME_SIZE];
static msg_sub_t gimbal_feed_sub;
static sentry_gimbal_feed_t gimbal_feed;
static uint32_t feedback_last_ms, feedback_sequence;
static uint64_t feedback_last_sample;

static void autonomy_usb_receive(uint32_t length)
{
    uint8_t next = (uint8_t)((rx_head + 1u) % 8u);
    if (length > 64u || next == rx_tail || usb_rx == NULL)
        return;
    memcpy(rx_slots[rx_head].bytes, usb_rx, length);
    rx_slots[rx_head].length = (uint16_t)length;
    rx_slots[rx_head].received_ms = kernel_tick_ms();
    rx_head = next;
}

static int autonomy_send(const nav_frame_t *frame)
{
    if (usb_is_transmitting())
        return 0;
    nav_protocol_encode(frame, tx_buffer);
    if (usb_try_transmit(tx_buffer, NAV_FRAME_SIZE) != 0u)
    {
        return 0;
    }
    return 1;
}

void autonomy_init(void)
{
    memset(&auto_cmd, 0, sizeof(auto_cmd));
    memset(&parser, 0, sizeof(parser));
    has_command = 0u;
    rx_head = rx_tail = 0u;
    usb_rx = usb_init((usb_init_config_t){.tx_cbk = NULL, .rx_cbk = autonomy_usb_receive});
    gimbal_feed_sub = msg_sub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
}

void autonomy_task(void)
{
    autonomy_rx_slot_t slot;
    for (unsigned count = 0; count < 7; ++count)
    {
        uint32_t saved = kernel_enter_critical();
        if (rx_tail == rx_head)
        {
            kernel_exit_critical(saved);
            break;
        }
        slot = rx_slots[rx_tail];
        rx_tail = (uint8_t)((rx_tail + 1u) % 8u);
        kernel_exit_critical(saved);
        for (unsigned i = 0; i < slot.length; ++i)
        {
            nav_frame_t frame;
            if (!nav_protocol_feed(&parser, slot.bytes[i], &frame))
                continue;
            if (frame.type == NAV_MSG_SYNC_REQUEST)
            {
                nav_frame_t response = {.type = NAV_MSG_SYNC_RESPONSE, .sequence = frame.sequence,
                                        .sample_us = (uint64_t)kernel_tick_ms() * 1000u};
                (void)autonomy_send(&response);
            }
            else if (nav_protocol_is_command_valid(&frame))
            {
                uint32_t now = kernel_tick_ms();
                if (now - slot.received_ms > SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS)
                    continue;
                /* 超时后允许新主机重启序号;有效会话只接受前进的序号(含 uint32 回绕)。 */
                uint32_t delta = frame.sequence - auto_sequence;
                if (has_command && now - auto_last_ms <= SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS &&
                    (delta == 0u || delta >= 0x80000000u))
                    continue;
                auto_sequence = frame.sequence;
                auto_last_ms = slot.received_ms;
                has_command = 1u;
                auto_cmd.vx = frame.value[0] * 1000.0f;
                auto_cmd.vy = -frame.value[1] * 1000.0f; /* ROS 左正 -> 电控右正 */
                auto_cmd.wz = frame.value[2] * 57.295779513f;
                auto_cmd.valid = (frame.flags & NAV_FLAG_VALID) != 0u;
            }
        }
    }
    if (kernel_tick_ms() - auto_last_ms > SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS)
        auto_cmd.valid = 0u;

    msg_recv(gimbal_feed_sub, &gimbal_feed);
    uint32_t now = kernel_tick_ms();
    if (now - feedback_last_ms >= 20u && gimbal_feed.big_yaw_sample_us != feedback_last_sample)
    {
        nav_frame_t feedback = {.type = NAV_MSG_FEEDBACK, .sequence = feedback_sequence,
                               .sample_us = gimbal_feed.big_yaw_sample_us,
                               .value = {gimbal_feed.big_yaw_rel_deg * 0.01745329252f, 0.0f, 0.0f},
                               .flags = gimbal_feed.big_yaw_online ? NAV_FLAG_VALID : 0u};
        if (autonomy_send(&feedback))
        {
            feedback_last_ms = now;
            feedback_last_sample = gimbal_feed.big_yaw_sample_us;
            ++feedback_sequence;
        }
    }
}

int autonomy_get_command(sentry_auto_cmd_t *command)
{
    *command = auto_cmd;
    command->valid = SENTRY_AUTONOMY_ENABLE && auto_cmd.valid && has_command &&
                     kernel_tick_ms() - auto_last_ms <= SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS;
    return command->valid;
}
