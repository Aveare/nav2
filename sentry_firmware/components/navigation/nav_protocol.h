/**
 * @file    nav_protocol.h
 * @brief   哨兵 USB 导航帧编解码,平台无关
 * @layer   COMPONENT
 * @author  小猫大王
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define SENTRY_NAV_PROTOCOL_FRAME_SIZE 32u
#define SENTRY_NAV_PROTOCOL_MSG_COMMAND 1u
#define SENTRY_NAV_PROTOCOL_MSG_FEEDBACK 2u
#define SENTRY_NAV_PROTOCOL_MSG_SYNC_REQUEST 3u
#define SENTRY_NAV_PROTOCOL_MSG_SYNC_RESPONSE 4u
#define SENTRY_NAV_PROTOCOL_FLAG_VALID 1u

typedef struct
{
    uint8_t type;
    uint32_t sequence;
    uint64_t sample_us;
    float value[3]; /* COMMAND: ROS base_link vx,vy,wz (m/s,m/s,rad/s). */
    uint16_t flags;
} nav_frame_t;

typedef struct
{
    uint8_t bytes[SENTRY_NAV_PROTOCOL_FRAME_SIZE];
    size_t used;
} nav_parser_t;

/** 编码 32 字节小端帧;CRC16/MODBUS 覆盖前 30 字节,无阻塞。 */
void nav_protocol_encode(const nav_frame_t *frame,
                         uint8_t output[SENTRY_NAV_PROTOCOL_FRAME_SIZE]);

/** 逐字节输入;校验成功返回 1,否则返回 0 并滑动重同步。仅单个消费者调用。 */
int nav_protocol_feed(nav_parser_t *parser, uint8_t byte, nav_frame_t *frame);

/** 校验命令类型、标志、有限值及速度边界;VALID=0 表示停止帧。 */
int nav_protocol_is_command_valid(const nav_frame_t *frame, float max_speed_m_s,
                                  float max_wz_rad_s);
