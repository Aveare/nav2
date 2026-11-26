/**
 * @file    ax650_uart1.c
 * @brief   AX650 USART1 链路:收发装配、模式切换与命令新鲜度
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    ins_state 任务单写发送状态，串口回调单写下行指令，CMD 任务读取快照。
 */
#include "ax650_uart1.h"
#include "ax650_pose.h"
#include "usart_port.h"
#include "kernel.h" // 时基走 framework 时间原语(RM-009;components 层禁用 HAL 时基)
#include "config.h" // 参数来自 config,不依赖 application 层
#include "usart.h"
#include <string.h>

static usart_t *pose_uart;
static ax650_parser_t parser;
static volatile ax650_uart1_state_t rx_state;
static uint8_t tx_frame[AX650_POSE_FRAME_SIZE];
static uint8_t tx_sequence;
static volatile uint8_t tx_mode = AX650_TX_MODE;
static volatile float tx_speed_mps = AX650_TX_SPEED_MPS;
static volatile uint8_t tx_robot_id;
static uint8_t init_requested;
static uint32_t last_attempt_ms;
static void *uart_handle;

static void ax650_uart1_on_command(void *context, const ax650_command_t *command)
{
    (void)context;
    rx_state.command = *command;
    rx_state.command_rx_ms = kernel_tick_ms();
    ++rx_state.command_count;
}

static void ax650_uart1_on_receive(void)
{
    /* 流式解析:一次消费可能含多帧(循环 DMA 架构,DEV-20261121-06),
     * 上限判断用装配缓冲上限,而不是 recv_buff_size(后者只是单帧提示长度) */
    if (pose_uart == NULL || pose_uart->recv_len > USART_RX_ASSEMBLY_SIZE)
        return;
    ax650_parser_feed(&parser, pose_uart->recv_buff, pose_uart->recv_len, ax650_uart1_on_command, NULL);
}

uint8_t ax650_uart1_init(void *handle)
{
    init_requested = 1;
    if (handle != NULL)
        uart_handle = handle;
    if (uart_handle == NULL)
        return 0;
    if (pose_uart != NULL)
        return 1;
    UART_HandleTypeDef *hu = (UART_HandleTypeDef *)uart_handle;
    if (!usart_configure8_e1(hu, AX650_UART1_BAUD))
        return 0;
    ax650_parser_init(&parser);
    usart_init_config_t config = {
        .usart_handle = hu,
        .recv_buff_size = 128u, // [协议] DMA 分块缓存；流解析器负责分片与粘包
        .module_callback = ax650_uart1_on_receive,
        .tx_only = AX650_UART1_RX_ENABLE ? 0u : 1u,
    };
    pose_uart = usart_register(&config);
    return pose_uart != NULL;
}

uint8_t ax650_uart1_send_quaternion(const float q_wxyz[4])
{
    if (!init_requested)
        return 0;
    const uint32_t now = kernel_tick_ms();
    if ((uint32_t)(now - last_attempt_ms) < AX650_POSE_PERIOD_MS)
        return 0;
    last_attempt_ms = now;
    if (pose_uart == NULL && !ax650_uart1_init(NULL))
        return 0;
    /* 注意:先查忙状态，再改缓冲区，避免破坏尚未完成的 IT 发送。 */
    if (!usart_is_ready(pose_uart))
        return 0;
    const size_t size =
        ax650_pose_pack(tx_frame, sizeof(tx_frame), q_wxyz, tx_speed_mps, tx_mode, tx_sequence, now,
                        tx_robot_id);
    if (size == 0 || !usart_try_send_it(pose_uart, tx_frame, (uint16_t)size))
        return 0;
    ++tx_sequence;
    return 1;
}

void ax650_uart1_set_projectile_speed(float speed_mps)
{
    uint32_t bits;
    memcpy(&bits, &speed_mps, sizeof(bits));
    /* 工程包含 -Ofast 测试，检查 IEEE754 指数位以免有限性判断被快速数学优化掉。 */
    if ((bits & 0x7F800000u) == 0x7F800000u || speed_mps <= 0.0f)
        speed_mps = AX650_TX_SPEED_MPS;
    tx_speed_mps = speed_mps;
}

void ax650_uart1_set_robot_id(uint8_t robot_id)
{
    tx_robot_id = robot_id;
}

void ax650_uart1_set_mode(uint8_t mode)
{
    tx_mode = mode;
}

void ax650_uart1_get_state(ax650_uart1_state_t *out)
{
    if (out == NULL)
        return;
    const uint32_t saved_mask = __get_PRIMASK();
    __disable_irq();
    memcpy(out, (const void *)&rx_state, sizeof(*out));
    __set_PRIMASK(saved_mask);
    out->command_valid = out->command_count != 0u &&
                         (uint32_t)(kernel_tick_ms() - out->command_rx_ms) <= AX650_COMMAND_TIMEOUT_MS;
}
