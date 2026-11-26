/**
 * @file    can_port.c
 * @brief   CAN 总线封装:实例/过滤器/收发队列
 * @layer   PORTS(stm32f4)
 * @author  modified by 小猫大王
* @note    接口说明见 can_port.h;接收在中断中遍历实例分发回调。
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#include "can_port.h"
#include "err.h" /* ERR_CODE_CAN_FILTER(B4a) */
#include "main.h"
#include "memory.h"
#include "stdlib.h"
#include "dwt_port.h"
#include "log_port.h"

/* can instance ptrs storage, used for recv callback */
// 在CAN产生接收中断会遍历数组,选出hcan和rxid与发生中断的实例相同的那个,调用其回调函数
// @todo: 后续为每个CAN总线单独添加一个can_instance指针数组,提高回调查找的性能
// @todo: 本模块中运用了锁，但可能影响实时性，后续可尝试使用无锁方案
static can_t *can_instance[CAN_MX_REGISTER_CNT] = {NULL};
static uint8_t idx;                                    // 全局CAN实例索引,每次有新的模块注册会自增
static can_send_fifo_t CAN1_Send_FIFO, CAN2_Send_FIFO;
/* B4a 增补:满丢最老计数与限流(方案 §4) */
static uint32_t tx_drop_cnt[2];
static uint32_t tx_drop_last_log_ms[2]; // CAN发送队列，每个hcan对应一个队列

/* ----------------two static function called by can_register()-------------------- */

/**
 * @brief  为指定实例的接收 ID 添加硬件过滤器,由 can_register() 调用
 *         过滤器生效后,bxCAN 只把 ID 匹配的报文填入 FIFO 并触发中断,其余丢弃。
 * @note   F407 的 bxCAN 共 28 个过滤器:前 14 个归 CAN1,后 14 个归 CAN2。
 *         奇数编号模块分配到 FIFO0,偶数编号到 FIFO1;CAN1 用过滤器 0-13,CAN2 用 14-27。
 * @note   只在初始化期执行一次,业务开发无需关心;底层细节见 RM0090 参考手册 CAN 章节。
 * @param  _instance 由具体模块持有的 CAN 实例
 */
static void can_add_filter(can_t *_instance)
{
    CAN_FilterTypeDef can_filter_conf = {0};
    static uint8_t can1_filter_idx = 0, can2_filter_idx = 14; // 0-13给can1用,14-27给can2用

    can_filter_conf.FilterMode =
        CAN_FILTERMODE_IDLIST; // 使用id
                               // list模式,即只有将rxid添加到过滤器中才会接收到,其他报文会被过滤
    can_filter_conf.FilterScale = CAN_FILTERSCALE_16BIT; // 使用16位id模式,即只有低16位有效

    if (_instance->can_handle == &hcan1)
    {
        can_filter_conf.FilterFIFOAssignment =
            (can1_filter_idx & 1)
                ? CAN_RX_FIFO0
                : CAN_RX_FIFO1; // 奇数编号的模块会被分配到FIFO0,偶数编号的模块会被分配到FIFO1
    }
    else if (_instance->can_handle == &hcan2)
    {
        can_filter_conf.FilterFIFOAssignment =
            (can2_filter_idx & 1)
                ? CAN_RX_FIFO0
                : CAN_RX_FIFO1; // 奇数编号的模块会被分配到FIFO0,偶数编号的模块会被分配到FIFO1
    }

    can_filter_conf.SlaveStartFilterBank =
        14; // 从第14个过滤器开始配置从机过滤器(在STM32的BxCAN控制器中CAN2是CAN1的从机)
    can_filter_conf.FilterIdLow =
        _instance->rx_id << 5; // 过滤器寄存器的低16位,因为使用STDID,所以只有低11位有效,高5位要填0
    uint8_t bank =
        _instance->can_handle == &hcan1
            ? (can1_filter_idx++)
            : (can2_filter_idx++); // 根据can_handle判断是CAN1还是CAN2,然后自增
    uint8_t bank_limit =
        _instance->can_handle == &hcan1 ? 14u : 28u; // F407:前14归CAN1,后14归CAN2
    if (bank >= bank_limit) // B4a 增补:filter bank 预算断言(方案 §4)
    {
        LOG_ERROR("[can] filter bank %u over budget %u", bank, bank_limit);
        err_report(ERR_CODE_CAN_FILTER, ERR_POLICY_FAILFAST_BOOT);
    }
    can_filter_conf.FilterBank = bank;
    can_filter_conf.FilterActivation = CAN_FILTER_ENABLE; // 启用过滤器

    /* B4a 增补:ConfigFilter 返回值必查(方案 §4-T06;失败=过滤器未生效=收不到帧) */
    if (HAL_CAN_ConfigFilter(_instance->can_handle, &can_filter_conf) != HAL_OK)
    {
        LOG_ERROR("[can] ConfigFilter failed, bank %u", bank);
        err_report(ERR_CODE_CAN_FILTER, ERR_POLICY_FAILFAST_BOOT);
    }
}

