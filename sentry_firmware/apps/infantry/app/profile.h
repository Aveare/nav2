/**
 * @file    profile.h
 * @brief   步兵装配锚点:config 引用与单板断言
 * @layer   APP
 * @author  小猫大王
* @note    **编译期层**:板型/链路开关在 config 伞头(board.h)与 robot_task.h 的
*          ROBOT_TASK_*_ENABLE 宏(§3.3 铁律:同一件事只能一层决定)。
*          **运行期层**:assembly_t(组件实例/任务频率/总线分配)于 B11 profile 化落地,
*          当前行为由 robot_task.c 常量表 + 各 Init 承载(零行为)。
* @note    单板装配断言:双板拆分随 B11 恢复(本车 2026 单板 ONE_BOARD)。
 */
#pragma once

#include "config.h" /* 本车参数唯一来源(config 伞头) */
#include "robot_def.h"           /* 协议合同(布局守卫) */

#if !defined(ONE_BOARD)
#error "infantry_skeleton 当前装配为单板(ONE_BOARD);双板构件随 B11 profile 化恢复"
#endif

/* 兵种标识(遥测/日志/调试工具契约用;B11 三骨架各自覆写) */
#define PROFILE_ROBOT_INFANTRY 1
