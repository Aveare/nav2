/**
 * @file    abi_dump.c
 * @brief   T11 ABI 快照:打印协议合同全部 pack(1) 结构的 sizeof/offsetof
 * @author  小猫大王
 * @layer   TESTS
 *
 * @note    布局是跨板/跨工具链合同,只靠 robot_def.h 尾部的两个 _Static_assert
 *          (chassis=21/3)守不住"加字段不更新断言"之外的所有漂移;本文件把
 *          全部字段偏移落成 golden,任何布局变化都会 diff 出来。
 * @note    ARM 侧的枚举宽度差异(1B vs 4B)由 robot_def.h 的双口径
 *          _Static_assert 覆盖;本 dump 在主机侧提供字段级明细。
 */
#include <stdio.h>
#include <stddef.h>

#include "robot_def.h"

#define DUMP_SIZE(T) printf("%s sizeof %lu\n", #T, (unsigned long)sizeof(T))
#define DUMP_OFF(T, F) printf("%s.%s offsetof %lu\n", #T, #F, (unsigned long)offsetof(T, F))

int main(void)
{
    /* 底盘控制指令(cmd → chassis) */
    DUMP_SIZE(chassis_ctrl_cmd_t);
    DUMP_OFF(chassis_ctrl_cmd_t, vx);
    DUMP_OFF(chassis_ctrl_cmd_t, vy);
    DUMP_OFF(chassis_ctrl_cmd_t, wz);
    DUMP_OFF(chassis_ctrl_cmd_t, offset_angle);
    DUMP_OFF(chassis_ctrl_cmd_t, yaw_rate_cmd);
    DUMP_OFF(chassis_ctrl_cmd_t, chassis_mode);

    /* 云台控制指令(cmd → gimbal) */
    DUMP_SIZE(gimbal_ctrl_cmd_t);
    DUMP_OFF(gimbal_ctrl_cmd_t, yaw);
    DUMP_OFF(gimbal_ctrl_cmd_t, pitch);
    DUMP_OFF(gimbal_ctrl_cmd_t, chassis_rotate_wz);
    DUMP_OFF(gimbal_ctrl_cmd_t, gimbal_mode);

    /* 发射控制指令(cmd → shoot) */
    DUMP_SIZE(shoot_ctrl_cmd_t);
    DUMP_OFF(shoot_ctrl_cmd_t, shoot_mode);
    DUMP_OFF(shoot_ctrl_cmd_t, load_mode);
    DUMP_OFF(shoot_ctrl_cmd_t, lid_mode);
    DUMP_OFF(shoot_ctrl_cmd_t, friction_mode);
    DUMP_OFF(shoot_ctrl_cmd_t, bullet_speed);
    DUMP_OFF(shoot_ctrl_cmd_t, rest_heat);
    DUMP_OFF(shoot_ctrl_cmd_t, shoot_rate);
    DUMP_OFF(shoot_ctrl_cmd_t, fire_tier);

    /* 底盘回传(chassis → cmd) */
    DUMP_SIZE(chassis_upload_t);
    DUMP_OFF(chassis_upload_t, rest_heat);
    DUMP_OFF(chassis_upload_t, bullet_speed);
    DUMP_OFF(chassis_upload_t, enemy_color);

    return 0;
}
