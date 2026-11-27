#pragma once
#include <stdint.h>
typedef struct
{
    struct { uint8_t robot_id; } GameRobotState;
    struct { float bullet_speed; } ShootData;
    uint8_t init_flag;
} referee_info_t;
referee_info_t *referee_get_info(void);
