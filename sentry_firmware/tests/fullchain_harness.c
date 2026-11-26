/**
 * @file    fullchain_harness.c
 * @brief   B0 host 整链 harness:真实控制链 + 确定性输入脚本 → 金标准 CSV
 * @layer   TESTS
 * @author  小猫大王
 * @date    2026-11-22
 *
 * 真实编译:robot_cmd 五件 → gimbal_task(gimbal+recenter)→ shoot_task(heat_guard)
 *   → chassis_task(kinematics)→ dji_motor(级联 PID + CAN 编码/解码)→ tx 帧记录;
 *   remote/message_center/daemon/telemetry/calib(无标定回退)/controller/user_lib 真实。
 * 桩(固定行为,不在迁移面):usart(直注入 DBUS 回调)、can(记录 tx 帧 + 反馈注入)、
 *   referee/UI、bsp_fault、bsp_flash(无标定)、super_cap、DWT 时钟。
 * 电机闭环走**真实路径**:harness 按 C620 报文格式造反馈字节 → dji_motor 真实
 *   dji_motor_decode 解析;电流→转速模型为 harness 固定常量(不是迁移面)。
 * 范围注记:bsp_usart 消费泵语义不在本金标准内(ax650 测试 + B4b 对拍承载);
 *   remote 与 chassis_remote_test 同构,回调直注入。
 *
 * 金标准纪律:CSV 由 **迁移前代码** 生成入库(.Doc/gate/arch-baseline/),B5/B6/B8
 *   重放后用 tools/check_golden.py 比对(rtol/atol ≤1e-6 + 模式字精确 + tick 对齐)。
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "robot_def.h"
#include "robot_cmd.h"
#include "gimbal.h"
#include "shoot.h"
#include "chassis.h"
#include "dji_motor.h"
#include "remote_control.h"
#include "msg.h"
#include "daemon.h"
#include "telemetry.h"
#include "calib.h"
#include "heat_guard.h"
#include "ins_task.h"
#include "referee_task.h"
#include "referee_ui.h"
#include "super_cap.h"
#include "fault_port.h"
#include "flash_port.h"
#include "usart_port.h"
#include "usart.h"
#include "ax650_uart1.h"
#include "dwt_port.h"
#include "log_port.h"
#include "general_def.h"

/* ================= 确定时钟:1 tick = 1 ms ================= */
static uint32_t fake_ms = 1000;
float dwt_get_delta_t(uint32_t *count)
{
    *count += 1;
    return 0.001f;
}
uint32_t dwt_get_timeline_ms(void)
{
    return ++fake_ms;
}
int kernel_is_in_isr(void) { return 0; } /* msg_publish ISR guard: host always no */

/* ================= usart 桩 ================= */
UART_HandleTypeDef huart1, huart3, huart6;
static usart_t remote_usart;
static unsigned usart_tx_cnt;

usart_t *usart_register(usart_init_config_t *config)
{
    if (config->recv_buff_size == 18)
    { /* 仅 remote 走真实实例 */
        assert(config->usart_handle == &huart3);
        remote_usart.usart_handle = config->usart_handle;
        remote_usart.recv_buff_size = config->recv_buff_size;
        remote_usart.module_callback = config->module_callback;
        return &remote_usart;
    }
    return (usart_t *)&usart_tx_cnt;
}
void usart_service_init(usart_t *i)
{
    (void)i;
}
void usart_process_all(void)
{
}
void usart_send(usart_t *i, uint8_t *b, uint16_t n, usart_transfer_mode_t m)
{
    (void)i;
    (void)b;
    (void)n;
    (void)m;
    ++usart_tx_cnt;
}
uint8_t usart_try_send_it(usart_t *i, uint8_t *b, uint16_t n)
{
    (void)i;
    (void)b;
    (void)n;
    ++usart_tx_cnt;
    return 1;
}
uint8_t usart_is_ready(usart_t *i)
{
    (void)i;
    return 1;
}
uint8_t usart_configure8_e1(UART_HandleTypeDef *h, uint32_t baud)
{
    (void)h;
    (void)baud;
    return 1;
}
uint32_t usart_get_err_count(usart_t *i)
{
    (void)i;
    return 0;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t s, uint32_t t)
{
    (void)h;
    (void)d;
    (void)s;
    (void)t;
    ++usart_tx_cnt;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t s)
{
    (void)h;
    (void)d;
    (void)s;
    ++usart_tx_cnt;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *h, uint8_t *d, uint16_t s)
{
    (void)h;
    (void)d;
    (void)s;
    ++usart_tx_cnt;
    return HAL_OK;
}

