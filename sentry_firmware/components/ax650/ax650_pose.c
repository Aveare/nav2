/**
 * @file    ax650_pose.c
 * @brief   RMCV 姿态帧编解码:0x1021/0x1022 打包与 0x0503 解析
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    不发送结构体，避免对齐填充与主机字节序改变协议布局。
 */
#include "ax650_pose.h"
#include <float.h>
#include <math.h>
#include <string.h>

#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error "AX650 protocol requires IEEE754 binary32 float"
#endif
_Static_assert(sizeof(float) == 4, "AX650 float size must be 4 bytes");

#define AX650_SOF                  0xA5u
#define AX650_HEADER_SIZE          5u
#define AX650_FRAME_OVERHEAD       9u
#define AX650_CONTROL_CMD          0x0503u
#define AX650_CONTROL_PAYLOAD_SIZE 30u

static int ax650_pose_float_is_finite(float value);

uint8_t ax650_pose_crc8(const uint8_t *data, size_t count)
{
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < count; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit)
            crc = (uint8_t)((crc >> 1) ^ ((crc & 1u) ? 0x8Cu : 0u));
    }
    return crc;
}

uint16_t ax650_pose_crc16(const uint8_t *data, size_t count)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < count; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit)
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0x8408u : 0u));
    }
    return crc;
}

