/**
 * @file    app_profile.h
 * @brief   兵种装配契约:通用壳与每兵种 assembly 的唯一接口
 * @layer   APP
 * @author  小猫大王
* @note    共享代码只认识本结构,不认识具体兵种;兵种差异全部收敛到
*          apps/<robot>/app/assembly.c 的一张静态表里。
* @note    不在本文件出现任何参数值;参数仍由 config/<robot>_<year>/config.h 提供。
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 一个兵种的装配表
 * @note  init / run_200hz 允许为 NULL(骨架兵种只跑公共服务时);
 *        ins_init 是姿态任务启动前的一次性钩子(如 AX650 链路),允许为 NULL。
 */
typedef struct
{
    const char *name;       // 兵种名,仅用于日志/遥测标识,不参与控制
    void (*init)(void);     // 应用层初始化:注册电机/订阅话题/加载兵种状态
    void (*run_200hz)(void); // 200Hz 应用任务:cmd → 执行单元的固定顺序
    void (*ins_init)(void); // 1kHz 姿态任务启动前钩子;可为 NULL
    bool calib_enable;      // 是否加载 Flash 零位标定(骨架兵种应为 false)
    bool ui_enable;         // 是否创建裁判 UI 任务(受 ROBOT_TASK_UI_ENABLE 二次门控)
} app_profile_t;

/**
 * @brief 当前编译兵种的装配表;由 apps/<robot>/app/assembly.c 唯一定义
 * @note  链接期必须且只能有一个定义;缺定义会直接链接失败,避免"选了兵种却没装配"。
 */
extern const app_profile_t g_app_profile;
