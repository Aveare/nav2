#include "autonomy.h"
#include "ax650_usb.h"
#include "nav_protocol.h"
#include "nav_rtos.h"
#include "robot_infantry_2026.h"
#include "bsp_usb.h"
#include "gimbal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t now_ms;
static uint8_t rx_buf[64];
static USBCallback rx_callback;
static uint8_t tx_busy;
static uint8_t tx_frames[8][NAV_FRAME_SIZE];
static uint16_t tx_lens[8];
static unsigned tx_count;

static float stub_yaw_rad;
static uint64_t stub_yaw_sample_us;
static uint8_t stub_yaw_online;

uint32_t nav_tick_ms(void) { return now_ms; }
uint32_t nav_enter_critical(void) { return 0u; }
void nav_exit_critical(uint32_t saved) { (void)saved; }

uint8_t *USBInit(USB_Init_Config_s config)
{
    rx_callback = config.rx_cbk;
    return rx_buf;
}

uint8_t USBIsTransmitting(void) { return tx_busy; }

uint8_t USBTryTransmit(uint8_t *buffer, uint16_t length)
{
    if (tx_busy || length > NAV_FRAME_SIZE)
        return 1u;
    if (tx_count < 8u)
    {
        memcpy(tx_frames[tx_count], buffer, length);
        tx_lens[tx_count] = length;
        ++tx_count;
    }
    return 0u;
}

void GimbalGetBigYawFeedback(float *rel_rad, uint64_t *sample_us, uint8_t *online)
{
    if (rel_rad) *rel_rad = stub_yaw_rad;
    if (sample_us) *sample_us = stub_yaw_sample_us;
    if (online) *online = stub_yaw_online;
}

static void deliver(const uint8_t *data, uint16_t length)
{
    memcpy(rx_buf, data, length);
    rx_callback(length);
}

static void print_hex(const uint8_t *bytes, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        printf("%02x", bytes[i]);
    putchar('\n');
}

int main(void)
{
    assert(nav_protocol_crc16((const uint8_t *)"123456789", 9u) == 0x4b37u);
    assert(ax650_pose_crc8((const uint8_t *)"1234", 4u) != 0u);

    uint8_t nav_golden[NAV_FRAME_SIZE];
    nav_frame_t gnav = {.type = NAV_MSG_COMMAND, .sequence = 7u, .sample_us = 123456789u,
                        .value = {0.25f, -0.125f, 0.5f}, .flags = NAV_FLAG_VALID};
    nav_protocol_encode(&gnav, nav_golden);
    print_hex(nav_golden, sizeof(nav_golden));

    /* Golden 0x0503 target frame matching Python TargetFrame(3, 30, -5, 0.25, -0.125, 1, -2, 4.5, 1, 3) */
    uint8_t t_raw[39];
    t_raw[0] = 0xA5u;
    t_raw[1] = 30u; t_raw[2] = 0u;
    t_raw[3] = 3u;
    t_raw[4] = ax650_pose_crc8(t_raw, 4u);
    t_raw[5] = 0x03u; t_raw[6] = 0x05u;
    float t_floats[7] = {30.0f, -5.0f, 0.25f, -0.125f, 1.0f, -2.0f, 4.5f};
    memcpy(&t_raw[7], t_floats, sizeof(t_floats));
    t_raw[35] = 1u;
    t_raw[36] = 3u;
    uint16_t t_crc = ax650_pose_crc16(t_raw, 37u);
    t_raw[37] = (uint8_t)(t_crc & 0xffu);
    t_raw[38] = (uint8_t)(t_crc >> 8);
    print_hex(t_raw, sizeof(t_raw));

    uint8_t p_raw[AX650_POSE_FRAME_SIZE];
    float q_id[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    size_t p_len = ax650_pose_pack(p_raw, sizeof(p_raw), q_id, 0.0f, 0u, 0u, 950u, 0u);
    assert(p_len == AX650_POSE_FRAME_SIZE);
    print_hex(p_raw, p_len);

    now_ms = 100u;
    tx_count = 0u;
    tx_busy = 0u;
    autonomy_init();

    /* Sync request -> response */
    uint8_t sync_bytes[NAV_FRAME_SIZE];
    nav_frame_t sreq = {.type = NAV_MSG_SYNC_REQUEST, .sequence = 42u, .sample_us = 999u};
    nav_protocol_encode(&sreq, sync_bytes);
    deliver(sync_bytes, sizeof(sync_bytes));
    autonomy_task();
    assert(tx_count == 1u && tx_lens[0] == NAV_FRAME_SIZE);
    nav_parser_t check_parser;
    memset(&check_parser, 0, sizeof(check_parser));
    nav_frame_t sresp;
    int got = 0;
    for (unsigned i = 0; i < NAV_FRAME_SIZE; ++i)
        got |= nav_protocol_feed(&check_parser, tx_frames[0][i], &sresp);
    assert(got && sresp.type == NAV_MSG_SYNC_RESPONSE && sresp.sequence == 42u && sresp.sample_us == 100000u);

    /* Split NV command delivery + SENTRY_AUTONOMY_ENABLE gate check */
    now_ms = 200u;
    deliver(nav_golden, 9u);
    autonomy_task();
    sentry_auto_cmd_t cmd;
    assert(!autonomy_get_command(&cmd));
    deliver(nav_golden + 9u, NAV_FRAME_SIZE - 9u);
    autonomy_task();
#if SENTRY_AUTONOMY_ENABLE
    assert(autonomy_get_command(&cmd));
    assert(fabsf(cmd.vx - 250.0f) < 1e-3f && fabsf(cmd.vy - 125.0f) < 1e-3f);
    assert(fabsf(cmd.wz - 28.64789f) < 1e-3f);
#else
    assert(!autonomy_get_command(&cmd));
    assert(cmd.vx == 0.0f && cmd.vy == 0.0f && cmd.wz == 0.0f);
#endif

    /* Duplicate sequence rejected & watchdog expires after 300 ms */
    now_ms = 350u;
    deliver(nav_golden, NAV_FRAME_SIZE);
    autonomy_task();
    now_ms = 505u;
    autonomy_task();
    assert(!autonomy_get_command(&cmd));

    /* Shared-CDC 0x0503 target RX + 100 ms timeout + 0x1021 pose TX */
    now_ms = 900u;
    deliver(t_raw, 15u);
    deliver(t_raw + 15u, sizeof(t_raw) - 15u);
    ax650_usb_update_quaternion(q_id, 950u);
    tx_count = 0u;
    stub_yaw_rad = 0.5f;
    stub_yaw_sample_us = 900000u;
    stub_yaw_online = 1u;
    autonomy_task();
    ax650_usb_state_t aim;
    ax650_usb_get_state(&aim);
    assert(aim.command_valid && aim.command.target_id == 3u && fabsf(aim.command.yaw - 30.0f) < 1e-3f);
    assert(tx_count == 2u && tx_lens[0] == NAV_FRAME_SIZE && tx_lens[1] == AX650_POSE_FRAME_SIZE);
    now_ms = 1005u;
    ax650_usb_get_state(&aim);
    assert(!aim.command_valid);

    puts("shaobin firmware navigation and auto-aim USB tests passed");
    return 0;
}
