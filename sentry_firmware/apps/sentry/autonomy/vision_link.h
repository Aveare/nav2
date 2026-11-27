/**
 * @file    vision_link.h
 * @brief   哨兵 USB 自瞄协议状态与姿态上行接口
 * @layer   APP
 * @author  小猫大王
 * @note    负责 RMCV 0x0503 接收、0x1022 完整姿态发送和 0x1021 兼容打包,
 *          本分支在其上提供 mode/弹速/robot_id 运行期注入,由调用方决定是否消费。
 */
#ifndef SENTRY_VISION_LINK_H
#define SENTRY_VISION_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "ax650_pose.h"

typedef struct
{
    ax650_command_t command; // 最近一帧通过校验的 0x0503
    uint32_t rx_ms;          // 完成该帧的 USB 数据块接收时刻,ms
    uint32_t command_count;  // 有效帧累计
    uint16_t dropped;        // 字段检查失败帧累计
    uint8_t valid;           // 1=最近 SENTRY_NUC_RMCV_COMMAND_TIMEOUT_MS 内收到有效帧
} vision_state_t;

/** @brief 清空 RMCV 流解析器与状态;由 autonomy_init 调用。 */
void vision_link_init(void);

/** @brief 喂入一段 USB 字节流;支持分包、粘包和错误后重同步。 */
void vision_link_feed(const uint8_t *data, size_t length, uint32_t rx_ms);

/**
 * @brief 按配置周期用最近一次完整 INS 四元数构建 0x1021（旧协议兼容）
 * @param out 输出缓冲;USB 发送完成前调用方不得改写
 * @param capacity 输出缓冲容量,字节
 * @param now_ms 当前 MCU 单调时钟,ms
 * @return 成功返回 31;未到周期、无新 INS 样本或输入非法返回 0
 */
size_t vision_link_build_pose(uint8_t *out, size_t capacity, uint32_t now_ms);

/** @brief 更新 0x1022 所需的三轴关节角、各自采样时间与有效位。 */
void vision_link_update_gimbal(float big_yaw_rel_rad, float small_yaw_rad, float pitch_rad,
                               uint32_t big_yaw_sample_ms, uint32_t small_yaw_sample_ms,
                               uint32_t pitch_sample_ms, uint16_t valid_flags);

/**
 * @brief 用最新 INS 四元数和三轴快照构建 61 B `0x1022`
 * @return 成功返回 AX650_FULL_POSE_FRAME_SIZE；未到周期、无新样本或输入非法返回 0
 */
size_t vision_link_build_full_pose(uint8_t *out, size_t capacity, uint32_t now_ms);

/** @brief 设置姿态上行的 program_mode（自瞄激活=1）。 */
void vision_link_set_mode(uint8_t mode);

/** @brief 设置姿态上行的实测弹速,m/s;非有限值或负值按 0 发送。 */
void vision_link_set_projectile_speed(float speed_mps);

/** @brief 设置姿态上行的本车 robot_id;0=尚未收到裁判数据。 */
void vision_link_set_robot_id(uint8_t robot_id);

/** @brief 读取最近 0x0503 快照;超时只清 valid,不擦除诊断数据。 */
void vision_link_get_state(vision_state_t *out);

#endif // SENTRY_VISION_LINK_H
