/**
 * @file    sentry_yaw.h
 * @brief   哨兵双 yaw 参考计算与雷达朝向接口
 * @layer   APP
 * @note    电池方向为底盘 +X。俯视逆时针为正，角度单位为度。
 */
#ifndef SENTRY_YAW_H
#define SENTRY_YAW_H

#include <stdbool.h>
#include <stdint.h>

/** 大 yaw 相对于底盘的机械角度参考，保留连续角度。 */
typedef struct
{
    float big_to_chassis_deg; // 大 yaw 相对底盘
} SentryYawReference;

/** 上电建立的保持基准，运行期由单一控制任务读写。 */
typedef struct
{
    float chassis_heading_at_start_deg;  // 上电完成对准时的底盘朝向
    float big_yaw_battery_align_deg;     // 大 yaw 对准电池方向时的标定角
    bool initialized;                    // 有效基准标志
} SentryYawState;

/** 调用方将底盘姿态和两轴反馈对齐到同一采样时刻后提供。 */
typedef struct
{
    uint64_t timestamp_us;        // 位姿时刻，须和点云使用同一时基
    float chassis_to_map_deg;    // 底盘 +X 相对地图北向的角度
    float big_to_chassis_deg;     // 大 yaw 相对底盘的实测机械角度
    float small_to_big_deg;       // 小 yaw 相对大 yaw 的实测机械角度
    float lidar_to_small_deg;     // 雷达 +X 相对小 yaw 零位的安装偏角
} SentryLidarYawSample;

/** 上位机做点云坐标变换时使用的平面朝向。 */
typedef struct
{
    uint64_t timestamp_us;
    float lidar_to_map_deg;       // [-180, 180)
} SentryLidarHeading;

/**
 * @brief  建立上电时的大 yaw 电池方向对准基准
 * @param  chassis_heading_deg 当前底盘相对地图北向的角度，度
 * @param  big_yaw_battery_align_deg 大 yaw 对准电池方向时的标定角，度
 * @param  state 保存初始保持基准
 * @return 输入有效且基准建立成功时返回 true，失败时不改状态
 * @note   调用前须确认 IMU 航向和大 yaw 角度反馈有效。
 */
bool SentryYawInitialize(float chassis_heading_deg, float big_yaw_battery_align_deg,
                         SentryYawState *state);

/**
 * @brief  计算大 yaw 相对底盘的反向补偿目标
 * @param  chassis_to_map_deg 底盘前进方向相对地图北向的角度，度
 * @param  state 上电时建立的保持基准
 * @param  reference 大 yaw 机械角度参考，度
 * @return 输入有效且计算成功时返回 true，失败时不改输出
 * @note   底盘航向按连续角度输入；调用方负责机械限位和电机使能。
 */
bool SentryYawCalculateReference(float chassis_to_map_deg, const SentryYawState *state,
                                 SentryYawReference *reference);

/**
 * @brief  用实测角度计算雷达相对地图北向的平面朝向
 * @param  sample 同一时刻的位姿样本，角度用度，时间用微秒
 * @param  heading 朝向及其采样时刻，角度用度，时间用微秒
 * @return 输入有效且计算成功时返回 true，失败时不改输出
 * @note   地图北向取建图起始时的底盘 +X。完整点云变换还需要位移和其他姿态分量。
 */
bool SentryLidarCalculateHeading(const SentryLidarYawSample *sample,
                                SentryLidarHeading *heading);

#endif
