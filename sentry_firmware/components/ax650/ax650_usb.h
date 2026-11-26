/**
 * @file    ax650_usb.h
 * @brief   AX650/RMCV 协议在共享 USB CDC 网关中的状态与姿态接口
 * @layer   COMPONENT
 * @author  Codex
 * @note    本组件不直接访问 USB 硬件；autonomy 是唯一 USB 端口所有者。
 */
#pragma once

#include "ax650_pose.h"
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    ax650_command_t command;
    uint32_t command_rx_ms;
    uint32_t command_count;
    uint8_t command_valid;
} ax650_usb_state_t;

/** 初始化流解析器、收发状态和默认姿态元数据。 */
void ax650_usb_init(void);

/** 向 0x0503 流解析器输入一段 USB 数据；支持分片、粘包和混合 NV 字节流。 */
void ax650_usb_feed(const uint8_t *data, size_t count, uint32_t received_ms);

/** INS 任务更新最新姿态快照；不在该任务中直接发送 USB。 */
void ax650_usb_update_quaternion(const float q_wxyz[4], uint32_t sample_time_ms);

/**
 * USB 所有者在端口空闲时生成一帧 0x1021。
 * @return 成功返回 AX650_POSE_FRAME_SIZE；未到周期或尚无姿态返回 0。
 */
size_t ax650_usb_prepare_pose(uint8_t *output, size_t capacity, uint32_t now_ms);

void ax650_usb_set_projectile_speed(float speed_mps);
void ax650_usb_set_robot_id(uint8_t robot_id);
void ax650_usb_set_mode(uint8_t mode);
void ax650_usb_get_state(ax650_usb_state_t *out);
