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
#define SHOOT_FRICTION_STOP_DELAY_MS 1000.0f /* [本地工程] 松开扳机后摩擦轮延时停转,ms */

/* ---------------- 摩擦轮 ---------------- */

#define SHOOT_FRICTION_TARGET_RPM 4000.0f   /* [待实测] 摩擦轮目标转速,RPM(转子) */
#define SHOOT_FRICTION_READY_TOL_RPM 500.0f /* [理论] 到速容差,RPM */
#define SHOOT_FRICTION_RPM_OFFSET 0.0f      /* [待实测] 弹速标定微调,RPM */
#define SHOOT_FRICTION_REVERSE_L 0          /* [实测-参考 shaobin 2026-11-22] 左轮输出正常(安装镜像在右轮) */
#define SHOOT_FRICTION_REVERSE_R 1          /* [实测-参考 shaobin 2026-11-22] 右轮输出反向;到速互锁符号随宏自动跟随 */
#define SENTRY_SHOOT_AUTO_AIM_REMOTE_ENABLE 1 /* [配置] 自瞄挡允许遥控拨轮单发,摩擦轮常开 */

/* ---------------- 拨盘堵转 ---------------- */

#define LOADER_JAM_CURRENT 1800.0f /* [待实测] 堵转电流阈值(C610 满量程 10000) */
#define LOADER_JAM_SPEED_RPM 30.0f /* [待实测] 堵转转速门限,RPM */
#define LOADER_JAM_HOLD_MS 500.0f  /* [待实测] 堵转持续判定,ms */
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
#define LOADER_ANGLE_PID_KP 2.5f           /* [参考 shaobin] 单发/三发角度环 Kp(原 0 等于禁用) */
#define LOADER_ANGLE_PID_KI 0.1f           /* [参考 shaobin] 角度环 Ki */
#define LOADER_ANGLE_PID_MAXOUT 6000       /* [理论] 角度环输出上限,deg/s */
#define LOADER_SPEED_PID_KP 3.0f           /* [参考 shaobin] 拨盘速度环 Kp;6.0 偏硬有顿挫 */
#define LOADER_SPEED_PID_KI 0.4f           /* [参考 shaobin] 拨盘速度环 Ki */
#define LOADER_SPEED_PID_IL 600            /* [理论] 积分限幅 */
#define LOADER_SPEED_PID_MAXOUT 5000       /* [理论] 输出限幅(C610 码) */

#define SHOOT_CALIB_LOG 0 /* [配置] 发射标定日志开关,首车调试时可置 1 */
