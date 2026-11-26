/**
 * @file    assembly.c
 * @brief   重装兵种装配表:42mm 控制待 M5 实装
 * @layer   APP
 * @author  小猫大王
* @note    骨架阶段只跑公共板级/IMU/守护任务;init/run 为空是**显式**的,
*          不是忘记接线。M5 实装时只改本文件与 config/heavy_2026/。
 */
#include "app_profile.h"
#include "profile.h"

static void heavy_init(void)
{
    /* M5:chassis_init(); gimbal_init(); shoot_init()(42mm) */
}

static void heavy_task(void)
{
    /* M5:chassis_task(); gimbal_task(); shoot_task()(42mm) */
}

const app_profile_t g_app_profile = {
    .name = "heavy",
    .init = heavy_init,
    .run_200hz = heavy_task,
    .ins_init = NULL,
    .calib_enable = false,
    .ui_enable = false,
};
