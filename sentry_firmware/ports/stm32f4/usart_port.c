/**
 * @file    usart_port.c
 * @brief   USART 总线封装:循环 DMA/装配缓冲/错误计数
 * @layer   PORTS(stm32f4)
 * @author  neozng
 * @author  modified by JIT CR (2026-10-05 recv_len;2026-11-21 重构为循环 DMA 架构)
 * @author  modified by 小猫大王
* @note   DEV-20261121-06:旧实现(DMA_NORMAL + 每次 IDLE/收满后软件重启)在实车上出现两类
*         故障:①`HAL_UARTEx_ReceiveToIdle_DMA` 在 RxState != READY 时返回 HAL_BUSY 被
*         静默吞掉 → 接收永久停摆;②ORE 属 blocking error,HAL 错误分支清不掉标志
*         (清它要读 SR + 读 DR),错误中断反复进入 → CPU 被打满 → 全车失控。
*         新实现把"重启接收"从正常数据路径中彻底移除:
*         - DMA 配成 CIRCULAR,写到缓冲尾自动回绕,永不停止,无需 re-arm;
*         - 中断只做两件事:错误计数、DMA 满圈(空回调);
*         - 消费由 200Hz robot_task 轮询 NDTR 完成,解析在任务上下文。
 */
#include "usart_port.h"
#include "log_port.h"
#include "stdlib.h"
#include <string.h>

/* usart服务实例,所有注册了usart的模块信息会被保存在这里 */
static uint8_t idx;
static usart_t *usart_instance[USART_DEVICE_CNT] = {NULL};

/* 实例改成**静态池**(DEV-20261121-09):实例含 256B 环 + 512B 装配缓冲,
 * 走 FreeRTOS 堆时 3 个要 2.4KB,而堆总量仅 20KB、5 个任务栈已占 11.8KB ——
 * 实车上表现为烧录后启动即 HardFault(堆耗尽 → malloc 返回 NULL → memset(NULL) 崩)。
 * 静态分配后零堆占用,且不存在碎片/失败路径。 */
static usart_t usart_pool[USART_DEVICE_CNT];

/* 每实例的循环 DMA 是否已配置(只在首次/失联恢复时做一次) */
static uint8_t dma_configured[USART_DEVICE_CNT];

/**
 * @brief  启动循环 DMA 接收
 * @param  instance 目标串口实例
 * @return 1 = 已启动;0 = 失败
 * @note   先把 DMA 从 CubeMX 生成时的 NORMAL/FIFO 模式改为 CIRCULAR/DIRECT:
 *         ① CIRCULAR 让 DMA 永不停止,这是消除 re-arm 死亡路径的根本;
 *         ② FIFO 改 DIRECT 与 DBUS 路一致 —— USART6 原为 FIFO + FULL 阈值,
 *            P2M 下包尾字节滞留 FIFO、NDTR 不递减,是帧错位的独立嫌疑(DEV-20261121-04)。
 * @note   RxState 非 READY(上次接收未终结)时先 Abort:循环模式下 RxState 恒为 BUSY_RX,
 *         失联恢复路径必须先终结再启动,否则 Receive_DMA 直接返回 HAL_BUSY。
 */
static uint8_t USART_StartRx(usart_t *instance)
{
    UART_HandleTypeDef *hu = instance->usart_handle;

    if (!dma_configured[instance->slot])
    {
        if (hu->RxState != HAL_UART_STATE_READY)
            HAL_UART_AbortReceive(hu);
        hu->hdmarx->Init.Mode = DMA_CIRCULAR;
        hu->hdmarx->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(hu->hdmarx) != HAL_OK)
        {
            LOG_ERROR("[bsp_usart] DMA config (circular) failed on slot %u",
                     (unsigned)instance->slot);
            return 0;
        }
        dma_configured[instance->slot] = 1;
    }

    if (hu->RxState != HAL_UART_STATE_READY)
        HAL_UART_AbortReceive(hu);

    if (HAL_UART_Receive_DMA(hu, instance->ring, USART_RX_RING_SIZE) != HAL_OK)
    {
        LOG_ERROR("[bsp_usart] start circular rx failed on slot %u", (unsigned)instance->slot);
        return 0;
    }
    /* 半传输中断无用(消费是轮询驱动),关掉减少中断 */
    __HAL_DMA_DISABLE_IT(hu->hdmarx, DMA_IT_HT);
    return 1;
}

/**
 * @brief 启动串口接收(初始化与 daemon 失联恢复时调用)
 * @note  正常数据路径不调用本函数:DMA 循环运行,由 usart_process_all 消费。
 */
void usart_service_init(usart_t *_instance)
{
    if (_instance == NULL || !_instance->rx_enable)
        return;
    (void)USART_StartRx(_instance);
}

