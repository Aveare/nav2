/**
 * @file    autonomy.h
 * @brief   哨兵导航底盘命令接口,始终经过原控制权仲裁
 * @layer   APP
 * @author  小猫大王
 * @note    本次仅使用底盘速度与 valid,预留云台/开火字段保持零;
 *          最终仍经 cmd 仲裁后才能进入执行单元。
 */
#ifndef AUTONOMY_H
#define AUTONOMY_H

#include <stdint.h>

/** 自主状态(仅用于遥测/日志,导航通信不消费交战状态) */
typedef enum
{
    SENTRY_AUTO_IDLE = 0,
    SENTRY_AUTO_PATROL,
    SENTRY_AUTO_ENGAGE,
    SENTRY_AUTO_RETREAT,
} sentry_auto_state_e;

/** 自主层输出合同(本次只填充底盘速度与 valid) */
typedef struct
{
    float vx;          // mm/s
    float vy;          // mm/s
    float wz;          // deg/s
    float yaw_target;  // 世界 yaw 目标,deg
    float pitch_target; // 世界 pitch 目标,deg
    uint8_t fire;      // 1=请求开火
    uint8_t valid;     // 1=数据有效
    sentry_auto_state_e state;
} sentry_auto_cmd_t;

void autonomy_init(void);
void autonomy_task(void);

/** 200Hz cmd 任务读取最新底盘命令;返回 1 表示有效。单位 mm/s、右正、deg/s。 */
int autonomy_get_command(sentry_auto_cmd_t *command);

#endif // AUTONOMY_H
