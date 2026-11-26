/**
 * @file    can_filter_golden.c
 * @brief   B0-⑥c CAN 过滤器金标准:真实 can_port.c + HAL 桩 → 过滤器分配 CSV
 * @layer   TESTS
 * @author  小猫大王
 * @date    2026-11-22
 *
 * 依据方案 P1-3/B4a:FIFO 归属=注册顺序 是一等不变量,迁移(B4a bus 抽象)前后
 * 过滤器分配必须逐字段一致。本程序按 **真实注册顺序**(robot.c:init 序)
 * 用真实 bsp_can.c 的 can_register→can_add_filter 路径生成过滤器表,入库
 * .Doc/gate/arch-baseline/can-filter-golden.csv;B4a 重放后 diff 必须为空。
 *
 * 注册序与 rx_id(镜像 robot.c 初始化序与 dji_motor 分组 rx 规则):
 *   yaw GM6020 id5 CAN1 rx=0x205(gimbal_init)
 *   pitch C620 id4 CAN2 rx=0x204(gimbal_init)
 *   fricL id1/fricR id2/loader id3 CAN2 rx=0x201..203(shoot_init)
 *   wheels LF4/RF1/LB2/RB3 CAN1 rx=0x201..204(chassis_init,顺序=chassis.c)
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "can_port.h"
#include "main.h"
#include "config.h" /* 电机电调 ID 唯一来源(config 子头) */

CAN_HandleTypeDef hcan1, hcan2;

/* B4a:can_port.c 的丢帧限流引用 DWT 时钟,主机桩给单调假钟 */
static uint32_t golden_ms;
uint32_t dwt_get_timeline_ms(void) { return ++golden_ms; }

/* bsp_can.c 的临界区(FreeRTOS BASEPRI)主机桩:单线程,进出即置零 */
static uint32_t basepri_shadow;
uint32_t __get_BASEPRI(void)
{
    return basepri_shadow;
}
void __set_BASEPRI(uint32_t v)
{
    basepri_shadow = v;
}

/* B8:can_port.c 经 log_port.h 引用 log_printf;单测不验日志,给空实现 */
int log_printf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

/* ---- HAL 记录桩 ---- */
static FILE *g_csv;
static unsigned row_cnt;

static const char *bus_name(const CAN_HandleTypeDef *h)
{
    return h == &hcan2 ? "CAN2" : "CAN1";
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, const CAN_FilterTypeDef *f)
{
    fprintf(g_csv, "filter,%s,%u,%u,%u,%u,%u,%u,%u\n", bus_name(hcan), f->FilterBank,
            f->FilterFIFOAssignment, f->FilterIdLow, f->SlaveStartFilterBank, f->FilterMode,
            f->FilterScale, f->FilterActivation);
    ++row_cnt;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    fprintf(g_csv, "start,%s,,,,,,,\n", bus_name(hcan));
    return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t it)
{
    fprintf(g_csv, "notify,%s,%u,,,,,,\n", bus_name(hcan), it);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *h, const CAN_TxHeaderTypeDef *hdr,
                                       const uint8_t *data, uint32_t *mailbox)
{
    (void)h;
    (void)hdr;
    (void)data;
    (void)mailbox;
    return HAL_OK; /* 发送路径不在本金标准内(FIFO 语义随 B4a 对拍) */
}
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *h, uint32_t fifo,
                                       CAN_RxHeaderTypeDef *hdr, uint8_t *data)
{
    (void)h;
    (void)fifo;
    (void)hdr;
    (void)data;
    return HAL_ERROR; /* 本金标准不注入 rx */
}
uint32_t HAL_CAN_GetRxFifoFillLevel(CAN_HandleTypeDef *h, uint32_t fifo)
{
    (void)h;
    (void)fifo;
    return 0;
}

static void reg(CAN_HandleTypeDef *h, uint32_t tx_id, uint32_t rx_id)
{
    can_init_config_t cfg = {0};
    cfg.can_handle = h;
    cfg.tx_id = tx_id;
    cfg.rx_id = rx_id;
    can_t *inst = can_register(&cfg);
    assert(inst != NULL);
}

int main(void)
{
    g_csv = fopen(".Doc/gate/arch-baseline/can-filter-golden.csv", "w");
    assert(g_csv);
    fprintf(g_csv, "kind,bus,bank,fifo,id_low,slave_bank,mode,scale,activation\n");

    /* 真实注册序(robot.c init 序;rx 规则=dji_motor.c 分组) */
    reg(&hcan1, YAW_MOTOR_ID, 0x205u); /* gimbal_init: yaw GM6020 */
    reg(&hcan2, PITCH_MOTOR_ID, 0x200u + PITCH_MOTOR_ID);
    reg(&hcan2, SHOOT_FRICTION_ID_L, 0x200u + SHOOT_FRICTION_ID_L); /* shoot_init */
    reg(&hcan2, SHOOT_FRICTION_ID_R, 0x200u + SHOOT_FRICTION_ID_R);
    reg(&hcan2, LOADER_ID, 0x200u + LOADER_ID);
    reg(&hcan1, CHASSIS_MOTOR_ID_LF, 0x200u + CHASSIS_MOTOR_ID_LF); /* chassis_init */
    reg(&hcan1, CHASSIS_MOTOR_ID_RF, 0x200u + CHASSIS_MOTOR_ID_RF);
    reg(&hcan1, CHASSIS_MOTOR_ID_LB, 0x200u + CHASSIS_MOTOR_ID_LB);
    reg(&hcan1, CHASSIS_MOTOR_ID_RB, 0x200u + CHASSIS_MOTOR_ID_RB);

    fclose(g_csv);
    printf("PASS: can filter golden %u rows -> .Doc/gate/arch-baseline/can-filter-golden.csv\n",
           row_cnt);
    return 0;
}
