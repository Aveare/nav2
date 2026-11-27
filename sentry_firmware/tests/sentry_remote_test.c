/**
 * @file    chassis_remote_test.c
 * @brief   主机测试:CMD 层仲裁与底盘/云台/发射命令断言
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-27
 */
/* 编译真实CMD应用、DBUS遥控、消息中心与守护线程,仅模拟串口收发.
 * 仲裁输出通过订阅话题断言,不依赖底盘/电机. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sentry_def.h"
#include "general_def.h" // MATH_ABS
#include "dji_motor.h" // DJI_ECD_TO_DEG:云台反馈桩换算用,避免再抄一个魔数
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "msg.h"
#include "daemon.h"
#include "telemetry.h"
#include "usart_port.h"
#include "ax650_uart1.h"
#include "vision_link.h"
#include "autonomy.h"

UART_HandleTypeDef huart3; // main.h桩中声明,此处定义
static usart_t remote_usart;
static msg_sub_t chassis_sub, gimbal_sub, shoot_sub;
static chassis_ctrl_cmd_t chassis_cmd_ut;
static sentry_gimbal_cmd_t gimbal_cmd_ut;
static shoot_ctrl_cmd_t shoot_cmd_ut;
static ax650_uart1_state_t ax650_state;
static uint8_t ax650_tx_mode;

static uint32_t fake_ms = 1000;
float dwt_get_delta_t(uint32_t *count)
{
    *count += 1;
    return 0.001f;
}
uint32_t dwt_get_timeline_ms(void)
{
    return ++fake_ms; // 每1ms
}
int kernel_is_in_isr(void) { return 0; } /* msg_publish ISR 守卫桩:主机恒否 */

void ax650_uart1_set_mode(uint8_t mode)
{
    ax650_tx_mode = mode;
}
void ax650_uart1_get_state(ax650_uart1_state_t *out)
{
    *out = ax650_state;
}

/* 视觉链路桩:本用例只验证手动 DBUS 链路,自瞄恒为无目标。 */
void vision_link_set_mode(uint8_t mode)
{
    (void)mode;
}
void vision_link_get_state(vision_state_t *out)
{
    memset(out, 0, sizeof(*out));
}

/* 导航桩:本用例不验证自动导航,恒无有效命令。 */
int autonomy_get_command(sentry_auto_cmd_t *command)
{
    memset(command, 0, sizeof(*command));
    return 0;
}

/* log_float_to_str桩:与bsp_log.c相同语义(整数.3位小数,带符号),供telemetry链接 */
void log_float_to_str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = MATH_ABS(head);
    point = MATH_ABS(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}

usart_t *usart_register(usart_init_config_t *config)
{
    assert(config->usart_handle == &huart3);
    assert(config->recv_buff_size == 18);
    remote_usart.usart_handle = config->usart_handle;
    remote_usart.recv_buff_size = config->recv_buff_size;
    remote_usart.module_callback = config->module_callback;
    return &remote_usart;
}
void usart_service_init(usart_t *instance)
{
    (void)instance;
}

/* DJI DBUS 18字节帧:4路11位摇杆(ch0右横,ch1右纵,ch2左横,ch3左纵),
 * byte5低2位右拨杆/高2位左拨杆(1上2下3中),byte16-17拨轮11位. */
static void RcFrame(int r_h, int r_v, int l_h, int l_v, unsigned right, unsigned left, int dial,
                    uint16_t len)
{
    uint16_t ch[5] = {(uint16_t)(r_h + 1024), (uint16_t)(r_v + 1024), (uint16_t)(l_h + 1024),
                      (uint16_t)(l_v + 1024), (uint16_t)(dial + 1024)};
    memset(remote_usart.recv_buff, 0, sizeof(remote_usart.recv_buff));
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned b = 0; b < 11; ++b)
            if (ch[c] & (1u << b))
                remote_usart.recv_buff[(c * 11 + b) / 8] |= (uint8_t)(1u << ((c * 11 + b) % 8));
    remote_usart.recv_buff[5] |= (uint8_t)((right & 3u) << 4);
    remote_usart.recv_buff[5] |= (uint8_t)((left & 3u) << 6);
    remote_usart.recv_buff[16] = (uint8_t)(ch[4] & 0xff);
    remote_usart.recv_buff[17] = (uint8_t)(ch[4] >> 8);
    remote_usart.recv_len = len;
    remote_usart.module_callback();
}

