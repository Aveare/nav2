/**
 * @file    vision_link.c
 * @brief   哨兵 USB RMCV 0x0503/0x1021 协议实现
 * @layer   APP
 * @author  小猫大王
 * @note    USB 仍由 autonomy 独占;本模块复用无硬件依赖的 ax650_pose 编解码器。
 * @note    协议核心沿用实测版;mode/弹速/robot_id 由调用方运行期注入。
 */
#include "vision_link.h"

#include <math.h>
#include <string.h>

#include "config.h"
#include "kernel.h"
#include "quaternion_ekf.h"

static ax650_parser_t vision_parser;
static vision_state_t vision_state;
static uint32_t vision_feed_rx_ms;
static uint32_t pose_last_ms;
static uint64_t pose_last_update_count;
static uint8_t pose_sequence;
static uint8_t pose_mode = AX650_TX_MODE;
static float pose_speed_mps = AX650_TX_SPEED_MPS;
static uint8_t pose_robot_id = AX650_TX_ROBOT_ID;
static ax650_full_pose_t gimbal_snapshot;
static uint8_t gimbal_has_snapshot;

static void vision_link_on_command(void *context, const ax650_command_t *command)
{
    (void)context;
    if (command == NULL)
        return;
    /* ax650_pose 已检查有限数与枚举范围;应用层补充距离非负合同。 */
    if (command->distance < 0.0f)
    {
        ++vision_state.dropped;
        return;
    }
    vision_state.command = *command;
    vision_state.rx_ms = vision_feed_rx_ms;
    ++vision_state.command_count;
}

void vision_link_init(void)
{
    ax650_parser_init(&vision_parser);
    memset(&vision_state, 0, sizeof(vision_state));
    vision_feed_rx_ms = 0u;
    pose_last_ms = 0u;
    pose_last_update_count = 0u;
    pose_sequence = 0u;
    pose_mode = AX650_TX_MODE;
    pose_speed_mps = AX650_TX_SPEED_MPS;
    pose_robot_id = AX650_TX_ROBOT_ID;
    memset(&gimbal_snapshot, 0, sizeof(gimbal_snapshot));
    gimbal_has_snapshot = 0u;
}

void vision_link_feed(const uint8_t *data, size_t length, uint32_t rx_ms)
{
    if (data == NULL || length == 0u)
        return;
    vision_feed_rx_ms = rx_ms;
    ax650_parser_feed(&vision_parser, data, length, vision_link_on_command, NULL);
}

size_t vision_link_build_pose(uint8_t *out, size_t capacity, uint32_t now_ms)
{
    if (out == NULL || capacity < AX650_POSE_FRAME_SIZE)
        return 0u;
    if (pose_last_ms != 0u && (uint32_t)(now_ms - pose_last_ms) < AX650_POSE_PERIOD_MS)
        return 0u;

    /* INS 任务写、本任务读:短临界区同时快照就绪位、序号与四元数。 */
    const uint32_t saved = kernel_enter_critical();
    const uint8_t initialized = ekf_ins.Initialized;
    const uint64_t update_count = ekf_ins.UpdateCount;
    float q[4];
    memcpy(q, ekf_ins.q, sizeof(q));
    kernel_exit_critical(saved);

    if (initialized == 0u || update_count == 0u || update_count == pose_last_update_count)
        return 0u;

    const size_t length = ax650_pose_pack(out, capacity, q, pose_speed_mps,
                                          pose_mode, pose_sequence, now_ms,
                                          pose_robot_id);
    if (length == 0u)
        return 0u;

    pose_last_ms = now_ms;
    pose_last_update_count = update_count;
    ++pose_sequence;
    return length;
}

