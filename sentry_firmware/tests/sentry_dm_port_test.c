/* 达妙协议移植回归：模拟 CAN，覆盖真正的初始化/任务入口。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dm_motor.h"
#include "config.h"

CAN_HandleTypeDef hcan1, hcan2;
static can_t test_can;
static uint8_t last_frame[8];
static unsigned sent_count;

can_t *can_register(can_init_config_t *config)
{
    memset(&test_can, 0, sizeof(test_can));
    test_can.can_handle = config->can_handle;
    test_can.tx_id = config->tx_id;
    test_can.rx_id = config->rx_id;
    test_can.id = config->id;
    test_can.can_module_callback = config->can_module_callback;
    return &test_can;
}

void can_transmit(bus_tx_frame_t *frame, uint8_t length)
{
    can_t *can = (can_t *)frame;
    assert(can == &test_can && can->tx_id == 5 && can->rx_id == 0 && length == 8);
    memcpy(last_frame, can->tx_buff, 8);
    ++sent_count;
}

void pid_init(pid_instance_t *pid, pid_init_config_t *config) { (void)pid; (void)config; }
float pid_calculate(pid_instance_t *pid, float measure, float ref) { (void)pid; return ref - measure; }
void pid_clear(pid_instance_t *pid) { (void)pid; }
void dwt_delay(float delay) { (void)delay; }

/* dm_motor 只在接收回调里把 kernel_tick_ms() 写进 rx_sample_us，主机测试给常量即可。 */
uint32_t kernel_tick_ms(void) { return 0u; }

#include "../components/motor/dm/dm_motor.c"

static void RunCycle(void)
{
    for (unsigned i = 0; i < DM_TASK_CYCLE; ++i)
        dm_motor_task();
}

static void Feedback(uint8_t id, uint8_t state, uint16_t position, uint8_t length)
{
    const uint8_t data[8] = {
        (uint8_t)((state << 4) | id), (uint8_t)(position >> 8), (uint8_t)position,
        0x7f, 0xf7, 0xff, 30, 31,
    };
    memcpy(test_can.rx_buff, data, 8);
    test_can.rx_len = length;
    test_can.can_module_callback(&test_can);
}

int main(void)
{
    dm_motor_mit_init_config_t config = {
        .controller_param_init_config = {.Kp = 54, .Kd = 0.5f},
        .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        .can_init_config = {.can_handle = &hcan1, .tx_id = 5, .rx_id = 0},
    };
    dm_motor_t *motor = dm_motor_mit_init(&config);
    assert(motor && sent_count == 1 && last_frame[7] == 0xfc);
    for (unsigned i = 0; i < 7; ++i) assert(last_frame[i] == 0xff);

    dm_motor_stop(motor);
    for (unsigned i = 0; i < DM_STARTUP_WAIT_MS / DM_TASK_CYCLE; ++i) RunCycle();
    assert(sent_count == 1);
    RunCycle();
    const uint8_t zero[8] = {0x7f,0xff,0x7f,0xf0,0,0,7,0xff};
    assert(memcmp(last_frame, zero, 8) == 0);

    Feedback(6, DM_STATE_ENABLE, 40000, 8);
    Feedback(5, DM_STATE_ENABLE, 40000, 7);
    assert(motor->rx_init_flag == 0);
    Feedback(5, DM_STATE_ENABLE, 40000, 8);
    assert(motor->rx_init_flag == 1 && motor->measure.position_raw == 40000);
    assert(fabsf(motor->measure.position - (40000.0f * 25.0f / 65535.0f - 12.5f)) < 0.0001f);
    assert(motor->measure.velocity_raw == 2047 && motor->measure.torque_raw == 2047);
    assert(motor->measure.T_Mos == 30 && motor->measure.T_Rotor == 31);
    dm_motor_set_mit_angle(motor, motor->measure.position);
    dm_motor_enable(motor);
    RunCycle();
    assert(last_frame[4] != 0); // 有效反馈后才允许位置刚度
    dm_motor_stop(motor);
    RunCycle();
    assert(last_frame[4] == 0 && last_frame[5] == 0 && last_frame[6] == 7);

    motor->recovery_wait_ms = 0;
    motor->rx_refresh_dt = DM_ENABLE_RETRY_MS / DM_TASK_CYCLE;
    for (unsigned i = 0; i < DM_ENABLE_RETRY_MS / DM_TASK_CYCLE; ++i) RunCycle();
    assert(last_frame[7] == 0xfc); // 陈旧 ENABLE 状态下也会重新使能

    Feedback(5, DM_STATE_OVER_CURRENT, 40000, 8);
    sent_count = 0;
    motor->recovery_wait_ms = DM_ENABLE_RETRY_MS;
    RunCycle();
    assert(sent_count == 1 && memcmp(last_frame, zero, 8) != 0 && last_frame[4] == 0);
    assert(motor->measure.state == DM_STATE_OVER_CURRENT);

    dm_motor_send_mit(motor, 100, 100, 1000, 100, 100);
    for (unsigned i = 0; i < 8; ++i) assert(last_frame[i] == 0xff);
    dm_motor_send_mit(motor, -100, -100, -1000, -100, -100);
    for (unsigned i = 0; i < 8; ++i) assert(last_frame[i] == 0);
    dm_motor_send_mit(motor, NAN, NAN, NAN, NAN, NAN);
    assert(memcmp(last_frame, zero, 8) == 0);
    puts("PASS: DM port command/packing/feedback/startup/stop/recovery/limits");
    return 0;
}
