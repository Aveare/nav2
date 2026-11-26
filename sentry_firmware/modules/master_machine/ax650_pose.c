/**
 * @file    ax650_pose.c
 * @brief   四元数转换及 RMCV 姿态帧封装
 * @layer   MODULE
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

uint8_t Ax650PoseCrc8(const uint8_t *data, size_t count)
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

uint16_t Ax650PoseCrc16(const uint8_t *data, size_t count)
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

static int FloatIsFinite(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    /* 注意:工程可能使用 -Ofast，直接检查指数位，避免 isfinite 被优化掉。 */
    return (bits & 0x7F800000u) != 0x7F800000u;
}

static void WriteFloat(uint8_t *data, float value)
{
    uint32_t bits;
    if (value == 0.0f) value = 0.0f; // 正负零统一，便于核对零姿态测试帧
    memcpy(&bits, &value, sizeof(bits));
    data[0] = (uint8_t)bits;
    data[1] = (uint8_t)(bits >> 8);
    data[2] = (uint8_t)(bits >> 16);
    data[3] = (uint8_t)(bits >> 24);
}

size_t Ax650PosePack(uint8_t *out, size_t capacity, const float q_wxyz[4],
                     float speed_mps, uint8_t mode, uint8_t sequence)
{
    if (out == NULL || q_wxyz == NULL || capacity < AX650_POSE_FRAME_SIZE ||
        !FloatIsFinite(speed_mps) || speed_mps < 0.0f) return 0u;
    float q[4];
    float norm_squared = 0.0f;
    for (size_t i = 0; i < 4u; ++i)
    {
        q[i] = q_wxyz[i];
        if (!FloatIsFinite(q[i])) return 0u;
        norm_squared += q[i] * q[i];
    }
    if (!FloatIsFinite(norm_squared) || norm_squared < 1e-12f) return 0u;
    const float inverse_norm = 1.0f / sqrtf(norm_squared);
    for (size_t i = 0; i < 4u; ++i) q[i] *= inverse_norm;
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    const float r01 = 2.0f * (x * y - w * z);
    const float r11 = 1.0f - 2.0f * (x * x + z * z);
    const float r20 = 2.0f * (x * z - w * y);
    float r21 = 2.0f * (y * z + w * x);
    const float r22 = 1.0f - 2.0f * (x * x + y * y);
    if (r21 > 1.0f) r21 = 1.0f;
    if (r21 < -1.0f) r21 = -1.0f;
    /* ZXY pitch 接近 ±90 deg 时 yaw/roll 不唯一，不用伪造的零姿态替代。 */
    if (1.0f - r21 * r21 < 1e-6f) return 0u;
    const float radians_to_degrees = 57.29577951308232f; // [换算] rad → deg
    const float yaw = atan2f(-r01, r11) * radians_to_degrees;
    const float pitch = asinf(r21) * radians_to_degrees;
    const float roll = atan2f(-r20, r22) * radians_to_degrees;

    memset(out, 0, AX650_POSE_FRAME_SIZE);
    out[0] = 0xA5u;
    out[1] = 17u; // [协议] payload 字节数，不包含命令号和 CRC
    out[3] = sequence;
    out[4] = Ax650PoseCrc8(out, 4u);
    out[5] = 0x21u; // [协议] 命令号 0x1021，小端序
    out[6] = 0x10u;
    WriteFloat(out + 7u, yaw);
    WriteFloat(out + 11u, pitch);
    WriteFloat(out + 15u, roll);
    WriteFloat(out + 19u, speed_mps);
    out[23] = mode;
    const uint16_t crc = Ax650PoseCrc16(out, 24u);
    out[24] = (uint8_t)crc;
    out[25] = (uint8_t)(crc >> 8);
    return AX650_POSE_FRAME_SIZE;
}
