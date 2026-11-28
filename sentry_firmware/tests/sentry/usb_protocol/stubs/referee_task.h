/**
 * @file    referee_task.h
 * @brief   哨兵 USB 双协议主机测试裁判数据桩
 * @layer   TESTS
 * @author  小猫大王
 */
#pragma once

#include <stdint.h>

typedef struct
{
    uint8_t robot_id;
} test_game_robot_state_t;

typedef struct
{
    float bullet_speed;
} test_shoot_data_t;

typedef struct
{
    uint8_t game_progress;
} test_game_state_t;

typedef struct
{
    uint8_t init_flag;
    test_game_robot_state_t GameRobotState;
    test_game_state_t GameState; /* 开局自动寻敌(DEV-20261128-153543)需要 game_progress */
    test_shoot_data_t ShootData;
} referee_info_t;

/* 测试里没有裁判数据:返回 NULL,姿态帧保持配置占位值。 */
static inline referee_info_t *referee_get_info(void)
{
    return 0;
}