static uint16_t ax650_pose_read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static float ax650_pose_read_float(const uint8_t *data)
{
    uint32_t bits = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                    ((uint32_t)data[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void ax650_pose_parser_consume(ax650_parser_t *parser, size_t count)
{
    parser->used -= count;
    memmove(parser->buffer, parser->buffer + count, parser->used);
}

static int ax650_pose_decode_command(const uint8_t *payload, ax650_command_t *command)
{
    float values[7];
    for (size_t i = 0; i < 7u; ++i)
    {
        values[i] = ax650_pose_read_float(payload + i * sizeof(float));
        if (!ax650_pose_float_is_finite(values[i]))
            return 0;
    }
    if (payload[28] > 3u || payload[29] > 9u)
        return 0;
    command->yaw = values[0];
    command->pitch = values[1];
    command->yaw_speed = values[2];
    command->pitch_speed = values[3];
    command->yaw_acc = values[4];
    command->pitch_acc = values[5];
    command->distance = values[6];
    command->shoot = payload[28];
    command->target_id = payload[29];
    return 1;
}

static void ax650_pose_parse_buffered(ax650_parser_t *parser, Ax650CommandHandler handler, void *context)
{
    while (parser->used != 0u)
    {
        if (parser->buffer[0] != AX650_SOF)
        {
            ax650_pose_parser_consume(parser, 1u);
            continue;
        }
        if (parser->used < AX650_HEADER_SIZE)
            return;
        if (ax650_pose_crc8(parser->buffer, 4u) != parser->buffer[4])
        {
            ax650_pose_parser_consume(parser, 1u);
            continue;
        }
        const size_t payload_size = ax650_pose_read_u16(parser->buffer + 1u);
        if (payload_size > AX650_RX_BUFFER_SIZE - AX650_FRAME_OVERHEAD)
        {
            ax650_pose_parser_consume(parser, 1u);
            continue;
        }
        const size_t frame_size = payload_size + AX650_FRAME_OVERHEAD;
        if (parser->used < frame_size)
            return;
        if (ax650_pose_crc16(parser->buffer, frame_size - 2u) !=
            ax650_pose_read_u16(parser->buffer + frame_size - 2u))
        {
            ax650_pose_parser_consume(parser, 1u);
            continue;
        }
        const uint16_t command_id = ax650_pose_read_u16(parser->buffer + AX650_HEADER_SIZE);
        if (command_id == AX650_CONTROL_CMD && payload_size == AX650_CONTROL_PAYLOAD_SIZE)
        {
            ax650_command_t command;
            if (ax650_pose_decode_command(parser->buffer + AX650_HEADER_SIZE + 2u, &command) && handler != NULL)
                handler(context, &command);
        }
        ax650_pose_parser_consume(parser, frame_size);
    }
}

void ax650_parser_init(ax650_parser_t *parser)
{
    if (parser != NULL)
        memset(parser, 0, sizeof(*parser));
}

void ax650_parser_feed(ax650_parser_t *parser, const uint8_t *data, size_t count,
                     Ax650CommandHandler handler, void *context)
{
    if (parser == NULL || (data == NULL && count != 0u))
        return;
    for (size_t i = 0; i < count; ++i)
    {
        if (parser->used == AX650_RX_BUFFER_SIZE)
            ax650_pose_parser_consume(parser, 1u);
        parser->buffer[parser->used++] = data[i];
        ax650_pose_parse_buffered(parser, handler, context);
    }
}

static int ax650_pose_float_is_finite(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    /* 注意:工程可能使用 -Ofast，直接检查指数位，避免 isfinite 被优化掉。 */
    return (bits & 0x7F800000u) != 0x7F800000u;
}

static void ax650_pose_write_float(uint8_t *data, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    /* 正负零统一，便于核对零姿态测试帧。
       注意：不能写成 `if (value == 0.0f) value = 0.0f;` —— 工程按 -Ofast 编译，
       快速数学假定无符号零，该归一化会被优化掉，零姿态帧会写出 0x80000000(-0.0)
       导致逐字节比较的金标准帧与 CRC 全部错位(主机 -Ofast 下实测复现)。 */
    if (bits == 0x80000000u)
        bits = 0u;
    data[0] = (uint8_t)bits;
    data[1] = (uint8_t)(bits >> 8);
    data[2] = (uint8_t)(bits >> 16);
    data[3] = (uint8_t)(bits >> 24);
}

static void ax650_pose_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void ax650_pose_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static int ax650_pose_normalize_quaternion(const float input[4], float output[4])
{
    float norm_squared = 0.0f;
    for (size_t i = 0; i < 4u; ++i)
    {
        output[i] = input[i];
        if (!ax650_pose_float_is_finite(output[i]))
            return 0;
        norm_squared += output[i] * output[i];
    }
    if (!ax650_pose_float_is_finite(norm_squared) || norm_squared < 1e-12f)
        return 0;
    const float inverse_norm = 1.0f / sqrtf(norm_squared);
    for (size_t i = 0; i < 4u; ++i)
        output[i] *= inverse_norm;
    return 1;
}

size_t ax650_pose_pack(uint8_t *out, size_t capacity, const float q_wxyz[4], float speed_mps,
                      uint8_t mode, uint8_t sequence, uint32_t sample_time_ms, uint8_t robot_id)
{
    if (out == NULL || q_wxyz == NULL || capacity < AX650_POSE_FRAME_SIZE ||
        !ax650_pose_float_is_finite(speed_mps) || speed_mps < 0.0f)
        return 0u;
    float q[4];
    if (!ax650_pose_normalize_quaternion(q_wxyz, q))
        return 0u;
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    const float r01 = 2.0f * (x * y - w * z);
    const float r11 = 1.0f - 2.0f * (x * x + z * z);
    const float r20 = 2.0f * (x * z - w * y);
    float r21 = 2.0f * (y * z + w * x);
    const float r22 = 1.0f - 2.0f * (x * x + y * y);
    if (r21 > 1.0f)
        r21 = 1.0f;
    if (r21 < -1.0f)
        r21 = -1.0f;
    /* ZXY pitch 接近 ±90 deg 时 yaw/roll 不唯一，不用伪造的零姿态替代。 */
    if (1.0f - r21 * r21 < 1e-6f)
        return 0u;
    const float radians_to_degrees = 57.29577951308232f; // [换算] rad → deg
    const float yaw = atan2f(-r01, r11) * radians_to_degrees;
    const float pitch = asinf(r21) * radians_to_degrees;
    const float roll = atan2f(-r20, r22) * radians_to_degrees;

    memset(out, 0, AX650_POSE_FRAME_SIZE);
    out[0] = 0xA5u;
    out[1] = 22u; // [协议] payload 字节数，不包含命令号和 CRC
    out[3] = sequence;
    out[4] = ax650_pose_crc8(out, 4u);
    out[5] = 0x21u; // [协议] 命令号 0x1021，小端序
    out[6] = 0x10u;
    ax650_pose_write_float(out + 7u, yaw);
    ax650_pose_write_float(out + 11u, pitch);
    ax650_pose_write_float(out + 15u, roll);
    ax650_pose_write_float(out + 19u, speed_mps);
    out[23] = mode;
    ax650_pose_write_u32(out + 24u, sample_time_ms);
    out[28] = robot_id;
    const uint16_t crc = ax650_pose_crc16(out, 29u);
    out[29] = (uint8_t)crc;
    out[30] = (uint8_t)(crc >> 8);
    return AX650_POSE_FRAME_SIZE;
}

size_t ax650_full_pose_pack(uint8_t *out, size_t capacity, const ax650_full_pose_t *pose,
                           uint8_t sequence)
{
    if (out == NULL || pose == NULL || capacity < AX650_FULL_POSE_FRAME_SIZE ||
        !ax650_pose_float_is_finite(pose->big_yaw_rel_rad) ||
        !ax650_pose_float_is_finite(pose->small_yaw_rad) ||
        !ax650_pose_float_is_finite(pose->pitch_rad) ||
        !ax650_pose_float_is_finite(pose->projectile_speed_mps) ||
        pose->projectile_speed_mps < 0.0f || (pose->valid_flags & 0xFC00u) != 0u)
        return 0u;

    float q[4];
    if (!ax650_pose_normalize_quaternion(pose->q_wxyz, q))
        return 0u;

    memset(out, 0, AX650_FULL_POSE_FRAME_SIZE);
    out[0] = AX650_SOF;
    ax650_pose_write_u16(out + 1u, 52u);
    out[3] = sequence;
    out[4] = ax650_pose_crc8(out, 4u);
    ax650_pose_write_u16(out + 5u, 0x1022u);
    for (size_t i = 0; i < 4u; ++i)
        ax650_pose_write_float(out + 7u + i * 4u, q[i]);
    ax650_pose_write_float(out + 23u, pose->big_yaw_rel_rad);
    ax650_pose_write_float(out + 27u, pose->small_yaw_rad);
    ax650_pose_write_float(out + 31u, pose->pitch_rad);
    ax650_pose_write_float(out + 35u, pose->projectile_speed_mps);
    ax650_pose_write_u32(out + 39u, pose->imu_sample_time_ms);
    ax650_pose_write_u32(out + 43u, pose->big_yaw_sample_time_ms);
    ax650_pose_write_u32(out + 47u, pose->small_yaw_sample_time_ms);
    ax650_pose_write_u32(out + 51u, pose->pitch_sample_time_ms);
    ax650_pose_write_u16(out + 55u, pose->valid_flags);
    out[57] = pose->program_mode;
    out[58] = pose->robot_id;
    const uint16_t crc = ax650_pose_crc16(out, 59u);
    ax650_pose_write_u16(out + 59u, crc);
    return AX650_FULL_POSE_FRAME_SIZE;
}
