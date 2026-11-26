/**
 * @file    ax650_usb.c
 * @brief   AX650/RMCV USB 传输状态，复用既有 0x1021/0x0503 编解码
 * @layer   COMPONENT
 * @author  Codex
 */
#include "ax650_usb.h"
#include "robot_infantry_2026.h"
#include "nav_rtos.h"
#include <string.h>

static ax650_parser_t rx_parser;
static ax650_usb_state_t rx_state;
static float tx_quaternion[4];
static float tx_speed_mps;
static uint32_t tx_sample_time_ms;
static uint32_t tx_last_ms;
static uint8_t tx_mode;
static uint8_t tx_robot_id;
static uint8_t tx_sequence;
static uint8_t tx_has_pose;

static int ax650_usb_float_is_finite(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

static void ax650_usb_on_command(void *context, const ax650_command_t *command)
{
    const uint32_t received_ms = *(const uint32_t *)context;
    uint32_t saved = nav_enter_critical();
    rx_state.command = *command;
    rx_state.command_rx_ms = received_ms;
    ++rx_state.command_count;
    nav_exit_critical(saved);
}

void ax650_usb_init(void)
{
    memset(&rx_state, 0, sizeof(rx_state));
    ax650_parser_init(&rx_parser);
    tx_quaternion[0] = 1.0f;
    tx_quaternion[1] = tx_quaternion[2] = tx_quaternion[3] = 0.0f;
    tx_speed_mps = AX650_TX_SPEED_MPS;
    tx_mode = AX650_TX_MODE;
    tx_robot_id = 0u;
    tx_sequence = 0u;
    tx_last_ms = 0u;
    tx_sample_time_ms = 0u;
    tx_has_pose = 0u;
}

void ax650_usb_feed(const uint8_t *data, size_t count, uint32_t received_ms)
{
    ax650_parser_feed(&rx_parser, data, count, ax650_usb_on_command, &received_ms);
}

void ax650_usb_update_quaternion(const float q_wxyz[4], uint32_t sample_time_ms)
{
    if (q_wxyz == NULL)
        return;
    uint32_t saved = nav_enter_critical();
    memcpy(tx_quaternion, q_wxyz, sizeof(tx_quaternion));
    tx_sample_time_ms = sample_time_ms;
    tx_has_pose = 1u;
    nav_exit_critical(saved);
}

size_t ax650_usb_prepare_pose(uint8_t *output, size_t capacity, uint32_t now_ms)
{
    if (output == NULL || (uint32_t)(now_ms - tx_last_ms) < AX650_POSE_PERIOD_MS)
        return 0u;
    float quaternion[4];
    float speed_mps;
    uint32_t sample_time_ms;
    uint8_t mode, robot_id, has_pose;
    uint32_t saved = nav_enter_critical();
    memcpy(quaternion, tx_quaternion, sizeof(quaternion));
    speed_mps = tx_speed_mps;
    sample_time_ms = tx_sample_time_ms;
    mode = tx_mode;
    robot_id = tx_robot_id;
    has_pose = tx_has_pose;
    nav_exit_critical(saved);
    if (!has_pose)
        return 0u;
    const size_t size = ax650_pose_pack(output, capacity, quaternion, speed_mps, mode,
                                         tx_sequence, sample_time_ms, robot_id);
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
    uint32_t saved = nav_enter_critical();
    tx_speed_mps = speed_mps;
    nav_exit_critical(saved);
}

void ax650_usb_set_robot_id(uint8_t robot_id)
{
    uint32_t saved = nav_enter_critical();
    tx_robot_id = robot_id;
    nav_exit_critical(saved);
}

void ax650_usb_set_mode(uint8_t mode)
{
    uint32_t saved = nav_enter_critical();
    tx_mode = mode;
    nav_exit_critical(saved);
}

void ax650_usb_get_state(ax650_usb_state_t *out)
{
    if (out == NULL)
        return;
    uint32_t saved = nav_enter_critical();
    memcpy(out, &rx_state, sizeof(*out));
    nav_exit_critical(saved);
    out->command_valid = out->command_count != 0u &&
                         (uint32_t)(nav_tick_ms() - out->command_rx_ms) <=
                             AX650_COMMAND_TIMEOUT_MS;
}
