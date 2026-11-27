/**
 * @file    ax650_pose_test.c
 * @brief   AX650 姿态角约定、封包布局与非法输入测试
 * @layer   TESTS
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
    const uint32_t bits = (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
                          ((uint32_t)in[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void PutFloat(uint8_t *out, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    out[0] = (uint8_t)bits;
    out[1] = (uint8_t)(bits >> 8);
    out[2] = (uint8_t)(bits >> 16);
    out[3] = (uint8_t)(bits >> 24);
}

static size_t MakeCommand(uint8_t frame[39], float yaw, float pitch, uint8_t target_id)
{
    memset(frame, 0, 39u);
    frame[0] = 0xA5u;
    frame[1] = 30u;
    frame[4] = ax650_pose_crc8(frame, 4u);
    frame[5] = 0x03u;
    frame[6] = 0x05u;
    PutFloat(frame + 7u, yaw);
    PutFloat(frame + 11u, pitch);
    frame[35] = 1u;
    frame[36] = target_id;
    const uint16_t crc = ax650_pose_crc16(frame, 37u);
    frame[37] = (uint8_t)crc;
    frame[38] = (uint8_t)(crc >> 8);
    return 39u;
}

static unsigned command_count;
static ax650_command_t last_command;
static void ax650_uart1_on_command(void *context, const ax650_command_t *command)
{
    (void)context;
    last_command = *command;
    ++command_count;
}

static void TestCommandParser(void)
{
    ax650_parser_t parser;
    uint8_t frame[39];
    const size_t size = MakeCommand(frame, 37.5f, -8.25f, 4u);
    ax650_parser_init(&parser);
    ax650_parser_feed(&parser, frame, 7u, ax650_uart1_on_command, NULL);
    assert(command_count == 0u);
    ax650_parser_feed(&parser, frame + 7u, size - 7u, ax650_uart1_on_command, NULL);
    assert(command_count == 1u);
    assert(last_command.yaw == 37.5f && last_command.pitch == -8.25f);
    assert(last_command.shoot == 1u && last_command.target_id == 4u);

    uint8_t joined[79];
    joined[0] = 0x55u;
    memcpy(joined + 1u, frame, size);
    memcpy(joined + 1u + size, frame, size);
    ax650_parser_feed(&parser, joined, sizeof(joined), ax650_uart1_on_command, NULL);
    assert(command_count == 3u);
    frame[20] ^= 0x80u;
    ax650_parser_feed(&parser, frame, size, ax650_uart1_on_command, NULL);
    assert(command_count == 3u);
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
    static const uint8_t zero_golden[AX650_POSE_FRAME_SIZE] = {
        0xA5u, 0x16u, 0x00u, 0x00u, 0x58u, 0x21u, 0x10u, 0u, 0u, 0u,
        0u,    0u,    0u,    0u,    0u,    0u,    0u,    0u, 0u, 0u,
        0u,    0u,    0u,    0u,    0xD2u, 0x04u, 0u,    0u, 0x67u, 0xAFu,
        0xA7u};
    float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    uint8_t frame[AX650_POSE_FRAME_SIZE];
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 1234u, 103u) == sizeof(frame));
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
                assert(ax650_pose_pack(frame, sizeof(frame), q, 24.5f, 1u, (uint8_t)rotations,
                                       1000u + rotations, 103u) ==
                       sizeof(frame));
                assert(fabsf(GetFloat(frame + 7u) - (float)yaw) < 0.0002f);
                assert(fabsf(GetFloat(frame + 11u) - (float)pitch) < 0.0002f);
                assert(fabsf(GetFloat(frame + 15u) - (float)roll) < 0.0002f);
                assert(GetFloat(frame + 19u) == 24.5f);
                assert(frame[23] == 1u);
                assert(((uint32_t)frame[24] | ((uint32_t)frame[25] << 8) |
                        ((uint32_t)frame[26] << 16) | ((uint32_t)frame[27] << 24)) ==
                       1000u + rotations);
                assert(frame[3] == (uint8_t)rotations);
                assert(frame[4] == ax650_pose_crc8(frame, 4u));
                assert(frame[28] == 103u);
                assert(((uint16_t)frame[29] | (uint16_t)((uint16_t)frame[30] << 8)) ==
                       ax650_pose_crc16(frame, 29u));
                ++rotations;
            }
    assert(rotations == 225u);
    MakeZxyQuaternion(0.0f, 90.0f, 0.0f, q);
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    MakeZxyQuaternion(0.0f, -90.0f, 0.0f, q);
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    memset(q, 0, sizeof(q));
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    q[0] = NAN;
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    q[0] = INFINITY;
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    q[0] = FLT_MAX;
    assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    q[0] = 1.0f;
    assert(ax650_pose_pack(frame, sizeof(frame), q, NAN, 0u, 0u, 0u, 0u) == 0u);
    assert(ax650_pose_pack(frame, sizeof(frame), q, -1.0f, 0u, 0u, 0u, 0u) == 0u);
    assert(ax650_pose_pack(frame, sizeof(frame) - 1u, q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    assert(ax650_pose_pack(NULL, sizeof(frame), q, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    assert(ax650_pose_pack(frame, sizeof(frame), NULL, 0.0f, 0u, 0u, 0u, 0u) == 0u);
    for (unsigned seq = 254u; seq < 258u; ++seq)
    {
        assert(ax650_pose_pack(frame, sizeof(frame), q, 0.0f, 0u, (uint8_t)seq, seq, 0u) == sizeof(frame));
        assert(frame[3] == (uint8_t)seq);
    }
}

static uint32_t GetU32(const uint8_t *in)
{
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
           ((uint32_t)in[3] << 24);
}

static void TestFullPose(void)
{
    ax650_full_pose_t pose = {
        .q_wxyz = {2.0f, 0.0f, 0.0f, 0.0f},
        .big_yaw_rel_rad = 0.25f,
        .small_yaw_rad = -0.5f,
        .pitch_rad = 0.125f,
        .projectile_speed_mps = 24.5f,
        .imu_sample_time_ms = 100u,
        .big_yaw_sample_time_ms = 101u,
        .small_yaw_sample_time_ms = 102u,
        .pitch_sample_time_ms = 103u,
        .valid_flags = AX650_POSE_VALID_IMU | AX650_POSE_VALID_BIG_YAW |
                       AX650_POSE_VALID_SMALL_YAW | AX650_POSE_VALID_PITCH,
        .program_mode = 1u,
        .robot_id = 107u,
    };
    uint8_t frame[AX650_FULL_POSE_FRAME_SIZE];
    assert(ax650_full_pose_pack(frame, sizeof(frame), &pose, 255u) == sizeof(frame));
    assert(frame[0] == 0xA5u && frame[1] == 52u && frame[2] == 0u);
    assert(frame[3] == 255u && frame[4] == ax650_pose_crc8(frame, 4u));
    assert(frame[5] == 0x22u && frame[6] == 0x10u);
    assert(GetFloat(frame + 7u) == 1.0f);
    assert(GetFloat(frame + 23u) == 0.25f);
    assert(GetFloat(frame + 27u) == -0.5f);
    assert(GetFloat(frame + 31u) == 0.125f);
    assert(GetFloat(frame + 35u) == 24.5f);
    assert(GetU32(frame + 39u) == 100u && GetU32(frame + 51u) == 103u);
    assert(frame[55] == 0x0Fu && frame[56] == 0u);
    assert(frame[57] == 1u && frame[58] == 107u);
    assert(((uint16_t)frame[59] | ((uint16_t)frame[60] << 8)) ==
           ax650_pose_crc16(frame, 59u));

    pose.valid_flags = 0x8000u;
    assert(ax650_full_pose_pack(frame, sizeof(frame), &pose, 0u) == 0u);
    pose.valid_flags = AX650_POSE_VALID_IMU;
    pose.small_yaw_rad = NAN;
    assert(ax650_full_pose_pack(frame, sizeof(frame), &pose, 0u) == 0u);
    assert(ax650_full_pose_pack(frame, sizeof(frame) - 1u, &pose, 0u) == 0u);
}

int main(void)
{
    TestPose();
    TestFullPose();
    TestCommandParser();
    puts("PASS: pose TX plus fragmented/sticky/corrupt AX650 command RX");
    return 0;
}