/* ================= can 桩:tx 帧记录 + 反馈注入注册表 ================= */
CAN_HandleTypeDef hcan1, hcan2;
#define FRAME_SLOTS 5 /* [0]=0x200 [1]=0x1ff [2]=0x1fe [3]=0x2ff [4]=0x2fe */
static struct
{
    uint8_t buff[8];
    unsigned cnt;
} frames[2][FRAME_SLOTS]; /* [bus0=can1][slot] */

static int slot_for(uint32_t tx_id)
{
    switch (tx_id)
    {
    case 0x200u:
        return 0;
    case 0x1ffu:
        return 1;
    case 0x1feu:
        return 2;
    case 0x2ffu:
        return 3;
    case 0x2feu:
        return 4;
    default:
        return -1;
    }
}

#define MAX_MOTORS 16
typedef struct
{
    can_t *inst;
    int bus; /* 0=can1 1=can2 */
} RegMotor;
static RegMotor reg[MAX_MOTORS];
static unsigned nreg = 0;

int can_port_bus_of(const CAN_HandleTypeDef *h) /* B5 bus_id 桩:与主实现同语义(0=can1) */
{
    return (h == &hcan2) ? 1 : 0;
}

can_t *can_register(can_init_config_t *config)
{
    static can_t pool[MAX_MOTORS];
    assert(nreg < MAX_MOTORS);
    can_t *inst = &pool[nreg];
    memset(inst, 0, sizeof(*inst));
    inst->can_handle = config->can_handle;
    inst->tx_id = config->tx_id;
    inst->id = config->id;
    inst->can_module_callback = config->can_module_callback;
    reg[nreg].inst = inst;
    reg[nreg].bus = (config->can_handle == &hcan2) ? 1 : 0;
    ++nreg;
    return inst;
}

void can_transmit(bus_tx_frame_t *inst, uint8_t length) /* B4a:伪实例签名 */
{
    assert(inst && length == 8);
    int bus = (inst->can_handle == &hcan2) ? 1 : 0;
    int slot = slot_for(inst->tx_id);
    assert(slot >= 0);
    memcpy(frames[bus][slot].buff, inst->tx_buff, 8);
    ++frames[bus][slot].cnt;
}

/* ================= referee/UI/fault/flash/supercap 桩 ================= */
static referee_info_t referee_fix;
char _etext; /* calib.c 的 flash 容量自检引用代码段末符号 */

/* bsp_log 的 log_float_to_str 语义(整数.3 位小数,带符号)—— telemetry/各任务引用 */
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

/* AX650 下行命令桩:bench 链路关闭(ax650_uart1_get_state 全零 = 无有效命令) */
static ax650_uart1_state_t ax650_state;
static uint8_t ax650_tx_mode;
void ax650_uart1_set_mode(uint8_t mode)
{
    ax650_tx_mode = mode;
}
void ax650_uart1_get_state(ax650_uart1_state_t *out)
{
    memset(out, 0, sizeof(*out));
}
referee_info_t *referee_get_info(void)
{
    return &referee_fix;
}
referee_info_t *referee_ui_task_init(UART_HandleTypeDef *h, referee_interactive_info_t *d)
{
    (void)h;
    (void)d;
    return &referee_fix;
}
void referee_ui_delete(referee_id_t *a, uint8_t b, uint8_t c)
{
    (void)a;
    (void)b;
    (void)c;
}
void fault_init(void)
{
}
void fault_record_and_reset(uint32_t c)
{
    (void)c;
}
void fault_record_detail(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f,
                       uint32_t g)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
}
void fault_dump_to_uart(void)
{
}
uint8_t fault_hold_active(void)
{
    return 0;
}
void flash_erase_address(uint32_t a, uint16_t l)
{
    (void)a;
    (void)l;
}
int8_t flash_write_single_address(uint32_t a, uint32_t *b, uint32_t l)
{
    (void)a;
    (void)b;
    (void)l;
    return -1;
}
int8_t flash_write_muli_address(uint32_t s, uint32_t e, uint32_t *b, uint32_t l)
{
    (void)s;
    (void)e;
    (void)b;
    (void)l;
    return -1;
}
void flash_read(uint32_t a, uint32_t *b, uint32_t l)
{
    for (uint32_t i = 0; i < l; ++i)
        b[i] = 0xFFFFFFFFu;
    (void)a;
}
super_cap_t *super_cap_init(super_cap_init_config_t *c)
{
    (void)c;
    return NULL;
}
void super_cap_send(super_cap_t *i, uint8_t *d)
{
    (void)i;
    (void)d;
}