void vision_link_update_gimbal(float big_yaw_rel_rad, float small_yaw_rad, float pitch_rad,
                               uint32_t big_yaw_sample_ms, uint32_t small_yaw_sample_ms,
                               uint32_t pitch_sample_ms, uint16_t valid_flags)
{
    const uint32_t saved = kernel_enter_critical();
    gimbal_snapshot.big_yaw_rel_rad = big_yaw_rel_rad;
    gimbal_snapshot.small_yaw_rad = small_yaw_rad;
    gimbal_snapshot.pitch_rad = pitch_rad;
    gimbal_snapshot.big_yaw_sample_time_ms = big_yaw_sample_ms;
    gimbal_snapshot.small_yaw_sample_time_ms = small_yaw_sample_ms;
    gimbal_snapshot.pitch_sample_time_ms = pitch_sample_ms;
    const uint16_t mask = AX650_POSE_VALID_BIG_YAW | AX650_POSE_VALID_SMALL_YAW |
                          AX650_POSE_VALID_PITCH | AX650_POSE_CALIBRATED_SMALL_YAW |
                          AX650_POSE_CALIBRATED_PITCH | AX650_POSE_GIMBAL_ONLINE;
    gimbal_snapshot.valid_flags =
        (uint16_t)((gimbal_snapshot.valid_flags & ~mask) | (valid_flags & mask));
    gimbal_has_snapshot = 1u;
    kernel_exit_critical(saved);
}

size_t vision_link_build_full_pose(uint8_t *out, size_t capacity, uint32_t now_ms)
{
    if (out == NULL || capacity < AX650_FULL_POSE_FRAME_SIZE)
        return 0u;
    if (pose_last_ms != 0u && (uint32_t)(now_ms - pose_last_ms) < AX650_POSE_PERIOD_MS)
        return 0u;

    const uint32_t saved = kernel_enter_critical();
    const uint8_t initialized = ekf_ins.Initialized;
    const uint64_t update_count = ekf_ins.UpdateCount;
    ax650_full_pose_t pose = gimbal_snapshot;
    const uint8_t has_gimbal = gimbal_has_snapshot;
    memcpy(pose.q_wxyz, ekf_ins.q, sizeof(pose.q_wxyz));
    kernel_exit_critical(saved);

    if (initialized == 0u || update_count == 0u || update_count == pose_last_update_count ||
        has_gimbal == 0u)
        return 0u;

    pose.imu_sample_time_ms = now_ms;
    pose.projectile_speed_mps = pose_speed_mps;
    pose.program_mode = pose_mode;
    pose.robot_id = pose_robot_id;
    pose.valid_flags |= AX650_POSE_VALID_IMU;
    if (pose_speed_mps > 0.0f)
        pose.valid_flags |= AX650_POSE_VALID_PROJECTILE_SPEED;
    else
        pose.valid_flags &= (uint16_t)~AX650_POSE_VALID_PROJECTILE_SPEED;
    if (pose_robot_id != 0u)
        pose.valid_flags |= AX650_POSE_VALID_ROBOT_ID;
    else
        pose.valid_flags &= (uint16_t)~AX650_POSE_VALID_ROBOT_ID;
    if (pose_mode == AX650_AUTO_AIM_MODE)
        pose.valid_flags |= AX650_POSE_AUTO_AIM_ALLOWED;
    else
        pose.valid_flags &= (uint16_t)~AX650_POSE_AUTO_AIM_ALLOWED;

    const size_t length = ax650_full_pose_pack(out, capacity, &pose, pose_sequence);
    if (length == 0u)
        return 0u;

    pose_last_ms = now_ms;
    pose_last_update_count = update_count;
    ++pose_sequence;
    return length;
}

void vision_link_set_mode(uint8_t mode)
{
    pose_mode = mode;
}

void vision_link_set_projectile_speed(float speed_mps)
{
    pose_speed_mps = (isfinite(speed_mps) && speed_mps >= 0.0f) ? speed_mps : 0.0f;
}

void vision_link_set_robot_id(uint8_t robot_id)
{
    pose_robot_id = robot_id;
}

void vision_link_get_state(vision_state_t *out)
{
    if (out == NULL)
        return;
    *out = vision_state;
    out->valid = (vision_state.command_count != 0u &&
                  (uint32_t)(kernel_tick_ms() - vision_state.rx_ms) <=
                      AX650_COMMAND_TIMEOUT_MS)
                     ? 1u
                     : 0u;
}
