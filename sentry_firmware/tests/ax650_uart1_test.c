/**
 * @file    ax650_uart1_test.c
 * @brief   真实 UART1 发送模块与 BSP 的主机集成测试
 * @layer   TESTS
 * @note    只替代 HAL，不访问探针、串口或实体机器人。
 */
#include "ax650_uart1.h"
#include "ax650_pose.h"
#include "usart_port.h"
#include "robot_def.h"
#include "usart.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;
static DMA_HandleTypeDef dma_handle;
static uint32_t tick;
static unsigned init_calls, rx_calls, tx_calls, dma_init_calls;
static HAL_StatusTypeDef init_result = HAL_ERROR, tx_result = HAL_OK;
static uint8_t *inflight;
static uint8_t *rx_ring; // HAL_UART_Receive_DMA 注入的循环 DMA 环(bsp 实例内部缓冲)
static uint16_t rx_ring_size;
static uint8_t sent[AX650_POSE_FRAME_SIZE];
static uint32_t irq_mask;

/* HAL_GetTick:仅 usart_port.c(ports 层豁免)仍在用;ax650 已改走 DWT(RM-009) */
uint32_t HAL_GetTick(void)
{
    return tick;
}
/* RM-009:ax650 时基改 kernel_tick_ms(framework 原语);桩与 HAL_GetTick 同一注入源,
 * 整数 ms 回绕语义与原 HAL_GetTick 完全一致,freshness/wrap 断言不变 */
uint32_t kernel_tick_ms(void)
{
    return tick;
}
uint32_t __get_PRIMASK(void)
{
    return irq_mask;
}
void __disable_irq(void)
{
    irq_mask = 1u;
}
void __set_PRIMASK(uint32_t mask)
{
    irq_mask = mask;
}

int log_printf(const char *fmt, ...) /* B8:log 同目录遮蔽后主机自供日志桩 */
{
    (void)fmt;
    return 0;
}

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *handle)
{
    assert(handle == &huart1);
    ++init_calls;
    return init_result;
}

HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *handle)
{
    assert(handle == &dma_handle);
    ++dma_init_calls;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size)
{
    assert(handle == &huart1 && data != NULL && size == USART_RX_RING_SIZE);
    rx_ring = data;
    rx_ring_size = size;
    ++rx_calls;
    handle->RxState = HAL_UART_STATE_BUSY_RX;
    dma_handle.NDTR = size; // 循环 DMA 就绪:剩余计数 = 环容量,写位置 0
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *handle)
{
    (void)handle;
    return HAL_OK;
}

