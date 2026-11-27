/**
 * @file    sentry_def.h
 * @brief   哨兵应用层消息协议:三轴云台/底盘/发射的 cmd↔执行单元结构
 * @layer   APP
 * @author  小猫大王
 * @note    仅哨兵 profile 使用,不跨板传输,因此不做 pack(1),避免 Cortex-M4 非对齐访问。
 * @note    话题沿用 common 的 6 个名字,不新增话题(MSG_TOPIC_MAX=8)。
 */
#ifndef SENTRY_DEF_H
#define SENTRY_DEF_H

#include <stdint.h>
#include "robot_def.h" /* chassis_ctrl_cmd_t / shoot_ctrl_cmd_t / attitude_t / gimbal_mode_e */

/* cmd → gimbal：pitch为相对解锁角增量，小yaw为独立速率；大yaw由底盘模式选择机械回正/纯速度补偿。
 * AUTO_AIM 下 big_yaw 改为世界 yaw 目标、pitch_abs 改为世界 pitch 目标,aim_valid=1。 */
typedef struct
{
    float big_yaw;       // AUTO_AIM: 世界 yaw 目标,deg; 手动: 兼容字段
    float pitch;         // 解锁点相对电机轴角指令,deg
    float pitch_abs;     // AUTO_AIM: 枪管世界 pitch 目标,deg
    float small_yaw_rate_deg_s; // independent relative-yaw command, deg/s
    float small_yaw_rel; // 预留:操作手直接给相对精瞄目标,当前恒 0
    gimbal_mode_e mode;  // ZERO_FORCE / GYRO / AUTO_AIM
    uint8_t aim_valid;   // 1=本拍自瞄目标有效(gimbal 只在该位为 1 时用 AUTO_AIM)
} sentry_gimbal_cmd_t;

/* gimbal → cmd:三轴反馈与 IMU 快照 */
typedef struct
{
    attitude_t imu;            // IMU 姿态(与 chassis 共用 ins_state)
    float big_yaw_world_deg;   // 大 yaw 世界角,deg
    float big_yaw_rel_deg;     // 大 yaw 相对底盘机械零位,deg(±180)
    float small_yaw_rel_deg;   // 小 yaw 相对机械中位,deg
    float small_yaw_target_deg;// 小 yaw 分配后的目标角,deg
    float pitch_deg;           // pitch 电机输出轴角,deg
    float pitch_abs_deg;       // 枪管世界 pitch 观察值,deg
    float big_yaw_torque_nm;   // 大 yaw 当前力矩指令,N·m
    uint8_t big_yaw_state;     // DM 状态位(dm_motor_err_state_e)
    uint8_t pitch_state;       // DM 状态位
    uint8_t big_yaw_online;       // 1=DM 反馈新鲜
    uint8_t pitch_online;         // 1=pitch DM 反馈新鲜
    uint8_t small_yaw_online;     // 1=小 yaw 反馈在线
    uint64_t big_yaw_sample_us;   // 大 yaw CAN 反馈进入 MCU 的采样时间,us
    uint64_t small_yaw_sample_us; // 小 yaw 反馈快照时间,MCU us
    uint64_t pitch_sample_us;     // pitch CAN 反馈进入 MCU 的采样时间,us
} sentry_gimbal_feed_t;

/* chassis → cmd:轮速逆解估计与模式快照 */
typedef struct
{
    float est_vx; // mm/s
    float est_vy; // mm/s
    float est_wz; // deg/s
    uint8_t mode; // chassis_mode_e
} sentry_chassis_feed_t;

/* shoot → cmd:热量与摩擦轮状态 */
typedef struct
{
    float heat;          // 热量估计,热量点
    float heat_limit;    // 热量上限,热量点
    float cooling;       // 冷却速率,热量点/s
    float friction_rpm_l; // 左摩擦轮实测,RPM
    float friction_rpm_r; // 右摩擦轮实测,RPM
    uint8_t heat_state;  // heat_guard_state_e
} sentry_shoot_feed_t;

#endif // SENTRY_DEF_H
