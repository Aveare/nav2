/**
 * @file    config_shoot.h
 * @brief   哨兵 17mm 发射参数:摩擦轮/拨盘/射速/ID
 * @layer   CONFIG
 * @author  小猫大王
 * @note    摩擦轮与拨盘硬件映射同参考工程:CAN2 摩擦轮 ESC1/2,M2006 拨盘 ESC3。
 * @note    射速档位按哨兵 260 热 / 30 冷却计算;实际放行仍由 heat_guard 裁决。
 */
#pragma once

/* ---------------- 弹道/拨盘 ---------------- */

#define SHOOT_NUM_PER_CIRCLE 7              /* [待实测] 拨盘一圈槽数 */
#define SHOOT_LOADER_REDUCTION 36.0f        /* [规格] M2006 减速比 */
#define SHOOT_ONE_BULLET_DELTA_ANGLE (360.0f / SHOOT_NUM_PER_CIRCLE) /* 输出轴一发角,deg */
#define LOADER_DIRECTION_SIGN (-1.0f) /* [实测 2026-11-26] 送弹方向: -1=顺时针(角度递减), +1=逆时针 */
#define LOADER_ANGLE_TOL_DEG 6.0f           /* [理论] 单发到位容差,输出轴 deg */
#define LOADER_SINGLE_TIMEOUT_MS 1000.0f    /* [理论] 单发到位超时,ms */
#define LOADER_WAIT_TIMEOUT_MS 2000.0f      /* [理论] 等待互锁超时,ms */
#define LOADER_FRICTION_OFF_GRACE_MS 100.0f /* [理论] 摩擦轮关闭到丢弃队列的宽限,ms */

/* ---------------- 射速 ---------------- */

#define SHOOT_RATE_DEFAULT 6.0f    /* [待实测] 常规请求射速,发/s */
#define SHOOT_RATE_MAX 20.0f       /* [安全] 请求射速绝对上限,发/s */
#define SHOOT_RATE_ECO 3.0f        /* [换算] 30 冷却 / 10 每发 = 3 发/s 热量平衡 */
#define SHOOT_RATE_BURST 12.0f     /* [待实测] 爆发请求射速,发/s */

/* ---------------- 遥控拨轮 ---------------- */

#define SHOOT_DIAL_TRIGGER_ZONE (-240)   /* [实测-沿用] 扳机区入口,原始刻度 */
#define SHOOT_DIAL_TRIGGER_RELEASE (-220) /* [实测-沿用] 扳机区退出,原始刻度(迟滞) */
#define SHOOT_DIAL_SINGLE_HOLD_MS 250.0f  /* [实测-沿用] 短拨/长按判别,ms */
#define SHOOT_DIAL_FRICTION_KEEP_MS 1000.0f /* RM_shaobin2 current configuration */

/* ---------------- 摩擦轮 ---------------- */

#define SHOOT_FRICTION_TARGET_RPM 4000.0f   /* [待实测] 摩擦轮目标转速,RPM(转子) */
#define SHOOT_FRICTION_READY_TOL_RPM 500.0f /* [理论] 到速容差,RPM */
#define SHOOT_FRICTION_RPM_OFFSET 0.0f      /* [待实测] 弹速标定微调,RPM */
#define SHOOT_FRICTION_REVERSE_L 0 /* RM_shaobin2 current configuration */
#define SHOOT_FRICTION_REVERSE_R 1 /* RM_shaobin2 current configuration */
#define SENTRY_SHOOT_AUTO_AIM_REMOTE_ENABLE 0 /* RM_shaobin2 current configuration */

/* ---------------- 拨盘堵转 ---------------- */

#define LOADER_JAM_CURRENT 5000.0f /* [实测 2026-11-27] 1800→5000:配合 9A 硬推,真堵转仍触发 */
#define LOADER_JAM_SPEED_RPM 30.0f /* [待实测] 堵转转速门限,RPM */
#define LOADER_JAM_HOLD_MS 1300.0f /* [实测 2026-11-27] 升档后满力仍卡 1.3s 才锁存 */
#define LOADER_PUSH_CURRENT 5500.0f          /* [实测 2026-11-27] 正常顶弹电流,5.5A */
#define LOADER_ESCALATE_STEP_CURRENT 1750.0f /* [实测 2026-11-27] 每档 +1.75A,约 60ms 到顶 */
#define LOADER_ESCALATE_STEP_MS 60.0f        /* [实测 2026-11-27] 每档 60ms;检测到即先加一档 */
#define LOADER_ESCALATE_MAX_CURRENT 9000.0f  /* [安全] 升档上限,C610 满量程 90% */
#define LOADER_BACKOFF_EN 0        /* [配置] 自动退弹开关,0=锁存等回中 */

/* ---------------- 电机 ID ---------------- */

#define SHOOT_FRICTION_ID_L 1 /* [配置] 左摩擦轮 M3508,CAN2 ESC1 */
#define SHOOT_FRICTION_ID_R 2 /* [配置] 右摩擦轮 M3508,CAN2 ESC2 */
#define LOADER_ID 3           /* [配置] 拨盘 M2006,CAN2 ESC3 */

/* ---------------- PID ---------------- */

#define SHOOT_FRICTION_SPEED_PID_KP 2.4f   /* [待实测] 摩擦轮速度环 Kp */
#define SHOOT_FRICTION_SPEED_PID_KI 0.5f   /* [待实测] 摩擦轮速度环 Ki */
#define SHOOT_FRICTION_SPEED_PID_IL 1500   /* [理论] 积分限幅 */
#define SHOOT_FRICTION_SPEED_PID_MAXOUT 6000 /* [理论] 输出限幅(C620 码) */
#define LOADER_ANGLE_PID_KP 2.5f /* RM_shaobin2 current configuration */
#define LOADER_ANGLE_PID_KI 0.1f /* RM_shaobin2 current configuration */
#define LOADER_ANGLE_PID_MAXOUT 8000.0f /* [实测 2026-11-27] 6000→8000:到位速度上限同步放宽 */
#define LOADER_SPEED_PID_KP 3.0f /* RM_shaobin2 current configuration */
#define LOADER_SPEED_PID_KI 0.4f /* RM_shaobin2 current configuration */
#define LOADER_SPEED_PID_IL 600 /* RM_shaobin2 current configuration */
#define LOADER_SPEED_PID_MAXOUT 9000 /* [实测 2026-11-27] 5→9A(C610 满 10000,90%);管道小堵塞硬推 */
#define LOADER_TEMP_LIMIT_C 80.0f    /* [安全] C610 温度到 80°C 锁存停转 */
#define LOADER_TEMP_RESUME_C 60.0f   /* [安全] 降到 60°C 才允许恢复 */

#define SHOOT_CALIB_LOG 0 /* [配置] 发射标定日志开关,首车调试时可置 1 */

#define SHOOT_FRICTION_STOP_DELAY_MS 1000.0f /* RM_shaobin2 current configuration */
