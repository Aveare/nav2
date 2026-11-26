/**
 * @file    telemetry_arena.c
 * @brief   遥测五环对象(.ccmbss)与 tag 赋值
 * @layer   PORTS(stm32f4)
 * @author  小猫大王
* @note    五环对象 + tag 赋值在此(对象=平台存储面);memset/逻辑在 framework/diag/telemetry.c。
*          T13 工具契约:telemetry_* 符号名与地址段(0x1000xxxx)不变,Ozone/采样工具无感。
 */
#include "telemetry.h"

telemetry_ring_t telemetry_yaw TELEMETRY_CCM;
telemetry_ring_t telemetry_pitch TELEMETRY_CCM;
telemetry_ring_t telemetry_chassis_cmd TELEMETRY_CCM;
telemetry_ring_t telemetry_chassis_fb TELEMETRY_CCM;
telemetry_ring_t telemetry_shoot TELEMETRY_CCM;

void diag_arena_assign_tags(void)
{
    telemetry_yaw.tag = "yaw";
    telemetry_pitch.tag = "pit";
    telemetry_chassis_cmd.tag = "chs";
    telemetry_chassis_fb.tag = "chf";
    telemetry_shoot.tag = "sho";
}
