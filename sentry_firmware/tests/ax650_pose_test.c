/**
 * @file    ax650_pose_test.c
 * @brief   AX650 姿态角约定、封包布局与非法输入测试
 * @layer   MODULE
 * @note    只操作内存；固定测试帧来自已核对的 RMCVSerial 布局。
 */
#include "ax650_pose.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float GetFloat(const uint8_t *in)
{
    const uint32_t bits = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                          ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void QuaternionMultiply(const float a[4], const float b[4], float out[4])
{
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

static void MakeZxyQuaternion(float yaw, float pitch, float roll, float out[4])
{
    const float to_half_radians = 0.008726646259971648f;
    const float az = yaw * to_half_radians;
    const float ax = pitch * to_half_radians;
    const float ay = roll * to_half_radians;
    const float qz[4] = {cosf(az), 0.0f, 0.0f, sinf(az)};
    const float qx[4] = {cosf(ax), sinf(ax), 0.0f, 0.0f};
    const float qy[4] = {cosf(ay), 0.0f, sinf(ay), 0.0f};
    float temp[4];
    QuaternionMultiply(qz, qx, temp);
    QuaternionMultiply(temp, qy, out);
}

static void TestPose(void)
{
    static const uint8_t zero_golden[26] = {
        0xA5u, 0x11u, 0x00u, 0x00u, 0x22u, 0x21u, 0x10u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0xB6u, 0xD2u};
    float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    uint8_t frame[26];
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == sizeof(frame));
    assert(memcmp(frame, zero_golden, sizeof(frame)) == 0);

    unsigned rotations = 0;
    for (int yaw = -160; yaw <= 160; yaw += 40)
        for (int pitch = -70; pitch <= 70; pitch += 35)
            for (int roll = -140; roll <= 140; roll += 70)
            {
                MakeZxyQuaternion((float)yaw, (float)pitch, (float)roll, q);
                /* 四元数符号和归一化不应改变输出姿态。 */
                const float scale = (rotations & 1u) ? -3.0f : 2.0f;
                for (unsigned j = 0; j < 4u; ++j)
                    q[j] *= scale;
                assert(Ax650PosePack(frame, sizeof(frame), q, 24.5f, 1u,
                                      (uint8_t)rotations) == sizeof(frame));
                assert(fabsf(GetFloat(frame + 7u) - (float)yaw) < 0.0002f);
                assert(fabsf(GetFloat(frame + 11u) - (float)pitch) < 0.0002f);
                assert(fabsf(GetFloat(frame + 15u) - (float)roll) < 0.0002f);
                assert(GetFloat(frame + 19u) == 24.5f);
                assert(frame[23] == 1u);
                assert(frame[3] == (uint8_t)rotations);
                assert(frame[4] == Ax650PoseCrc8(frame, 4u));
                assert(((uint16_t)frame[24] | (uint16_t)((uint16_t)frame[25] << 8)) ==
                       Ax650PoseCrc16(frame, 24u));
                ++rotations;
            }
    assert(rotations == 225u);
    MakeZxyQuaternion(0.0f, 90.0f, 0.0f, q);
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    MakeZxyQuaternion(0.0f, -90.0f, 0.0f, q);
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    memset(q, 0, sizeof(q));
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    q[0] = NAN;
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    q[0] = INFINITY;
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    q[0] = FLT_MAX;
    assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    q[0] = 1.0f;
    assert(Ax650PosePack(frame, sizeof(frame), q, NAN, 0u, 0u) == 0u);
    assert(Ax650PosePack(frame, sizeof(frame), q, -1.0f, 0u, 0u) == 0u);
    assert(Ax650PosePack(frame, sizeof(frame) - 1u, q, 0.0f, 0u, 0u) == 0u);
    assert(Ax650PosePack(NULL, sizeof(frame), q, 0.0f, 0u, 0u) == 0u);
    assert(Ax650PosePack(frame, sizeof(frame), NULL, 0.0f, 0u, 0u) == 0u);
    for (unsigned seq = 254u; seq < 258u; ++seq)
    {
        assert(Ax650PosePack(frame, sizeof(frame), q, 0.0f, 0u,
                              (uint8_t)seq) == sizeof(frame));
        assert(frame[3] == (uint8_t)seq);
    }
}

int main(void)
{
    TestPose();
    puts("PASS: golden 26-byte frame, 225 ZXY rotations, normalization/sign, CRC, invalid inputs and sequence wrap");
    return 0;
}
