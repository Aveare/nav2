/**
 * @file    config_ax650.h
 * @brief   AX650 自瞄参数:链路/PID/目标整形
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/spec/本车调参结论.md。
 */
#pragma once

#define AX650_UART1_ENABLE 1
/* [配置] USART1 编译期所有者宏:1=AX650 自瞄；0=释放 USART1 给底盘 VOFA 调试。
 * 该宏与 CHASSIS_VOFA_UART1_ENABLE 互斥，跨域检查在 config.h。
 * 0=不初始化、不发送姿态、不注册串口。
 * 2026-11-22 恢复 AX650 自瞄链路(用户裁决，批间隙合入，DEV-20261122-18)；
 * 同一物理串口上的底盘 VOFA 必须保持 0(ax650.h #error 防呆)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */

#ifndef AX650_UART1_RX_ENABLE
#define AX650_UART1_RX_ENABLE 1
#endif
/* [配置] AX650 0x0503 下行接收开关；1=启动循环 DMA 并解析自瞄命令，0=仅上报姿态。
 * 该开关依赖 AX650_UART1_ENABLE；ifndef 仅供主机测试分别验证开/关路径。 */
/* USART1 只有一个：AX650 自瞄链路与底盘 VOFA 调试互斥。跨子头的互斥检查统一放在
 * 伞头 config.h(所有宏已展开),本文件不再引用其它子头的宏。 */
#define AX650_POSE_PERIOD_MS 10u // [协议] 姿态上报间隔，ms；100 Hz，实机时序待验证
#define AX650_COMMAND_TIMEOUT_MS 100u // [待实测] 0x0503 下行命令有效期，ms；超时后自瞄保持最后目标
#define AX650_TX_SPEED_MPS 0.0f
/* [协议] 裁判 0x0207 首个实测弹速到达前发送 0，表示弹速尚未可用。
 * AX650 将该状态替换为 24 m/s 首发默认值；首帧有效实测值后原值转发。 */
#define AX650_TX_MODE 0u // [协议] 标定模式占位，无量纲；AX650 正常自瞄枚举为 1
#define AX650_AUTO_AIM_MODE 1u // [协议] 左拨杆中挡时上报给 AX650 的自瞄模式

/* ---------------- AX650 自瞄 yaw 独立 PID ---------------- */

#define YAW_AUTO_ANGLE_KP 29.0f
/* 自瞄角度环 P：24 实车反馈修正偏强，降至 20 以减小超调与正反冲击。 */
#define YAW_AUTO_KD_NEAR 3.0f
/* |err|<=2deg 时保留适量末端阻尼；6.5 实车反馈发肉，降至 4.0。 */
#define YAW_AUTO_KD_FAR 0.7f
/* |err|>=6deg 时只保留轻阻尼；1.0 实车反馈仍拖慢追踪，降至 0.6。 */
#define YAW_AUTO_KD_BLEND_LO 2.0f
#define YAW_AUTO_KD_BLEND_HI 6.0f
#define YAW_AUTO_ANGLE_DEADBAND 0.5f
/* 保留原 0.5deg 死区，避免视觉目标小抖动反复驱动 6020 正反出力。 */
#define YAW_AUTO_SPEED_KP_LOW 28.0f
#define YAW_AUTO_SPEED_KP_HIGH 34.0f
/* 自瞄通常工作在低速区；低速 P 保持响应，高速 P 比手动模式保守以减少异响。 */
#define YAW_AUTO_SPEED_KI 0.5f
#define YAW_AUTO_SPEED_INTEGRAL_LIMIT 30.0f
/* yaw 水平轴无需大稳态积分；降低积分蓄能，减少越过目标后的反向回拉。 */
#define YAW_AUTO_KF_ENABLE 1
/* 仅过滤 AX650 下发的 yaw 目标；不滤 IMU 实际角度，避免给闭环反馈增加相位延迟。 */
#define YAW_AUTO_KF_ANGLE_NOISE_STD 0.40f
/* AX650 目标角测量标准差，deg；增大更平滑但延迟更明显。 */
#define YAW_AUTO_KF_RATE_NOISE_STD 100.0f
/* AX650 yaw_speed 测量标准差，deg/s。 */
#define YAW_AUTO_KF_ACCEL_NOISE_STD 1200.0f
/* 常速度模型的加速度噪声，deg/s²；较大值保证移动目标响应速度。 */
#define YAW_AUTO_KF_RATE_LIMIT 500.0f
/* 角速度测量限幅，deg/s，防止异常帧把预测状态拉飞。 */
#define YAW_AUTO_KF_RESET_ANGLE 20.0f
/* 目标切换或角度创新超过该值时直接重置，避免长时间追赶。 */
#define YAW_AUTO_SMOOTH_ENABLE 1
/* 卡尔曼输出后增加速度/加速度受限的二阶目标整形，保证下发到PID的曲线连续。 */
#define YAW_AUTO_SMOOTH_POSITION_GAIN 20.0f
/* 角度误差到期望角速度的比例，1/s；越大响应越快，越小越平滑。 */
#define YAW_AUTO_SMOOTH_MAX_RATE 300.0f
/* 平滑目标最大角速度，deg/s。 */
#define YAW_AUTO_SMOOTH_MAX_ACCEL 3000.0f
/* 平滑目标最大角加速度，deg/s²，限制换向与阶跃时的斜率突变。 */
#define YAW_AUTO_VOFA_USART6_ENABLE 0
/* [配置] USART6 二选一:1=自瞄 yaw VOFA 调试;0=裁判系统。
 * 调试模式会关闭裁判 UI/数据(热量、功率暂不可用),比赛配置必须回 0。
 * USART1 的系统调试由 CHASSIS_VOFA_UART1_ENABLE 独立控制。 */
#define YAW_AUTO_VOFA_SEND_DIVIDER 4u
/* gimbal_task 200Hz / 4 = 50Hz;USART6 115200 8N1。 */
#define AX650_UART1_BAUD 921600u
/* [协议] AX650 视觉链路(USART1)波特率,8E1。改这里要同步 CubeMX 侧 USART1 配置。 */
