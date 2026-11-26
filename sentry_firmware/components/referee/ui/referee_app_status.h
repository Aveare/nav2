/**
 * @file    referee_app_status.h
 * @brief   裁判 UI 中立状态契约(解 apps 反向依赖)
 * @layer   COMPONENT
 * @author  小猫大王
* @note    背景:裁判 UI 需要热量守卫/战术档位,但这些属于 app 层。此前
*          referee_task.c 直接 include heat_guard.h/robot_cmd.h,形成
*          components → apps 反向依赖(T01),heavy/sentry 无法编译。
*          现在改为"app 注册提供者,referee 只读中立结构"。
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** 热量守卫状态(与 app 的 heat_guard_state_e 数值解耦,由提供者映射) */
typedef enum
{
    REFEREE_HEAT_STATE_OFF = 0,
    REFEREE_HEAT_STATE_NORMAL = 1,
    REFEREE_HEAT_STATE_SINGLE = 2,
    REFEREE_HEAT_STATE_HOLD = 3,
} referee_heat_state_e;

/** 小陀螺显示状态(数值中立,由 app 按编译开关/模式映射) */
typedef enum
{
    REFEREE_SPIN_OFF = 0,   // 未进小陀螺
    REFEREE_SPIN_CONST = 1, // 小陀螺,匀速
    REFEREE_SPIN_VARY = 2,  // 小陀螺,变速轨迹
} referee_spin_mode_e;

/** 发射档位显示状态(0=空闲 1=单发 2=三发 3=连发) */
typedef enum
{
    REFEREE_LOAD_OFF = 0,
    REFEREE_LOAD_SINGLE = 1,
    REFEREE_LOAD_BURST3 = 2,
    REFEREE_LOAD_AUTO = 3,
} referee_load_mode_e;

/** 裁判 UI 所需的只读应用状态 */
typedef struct
{
    float heat;        // 当前枪口热量,热量点
    float heat_limit;  // 热量上限,热量点
    float cool_per_s;  // 冷却速率,热量点/s
    uint8_t fire_tier; // 战术档位:0=ECO 1=NORMAL 2=BURST
    uint8_t heat_state; // referee_heat_state_e
    uint8_t auto_aim;   // 1=自瞄开启(左中挡或键鼠 Shift 锁存)
    uint8_t spin_mode;  // referee_spin_mode_e
    uint8_t friction_on; // 1=摩擦轮开启
    uint8_t load_mode;   // referee_load_mode_e
    uint8_t loader_on;   // 1=拨弹盘正在拨弹(有非 STOP 发射请求)
    float fire_rate;     // 当前档位请求射速,发/s(UI 显示具体数值)
} referee_app_status_t;

/**
 * @brief 注册应用状态提供者
 * @param provider 返回 true 表示 out 已填充;传 NULL 表示注销
 * @note  只在初始化期调用一次;提供者运行在 UI 任务上下文,不得阻塞。
 */
void referee_app_status_register(bool (*provider)(referee_app_status_t *out));

/**
 * @brief 读取应用状态
 * @param out 输出缓冲;未注册提供者时返回 false 且不修改 out
 * @return 有提供者且其返回 true 时为 true
 */
bool referee_app_status_get(referee_app_status_t *out);