/* ================= IMU 脚本输入(传感器=输入,非被控对象) ================= */
attitude_t ins_state;
attitude_t *gimbal_imu_data = &ins_state; /* gimbal_internal.h 的 extern 契约 */
attitude_t *ins_init(void)
{
    return &ins_state;
}

/* ================= 输入脚本(确定性场景,200Hz 链 + 1kHz 级联) ================= */
#define HARNESS_TICKS 700
#define CHAIN_DIV     5

typedef struct
{
    uint16_t tick;
    int r_h, r_v, l_h, l_v, dial;
    unsigned right, left;
    float imu_pitch, gyro0, gyro2;
} ScriptRow;

/* 拨杆值:1=上 2=下 3=中(RC_SW_*)。解锁=右上+左中+全回中(cmd_authority.c);
 * 拨轮负向=扳机(-240 入区 / -220 松手沿=单发,shoot.h);正拨 ≤250 在
 * SHOOT_DIAL_STOP_THRESHOLD(300)以下,不触发拨轮急停。 */
static const ScriptRow script[] = {
    {0, 0, 0, 0, 0, 0, 3, 3, 0.0f, 0.0f, 0.0f},
    {100, 0, 0, 0, 0, 0, 1, 3, 0.0f, 0.0f, 0.0f},        /* 解锁沿(本周期仍零力) */
    {121, 0, 0, 0, 0, 0, 3, 2, 0.0f, 0.0f, 0.0f},        /* 右中+左下:跟随运行 */
    {150, 0, 200, 0, 0, 0, 3, 2, 0.0f, 0.0f, 0.0f},      /* vx 前进 */
    {200, 0, 200, 0, 0, 200, 3, 2, 5.0f, 0.0f, 0.0f},    /* 正拨拨轮 + pitch 抬头 */
    {250, 0, 200, 300, 0, 200, 3, 2, 5.0f, 0.5f, 0.0f},  /* vy + wz */
    {300, 0, 200, 300, 0, -240, 3, 2, 5.0f, 0.5f, 0.5f}, /* 拨轮入扳机区:摩擦轮开 */
    {350, 0, 200, 300, 0, -240, 3, 2, 5.0f, 0.5f, 0.5f},
    {400, 0, 200, 300, 0, -240, 3, 2, 5.0f, 0.5f, 0.5f},
    {430, 0, 200, 300, 0, -200, 3, 2, 5.0f, 0.5f, 0.5f}, /* 松手沿:登记单发 */
    {500, 0, 200, 300, 0, 0, 3, 2, 5.0f, 0.0f, 0.0f},
    {600, 0, 0, 0, 0, 0, 3, 1, 0.0f, 0.0f, 0.0f}, /* 左上急停 */
    {700, 0, 0, 0, 0, 0, 3, 1, 0.0f, 0.0f, 0.0f},
};

static const ScriptRow *row_at(uint16_t t)
{
    const ScriptRow *cur = &script[0];
    for (unsigned i = 0; i < sizeof(script) / sizeof(script[0]); ++i)
        if (script[i].tick <= t)
            cur = &script[i];
    return cur;
}

