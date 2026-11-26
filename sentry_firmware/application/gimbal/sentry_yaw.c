/**
 * @file    sentry_yaw.c
 * @brief   双 yaw 参考计算与雷达平面朝向解算
 * @layer   APP
 */
#include "sentry_yaw.h"

#include <math.h>
#include <stddef.h>

bool SentryYawInitialize(float chassis_heading_deg, float big_yaw_battery_align_deg,
                         SentryYawState *state)
{
    if (state == NULL || !isfinite(chassis_heading_deg) || !isfinite(big_yaw_battery_align_deg))
        return false;

    *state = (SentryYawState){
        .chassis_heading_at_start_deg = chassis_heading_deg,
        .big_yaw_battery_align_deg = big_yaw_battery_align_deg,
        .initialized = true,
    };
    return true;
}

bool SentryYawCalculateReference(float chassis_to_map_deg, const SentryYawState *state,
                                 SentryYawReference *reference)
{
    if (state == NULL || reference == NULL || !state->initialized ||
        !isfinite(chassis_to_map_deg) || !isfinite(state->chassis_heading_at_start_deg) ||
        !isfinite(state->big_yaw_battery_align_deg))
        return false;

    const float chassis_rotation_deg = chassis_to_map_deg - state->chassis_heading_at_start_deg;
    reference->big_to_chassis_deg = state->big_yaw_battery_align_deg - chassis_rotation_deg;
    return true;
}

bool SentryLidarCalculateHeading(const SentryLidarYawSample *sample,
                                SentryLidarHeading *heading)
{
    if (sample == NULL || heading == NULL)
        return false;

    const float lidar_to_map_deg = sample->chassis_to_map_deg + sample->big_to_chassis_deg
                                 + sample->small_to_big_deg + sample->lidar_to_small_deg;
    if (!isfinite(lidar_to_map_deg))
        return false;

    // 只规范化地图朝向，电机参考保留连续角度供接入层做限位。
    float wrapped_deg = remainderf(lidar_to_map_deg, 360.0f);
    if (wrapped_deg == 180.0f)
        wrapped_deg = -180.0f;

    *heading = (SentryLidarHeading){
        .timestamp_us = sample->timestamp_us,
        .lidar_to_map_deg = wrapped_deg,
    };
    return true;
}
