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

#define CHASSIS_WHEEL_BASE 500       /* [待实测] 纵轴距,mm */
#define CHASSIS_TRACK_WIDTH 460      /* [待实测] 横轮距,mm */
#define CHASSIS_GIMBAL_OFFSET_X 0    /* [待实测] 大 yaw 轴相对底盘中心前后偏心,mm */
#define CHASSIS_GIMBAL_OFFSET_Y 0    /* [待实测] 大 yaw 轴左右偏心,mm */
#define CHASSIS_WHEEL_RADIUS 75      /* [待实测] 轮半径,mm */
#define CHASSIS_WHEEL_REDUCTION 19.0f /* [规格] M3508 内置减速比 */
#define CHASSIS_MAX_MOTOR_RPM 6000.0f /* [待实测] 轮电机转子限速,RPM */

/* ---------------- 底盘速度 ---------------- */

#define CHASSIS_MAX_SPEED 1500.0f    /* [待实测] 平移最大速度,mm/s */
#define CHASSIS_ROTATE_SPEED 250.0f  /* [待实测] 小陀螺自转速度,deg/s */
#define CHASSIS_FOLLOW_WZ_LIMIT 400.0f /* [待实测] 跟随律 wz 总限幅,deg/s;0=不限 */
#define CHASSIS_TRANSLATE_BIAS_ANGLE 0.0f /* [待实测] 小陀螺平移偏置角,deg;先置 0 */

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