/* 跑一个CMD周期,并从话题取回三路发布值 */
static void Pump(void)
{
    cmd_task();
    assert(msg_recv(chassis_sub, &chassis_cmd_ut) == 1);
    assert(msg_recv(gimbal_sub, &gimbal_cmd_ut) == 1);
    assert(msg_recv(shoot_sub, &shoot_cmd_ut) == 1);
}

static void ExpectSafe(void)
{
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(gimbal_cmd_ut.mode == GIMBAL_ZERO_FORCE);
    assert(shoot_cmd_ut.shoot_mode == SHOOT_OFF);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF);
    assert(shoot_cmd_ut.load_mode == LOADER_STOP);
}

/* 原解锁流程:右上+左中+全回中;解锁当周期仍保持零力 */
static void Unlock(void)
{
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_UP, REMOTE_RC_SW_MID, 0, 18);
    Pump();
    ExpectSafe();
}

/* 解锁后右中+左下:跟随模式+陀螺闭环，摩擦轮保持默认关闭 */
static void ArmFollow(void)
{
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
#else
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW);
#endif
    assert(gimbal_cmd_ut.mode == GIMBAL_GYRO_MODE);
    assert(shoot_cmd_ut.shoot_mode == SHOOT_ON);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF);
    assert(shoot_cmd_ut.load_mode == LOADER_STOP);
}


int main(void)
{
    cmd_init();
    chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(sentry_gimbal_cmd_t));
    shoot_sub = msg_sub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));
    Pump(); ExpectSafe();
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump(); ExpectSafe();
    Unlock(); ArmFollow();
    RcFrame(0,660,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump();
    assert(chassis_cmd_ut.vx == CHASSIS_MAX_SPEED && chassis_cmd_ut.vy == 0);
    RcFrame(0,-660,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump();
    assert(chassis_cmd_ut.vx == -CHASSIS_MAX_SPEED && chassis_cmd_ut.vy == 0);
    RcFrame(660,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump();
    assert(chassis_cmd_ut.vy == CHASSIS_MAX_SPEED);
    RcFrame(-660,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump();
    assert(chassis_cmd_ut.vy == -CHASSIS_MAX_SPEED);
    RcFrame(0,0,660,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_MID,0,18); Pump();
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW && chassis_cmd_ut.wz == -CHASSIS_ROTATE_SPEED);
    assert(gimbal_cmd_ut.small_yaw_rate_deg_s == 0);
    RcFrame(0,0,660,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump();
    assert(chassis_cmd_ut.wz == 0 && gimbal_cmd_ut.small_yaw_rate_deg_s == -SENTRY_SMALL_YAW_STICK_DIR * SENTRY_SMALL_YAW_STICK_RATE_DEG_S);
    RcFrame(0,0,0,0,REMOTE_RC_SW_DOWN,REMOTE_RC_SW_MID,0,18); Pump();
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ROTATE && chassis_cmd_ut.offset_angle == 0);
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_MID,0,18); Pump();
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW);
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_MID,-660,18); Pump();
    assert(shoot_cmd_ut.friction_mode == FRICTION_ON && shoot_cmd_ut.load_mode == LOADER_STOP);
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_MID,0,18); Pump();
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF && shoot_cmd_ut.load_mode == LOADER_SINGLE);
    const float yaw = gimbal_cmd_ut.big_yaw;
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_MID,0,18);
    remote_usart.recv_buff[6] = 100;
    remote_usart.module_callback(); Pump();
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW && chassis_cmd_ut.wz < 0);
    assert(chassis_cmd_ut.wz >= -SENTRY_CHASSIS_MOUSE_RATE_DEG_S);
    assert(gimbal_cmd_ut.big_yaw == yaw && gimbal_cmd_ut.small_yaw_rate_deg_s == 0);
    RcFrame(0,0,0,0,REMOTE_RC_SW_UP,REMOTE_RC_SW_MID,0,18); Pump(); ExpectSafe();
    Unlock(); ArmFollow();
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,17); Pump(); for(int i=0;i<20;++i) daemon_task(); Pump(); ExpectSafe();
    RcFrame(0,0,0,0,REMOTE_RC_SW_MID,REMOTE_RC_SW_DOWN,0,18); Pump(); ExpectSafe();
    puts("PASS: sentry DBUS four-way drive, steering/small-yaw split, spin exit, dial, mouse and safety");
    return 0;
}
