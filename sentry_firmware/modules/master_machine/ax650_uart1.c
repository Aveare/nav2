/**
 * @file    ax650_uart1.c
 * @brief   USART1 配置及非阻塞姿态发送调度
 * @layer   MODULE
 * @note    静态缓冲区和序号只有 INS 任务写；不解析下行控制。
 */
#include "ax650_uart1.h"
#include "ax650_pose.h"
#include "bsp_usart.h"
#include "robot_def.h"
#include "usart.h"

static USARTInstance *pose_uart;
static uint8_t tx_frame[AX650_POSE_FRAME_SIZE];
static uint8_t tx_sequence;
static uint8_t init_requested;
static uint32_t last_attempt_ms;

uint8_t Ax650Uart1Init(void)
{
    init_requested = 1;
    if (pose_uart != NULL) return 1;
    if (!USARTConfigure8E1(&huart1, 921600u)) return 0;
    USART_Init_Config_s config = {
        .usart_handle = &huart1,
        .recv_buff_size = 128u, // [协议] BSP 接收缓存，字节；下行暂不解析
        .module_callback = NULL,
    };
    pose_uart = USARTRegister(&config);
    return pose_uart != NULL;
}

uint8_t Ax650Uart1SendQuaternion(const float q_wxyz[4])
{
    if (!init_requested) return 0;
    const uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_attempt_ms) < AX650_POSE_PERIOD_MS) return 0;
    last_attempt_ms = now;
    if (pose_uart == NULL && !Ax650Uart1Init()) return 0;
    /* 注意:先查忙状态，再改缓冲区，避免破坏尚未完成的 IT 发送。 */
    if (!USARTIsReady(pose_uart)) return 0;
    const size_t size = Ax650PosePack(tx_frame, sizeof(tx_frame), q_wxyz,
                                     AX650_TX_SPEED_MPS, AX650_TX_MODE, tx_sequence);
    if (size == 0 || !USARTTrySendIT(pose_uart, tx_frame, (uint16_t)size)) return 0;
    ++tx_sequence;
    return 1;
}