/**
 * @brief 在第一个CAN实例初始化的时候会自动调用此函数,启动CAN服务
 *
 * @note 此函数会启动CAN1和CAN2,开启CAN1和CAN2的FIFO0 & FIFO1报文挂起通知(MSG_PENDING;
 *       2026-11-23 注释修正:原"溢出通知"表述有误,本工程未启用 FIFO 溢出中断)
 *
 */
static void can_service_init()
{
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO1_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_TX_MAILBOX_EMPTY);
}

// 入队函数，将消息存放至消息队列中
static void can_send_fifo_put(bus_tx_frame_t *_instance, can_send_fifo_t *CAN_Send_FIFO, uint8_t length)
{
    if (CAN_Send_FIFO->current_size >= CAN_SEND_FIFO_SIZE)
    {
        /* B4a 增补(方案 §4):满丢最老必须计数 + 日志限流 1/100ms(防风暴占满 RTT) */
        uint8_t bus = (CAN_Send_FIFO == &CAN1_Send_FIFO) ? 0u : 1u;
        tx_drop_cnt[bus]++;
        uint32_t now_ms = dwt_get_timeline_ms();
        if (now_ms - tx_drop_last_log_ms[bus] >= 100u)
        {
            tx_drop_last_log_ms[bus] = now_ms;
            LOG_ERROR("[can] TX FIFO full bus %u, drop-oldest (total %u)", bus + 1u,
                     tx_drop_cnt[bus]);
        }
    }

    uint32_t saved_baspri = __get_BASEPRI();
    __set_BASEPRI(0x50U); // 临界区开始，暂时关闭优先级5及以上的中断，防止竞态

    if (CAN_Send_FIFO->current_size >= CAN_SEND_FIFO_SIZE)
    {
        CAN_Send_FIFO->front_idx = (CAN_Send_FIFO->front_idx + 1) % CAN_SEND_FIFO_SIZE;
        CAN_Send_FIFO->current_size--; // 队列长度自减,丢弃最早的消息
    }

    memcpy(CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].data_buff, _instance->tx_buff,
           8);                                                         // 将发送缓存拷贝到队列中
    CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].DLC_length = length; // 将发送长度存入队列中
    CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].can_instance =
        _instance; // 将can实例指针存入队列中,用于发送时获取发送缓存和发送配置

    CAN_Send_FIFO->back_idx = (CAN_Send_FIFO->back_idx + 1) % CAN_SEND_FIFO_SIZE; // 队列尾索引自增
    CAN_Send_FIFO->current_size++;                                                // 队列长度自增

    __set_BASEPRI(saved_baspri); // 临界区结束，开启中断
}

// 发送函数，同时进行出队和将消息填入邮箱
static void can_send_fifo_transmit(can_send_fifo_t *CAN_Send_FIFO)
{
    uint32_t saved_baspri = __get_BASEPRI();
    __set_BASEPRI(0x50U); // 临界区开始，暂时关闭优先级5及以上的中断，防止竞态

    if (CAN_Send_FIFO->current_size == 0)
    {
        __set_BASEPRI(saved_baspri); // 临界区结束，开启中断
        return;                      // 队列为空,没有数据可以发送
    }

    CAN_TxHeaderTypeDef temp_txconf = {0}; // 临时header变量，用于CAN发送

    temp_txconf.StdId = CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->tx_id;
    temp_txconf.IDE = CAN_ID_STD;
    temp_txconf.RTR = CAN_RTR_DATA;
    temp_txconf.DLC =
        CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].DLC_length; // 将数据帧信息填入至header中

    // 将消息填入至邮箱中
    if (HAL_CAN_AddTxMessage(
            CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->can_handle, &temp_txconf,
            CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].data_buff,
            &CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->tx_mailbox) != HAL_OK)
    {
        __set_BASEPRI(saved_baspri); // 临界区结束，开启中断
        return;                      // 发送失败,邮箱满了,等待下一次发送机会
    }

    CAN_Send_FIFO->front_idx =
        (CAN_Send_FIFO->front_idx + 1) % CAN_SEND_FIFO_SIZE; // 队列头索引自增
    CAN_Send_FIFO->current_size--;                           // 队列长度自减

    __set_BASEPRI(saved_baspri); // 临界区结束，开启中断
}

/* ----------------------- two extern callable function -----------------------*/