usart_t *usart_register(usart_init_config_t *init_config)
{
    if (idx >= USART_DEVICE_CNT) // 超过最大实例数
        while (1)
            LOG_ERROR("[bsp_usart] USART exceed max instance count!");

    for (uint8_t i = 0; i < idx; i++) // 检查是否已经注册过
        if (usart_instance[i]->usart_handle == init_config->usart_handle)
            while (1)
                LOG_ERROR("[bsp_usart] USART instance already registered!");

    usart_t *instance = &usart_pool[idx]; // 静态池,不占堆(DEV-20261121-09)
    memset(instance, 0, sizeof(usart_t));

    instance->usart_handle = init_config->usart_handle;
    instance->recv_buff_size = init_config->recv_buff_size;
    instance->module_callback = init_config->module_callback;
    /* 反向语义:只有显式 tx_only==1 才关闭接收(未清零的 config 不会误关) */
    instance->rx_enable = (init_config->tx_only == 1u) ? 0u : 1u;
    instance->slot = idx;

    usart_instance[idx++] = instance;
    if (instance->rx_enable)
        usart_service_init(instance);
    return instance;
}

/**
 * @brief 消费所有实例的新数据并调用模块回调(200Hz robot_task 每拍调用)
 * @note  纯轮询,不依赖 IDLE/TC 中断:中断事件丢失只影响延迟,不影响正确性。
 *        写指针取自硬件 NDTR(真实位置),读指针只在本函数推进,天然幂等 ——
 *        多调用几次不会重复消费,少调用一次只是推迟到下一拍。
 * @note  装配:新字节追加到 recv_buff 尾部而不是覆盖(DEV-20261121-06 修正)。
 *        5ms 轮询会把 1.56ms 的 DBUS 帧切成两半,若每次覆盖,定长帧永远拼不齐。
 *        回调通过改小 recv_len 报告"已消费字节数",余量 memmove 到缓冲头部留待下次。
 */
void usart_process_all(void)
{
    for (uint8_t i = 0; i < idx; ++i)
    {
        usart_t *ins = usart_instance[i];
        if (ins == NULL || !ins->rx_enable || ins->module_callback == NULL ||
            ins->usart_handle->hdmarx == NULL)
            continue;

        uint16_t ndtr = (uint16_t)__HAL_DMA_GET_COUNTER(ins->usart_handle->hdmarx); // DMA 剩余计数
        uint16_t write_pos = (uint16_t)(USART_RX_RING_SIZE - ndtr); // 硬件写位置,0~RING

        uint16_t avail;
        if (write_pos >= ins->read_pos)
            avail = (uint16_t)(write_pos - ins->read_pos);
        else
            avail = (uint16_t)(USART_RX_RING_SIZE - ins->read_pos + write_pos);

        /* 读写指针相等 = 无新数据。若消费长期落后导致写指针追平读指针,
         * DMA 已覆盖未读数据(环形缓冲固有),最多拷 RING-1 字节后继续 */
        if (avail == 0)
            continue;
        if (avail > USART_RX_RING_SIZE - 1)
        {
            /* B4b 增补(方案 §4):追平=DMA 覆盖未读数据,计数 + 限流留痕
             * (RING 扩到 1024 后,只有消费长期停摆才会触达) */
            ins->overrun_cnt++;
            static uint32_t overrun_last_log[USART_DEVICE_CNT];
            uint32_t now_ms = HAL_GetTick(); /* 主机/ARM 皆有定义(ax650 桩无 bsp_dwt) */
            if (now_ms - overrun_last_log[ins->slot] >= 100u)
            {
                overrun_last_log[ins->slot] = now_ms;
                LOG_ERROR("[usart] slot %u overrun: consumer stalled, DMA overwrote unread (total %u)",
                         (unsigned)ins->slot, (unsigned)ins->overrun_cnt);
            }
            avail = USART_RX_RING_SIZE - 1;
        }

        /* 装配缓冲装不下(上层解析长期停滞):整块丢弃重来,并计一次错误 */
        if ((uint32_t)ins->assemble_len + avail > USART_RX_ASSEMBLY_SIZE)
        {
            ins->assemble_len = 0;
            ins->err_cnt++;
        }
        uint16_t space = (uint16_t)(USART_RX_ASSEMBLY_SIZE - ins->assemble_len);
        if (avail > space)
            avail = space;

        /* 把 ring 中连续的一段追加到装配缓冲尾部(消化回绕) */
        uint16_t first = (uint16_t)(USART_RX_RING_SIZE - ins->read_pos);
        if (first > avail)
            first = avail;
        memcpy(&ins->recv_buff[ins->assemble_len], &ins->ring[ins->read_pos], first);
        if (avail > first)
            memcpy(&ins->recv_buff[ins->assemble_len + first], ins->ring, (size_t)(avail - first));

        ins->read_pos = (uint16_t)((ins->read_pos + avail) % USART_RX_RING_SIZE);
        ins->assemble_len = (uint16_t)(ins->assemble_len + avail);

        /* 回调:入口 recv_len = 可用总长;回调改小它表示实际消费的字节数 */
        ins->recv_len = ins->assemble_len;
        ins->module_callback();

        uint16_t consumed = ins->recv_len;
        if (consumed > ins->assemble_len)
            consumed = ins->assemble_len;
        uint16_t remain = (uint16_t)(ins->assemble_len - consumed);
        if (remain > 0u && consumed > 0u)
            memmove(ins->recv_buff, &ins->recv_buff[consumed], remain);
        ins->assemble_len = remain;
    }
}

