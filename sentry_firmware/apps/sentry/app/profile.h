/**
 * @file    profile.h
 * @brief   哨兵装配锚点:兵种标识与板型守卫
 * @layer   APP
 * @author  小猫大王
* @note    自动运行决策/导航接口在 apps/sentry/autonomy/ 预留;参数入口为
*          `config/sentry_2026/config.h`(伞头 + config_<域>.h)。
* @note    sim 编译矩阵由 `tests/run_profile_tests.ps1` 验证(方案 G-B"三骨架编译")。
* @note    多机通信按 2027 手册禁止,本骨架不预留机间链路。
 */
#pragma once

#include "config.h" /* 板型/链路开关与参数唯一来源 */

/* 兵种标识(遥测/日志/调试工具契约用;三骨架互斥) */
#define PROFILE_ROBOT_SENTRY 1

/* 自动运行能力开关(B 类;骨架代码实装前仅作编译期声明) */
#define PROFILE_AUTO_OPERATION 1

/* 板型守卫:三值编译期选择;双板哨兵代码未实装(B11 仅单板锚点) */
#if !defined(ONE_BOARD) && !defined(CHASSIS_BOARD) && !defined(GIMBAL_BOARD)
#error "哨兵骨架需显式板型(ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD)"
#endif
#if defined(CHASSIS_BOARD) || defined(GIMBAL_BOARD)
#error "双板哨兵代码未实装(B11 仅参数锚点)"
#endif