static void RcApply(const ScriptRow *r)
{
    uint16_t ch[5] = {(uint16_t)(r->r_h + 1024), (uint16_t)(r->r_v + 1024),
                      (uint16_t)(r->l_h + 1024), (uint16_t)(r->l_v + 1024),
                      (uint16_t)(r->dial + 1024)};
    memset(remote_usart.recv_buff, 0, sizeof(remote_usart.recv_buff));
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned b = 0; b < 11; ++b)
            if (ch[c] & (1u << b))
                remote_usart.recv_buff[(c * 11 + b) / 8] |= (uint8_t)(1u << ((c * 11 + b) % 8));
    remote_usart.recv_buff[5] |= (uint8_t)((r->right & 3u) << 4);
    remote_usart.recv_buff[5] |= (uint8_t)((r->left & 3u) << 6);
    remote_usart.recv_buff[16] = (uint8_t)(ch[4] & 0xff);
    remote_usart.recv_buff[17] = (uint8_t)(ch[4] >> 8);
    remote_usart.recv_len = 18;
    remote_usart.module_callback();
}

/* ================= 电机闭环模型(harness 固定常量) ================= */
/* 反馈:C620 报文([ecd BE][rpm BE][cur BE][temp]);速度积分 current 码 → 转子
 * deg/s(满码 16384 → ACC deg/s^2);ecd 按 8192/360 度推进。 */
#define MODEL_ACC  90000.0f
#define MODEL_TEMP 30u
static float model_aps[MAX_MOTORS];
static uint16_t model_ecd[MAX_MOTORS];

static void ModelFeedback(void)
{
    for (unsigned i = 0; i < nreg; ++i)
    {
        uint8_t *rx = reg[i].inst->rx_buff;
        model_ecd[i] = (uint16_t)(model_ecd[i] + (int)(model_aps[i] * 0.001f * (8192.0f / 360.0f)));
        int16_t rpm = (int16_t)(model_aps[i] / 6.0f);
        int16_t cur = (int16_t)model_aps[i]; /* 占位,下方由 tx 帧解码覆盖前的上一拍值 */
        (void)cur;
        rx[0] = (uint8_t)(model_ecd[i] >> 8);
        rx[1] = (uint8_t)(model_ecd[i]);
        rx[2] = (uint8_t)((uint16_t)rpm >> 8);
        rx[3] = (uint8_t)(rpm);
        rx[4] = 0;
        rx[5] = 0;
        rx[6] = MODEL_TEMP;
        rx[7] = 0;
        reg[i].inst->rx_len = 8;
        if (reg[i].inst->can_module_callback)
            reg[i].inst->can_module_callback(reg[i].inst);
    }
}

static void ModelUpdate(void)
{
    /* 电流解码:按 dji_motor.c 的 sender_assignment 静态分组表(镜像,分组改动须同步)
     * group0=c1 0x1ff  group1=c1 0x200  group2=c1 0x2fe(GM6020 电流模式,本车 yaw)
     * group3=c2 0x1ff  group4=c2 0x200  group5=c2 0x2fe;槽内偏移=message_num */
    static const int group_bus[6] = {0, 0, 0, 1, 1, 1};
    static const uint32_t group_base[6] = {0x1ff, 0x200, 0x2fe, 0x1ff, 0x200, 0x2fe};
    for (unsigned i = 0; i < nreg; ++i)
    {
        dji_motor_t *m = (dji_motor_t *)reg[i].inst->id;
        if (m == NULL || m->sender_group > 5)
            continue;
        int slot = slot_for(group_base[m->sender_group]);
        int sub = m->message_num;
        int16_t cur = (int16_t)((frames[group_bus[m->sender_group]][slot].buff[sub * 2] << 8) |
                                frames[group_bus[m->sender_group]][slot].buff[sub * 2 + 1]);
        model_aps[i] += (cur / 16384.0f) * MODEL_ACC * 0.001f;
        if (model_aps[i] > 20000.0f)
            model_aps[i] = 20000.0f;
        if (model_aps[i] < -20000.0f)
            model_aps[i] = -20000.0f;
    }
}

/* robot_state 定义在 robot_cmd.c(非 static,T02 白名单锚注同文件) */
extern robot_status_e robot_state;

