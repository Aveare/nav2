/* 联合测试 HAL 边界。数值与 F407 HAL 的位布局一致。 */
#pragma once
#include <stdint.h>
typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;
typedef struct { uint32_t TSR; } CAN_TypeDef;
typedef struct { CAN_TypeDef *Instance; } CAN_HandleTypeDef;
typedef struct {
    uint32_t StdId, ExtId, IDE, RTR, DLC, TransmitGlobalTime;
} CAN_TxHeaderTypeDef;
typedef struct { uint32_t StdId, ExtId, IDE, RTR, DLC; } CAN_RxHeaderTypeDef;
typedef struct {
    uint32_t FilterIdHigh, FilterIdLow, FilterMaskIdHigh, FilterMaskIdLow;
    uint32_t FilterFIFOAssignment, FilterBank, FilterMode, FilterScale;
    uint32_t FilterActivation, SlaveStartFilterBank;
} CAN_FilterTypeDef;
extern CAN_HandleTypeDef hcan1, hcan2;
#define CAN_FILTERMODE_IDLIST 1u
#define CAN_FILTERMODE_IDMASK 0u
#define CAN_FILTERSCALE_16BIT 0u
#define CAN_FILTERSCALE_32BIT 1u
#define CAN_FILTER_ENABLE 1u
#define CAN_RX_FIFO0 0u
#define CAN_RX_FIFO1 1u
#define CAN_ID_STD 0u
#define CAN_ID_EXT 4u
#define CAN_RTR_DATA 0u
#define CAN_RTR_REMOTE 2u
#define CAN_TX_MAILBOX0 1u
#define CAN_TX_MAILBOX1 2u
#define CAN_TX_MAILBOX2 4u
#define CAN_TSR_RQCP0 1u
#define CAN_TSR_TME0 (1u << 26)
#define CAN_IT_TX_MAILBOX_EMPTY 1u
#define CAN_IT_RX_FIFO0_MSG_PENDING 2u
#define CAN_IT_RX_FIFO1_MSG_PENDING 16u
uint32_t __get_BASEPRI(void);
void __set_BASEPRI(uint32_t value);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *can);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *can, uint32_t mask);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *can, CAN_FilterTypeDef *filter);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *can);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *can, const CAN_TxHeaderTypeDef *header,
                                      const uint8_t data[], uint32_t *mailbox);
uint32_t HAL_CAN_GetRxFifoFillLevel(CAN_HandleTypeDef *can, uint32_t fifo);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *can, uint32_t fifo,
                                      CAN_RxHeaderTypeDef *header, uint8_t data[]);