can_t *can_register(can_init_config_t *config)
{
    if (!idx)
    {
        can_service_init(); // 第一次注册,先进行硬件初始化
        LOG_INFO("[bsp_can] CAN Service Init");
    }
    if (idx >= CAN_MX_REGISTER_CNT) // 超过最大实例数
    {
        /* T02-WHITELIST: 启动期 fail-fast(白名单 4 类 10 点;B0 台账键控,B3 锚注) */ while (1)
            LOG_ERROR(
                "[bsp_can] CAN instance exceeded MAX num, consider balance the load of CAN bus");
    }
    for (size_t i = 0; i < idx; i++)
    { // 重复注册 | id重复
        if (can_instance[i]->rx_id == config->rx_id &&
            can_instance[i]->can_handle == config->can_handle)
        {
            /* T02-WHITELIST: 启动期 fail-fast(白名单 4 类 10 点;B0 台账键控,B3 锚注) */ while (1)
                LOG_ERROR("[bsp_can] CAN id crash ,tx [%d] or rx [%d] already registered",
                         config->tx_id, config->rx_id);
        }
    }

    can_t *instance = (can_t *)malloc(sizeof(can_t)); // 分配空间
    memset(instance, 0, sizeof(can_t)); // 分配的空间未必是0,所以要先清空
    // 设置回调函数和接收发送id
    instance->can_handle = config->can_handle;
    instance->tx_id = config->tx_id;
    instance->rx_id = config->rx_id;
    instance->can_module_callback = config->can_module_callback;
    instance->id = config->id;

    can_add_filter(instance);         // 添加CAN过滤器规则
    can_instance[idx++] = instance; // 将实例保存到can_instance中

    return instance; // 返回can实例指针
}

// 外部调用的CAN发送函数，对比旧版进行了循环队列优化，防止while忙等导致阻塞，影响实时性
void can_transmit(bus_tx_frame_t *_instance, uint8_t length)
{
    if (_instance->can_handle == &hcan1)
    {
        can_send_fifo_put(_instance, &CAN1_Send_FIFO, length); // 将消息填入至队列中
        can_send_fifo_transmit(
            &CAN1_Send_FIFO); // 触发一次发送，防止因消息队列临时为空而导致后台停止触发发送
    }
    else if (_instance->can_handle == &hcan2)
    {
        can_send_fifo_put(_instance, &CAN2_Send_FIFO, length);
        can_send_fifo_transmit(&CAN2_Send_FIFO);
    }
}

/* -----------------------belows are callback definitions--------------------------*/

/**
 * @brief 此函数会被下面两个函数调用,用于处理FIFO0和FIFO1报文挂起中断(说明收到了新的数据;
 *        2026-11-23 注释修正,原"溢出中断"表述有误)
 *        所有的实例都会被遍历,找到can_handle和rx_id相等的实例时,调用该实例的回调函数
 *
 * @param _hcan
 * @param fifox passed to HAL_CAN_GetRxMessage() to get mesg from a specific fifo
 */
static void can_fifo_callback(CAN_HandleTypeDef *_hcan, uint32_t fifox)
{
    CAN_RxHeaderTypeDef rxconf; // 同上
    uint8_t can_rx_buff[8];
    while (HAL_CAN_GetRxFifoFillLevel(_hcan, fifox)) // FIFO不为空,有可能在其他中断时有多帧数据进入
    {
        HAL_CAN_GetRxMessage(_hcan, fifox, &rxconf, can_rx_buff); // 从FIFO中获取数据
        for (size_t i = 0; i < idx; ++i)
        { // 两者相等说明这是要找的实例
            if (_hcan == can_instance[i]->can_handle && rxconf.StdId == can_instance[i]->rx_id)
            {
                if (can_instance[i]->can_module_callback != NULL) // 回调函数不为空就调用
                {
                    can_instance[i]->rx_len = rxconf.DLC; // 保存接收到的数据长度
                    memcpy(can_instance[i]->rx_buff, can_rx_buff, rxconf.DLC); // 消息拷贝到对应实例
                    can_instance[i]->can_module_callback(
                        can_instance[i]); // 触发回调进行数据解析和处理
                }
                break;
            }
        }
    }
}

/**
 * @brief 注意,STM32的两个CAN设备共享两个FIFO
 * 下面两个函数是HAL库中的回调函数,他们被HAL声明为__weak,这里对他们进行重载(重写)
 * 当FIFO0或FIFO1收到报文(挂起中断)时会调用这两个函数
 */
// 下面的函数会调用can_fifo_callback()来进一步处理来自特定CAN设备的消息

/**
 * @brief rx fifo callback. Called when a message is pending in FIFO_0
 *
 * @param hcan CAN handle indicate which device the oddest mesg in FIFO_0 comes from
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    can_fifo_callback(hcan, CAN_RX_FIFO0); // 调用我们自己写的函数来处理消息
}

/**
 * @brief rx fifo callback. Once FIFO_1 is full,this func would be called
 *
 * @param hcan CAN handle indicate which device the oddest mesg in FIFO_1 comes from
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    can_fifo_callback(hcan, CAN_RX_FIFO1); // 调用我们自己写的函数来处理消息
}

/**
 * @brief 在CAN邮箱空闲中断回调函数中触发发送
 *
 * 三个函数作用基本一致
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    can_send_fifo_transmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    can_send_fifo_transmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    can_send_fifo_transmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}

int can_port_bus_of(const CAN_HandleTypeDef *h) /* B5 bus_id 白名单抽象 */
{
    return (h == &hcan2) ? 1 : 0;
}

unsigned CAN_GetTxFifoDropCount(uint8_t bus)
{
    return (bus < 2u) ? tx_drop_cnt[bus] : 0u;
}
