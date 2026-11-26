/**
 * @file    assembly.c
 * @brief   哨兵兵种装配表:cmd → gimbal → shoot → chassis 固定顺序
 * @layer   APP
 * @author  小猫大王
 * @note    与步兵装配表同构;哨兵差异全部收敛在 apps/sentry/ 与 config/sentry_2026/。
 * @note    导航 USB 在 cmd 前消费,控制权仍由原安全仲裁决定。
 */
#include "app_profile.h"
#include "profile.h"
#include "sentry_hooks.h"
#include "robot_cmd.h"
#include "gimbal.h"
#include "shoot.h"
#include "chassis.h"
#include "autonomy.h"

static void sentry_init(void)
{
    cmd_init();
    gimbal_init();
    shoot_init();
    chassis_init();
    autonomy_init();
}

static void sentry_task(void)
{
    autonomy_task(); /* 先消费本拍 USB 数据,再由 cmd 统一发布底盘命令。 */
    cmd_task();
    gimbal_task();
    shoot_task();
    chassis_task();
}

const app_profile_t g_app_profile = {
    .name = "sentry",
    .init = sentry_init,
    .run_200hz = sentry_task,
    .ins_init = sentry_ins_init,
    .calib_enable = false, /* 首批用 config 零位;实车标定流程接入后再置 true */
    .ui_enable = false,    /* 首批不创建 UI 任务;裁判数据仍由 chassis_init 注册接收 */
};
