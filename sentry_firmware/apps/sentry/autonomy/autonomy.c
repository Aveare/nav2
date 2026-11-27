/**
 * @file    autonomy.c
 * @brief   哨兵 USB 底盘导航输入与带采样时间的大 yaw 回传
 * @layer   APP
 * @author  小猫大王
 * @note    USB ISR 只复制数据;200Hz 应用任务解析,不产生云台/发射命令。
 */
#include "autonomy.h"
#include "ax650_usb.h"
#include "config.h"
#include "kernel.h"
#include "msg.h"
#include "nav_protocol.h"
#include "sentry_def.h"
#include "usb_port.h"

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
static uint8_t tx_buffer[64u]; // [协议] USB FS 单包，容纳 61 B 的 RMCV 0x1022
static msg_sub_t gimbal_feed_sub;
static sentry_gimbal_feed_t gimbal_feed;
static uint32_t feedback_last_ms, feedback_sequence;
static uint64_t feedback_last_sample;

_Static_assert(AX650_FULL_POSE_FRAME_SIZE <= sizeof(tx_buffer),
               "0x1022 must fit in one USB FS packet");
_Static_assert(SENTRY_NAV_PROTOCOL_FRAME_SIZE <= sizeof(tx_buffer),
               "navigation frame exceeds shared USB TX buffer");

static void autonomy_usb_receive(uint16_t length)
{
    const uint8_t next = (uint8_t)((rx_head + 1u) % 8u);
    if (length > 64u || next == rx_tail || usb_rx == NULL)
        return;
    memcpy(rx_slots[rx_head].bytes, usb_rx, length);
    rx_slots[rx_head].length = length;
    rx_slots[rx_head].received_ms = kernel_tick_ms();
    rx_head = next;
}

static int autonomy_send(const nav_frame_t *frame)
{
    if (usb_is_transmitting())
        return 0;
    nav_protocol_encode(frame, tx_buffer);
    return usb_try_transmit(tx_buffer, SENTRY_NAV_PROTOCOL_FRAME_SIZE) == 0u;
}

void autonomy_init(void)
{
    memset(&auto_cmd, 0, sizeof(auto_cmd));
    memset(&parser, 0, sizeof(parser));
    memset(&gimbal_feed, 0, sizeof(gimbal_feed));
    has_command = 0u;
    rx_head = rx_tail = 0u;
    ax650_usb_init();
    usb_rx = usb_init((usb_init_config_t){.tx_cbk = NULL, .rx_cbk = autonomy_usb_receive});
    gimbal_feed_sub = msg_sub("gimbal_feed", sizeof(sentry_gimbal_feed_t));
}

