/**
 * @file    config_chassis.h
 * @brief   哨兵底盘参数:几何/速度/跟随律/四轮 PID 与 ID
 * @layer   CONFIG
 * @author  小猫大王
 * @note    底盘构型按参考工程:4×M3508 45° 麦轮,CAN1 ESC 1-4。
 * @note    所有几何/方向参数均为 [待实测] 占位;首车必须重新量取并做方向标定。
 */
#pragma once

/* ---------------- 底盘几何 (mm) ---------------- */

#define CHASSIS_WHEEL_BASE 600 /* [实测 2026-11-27] 600×600 方形底盘:前后轮心距,mm */
#define CHASSIS_TRACK_WIDTH 600 /* [实测 2026-11-27] 600×600 方形底盘:左右轮心距,mm;轮心对角线 600√2 */
#define CHASSIS_GIMBAL_OFFSET_X 0    /* [实测] 大 yaw 轴与底盘中心重合,mm */
#define CHASSIS_GIMBAL_OFFSET_Y 0    /* [实测] 大 yaw 轴与底盘中心重合,mm */
#define CHASSIS_WHEEL_RADIUS 75      /* [实测-同步兵] 轮半径,mm */
#define CHASSIS_WHEEL_REDUCTION 19.0f /* [规格] M3508 内置减速比,同步兵 */
#define CHASSIS_MAX_MOTOR_RPM 9000.0f /* [配置 2026-11-27] 轮电机转子限速,RPM;自转优先取满,
                                       * 由 7000 抬到 M3508 标称上限 9000(原值 7000) */

/* ---------------- 底盘速度 ---------------- */

#define CHASSIS_MAX_SPEED 2000.0f    /* [配置 2026-11-27] 平移最大速度,mm/s;用户指定 2000 */
#define CHASSIS_ROTATE_SPEED 300.0f  /* [配置 2026-11-27] 自转暂降 502→300 复测断电:单跑 5373 rpm,
                                      * 模型估算 60~80 W;若断电消失即功率缓冲问题(见临时排障结论) */
#define CHASSIS_FOLLOW_WZ_LIMIT 400.0f /* [待实测] 跟随律 wz 总限幅,deg/s;0=不限 */
#define CHASSIS_TRANSLATE_BIAS_ANGLE 10.0f /* RM_shaobin2 current configuration */

/* ---------------- 运动平顺 ---------------- */

#define CHASSIS_RC_DEADBAND 30        /* [实测-沿用] 摇杆进入运动阈值,原始刻度 */
#define CHASSIS_RC_RELEASE 20         /* [实测-沿用] 摇杆退出运动阈值 */
#define CHASSIS_VX_RAMP 5000.0f       /* [理论] 前进加速度上限,mm/s² */
#define CHASSIS_VY_RAMP 5000.0f       /* [理论] 横移加速度上限,mm/s² */

/* ---------------- 跟随律(底盘追大 yaw) ---------------- */

#define CHASSIS_FOLLOW_KP 8.0f        /* [待实测] 位置环增益,(deg/s)/deg */
#define CHASSIS_FOLLOW_DEADBAND 2.0f  /* [待实测] 软死区,deg */
#define CHASSIS_FOLLOW_TARGET_OFFSET 0.0f /* [待实测] 跟随目标相对角,deg */
#define CHASSIS_FOLLOW_FF 0.75f       /* [待实测] 大 yaw 世界角速度前馈系数 */
#define CHASSIS_FOLLOW_P_RESERVE 0.25f /* [理论] 饱和时给位置项保留的额度比例 */
#define CHASSIS_ALIGN_SPEED 200.0f    /* [待实测] 回中恒速,deg/s */
#define CHASSIS_ALIGN_SOFT 8.0f       /* [待实测] 回中末端线性收敛区,deg */

/* ---------------- 四轮速度环 PID (M3508,反馈 deg/s) ---------------- */

