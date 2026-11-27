/**
 * @file    ax650_usb.c
 * @brief   AX650/RMCV USB 传输状态，生成 0x1022 并解析 0x0503
 * @layer   COMPONENT
 * @author  小猫大王
 */
#include "ax650_usb.h"
#include "config.h"
#include "kernel.h"
#include <string.h>

static ax650_parser_t rx_parser;
static ax650_usb_state_t rx_state;
static ax650_full_pose_t tx_pose;
static uint32_t tx_last_ms;
static uint8_t tx_sequence;
static uint8_t tx_has_pose;
static uint8_t tx_has_gimbal;

static int ax650_usb_float_is_finite(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

static void ax650_usb_on_command(void *context, const ax650_command_t *command)
{
    const uint32_t received_ms = *(const uint32_t *)context;
    uint32_t saved = kernel_enter_critical();
    rx_state.command = *command;
    rx_state.command_rx_ms = received_ms;
    ++rx_state.command_count;
    kernel_exit_critical(saved);
}

void ax650_usb_init(void)
{
    memset(&rx_state, 0, sizeof(rx_state));
    ax650_parser_init(&rx_parser);
    memset(&tx_pose, 0, sizeof(tx_pose));
    tx_pose.q_wxyz[0] = 1.0f;
    tx_pose.projectile_speed_mps = AX650_TX_SPEED_MPS;
    tx_pose.program_mode = AX650_TX_MODE;
    tx_sequence = 0u;
    tx_last_ms = 0u;
    tx_has_pose = 0u;
    tx_has_gimbal = 0u;
}

void ax650_usb_feed(const uint8_t *data, size_t count, uint32_t received_ms)
{
    ax650_parser_feed(&rx_parser, data, count, ax650_usb_on_command, &received_ms);
}

void ax650_usb_update_quaternion(const float q_wxyz[4], uint32_t sample_time_ms)
{
    if (q_wxyz == NULL)
        return;
    uint32_t saved = kernel_enter_critical();
    memcpy(tx_pose.q_wxyz, q_wxyz, sizeof(tx_pose.q_wxyz));
    tx_pose.imu_sample_time_ms = sample_time_ms;
    tx_pose.valid_flags |= AX650_POSE_VALID_IMU;
    tx_has_pose = 1u;
    kernel_exit_critical(saved);
}

void ax650_usb_update_gimbal(float big_yaw_rel_rad, float small_yaw_rad, float pitch_rad,
                             uint32_t big_yaw_sample_time_ms,
                             uint32_t small_yaw_sample_time_ms,
                             uint32_t pitch_sample_time_ms, uint16_t valid_flags)
{
    uint32_t saved = kernel_enter_critical();
    tx_pose.big_yaw_rel_rad = big_yaw_rel_rad;
    tx_pose.small_yaw_rad = small_yaw_rad;
    tx_pose.pitch_rad = pitch_rad;
    tx_pose.big_yaw_sample_time_ms = big_yaw_sample_time_ms;
    tx_pose.small_yaw_sample_time_ms = small_yaw_sample_time_ms;
    tx_pose.pitch_sample_time_ms = pitch_sample_time_ms;
    const uint16_t component_mask = AX650_POSE_VALID_BIG_YAW | AX650_POSE_VALID_SMALL_YAW |
                                    AX650_POSE_VALID_PITCH |
                                    AX650_POSE_CALIBRATED_SMALL_YAW |
                                    AX650_POSE_CALIBRATED_PITCH | AX650_POSE_GIMBAL_ONLINE;
    tx_pose.valid_flags = (uint16_t)((tx_pose.valid_flags & ~component_mask) |
                                     (valid_flags & component_mask));
    tx_has_gimbal = 1u;
    kernel_exit_critical(saved);
}

size_t ax650_usb_prepare_pose(uint8_t *output, size_t capacity, uint32_t now_ms)
{
    if (output == NULL || (uint32_t)(now_ms - tx_last_ms) < AX650_POSE_PERIOD_MS)
        return 0u;
    ax650_full_pose_t pose;
    uint8_t has_pose, has_gimbal;
    uint32_t saved = kernel_enter_critical();
    pose = tx_pose;
    has_pose = tx_has_pose;
    has_gimbal = tx_has_gimbal;
    kernel_exit_critical(saved);
    if (!has_pose || !has_gimbal)
        return 0u;
    const size_t size = ax650_full_pose_pack(output, capacity, &pose, tx_sequence);
    if (size != 0u)
    {
        ++tx_sequence;
        tx_last_ms = now_ms;
    }
    return size;
}

void ax650_usb_set_projectile_speed(float speed_mps)
{
    if (!ax650_usb_float_is_finite(speed_mps) || speed_mps <= 0.0f)
        speed_mps = AX650_TX_SPEED_MPS;
    uint32_t saved = kernel_enter_critical();
    tx_pose.projectile_speed_mps = speed_mps;
    tx_pose.valid_flags |= AX650_POSE_VALID_PROJECTILE_SPEED;
    kernel_exit_critical(saved);
}

void ax650_usb_set_robot_id(uint8_t robot_id)
{
    uint32_t saved = kernel_enter_critical();
    tx_pose.robot_id = robot_id;
    if (robot_id != 0u)
        tx_pose.valid_flags |= AX650_POSE_VALID_ROBOT_ID;
    else
        tx_pose.valid_flags &= (uint16_t)~AX650_POSE_VALID_ROBOT_ID;
    kernel_exit_critical(saved);
}

void ax650_usb_set_mode(uint8_t mode)
{
    uint32_t saved = kernel_enter_critical();
    tx_pose.program_mode = mode;
    if (mode == AX650_AUTO_AIM_MODE)
        tx_pose.valid_flags |= AX650_POSE_AUTO_AIM_ALLOWED;
    else
        tx_pose.valid_flags &= (uint16_t)~AX650_POSE_AUTO_AIM_ALLOWED;
    kernel_exit_critical(saved);
}

void ax650_usb_get_state(ax650_usb_state_t *out)
{
    if (out == NULL)
        return;
    uint32_t saved = kernel_enter_critical();
    memcpy(out, &rx_state, sizeof(*out));
    kernel_exit_critical(saved);
    out->command_valid = out->command_count != 0u &&
                         (uint32_t)(kernel_tick_ms() - out->command_rx_ms) <=
                             AX650_COMMAND_TIMEOUT_MS;
}
