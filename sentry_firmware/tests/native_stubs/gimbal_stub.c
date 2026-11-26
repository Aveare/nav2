/**
 * @file    gimbal_stub.c
 * @brief   主机测试替身:云台侧 ECD 回正/零位标定接口
 * @layer   TESTS
 * @note    robot_cmd.c 现在会调用 gimbal.h 的四个接口(回正目标、编码器读取、写 Flash),
 *          但主机测试只链接 CMD 层与仲裁逻辑,不链接 gimbal.c / calib.c,
 *          故在此提供"未标定"行为的替身:回正目标恒 0(= 旧的 IMU 水平回正),
 *          写 Flash 恒失败。测试断言因此仍对应"未标定"分支。
 * @note    2026-11-16 建立,作者 JIT CR。
 */
#include "gimbal.h"

float gimbal_get_pitch_ecd_deg(void)
{
    return 0.0f;
}

float gimbal_get_pitch_total_angle(void)
{
    return 0.0f;
}

float gimbal_pitch_recenter_target(void)
{
    return 0.0f;
}

int8_t gimbal_save_pitch_horizon_zero(void)
{
    return -1;
}