int main(void)
{
    referee_fix.referee_id.Robot_ID = 101;

    calib_init();
    telemetry_init();
    cmd_init(); /* 内部 remote_control_init(&huart3) */
    gimbal_init();
    shoot_init();
    chassis_init();

    static chassis_ctrl_cmd_t ch_cmd;
    static gimbal_ctrl_cmd_t gi_cmd;
    static shoot_ctrl_cmd_t sh_cmd;
    msg_sub_t ch_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    msg_sub_t gi_sub = msg_sub("gimbal_cmd", sizeof(gimbal_ctrl_cmd_t));
    msg_sub_t sh_sub = msg_sub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));

    FILE *f = fopen(".Doc/gate/arch-baseline/g8b-fullchain.csv", "w");
    assert(f);
    fprintf(
        f,
        "tick,robot_state,"
        "chassis_mode,vx_ref,vy_ref,wz_ref,"
        "yaw_ref,pitch_ref,friction_mode,load_mode,heat_state,"
        "c1_0200_b0,c1_0200_b1,c1_0200_b2,c1_0200_b3,c1_0200_b4,c1_0200_b5,c1_0200_b6,c1_0200_b7,"
        "c1_02fe_b0,c1_02fe_b1,c1_02fe_b2,c1_02fe_b3,c1_02fe_b4,c1_02fe_b5,c1_02fe_b6,c1_02fe_b7,"
        "c2_0200_b0,c2_0200_b1,c2_0200_b2,c2_0200_b3,c2_0200_b4,c2_0200_b5,c2_0200_b6,c2_0200_b7,"
        "usart_tx\n");

    for (uint16_t t = 0; t < HARNESS_TICKS; ++t)
    {
        const ScriptRow *r = row_at(t);
        RcApply(r);
        ins_state.YawTotalAngle = 0.0f;
        ins_state.Yaw = 0.0f;
        ins_state.Pitch = r->imu_pitch;
        ins_state.Roll = 0.0f;
        ins_state.Gyro[0] = r->gyro0;
        ins_state.Gyro[1] = 0.0f;
        ins_state.Gyro[2] = r->gyro2;

        ModelFeedback(); /* 上一拍模型的反馈 → 真实 dji_motor_decode 解析 */

        if (t % CHAIN_DIV == 0)
        {
            cmd_task();
            gimbal_task();
            shoot_task();
            chassis_task();
            while (msg_recv(ch_sub, &ch_cmd) == 1)
            {
            }
            while (msg_recv(gi_sub, &gi_cmd) == 1)
            {
            }
            while (msg_recv(sh_sub, &sh_cmd) == 1)
            {
            }
        }
        dji_motor_control(); /* MOTOR 1kHz:级联 PID → CAN 编码 → 帧记录 */

        ModelUpdate(); /* 本拍 current → 速度积分,供下一拍反馈 */

        if (t % CHAIN_DIV == 0)
        {
            fprintf(f,
                    "%u,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,"
                    "%u,%u,%u,%u,%u,%u,%u,%u,"
                    "%u,%u,%u,%u,%u,%u,%u,%u,"
                    "%u,%u,%u,%u,%u,%u,%u,%u,"
                    "%u\n",
                    t, (int)robot_state, (int)ch_cmd.chassis_mode, ch_cmd.vx, ch_cmd.vy, ch_cmd.wz,
                    gi_cmd.yaw, gi_cmd.pitch, (int)sh_cmd.friction_mode, (int)sh_cmd.load_mode,
                    (int)heat_guard_get_state(), frames[0][0].buff[0], frames[0][0].buff[1],
                    frames[0][0].buff[2], frames[0][0].buff[3], frames[0][0].buff[4],
                    frames[0][0].buff[5], frames[0][0].buff[6], frames[0][0].buff[7],
                    frames[0][4].buff[0], frames[0][4].buff[1], frames[0][4].buff[2],
                    frames[0][4].buff[3], frames[0][4].buff[4], frames[0][4].buff[5],
                    frames[0][4].buff[6], frames[0][4].buff[7], frames[1][0].buff[0],
                    frames[1][0].buff[1], frames[1][0].buff[2], frames[1][0].buff[3],
                    frames[1][0].buff[4], frames[1][0].buff[5], frames[1][0].buff[6],
                    frames[1][0].buff[7], usart_tx_cnt);
        }
    }
    fclose(f);
    printf("PASS: fullchain %d ticks -> .Doc/gate/arch-baseline/g8b-fullchain.csv\n",
           HARNESS_TICKS);
    return 0;
}
