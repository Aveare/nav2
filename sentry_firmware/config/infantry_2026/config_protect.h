/**
 * @file    config_protect.h
 * @brief   保护参数:看门狗/死区/停拨阈值
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/robots/infantry/params/调参结论.md。
 */
#pragma once

/* ---------------- 保护 ---------------- */

#define CMD_WATCHDOG_TIMEOUT_MS 200.0f
/* [理论] 命令断流看门狗超时,ms。消费端超时未收到新命令即切安全态。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 控制权仲裁阈值 ---------------- */
#define SHOOT_DIAL_STOP_THRESHOLD 300
/* [实测] 拨轮急停阈值(原始刻度,正方向)。超过即判急停并锁存,须回中后重新解锁。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define PROTECT_AUTH_NEUTRAL_DEADBAND 10
/* [实测] 解锁条件里的"全回中"死区(原始刻度)。拨轮与四路摇杆都须在 ±此值内才允许解锁,
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