static float get_float(const uint8_t *in)
{
    uint32_t bits = (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
                    ((uint32_t)in[3] << 24);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#if AX650_UART1_RX_ENABLE
static void PutFloat(uint8_t *out, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    out[0] = (uint8_t)bits;
    out[1] = (uint8_t)(bits >> 8);
    out[2] = (uint8_t)(bits >> 16);
    out[3] = (uint8_t)(bits >> 24);
}

/* 模拟循环 DMA 写入:字节落在写位置(= 容量-NDTR),NDTR 随写递减并按环回绕 */
static void FeedBytes(const uint8_t *src, uint16_t n)
{
    assert(rx_ring != NULL && rx_ring_size == USART_RX_RING_SIZE);
    for (uint16_t i = 0; i < n; ++i)
    {
        const uint16_t wpos = (uint16_t)(rx_ring_size - dma_handle.NDTR);
        rx_ring[wpos] = src[i];
        dma_handle.NDTR = (uint16_t)(rx_ring_size - ((uint16_t)(wpos + 1u) % rx_ring_size));
    }
}

static void ReceiveCommand(float yaw, float pitch, uint8_t target_id)
{
    uint8_t frame[39];
    memset(frame, 0, sizeof(frame));
    frame[0] = 0xA5u;
    frame[1] = 30u;
    frame[4] = ax650_pose_crc8(frame, 4u);
    frame[5] = 0x03u;
    frame[6] = 0x05u;
    PutFloat(frame + 7u, yaw);
    PutFloat(frame + 11u, pitch);
    frame[36] = target_id;
    const uint16_t crc = ax650_pose_crc16(frame, 37u);
    frame[37] = (uint8_t)crc;
    frame[38] = (uint8_t)(crc >> 8);
    FeedBytes(frame, sizeof(frame));
    usart_process_all(); // 200Hz 消费泵:ring → 装配缓冲 → 模块回调(DEV-20261121-06 架构)
}
#endif

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
    (void)handle;
    (void)data;
    (void)size;
    assert(!"UART1 pose must use IT TX");
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle, uint8_t *data, uint16_t size,
                                    uint32_t timeout)
{
    (void)handle;
    (void)data;
    (void)size;
    (void)timeout;
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
    usart_t invalid = {0};
    assert(!usart_is_ready(NULL) && !usart_is_ready(&invalid));
    assert(!usart_configure8_e1(NULL, 921600u));
    assert(!usart_configure8_e1(&huart1, 0u));
    assert(!usart_try_send_it(NULL, expected, sizeof(expected)));
    assert(!ax650_uart1_send_quaternion(zero) && init_calls == 0u);

    huart1.gState = huart1.RxState = HAL_UART_STATE_READY;
    huart1.hdmarx = &dma_handle;
    assert(!ax650_uart1_init(&huart1) && init_calls == 1u && rx_calls == 0u);
    assert(huart1.Init.BaudRate == 921600u && huart1.Init.WordLength == UART_WORDLENGTH_9B);
    assert(huart1.Init.Parity == UART_PARITY_EVEN && huart1.Init.StopBits == UART_STOPBITS_1);
    assert(huart1.Init.Mode == UART_MODE_TX_RX && huart1.Init.HwFlowCtl == UART_HWCONTROL_NONE);
    init_result = HAL_OK;
    tick = 9u;
    assert(!ax650_uart1_send_quaternion(zero) && init_calls == 1u);
    tick = 10u;
    assert(ax650_uart1_send_quaternion(zero));
    assert(init_calls == 2u && rx_calls == (AX650_UART1_RX_ENABLE ? 1u : 0u) && tx_calls == 1u);
    assert(ax650_pose_pack(expected, sizeof(expected), zero, 0.0f, 0, 0, tick, 0u) == sizeof(expected));
    assert(memcmp(sent, expected, sizeof(sent)) == 0);
    assert(ax650_uart1_init(&huart1) && init_calls == 2u &&
           rx_calls == (AX650_UART1_RX_ENABLE ? 1u : 0u));
    assert(!usart_configure8_e1(&huart1, 921600u));

    tick = 19u;
    assert(!ax650_uart1_send_quaternion(yaw45));
    tick = 20u;
    assert(!ax650_uart1_send_quaternion(yaw45) && tx_calls == 1u);
    CompleteTx(); // 忙时调用不得改写仍在发送的零姿态帧
    tick = 21u;
    assert(!ax650_uart1_send_quaternion(yaw45));
    tick = 30u;
    tx_result = HAL_BUSY;
    assert(!ax650_uart1_send_quaternion(yaw45) && tx_calls == 2u);
    ax650_uart1_set_projectile_speed(8.0f);
    ax650_uart1_set_robot_id(103u);
    ax650_uart1_set_mode(AX650_AUTO_AIM_MODE);
    tick = 40u;
    tx_result = HAL_OK;
    assert(ax650_uart1_send_quaternion(yaw45) && sent[3] == 1u);
    assert(ax650_pose_pack(expected, sizeof(expected), yaw45, 8.0f, AX650_AUTO_AIM_MODE, 1, tick, 103u) ==
           sizeof(expected));
    assert(sent[28] == 103u);
    assert(memcmp(sent, expected, sizeof(sent)) == 0); // 使用最新四元数，非缓存旧姿态
    CompleteTx();
    ax650_uart1_state_t state;
#if AX650_UART1_RX_ENABLE
    ReceiveCommand(22.5f, -6.0f, 3u);
    ax650_uart1_get_state(&state);
    assert(state.command_valid && state.command_count == 1u);
    assert(state.command.yaw == 22.5f && state.command.pitch == -6.0f &&
           state.command.target_id == 3u);
    tick += AX650_COMMAND_TIMEOUT_MS + 1u;
    ax650_uart1_get_state(&state);
    assert(!state.command_valid && state.command_count == 1u);
#else
    ax650_uart1_get_state(&state);
    assert(!state.command_valid && state.command_count == 0u);
#endif
    ax650_uart1_set_mode(AX650_TX_MODE);
    const float invalid_q[4] = {NAN, 0, 0, 0};
    tick = 50u;
    assert(!ax650_uart1_send_quaternion(invalid_q) && tx_calls == 3u);
    ax650_uart1_set_projectile_speed(16.5f);

    for (unsigned i = 0; i < 258u; ++i)
    {
        tick += 10u;
        assert(ax650_uart1_send_quaternion(zero));
        assert(sent[3] == (uint8_t)(i + 2u));
        if (i == 0u)
        {
            assert(get_float(sent + 19u) == 16.5f);
            ax650_uart1_set_projectile_speed(INFINITY);
        }
        else if (i == 1u)
        {
            assert(get_float(sent + 19u) == 0.0f);
        }
        CompleteTx();
    }
    tick = 0xFFFFFFF8u;
    assert(ax650_uart1_send_quaternion(zero));
    CompleteTx();
    tick = 1u;
    assert(!ax650_uart1_send_quaternion(zero));
    tick = 2u;
    assert(ax650_uart1_send_quaternion(zero));
    CompleteTx();
    assert(init_calls == 2u && rx_calls == (AX650_UART1_RX_ENABLE ? 1u : 0u)); // RX=1:循环 DMA 只启动一次;RX=0:不启动接收
    puts("PASS: UART1 pose TX, mode switch, command RX freshness and timer/sequence wrap");
    return 0;
}