void autonomy_task(void)
{
    autonomy_rx_slot_t slot;
    for (unsigned count = 0; count < 7u; ++count)
    {
        const uint32_t saved = kernel_enter_critical();
        if (rx_tail == rx_head)
        {
            kernel_exit_critical(saved);
            break;
        }
        slot = rx_slots[rx_tail];
        rx_tail = (uint8_t)((rx_tail + 1u) % 8u);
        kernel_exit_critical(saved);

#if AX650_USB_ENABLE
        ax650_usb_feed(slot.bytes, slot.length, slot.received_ms);
#endif
        for (unsigned i = 0; i < slot.length; ++i)
        {
            nav_frame_t frame;
            if (!nav_protocol_feed(&parser, slot.bytes[i], &frame))
                continue;

            if (frame.type == SENTRY_NAV_PROTOCOL_MSG_SYNC_REQUEST)
            {
                const nav_frame_t response = {
                    .type = SENTRY_NAV_PROTOCOL_MSG_SYNC_RESPONSE,
                    .sequence = frame.sequence,
                    .sample_us = (uint64_t)kernel_tick_ms() * 1000u,
                };
                (void)autonomy_send(&response);
            }
            else if (nav_protocol_is_command_valid(&frame, SENTRY_NAVIGATION_MAX_SPEED_M_S,
                                                    SENTRY_NAVIGATION_MAX_WZ_RAD_S))
            {
                const uint32_t now = kernel_tick_ms();
                if (now - slot.received_ms > SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS)
                    continue;

                /* 超时后允许新主机重启序号;有效会话只接受前进的序号(含 uint32 回绕)。 */
                const uint32_t delta = frame.sequence - auto_sequence;
                if (has_command && now - auto_last_ms <= SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS &&
                    (delta == 0u || delta >= 0x80000000u))
                    continue;

                auto_sequence = frame.sequence;
                auto_last_ms = slot.received_ms;
                has_command = 1u;
                auto_cmd.valid = (frame.flags & SENTRY_NAV_PROTOCOL_FLAG_VALID) != 0u;
                auto_cmd.vx = auto_cmd.valid ? frame.value[0] * 1000.0f : 0.0f;
                auto_cmd.vy = auto_cmd.valid ? -frame.value[1] * 1000.0f : 0.0f;
                auto_cmd.wz = auto_cmd.valid ? frame.value[2] * 57.295779513f : 0.0f;
            }
        }
    }

    if (kernel_tick_ms() - auto_last_ms > SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS)
        auto_cmd.valid = 0u;

    (void)msg_recv(gimbal_feed_sub, &gimbal_feed);
    const uint32_t now = kernel_tick_ms();
    uint16_t pose_flags = AX650_POSE_CALIBRATED_SMALL_YAW;
    if (gimbal_feed.big_yaw_online)
        pose_flags |= AX650_POSE_VALID_BIG_YAW;
    if (gimbal_feed.small_yaw_online)
        pose_flags |= AX650_POSE_VALID_SMALL_YAW;
    if (gimbal_feed.pitch_online)
        pose_flags |= AX650_POSE_VALID_PITCH;
    if (gimbal_feed.big_yaw_online && gimbal_feed.small_yaw_online &&
        gimbal_feed.pitch_online)
        pose_flags |= AX650_POSE_GIMBAL_ONLINE;
    ax650_usb_update_gimbal(
        gimbal_feed.big_yaw_rel_deg * 0.01745329252f,
        gimbal_feed.small_yaw_rel_deg * 0.01745329252f,
        gimbal_feed.pitch_deg * 0.01745329252f,
        (uint32_t)(gimbal_feed.big_yaw_sample_us / 1000u),
        (uint32_t)(gimbal_feed.small_yaw_sample_us / 1000u),
        (uint32_t)(gimbal_feed.pitch_sample_us / 1000u), pose_flags);
    if (now - feedback_last_ms >= SENTRY_NAVIGATION_FEEDBACK_PERIOD_MS &&
        gimbal_feed.big_yaw_sample_us != feedback_last_sample)
    {
        const nav_frame_t feedback = {
            .type = SENTRY_NAV_PROTOCOL_MSG_FEEDBACK,
            .sequence = feedback_sequence,
            .sample_us = gimbal_feed.big_yaw_sample_us,
            .value = {gimbal_feed.big_yaw_rel_deg * 0.01745329252f, 0.0f, 0.0f},
            .flags = gimbal_feed.big_yaw_online ? SENTRY_NAV_PROTOCOL_FLAG_VALID : 0u,
        };
        if (autonomy_send(&feedback))
        {
            feedback_last_ms = now;
            feedback_last_sample = gimbal_feed.big_yaw_sample_us;
            ++feedback_sequence;
        }
    }
#if AX650_USB_ENABLE
    if (!usb_is_transmitting())
    {
        const size_t size = ax650_usb_prepare_pose(tx_buffer, sizeof(tx_buffer), now);
        if (size != 0u)
            (void)usb_try_transmit(tx_buffer, (uint16_t)size);
    }
#endif
}

int autonomy_get_command(sentry_auto_cmd_t *command)
{
#if SENTRY_AUTONOMY_ENABLE
    *command = auto_cmd;
    command->valid = auto_cmd.valid && has_command &&
                     kernel_tick_ms() - auto_last_ms <= SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS;
    return command->valid;
#else
    /* 安全:只读联调固件不得把已解析的 USB 速度泄漏到底盘 PID。 */
    memset(command, 0, sizeof(*command));
    return 0;
#endif
}
