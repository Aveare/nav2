/**
 * @file    config_board.h
 * @brief   板型与链路开关参数
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/robots/infantry/params/调参结论.md。
 */
#pragma once

/* ----------板型(烧录时注意对应功能;修改后需重新编译,只能存在一个!) ---------- */
#define ONE_BOARD // 单板控制整车
// #define CHASSIS_BOARD //底盘板
// #define GIMBAL_BOARD  //云台板

#define VISION_USE_VCP // 旧 Seasky 模块的编译选择；AX650 使用下方独立 USART1 链路

#ifndef CHASSIS_M3508_FORCE_DISABLE
#define CHASSIS_M3508_FORCE_DISABLE 0 // 1=四个底盘3508强制零输出；不影响yaw/pitch及其他电机
#endif