#define CHASSIS_SPEED_PID_KP 0.75f    /* [待实测] 速度环 Kp */
#define CHASSIS_SPEED_PID_KI 1.0f     /* [待实测] 速度环 Ki */
#define CHASSIS_SPEED_PID_IL 3000     /* [理论] 积分限幅(C620 码) */
#define CHASSIS_SPEED_PID_MAXOUT 10000 /* [理论] 输出限幅(C620 满量程 16384 的 61%) */
#define CHASSIS_SPEED_PID_RC 0.3f     /* [理论] 输出低通 RC */

/* ---------------- 电机 ID / 方向 ---------------- */

#define CHASSIS_MOTOR_ID_LF 1 /* [配置] 左前 M3508,CAN1 ESC1 */
#define CHASSIS_MOTOR_ID_RF 2 /* [配置] 右前 M3508,CAN1 ESC2 */
#define CHASSIS_MOTOR_ID_LB 3 /* [配置] 左后 M3508,CAN1 ESC3 */
#define CHASSIS_MOTOR_ID_RB 4 /* [配置] 右后 M3508,CAN1 ESC4 */

/* 轮方向:1=正转, -1=反转。装反或麦轮辊子方向不同时改这里,不改运动学。 */
#define CHASSIS_WHEEL_DIR_LF (+1.0f)
#define CHASSIS_WHEEL_DIR_RF (+1.0f)
#define CHASSIS_WHEEL_DIR_LB (+1.0f)
#define CHASSIS_WHEEL_DIR_RB (+1.0f)

/* ---------------- 调试口 ---------------- */

#define CHASSIS_VOFA_UART1_ENABLE 0 /* [配置] 哨兵当前 UART1 给云台 VOFA,底盘 VOFA 保持关闭 */

/* ---------------- 底盘功率控制(移植自步兵,参数按哨兵重标前沿用) ---------------- */
/* 模型:P = K0 + K1|i| + K2|w| + K3|i||w| + K4 i² + K5 w²。
 * i=电流指令码/1000,w=转子 rpm;超限解二次方程求电流衰减系数,直接乘 output_scale。
 * [待实测-哨兵] 以下系数/余量来自步兵实车标定,首次哨兵台架必须用裁判功率复核。 */

#define CHASSIS_POWER_CTRL_EN 0
#define CHASSIS_POWER_LIMIT_DEFAULT 100.0f /* [规则] 裁判上限无效时兜底,W;哨兵 2026 限 100 */
#define CHASSIS_POWER_LIMIT_MAX 100.0f     /* [规则] 硬上限,W;有效控制目标 = 100 - 5 = 95 */
#define CHASSIS_POWER_MARGIN_W 5.0f       /* [实测-沿用] 控制余量,W */
#define CHASSIS_POWER_SCALE_LP 0.15f      /* [理论] 恢复方向低通(降额同拍生效) */
#define CHASSIS_POWER_MIN_SCALE 0.20f     /* [安全] 输出下限 */
#define CHASSIS_POWER_SCALE_MIN 0.05f     /* [安全] 模型内部下限 */
#define CHASSIS_POWER_BUFFER_MIN_SCALE 0.5f
#define CHASSIS_POWER_BUFFER_MAX_J 60.0f
#define CHASSIS_POWER_MODEL_SCALE 1.10f
#define CHASSIS_POWER_MODEL_BIAS 0.0f
#define CHASSIS_POWER_CURRENT_DIV 1000.0f
#define CHASSIS_POWER_MODEL_K0 0.65213f
#define CHASSIS_POWER_MODEL_K1 (-0.15659f)
#define CHASSIS_POWER_MODEL_K2 0.00041660f
#define CHASSIS_POWER_MODEL_K3 0.00235415f
#define CHASSIS_POWER_MODEL_K4 0.20022f
#define CHASSIS_POWER_MODEL_K5 0.000000108f
