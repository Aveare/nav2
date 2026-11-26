/**
 * @file    main.h
 * @brief   AX650 主机测试专用 HAL 状态与函数声明
 * @layer   TESTS
 * @note    仅由 run_ax650_tests.ps1 放入主机编译的搜索路径。
 */
#ifndef AX650_TEST_MAIN_H
#define AX650_TEST_MAIN_H

#include <stdint.h>

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;
typedef enum
{
    HAL_UART_STATE_RESET = 0,
    HAL_UART_STATE_READY = 0x20,
    HAL_UART_STATE_BUSY_TX = 0x21,
    HAL_UART_STATE_BUSY_RX = 0x22
} HAL_UART_StateTypeDef;
typedef struct
{
    uint32_t BaudRate;
    uint32_t WordLength;
    uint32_t StopBits;
    uint32_t Parity;
    uint32_t Mode;
    uint32_t HwFlowCtl;
    uint32_t OverSampling;
} UART_InitTypeDef;
typedef struct
{
    uint32_t Mode;     // DMA_CIRCULAR / DMA_NORMAL
    uint32_t FIFOMode; // DMA_FIFOMODE_DISABLE / DMA_FIFO_ENABLE
} DMA_InitTypeDef;
typedef struct
{
    DMA_InitTypeDef Init;
    volatile uint16_t NDTR; // 伪剩余计数:模拟循环 DMA 的硬件写位置(测试直写)
} DMA_HandleTypeDef;
typedef struct
{
    UART_InitTypeDef Init;
    HAL_UART_StateTypeDef gState;
    HAL_UART_StateTypeDef RxState;
    DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;

#define UART_WORDLENGTH_9B                      9u
#define UART_STOPBITS_1                         1u
#define UART_PARITY_EVEN                        2u
#define UART_MODE_TX_RX                         3u
#define UART_HWCONTROL_NONE                     0u
#define UART_OVERSAMPLING_16                    16u
#define DMA_CIRCULAR                            1u
#define DMA_NORMAL                              0u
#define DMA_FIFOMODE_DISABLE                    0u
#define DMA_FIFO_ENABLE                         1u
#define DMA_IT_HT                               1u
#define __HAL_DMA_DISABLE_IT(handle, interrupt) ((void)(handle), (void)(interrupt))
#define __HAL_DMA_GET_COUNTER(handle)           ((handle)->NDTR)
#define __HAL_UART_CLEAR_OREFLAG(huart)         ((void)(huart))

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size,
                                    uint32_t timeout);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *handle);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *handle);
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);

#endif
