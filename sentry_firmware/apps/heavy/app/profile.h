/**
 * @file    profile.h
 * @brief   重装装配锚点:兵种标识与板型守卫
 * @layer   APP
 * @author  小猫大王
* @note    **B11 只落参数锚点**:骨架代码与参数伞头(`config/robot_heavy_2026.h`)
*          待 M5 赛季实装;本文件是编译期层唯一插点,`#if` 只允许出现在 profile 内。
* @note    sim 编译矩阵由 `tests/run_profile_tests.ps1` 验证(方案 G-B"三骨架编译")。
* @note    参数一律不得写进本文件;将来唯一来源是 config 伞头(A 类账本口径)。
 */
#pragma once

#include "config.h" /* 板型/链路开关与参数唯一来源 */

/* 兵种标识(遥测/日志/调试工具契约用;三骨架互斥) */
#define PROFILE_ROBOT_HEAVY 1

/* 42mm 发射系统能力开关(B 类;骨架代码实装前仅作编译期声明) */
#define PROFILE_HAS_42MM_SHOOT 1

/* 板型守卫:三值编译期选择;双板重装代码未实装(B11 仅单板锚点) */
#if !defined(ONE_BOARD) && !defined(CHASSIS_BOARD) && !defined(GIMBAL_BOARD)
#error "重装骨架需显式板型(ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD)"
#endif
#if defined(CHASSIS_BOARD) || defined(GIMBAL_BOARD)
#error "双板重装代码未实装(B11 仅参数锚点)"
#endif
