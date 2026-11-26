/**
 * @file    assembly.c
 * @brief   步兵兵种装配表:cmd/gimbal/shoot/chassis 接线与 AX650 钩子
 * @layer   APP
 * @author  小猫大王
* @note    共享装配壳只读取 g_app_profile;本文件是步兵差异的唯一落点。
*          板型仍然是编译期三值(ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD),
*          因此这里保留板型裁剪,但不出现任何"哨兵/重装"条件。
 */
#include "app_profile.h"
#include "profile.h"
#include "robot_cmd.h"
#include "chassis.h"
#include "gimbal.h"
#include "shoot.h"
#include "ax650_uart1.h"
#include "heat_guard.h"
#include "referee_app_status.h"
#include "referee_task.h"
#include "log_port.h"
#include "usart.h"

static bool infantry_referee_status(referee_app_status_t *out);

static void infantry_init(void)
{
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
    cmd_init();
    gimbal_init();
    shoot_init();
    /* 裁判 UI 的热量/档位数据由本兵种提供(组件不反向依赖 apps,T01) */
    referee_app_status_register(infantry_referee_status);
#endif
#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    chassis_init();
#endif
}

static void infantry_task(void)
{
#if defined(ONE_BOARD) || defined(GIMBAL_BOARD)
#if AX650_UART1_ENABLE
    /* 裁判 0x0207 未到达时发送 0，由 AX650 使用 24 m/s 首发默认值。 */
    const referee_info_t *referee = referee_get_info();
    ax650_uart1_set_projectile_speed(referee != NULL ? referee->ShootData.bullet_speed : 0.0f);
    ax650_uart1_set_robot_id(referee != NULL ? referee->GameRobotState.robot_id : 0u);
#endif
    cmd_task();
    gimbal_task();
    shoot_task();
#endif
#if defined(ONE_BOARD) || defined(CHASSIS_BOARD)
    chassis_task();
#endif
}

static void infantry_ins_init(void)
{
#if AX650_UART1_ENABLE
    if (!ax650_uart1_init(&huart1))
        LOG_ERROR("[ax650] USART1 init failed; retrying with INS updates");
#endif
}

/**
 * @brief  把本兵种的热量守卫/战术档位翻译成裁判 UI 的中立状态
 * @note   枚举值 0-3 与 heat_guard_state_e 逐项对应(见两处头文件注释);
 *         若后续 app 枚举重排,这里必须同步,编译器不会提示。
 */
static bool infantry_referee_status(referee_app_status_t *out)
{
    float heat, limit, cool;
    heat_guard_get_snapshot(&heat, &limit, &cool);
    out->heat = heat;
    out->heat_limit = limit;
    out->cool_per_s = cool;
    out->fire_tier = (uint8_t)cmd_get_fire_tier();
    out->heat_state = (uint8_t)heat_guard_get_state();
    out->auto_aim = cmd_get_auto_aim_active();
    out->spin_mode = cmd_get_spin_mode();
    out->friction_on = cmd_get_friction_on();
    out->load_mode = cmd_get_fire_mode();
    out->loader_on = cmd_get_loader_on();
    out->fire_rate = cmd_get_fire_rate();
    return true;
}

const app_profile_t g_app_profile = {
    .name = "infantry",
    .init = infantry_init,
    .run_200hz = infantry_task,
    .ins_init = infantry_ins_init,
    .calib_enable = true,
    .ui_enable = true,
};
