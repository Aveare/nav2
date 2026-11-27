#pragma once
#include <stdint.h>
typedef struct {
    float big_yaw_rel_deg;
    float small_yaw_rel_deg;
    float pitch_deg;
    uint64_t big_yaw_sample_us;
    uint64_t small_yaw_sample_us;
    uint64_t pitch_sample_us;
    uint8_t big_yaw_online;
    uint8_t small_yaw_online;
    uint8_t pitch_online;
} sentry_gimbal_feed_t;
