/**
 * @file    main.h
 * @brief   主机测试桩:替代 main.h 的最小实现,仅供 PC 端编译测试
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-05
 * @note    修改记录见 RM_DEVLOG.md 对应 DEV 条目
 * @note    2026-11-22(B0-⑥)扩 CAN 面:CAN_FilterTypeDef/CAN_Tx(Rx)HeaderTypeDef
 *          + bsp_can.c 所需 HAL CAN 声明与常量(金标准 can-filter-golden 用)。
 */
#pragma once
#include <stdint.h>

/* can_port.c 的临界区在真实固件用 CMSIS 内建;主机测试由具体测试 TU 提供定义
 * (如 tests/can_filter_golden.c 的 basepri_shadow),这里只给声明。 */
uint32_t __get_BASEPRI(void);
void __set_BASEPRI(uint32_t value);
typedef struct
{
    int unused;
} UART_HandleTypeDef;
typedef struct
{
    int unused;
} CAN_HandleTypeDef;
typedef enum
{
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;
extern UART_HandleTypeDef huart3;
extern CAN_HandleTypeDef hcan1, hcan2;

/* ---- bxCAN 过滤器/收发面(字段名与 stm32f4xx_hal_can.h 一致) ---- */
typedef struct
{
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterBank;
    uint32_t FilterModeHigh; /* 占位:真实头为 union,主机桩不使用 */
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdLow;
    uint32_t FilterActivation;
    uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

typedef struct
{
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint8_t Data[8];
} CAN_TxHeaderTypeDef;

typedef struct
{
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint32_t Timestamp;
    uint32_t FilterMatchIndex;
} CAN_RxHeaderTypeDef;

#define CAN_FILTERMODE_IDLIST       0u
#define CAN_FILTERMODE_IDMASK       1u
#define CAN_FILTERSCALE_16BIT       0u
#define CAN_FILTERSCALE_32BIT       1u
#define CAN_FILTER_ENABLE           1u
#define CAN_RX_FIFO0                0u
#define CAN_RX_FIFO1                1u
#define CAN_IT_RX_FIFO0_MSG_PENDING (1u << 1)
#define CAN_IT_RX_FIFO1_MSG_PENDING (1u << 5)
#define CAN_IT_TX_MAILBOX_EMPTY     (1u << 21)
#define CAN_ID_STD                  0u
#define CAN_RTR_DATA                0u

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan, const CAN_FilterTypeDef *f);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan, uint32_t it);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, const CAN_TxHeaderTypeDef *hdr,
                                       const uint8_t *data, uint32_t *mailbox);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan, uint32_t fifo,
                                       CAN_RxHeaderTypeDef *hdr, uint8_t *data);
uint32_t HAL_CAN_GetRxFifoFillLevel(CAN_HandleTypeDef *hcan, uint32_t fifo);
