/**
 * @file    ax650_uart1_test.c
 * @brief   真实 UART1 发送模块与 BSP 的主机集成测试
 * @layer   MODULE
 * @note    只替代 HAL，不访问探针、串口或实体机器人。
 */
#include "ax650_uart1.h"
#include "ax650_pose.h"
#include "bsp_usart.h"
#include "usart.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
static uint32_t tick;
static unsigned init_calls, rx_calls, tx_calls;
static HAL_StatusTypeDef init_result = HAL_ERROR, tx_result = HAL_OK;
static uint8_t dma_handle;
static uint8_t *inflight;
static uint8_t sent[AX650_POSE_FRAME_SIZE];

uint32_t HAL_GetTick(void) { return tick; }

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *handle)
{
    assert(handle == &huart1);
    ++init_calls;
    return init_result;
}

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size)
{
    assert(handle == &huart1 && data != NULL && size == 128u);
    ++rx_calls;
    handle->RxState = HAL_UART_STATE_BUSY_RX;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size)
{
    assert(handle == &huart1 && data != NULL && size == sizeof(sent));
    ++tx_calls;
    if (tx_result == HAL_OK)
    {
        assert(handle->gState == HAL_UART_STATE_READY);
        inflight = data;
        memcpy(sent, data, size);
        handle->gState = HAL_UART_STATE_BUSY_TX;
    }
    return tx_result;
}

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size)
{
    (void)handle; (void)data; (void)size;
    assert(!"UART1 pose must use IT TX");
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)handle; (void)data; (void)size; (void)timeout;
    assert(!"UART1 pose must not block");
    return HAL_ERROR;
}

static void CompleteTx(void)
{
    assert(inflight != NULL && memcmp(inflight, sent, sizeof(sent)) == 0);
    huart1.gState = HAL_UART_STATE_READY;
}

int main(void)
{
    const float zero[4] = {1, 0, 0, 0};
    const float yaw45[4] = {0.923879533f, 0, 0, 0.382683432f};
    uint8_t expected[AX650_POSE_FRAME_SIZE];
    USARTInstance invalid = {0};
    assert(!USARTIsReady(NULL) && !USARTIsReady(&invalid));
    assert(!USARTConfigure8E1(NULL, 921600u));
    assert(!USARTConfigure8E1(&huart1, 0u));
    assert(!USARTTrySendIT(NULL, expected, sizeof(expected)));
    assert(!Ax650Uart1SendQuaternion(zero) && init_calls == 0u);

    huart1.gState = huart1.RxState = HAL_UART_STATE_READY;
    huart1.hdmarx = &dma_handle;
    assert(!Ax650Uart1Init() && init_calls == 1u && rx_calls == 0u);
    assert(huart1.Init.BaudRate == 921600u && huart1.Init.WordLength == UART_WORDLENGTH_9B);
    assert(huart1.Init.Parity == UART_PARITY_EVEN && huart1.Init.StopBits == UART_STOPBITS_1);
    assert(huart1.Init.Mode == UART_MODE_TX_RX && huart1.Init.HwFlowCtl == UART_HWCONTROL_NONE);
    init_result = HAL_OK;
    tick = 9u;
    assert(!Ax650Uart1SendQuaternion(zero) && init_calls == 1u);
    tick = 10u;
    assert(Ax650Uart1SendQuaternion(zero));
    assert(init_calls == 2u && rx_calls == 1u && tx_calls == 1u);
    assert(Ax650PosePack(expected, sizeof(expected), zero, 0, 0, 0) == sizeof(expected));
    assert(memcmp(sent, expected, sizeof(sent)) == 0);
    assert(Ax650Uart1Init() && init_calls == 2u && rx_calls == 1u);
    assert(!USARTConfigure8E1(&huart1, 921600u));

    tick = 19u;
    assert(!Ax650Uart1SendQuaternion(yaw45));
    tick = 20u;
    assert(!Ax650Uart1SendQuaternion(yaw45) && tx_calls == 1u);
    CompleteTx(); // 忙时调用不得改写仍在发送的零姿态帧
    tick = 21u;
    assert(!Ax650Uart1SendQuaternion(yaw45));
    tick = 30u; tx_result = HAL_BUSY;
    assert(!Ax650Uart1SendQuaternion(yaw45) && tx_calls == 2u);
    tick = 40u; tx_result = HAL_OK;
    assert(Ax650Uart1SendQuaternion(yaw45) && sent[3] == 1u);
    assert(Ax650PosePack(expected, sizeof(expected), yaw45, 0, 0, 1) == sizeof(expected));
    assert(memcmp(sent, expected, sizeof(sent)) == 0); // 使用最新四元数，非缓存旧姿态
    CompleteTx();
    const float invalid_q[4] = {NAN, 0, 0, 0};
    tick = 50u;
    assert(!Ax650Uart1SendQuaternion(invalid_q) && tx_calls == 3u);

    for (unsigned i = 0; i < 258u; ++i)
    {
        tick += 10u;
        assert(Ax650Uart1SendQuaternion(zero));
        assert(sent[3] == (uint8_t)(i + 2u));
        CompleteTx();
    }
    tick = 0xFFFFFFF8u;
    assert(Ax650Uart1SendQuaternion(zero)); CompleteTx();
    tick = 1u;
    assert(!Ax650Uart1SendQuaternion(zero));
    tick = 2u;
    assert(Ax650Uart1SendQuaternion(zero)); CompleteTx();
    assert(init_calls == 2u && rx_calls == 1u);
    puts("PASS: real BSP 8E1 setup, init retry, 100Hz, busy-buffer lifetime, HAL failure, fresh pose and timer/sequence wrap");
    return 0;
}