uint32_t usart_get_err_count(usart_t *_instance)
{
    return (_instance == NULL) ? 0u : _instance->err_cnt;
}

uint32_t usart_get_overrun_count(usart_t *_instance) /* B4b 增补:追平覆盖计数 */
{
    return (_instance == NULL) ? 0u : _instance->overrun_cnt;
}

/* @todo 当前仅进行了形式上的封装,后续要进一步考虑是否将module的行为与bsp完全分离 */
void usart_send(usart_t *_instance, uint8_t *send_buf, uint16_t send_size,
               usart_transfer_mode_t mode)
{
    switch (mode)
    {
    case USART_TRANSFER_BLOCKING:
        HAL_UART_Transmit(_instance->usart_handle, send_buf, send_size, 100);
        break;
    case USART_TRANSFER_IT:
        HAL_UART_Transmit_IT(_instance->usart_handle, send_buf, send_size);
        break;
    case USART_TRANSFER_DMA:
        HAL_UART_Transmit_DMA(_instance->usart_handle, send_buf, send_size);
        break;
    default:
        while (1)
            ; // illegal mode! check your code context!
              // 检查定义instance的代码上下文,可能出现指针越界
        break;
    }
}

/* 串口发送时,gstate会被设为BUSY_TX */
uint8_t usart_is_ready(usart_t *_instance)
{
    return _instance != NULL && _instance->usart_handle != NULL &&
           _instance->usart_handle->gState == HAL_UART_STATE_READY;
}

uint8_t usart_configure8_e1(UART_HandleTypeDef *handle, uint32_t baud)
{
    if (handle == NULL || baud == 0u || handle->gState != HAL_UART_STATE_READY ||
        handle->RxState != HAL_UART_STATE_READY)
        return 0;
    handle->Init.BaudRate = baud;
    handle->Init.WordLength = UART_WORDLENGTH_9B;
    handle->Init.StopBits = UART_STOPBITS_1;
    handle->Init.Parity = UART_PARITY_EVEN;
    handle->Init.Mode = UART_MODE_TX_RX;
    handle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle->Init.OverSampling = UART_OVERSAMPLING_16;
    return HAL_UART_Init(handle) == HAL_OK;
}

uint8_t usart_try_send_it(usart_t *instance, uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || length == 0u || !usart_is_ready(instance))
        return 0;
    return HAL_UART_Transmit_IT(instance->usart_handle, buffer, length) == HAL_OK;
}

/**
 * @brief 串口错误回调(ORE/FE/NE)
 * @note  DEV-20261121-06:**只清标志 + 计数,不重启任何接收**。
 *        CIRCULAR DMA 不会因错误停止,下一个字节即恢复数据流;
 *        ORE/FE 丢掉的字节由各解析器的帧重同步兜住(裁判按 0xA5 重扫;
 *        DBUS 是 18B 定长帧,丢字节后无法自愈,由 daemon 100ms 超时重启对齐)。
 * @note  清标志(读 SR + 读 DR)会从 DMA 手里抢走一个待搬字节 —— 但本路径只在
 *        已发生错误时进入,丢的本来就是坏字节,代价可忽略。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < idx; ++i)
    {
        if (huart == usart_instance[i]->usart_handle)
        {
            __HAL_UART_CLEAR_OREFLAG(huart); // 读 SR + 读 DR,清 ORE/RXNE/IDLE 残留
            usart_instance[i]->err_cnt++;
            return;
        }
    }
}

/**
 * @brief DMA 满圈回调(CIRCULAR 每写满 USART_RX_RING_SIZE 触发一次)
 * @note  消费由 usart_process_all 驱动,此处无需动作。保留空实现是为了显式声明
 *        "满圈不是数据边界",避免后来者在中断里解析。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
}
