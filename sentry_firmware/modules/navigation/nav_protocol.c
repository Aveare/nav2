/**
 * @file    nav_protocol.c
 * @brief   固定长度导航协议与滑动重同步
 * @layer   COMPONENT
 * @author  小猫大王
 */
#include "nav_protocol.h"
#include "robot_infantry_2026.h"
#include <math.h>
#include <string.h>

_Static_assert(sizeof(float) == 4, "Navigation requires binary32 float");

uint16_t nav_protocol_crc16(const uint8_t *bytes, size_t length)
{
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xa001u : 0u);
    }
    return crc;
}

static void nav_protocol_put(uint8_t *bytes, uint64_t value, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        bytes[i] = (uint8_t)(value >> (8u * i));
}

static uint64_t nav_protocol_get(const uint8_t *bytes, unsigned count)
{
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= (uint64_t)bytes[i] << (8u * i);
    return value;
}

void nav_protocol_encode(const nav_frame_t *frame, uint8_t output[NAV_FRAME_SIZE])
{
    output[0] = 'N';
    output[1] = 'V';
    output[2] = 1u;
    output[3] = frame->type;
    nav_protocol_put(output + 4, frame->sequence, 4);
    nav_protocol_put(output + 8, frame->sample_us, 8);
    for (unsigned i = 0; i < 3; ++i)
    {
        uint32_t bits;
        memcpy(&bits, &frame->value[i], sizeof(bits));
        nav_protocol_put(output + 16 + 4*i, bits, 4);
    }
    nav_protocol_put(output + 28, frame->flags, 2);
    nav_protocol_put(output + 30, nav_protocol_crc16(output, 30), 2);
}

int nav_protocol_feed(nav_parser_t *parser, uint8_t byte, nav_frame_t *frame)
{
    parser->bytes[parser->used++] = byte;
    if (parser->used < NAV_FRAME_SIZE)
        return 0;
    const uint8_t *bytes = parser->bytes;
    if (bytes[0] == 'N' && bytes[1] == 'V' && bytes[2] == 1u &&
        nav_protocol_get(bytes + 30, 2) == nav_protocol_crc16(bytes, 30))
    {
        frame->type = bytes[3];
        frame->sequence = (uint32_t)nav_protocol_get(bytes + 4, 4);
        frame->sample_us = nav_protocol_get(bytes + 8, 8);
        frame->flags = (uint16_t)nav_protocol_get(bytes + 28, 2);
        for (unsigned i = 0; i < 3; ++i)
        {
            uint32_t bits = (uint32_t)nav_protocol_get(bytes + 16 + 4*i, 4);
            memcpy(&frame->value[i], &bits, sizeof(bits));
        }
        parser->used = 0;
        return 1;
    }
    memmove(parser->bytes, parser->bytes + 1, NAV_FRAME_SIZE - 1);
    parser->used = NAV_FRAME_SIZE - 1;
    return 0;
}

static int nav_protocol_is_finite(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    /* Release 使用 -Ofast,不能依赖会被 finite-math 优化移除的 isfinite。 */
    return (bits & 0x7f800000u) != 0x7f800000u;
}

int nav_protocol_is_command_valid(const nav_frame_t *frame)
{
    return frame->type == NAV_MSG_COMMAND && (frame->flags & ~NAV_FLAG_VALID) == 0 &&
           nav_protocol_is_finite(frame->value[0]) && nav_protocol_is_finite(frame->value[1]) && nav_protocol_is_finite(frame->value[2]) &&
           fabsf(frame->value[0]) <= SENTRY_NAVIGATION_MAX_SPEED_M_S + 0.00001f &&
           fabsf(frame->value[1]) <= SENTRY_NAVIGATION_MAX_SPEED_M_S + 0.00001f &&
           fabsf(frame->value[2]) <= SENTRY_NAVIGATION_MAX_WZ_RAD_S + 0.00001f;
}
