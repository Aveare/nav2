/**
 * @file    robot_cmd.h
 * @brief   哨兵 cmd 应用接口:初始化/任务/档位查询
 * @layer   APP
 * @author  小猫大王
 */
#ifndef SENTRY_ROBOT_CMD_H
#define SENTRY_ROBOT_CMD_H

#include <stdint.h>

/**
 * @brief 控制命令应用初始化:注册遥控/图传输入,订阅三路反馈
 * @note 须在 RTOS 启动前调用,由 assembly 统一调度;只能调用一次。
 */
void cmd_init(void);

/**
 * @brief 控制命令应用任务:仲裁唯一控制源,生成底盘/三轴云台/发射指令
 * @note 由 robot_task() 以 200Hz 调用;内部持有仲裁与回中状态,不可并发调用。
 */
void cmd_task(void);

/** @brief 最近一帧有效 0x0503 的原始 yaw 目标,deg */
float cmd_get_nuc_raw_yaw(void);

/** @brief 当前战术射速档位(0=ECO 1=NORMAL 2=BURST) */
uint8_t cmd_get_fire_tier(void);

/** @brief 自瞄是否选中(左中挡或键鼠 Shift;不含目标有效性) */
uint8_t cmd_get_auto_aim_active(void);

#endif // SENTRY_ROBOT_CMD_H
