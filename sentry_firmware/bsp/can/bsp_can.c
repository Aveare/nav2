/**
 * @file    bsp_can.c
 * @brief   CAN 总线封装实现
 * @layer   BSP
 * @note    接口说明见 bsp_can.h;接收在中断中遍历实例分发回调。
 * @note    源自跃鹿 basic_framework，实例保留收发计数与过滤器配置结果。
 */

#include "bsp_can.h"
#include "main.h"
#include "memory.h"
#include "stdlib.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "robot_infantry_2026.h"

#if CAN1_DIAGNOSTIC_CAPTURE_ENABLE
/* 按ID保留最后一帧和累计次数，避免底盘高频反馈覆盖未知ID。 */
volatile struct {
    uint32_t id;
    uint32_t count;
    uint8_t data[8];
    uint8_t length;
} can1_rx_capture[16];
volatile uint32_t can1_rx_capture_overflow;
#endif

#if CAN2_DIAGNOSTIC_CAPTURE_ENABLE
/* CAN2 版同 CAN1:按ID保留最后一帧与累计帧数,用于"某个电机到底在哪个ID回话"的定性判断。
 * 刻意与 CAN1 的数组保持独立的匿名结构,不改动 CAN1 路径(主机联合测试直接引用其符号)。 */
volatile struct {
    uint32_t id;
    uint32_t count;
    uint8_t data[8];
    uint8_t length;
} can2_rx_capture[16];
volatile uint32_t can2_rx_capture_overflow;
#endif

/* can instance ptrs storage, used for recv callback */
// 在CAN产生接收中断会遍历数组,选出hcan和rxid与发生中断的实例相同的那个,调用其回调函数
// @todo: 后续为每个CAN总线单独添加一个can_instance指针数组,提高回调查找的性能
// @todo: 本模块中运用了锁，但可能影响实时性，后续可尝试使用无锁方案
static CANInstance *can_instance[CAN_MX_REGISTER_CNT] = {NULL};
static uint8_t idx; // 全局CAN实例索引,每次有新的模块注册会自增
static CAN_Send_FIFO_s CAN1_Send_FIFO, CAN2_Send_FIFO; // CAN发送队列，每个hcan对应一个队列
static CANInstance *tx_owner[DEVICE_CAN_CNT][3]; // 邮箱完成中断归属的发送实例
static HAL_StatusTypeDef can_start_status[DEVICE_CAN_CNT];
static HAL_StatusTypeDef can_notification_status[DEVICE_CAN_CNT];

/* ----------------two static function called by CANRegister()-------------------- */

/**
 * @brief  为指定实例的接收 ID 添加硬件过滤器,由 CANRegister() 调用
 *         过滤器生效后,bxCAN 只把 ID 匹配的报文填入 FIFO 并触发中断,其余丢弃。
 * @note   F407 的 bxCAN 共 28 个过滤器:前 14 个归 CAN1,后 14 个归 CAN2。
 *         奇数编号模块分配到 FIFO0,偶数编号到 FIFO1;CAN1 用过滤器 0-13,CAN2 用 14-27。
 * @note   只在初始化期执行一次,业务开发无需关心;底层细节见 RM0090 参考手册 CAN 章节。
 * @param  _instance 由具体模块持有的 CAN 实例
 */
static void CANAddFilter(CANInstance *_instance)
{
    CAN_FilterTypeDef can_filter_conf = {0};
    static uint8_t can1_filter_idx = 0, can2_filter_idx = 14; // 0-13给can1用,14-27给can2用

    can_filter_conf.FilterMode = CAN_FILTERMODE_IDLIST;                                                       // 使用id list模式,即只有将rxid添加到过滤器中才会接收到,其他报文会被过滤
    can_filter_conf.FilterScale = CAN_FILTERSCALE_16BIT;                                                     // 使用16位id模式,即只有低16位有效
    
    if(_instance->can_handle == &hcan1){
        can_filter_conf.FilterFIFOAssignment = (can1_filter_idx & 1) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;              // 奇数编号的模块会被分配到FIFO0,偶数编号的模块会被分配到FIFO1
    }else if(_instance->can_handle == &hcan2){
        can_filter_conf.FilterFIFOAssignment = (can2_filter_idx & 1) ? CAN_RX_FIFO0 : CAN_RX_FIFO1;              // 奇数编号的模块会被分配到FIFO0,偶数编号的模块会被分配到FIFO1
    }

    can_filter_conf.SlaveStartFilterBank = 14;                                                                // 从第14个过滤器开始配置从机过滤器(在STM32的BxCAN控制器中CAN2是CAN1的从机)
    can_filter_conf.FilterIdLow = _instance->rx_id << 5;                                                      // 过滤器寄存器的低16位,因为使用STDID,所以只有低11位有效,高5位要填0
    /* IDLIST 的四个槽都参与匹配，空槽填零会额外接收 ID 0。 */
    can_filter_conf.FilterIdHigh = can_filter_conf.FilterIdLow;
    can_filter_conf.FilterMaskIdLow = can_filter_conf.FilterIdLow;
    can_filter_conf.FilterMaskIdHigh = can_filter_conf.FilterIdLow;
    can_filter_conf.FilterBank = _instance->can_handle == &hcan1 ? (can1_filter_idx++) : (can2_filter_idx++); // 根据can_handle判断是CAN1还是CAN2,然后自增
    can_filter_conf.FilterActivation = CAN_FILTER_ENABLE;                                                     // 启用过滤器

#if CAN1_DIAGNOSTIC_CAPTURE_ENABLE
    if (_instance->can_handle == &hcan1 && can_filter_conf.FilterBank == 0u)
    {
        can_filter_conf.FilterMode = CAN_FILTERMODE_IDMASK;
        can_filter_conf.FilterScale = CAN_FILTERSCALE_32BIT;
        can_filter_conf.FilterIdHigh = can_filter_conf.FilterIdLow = 0u;
        can_filter_conf.FilterMaskIdHigh = 0u;
        can_filter_conf.FilterMaskIdLow = CAN_ID_EXT | CAN_RTR_REMOTE;
    }
#endif
#if CAN2_DIAGNOSTIC_CAPTURE_ENABLE
    /* CAN2 的第一个实例(bank 14)改成全通:让总线上所有标准数据帧都进 FIFO,
     * 未注册的 ID 才能被 can2_rx_capture 记录到(否则会被 IDLIST 过滤掉)。 */
    if (_instance->can_handle == &hcan2 && can_filter_conf.FilterBank == 14u)
    {
        can_filter_conf.FilterMode = CAN_FILTERMODE_IDMASK;
        can_filter_conf.FilterScale = CAN_FILTERSCALE_32BIT;
        can_filter_conf.FilterIdHigh = can_filter_conf.FilterIdLow = 0u;
        can_filter_conf.FilterMaskIdHigh = 0u;
        can_filter_conf.FilterMaskIdLow = CAN_ID_EXT | CAN_RTR_REMOTE;
    }
#endif

    _instance->filter_status = HAL_CAN_ConfigFilter(_instance->can_handle, &can_filter_conf);
    if (_instance->filter_status != HAL_OK)
        LOGERROR("[bsp_can] filter configuration failed, rx [%u]", _instance->rx_id);
}

/**
 * @brief 在第一个CAN实例初始化的时候会自动调用此函数,启动CAN服务
 *
 * @note 此函数会启动CAN1和CAN2,开启CAN1和CAN2的FIFO0 & FIFO1溢出通知
 *
 */
static void CANServiceInit()
{
    CAN_HandleTypeDef *handles[DEVICE_CAN_CNT] = {&hcan1, &hcan2};
    for (uint32_t bus = 0; bus < DEVICE_CAN_CNT; ++bus)
    {
        can_start_status[bus] = HAL_CAN_Start(handles[bus]);
        can_notification_status[bus] = HAL_CAN_ActivateNotification(
            handles[bus], CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_RX_FIFO1_MSG_PENDING |
                          CAN_IT_TX_MAILBOX_EMPTY);
        if (can_start_status[bus] != HAL_OK || can_notification_status[bus] != HAL_OK)
            LOGERROR("[bsp_can] CAN%u start [%u], notification [%u]", bus + 1u,
                     can_start_status[bus], can_notification_status[bus]);
    }
}

// 入队函数，将消息存放至消息队列中
static void CANSendFIFOPut(CANInstance *_instance, CAN_Send_FIFO_s *CAN_Send_FIFO, uint8_t length){

    if(CAN_Send_FIFO->current_size >= CAN_Send_FIFO_SIZE){
        LOGERROR("[bsp_can] CAN Send FIFO full! failed to add msg to FIFO. Cnt [%d]", CAN_Send_FIFO->current_size);
        // 队列满了,说明CAN总线负载过大,需要优化代码或降低CAN总线负载
    }

    uint32_t saved_baspri = __get_BASEPRI();
    __set_BASEPRI(0x50U);// 临界区开始，暂时关闭优先级5及以上的中断，防止竞态

    if(CAN_Send_FIFO->current_size >= CAN_Send_FIFO_SIZE){
        CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->tx_drop_count++;
        CAN_Send_FIFO->front_idx = (CAN_Send_FIFO->front_idx + 1) % CAN_Send_FIFO_SIZE; 
        CAN_Send_FIFO->current_size--; // 队列长度自减,丢弃最早的消息
    }

    memcpy(CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].data_buff, _instance->tx_buff, 8); // 将发送缓存拷贝到队列中
    CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].DLC_length = length; // 将发送长度存入队列中
    CAN_Send_FIFO->queue[CAN_Send_FIFO->back_idx].can_instance = _instance; // 将can实例指针存入队列中,用于发送时获取发送缓存和发送配置

    CAN_Send_FIFO->back_idx = (CAN_Send_FIFO->back_idx + 1) % CAN_Send_FIFO_SIZE; // 队列尾索引自增
    CAN_Send_FIFO->current_size++; // 队列长度自增

    __set_BASEPRI(saved_baspri);// 临界区结束，开启中断
}
 
// 发送函数，同时进行出队和将消息填入邮箱
static void CANSendFIFOTransmit(CAN_Send_FIFO_s *CAN_Send_FIFO){

    uint32_t saved_baspri = __get_BASEPRI();
    __set_BASEPRI(0x50U);// 临界区开始，暂时关闭优先级5及以上的中断，防止竞态

    if(CAN_Send_FIFO->current_size == 0){
        __set_BASEPRI(saved_baspri);// 临界区结束，开启中断
        return; // 队列为空,没有数据可以发送
    }

    CAN_TxHeaderTypeDef temp_txconf = {0};  // 临时header变量，用于CAN发送

    temp_txconf.StdId = CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->tx_id; 
    temp_txconf.IDE = CAN_ID_STD;      
    temp_txconf.RTR = CAN_RTR_DATA;    
    temp_txconf.DLC = CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].DLC_length;// 将数据帧信息填入至header中
    CANInstance *sender = CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance;
    if (HAL_CAN_GetTxMailboxesFreeLevel(sender->can_handle) == 0u)
    {
        __set_BASEPRI(saved_baspri);
        return;
    }
    /* 每条总线只保留一帧在途，由完成中断立即续发队列。
     * 硬件能在 HAL 选择邮箱前释放旧邮箱，单帧在途避免覆盖未结算归属。 */
    const uint32_t bus = sender->can_handle == &hcan1 ? 0u : 1u;
    const uint32_t status = sender->can_handle->Instance->TSR;
    for (uint32_t slot = 0; slot < 3u; ++slot)
    {
        if (tx_owner[bus][slot] != NULL)
        {
            if (!(status & (CAN_TSR_TME0 << slot)) ||
                (status & (CAN_TSR_RQCP0 << (8u * slot))))
            {
                __set_BASEPRI(saved_baspri);
                return;
            }
            /* HAL 已处理失败或撤销，成功回调未执行，此帧不能计为完成。 */
            tx_owner[bus][slot]->tx_error_count++;
            tx_owner[bus][slot] = NULL;
        }
    }
    
    // 将消息填入至邮箱中
    if(HAL_CAN_AddTxMessage(CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->can_handle, &temp_txconf, CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].data_buff, &CAN_Send_FIFO->queue[CAN_Send_FIFO->front_idx].can_instance->tx_mailbox) != HAL_OK){
        sender->tx_error_count++;
        __set_BASEPRI(saved_baspri);// 临界区结束，开启中断
        return; // 发送失败,邮箱满了,等待下一次发送机会
    }

    const uint32_t mailbox = sender->tx_mailbox == CAN_TX_MAILBOX0 ? 0u :
                             sender->tx_mailbox == CAN_TX_MAILBOX1 ? 1u : 2u;
    tx_owner[bus][mailbox] = sender;
    sender->tx_submit_count++;

    CAN_Send_FIFO->front_idx = (CAN_Send_FIFO->front_idx + 1) % CAN_Send_FIFO_SIZE; // 队列头索引自增
    CAN_Send_FIFO->current_size--; // 队列长度自减

    __set_BASEPRI(saved_baspri);// 临界区结束，开启中断
}

/* ----------------------- two extern callable function -----------------------*/

CANInstance *CANRegister(CAN_Init_Config_s *config)
{
    if (!idx)
    {
        CANServiceInit(); // 第一次注册,先进行硬件初始化
        LOGINFO("[bsp_can] CAN Service Init");
    }
    if (idx >= CAN_MX_REGISTER_CNT) // 超过最大实例数
    {
        while (1)
            LOGERROR("[bsp_can] CAN instance exceeded MAX num, consider balance the load of CAN bus");
    }
    for (size_t i = 0; i < idx; i++)
    { // 重复注册 | id重复
        if (can_instance[i]->rx_id == config->rx_id && can_instance[i]->can_handle == config->can_handle)
        {
            while (1)
                LOGERROR("[bsp_can] CAN id crash ,tx [%d] or rx [%d] already registered", config->tx_id, config->rx_id);
        }
    }
    
    CANInstance *instance = (CANInstance *)malloc(sizeof(CANInstance)); // 分配空间
    memset(instance, 0, sizeof(CANInstance));                           // 分配的空间未必是0,所以要先清空
    // 设置回调函数和接收发送id
    instance->can_handle = config->can_handle;
    instance->tx_id = config->tx_id;
    instance->rx_id = config->rx_id;
    instance->can_module_callback = config->can_module_callback;
    instance->id = config->id;
    const uint32_t bus = config->can_handle == &hcan1 ? 0u : 1u;
    instance->start_status = can_start_status[bus];
    instance->notification_status = can_notification_status[bus];

    CANAddFilter(instance);         // 添加CAN过滤器规则
    can_instance[idx++] = instance; // 将实例保存到can_instance中

    return instance; // 返回can实例指针
}

//外部调用的CAN发送函数，对比旧版进行了循环队列优化，防止while忙等导致阻塞，影响实时性
void CANTransmit(CANInstance *_instance, uint8_t length)
{
    if(_instance->can_handle == &hcan1){
        CANSendFIFOPut(_instance, &CAN1_Send_FIFO, length);// 将消息填入至队列中
        CANSendFIFOTransmit(&CAN1_Send_FIFO);// 触发一次发送，防止因消息队列临时为空而导致后台停止触发发送
    }else if(_instance->can_handle == &hcan2){
        CANSendFIFOPut(_instance, &CAN2_Send_FIFO, length);
        CANSendFIFOTransmit(&CAN2_Send_FIFO);
    }
}

/* -----------------------belows are callback definitions--------------------------*/

/**
 * @brief 此函数会被下面两个函数调用,用于处理FIFO0和FIFO1溢出中断(说明收到了新的数据)
 *        所有的实例都会被遍历,找到can_handle和rx_id相等的实例时,调用该实例的回调函数
 *
 * @param _hcan
 * @param fifox passed to HAL_CAN_GetRxMessage() to get mesg from a specific fifo
 */
static void CANFIFOxCallback(CAN_HandleTypeDef *_hcan, uint32_t fifox)
{
    CAN_RxHeaderTypeDef rxconf; // 同上
    uint8_t can_rx_buff[8];
    while (HAL_CAN_GetRxFifoFillLevel(_hcan, fifox)) // FIFO不为空,有可能在其他中断时有多帧数据进入
    {
        if (HAL_CAN_GetRxMessage(_hcan, fifox, &rxconf, can_rx_buff) != HAL_OK)
            break;
        if (rxconf.IDE != CAN_ID_STD || rxconf.RTR != CAN_RTR_DATA || rxconf.DLC > 8u)
            continue;
#if CAN1_DIAGNOSTIC_CAPTURE_ENABLE
        if (_hcan == &hcan1)
        {
            uint32_t slot;
            for (slot = 0; slot < 16u; ++slot)
            {
                if (can1_rx_capture[slot].count == 0u || can1_rx_capture[slot].id == rxconf.StdId)
                {
                    can1_rx_capture[slot].id = rxconf.StdId;
                    can1_rx_capture[slot].length = rxconf.DLC;
                    for (uint32_t byte = 0; byte < 8u; ++byte)
                        can1_rx_capture[slot].data[byte] = byte < rxconf.DLC ? can_rx_buff[byte] : 0u;
                    if (can1_rx_capture[slot].count != UINT32_MAX)
                        can1_rx_capture[slot].count++;
                    break;
                }
            }
            if (slot == 16u)
                can1_rx_capture_overflow++;
        }
#endif
#if CAN2_DIAGNOSTIC_CAPTURE_ENABLE
        if (_hcan == &hcan2)
        {
            uint32_t slot;
            for (slot = 0; slot < 16u; ++slot)
            {
                if (can2_rx_capture[slot].count == 0u || can2_rx_capture[slot].id == rxconf.StdId)
                {
                    can2_rx_capture[slot].id = rxconf.StdId;
                    can2_rx_capture[slot].length = rxconf.DLC;
                    for (uint32_t byte = 0; byte < 8u; ++byte)
                        can2_rx_capture[slot].data[byte] = byte < rxconf.DLC ? can_rx_buff[byte] : 0u;
                    if (can2_rx_capture[slot].count != UINT32_MAX)
                        can2_rx_capture[slot].count++;
                    break;
                }
            }
            if (slot == 16u)
                can2_rx_capture_overflow++;
        }
#endif
        for (size_t i = 0; i < idx; ++i)
        { // 两者相等说明这是要找的实例
            if (_hcan == can_instance[i]->can_handle && rxconf.StdId == can_instance[i]->rx_id)
            {
                if (can_instance[i]->can_module_callback != NULL) // 回调函数不为空就调用
                {
                    can_instance[i]->rx_len = rxconf.DLC;                      // 保存接收到的数据长度
                    can_instance[i]->rx_count++;
                    memcpy(can_instance[i]->rx_buff, can_rx_buff, rxconf.DLC); // 消息拷贝到对应实例
                    can_instance[i]->can_module_callback(can_instance[i]);     // 触发回调进行数据解析和处理
                }
                break;
            }
        }
    }
}

/**
 * @brief 注意,STM32的两个CAN设备共享两个FIFO
 * 下面两个函数是HAL库中的回调函数,他们被HAL声明为__weak,这里对他们进行重载(重写)
 * 当FIFO0或FIFO1溢出时会调用这两个函数
 */
// 下面的函数会调用CANFIFOxCallback()来进一步处理来自特定CAN设备的消息

/**
 * @brief rx fifo callback. Once FIFO_0 is full,this func would be called
 *
 * @param hcan CAN handle indicate which device the oddest mesg in FIFO_0 comes from
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO0); // 调用我们自己写的函数来处理消息
}

/**
 * @brief rx fifo callback. Once FIFO_1 is full,this func would be called
 *
 * @param hcan CAN handle indicate which device the oddest mesg in FIFO_1 comes from
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CANFIFOxCallback(hcan, CAN_RX_FIFO1); // 调用我们自己写的函数来处理消息
}

/**
 * @brief 在CAN邮箱空闲中断回调函数中触发发送
 * 
 * 三个函数作用基本一致
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan){
    CANInstance **owner = &tx_owner[hcan == &hcan1 ? 0 : 1][0];
    if (*owner != NULL)
    {
        (*owner)->tx_complete_count++;
        *owner = NULL;
    }
    CANSendFIFOTransmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) { 
    CANInstance **owner = &tx_owner[hcan == &hcan1 ? 0 : 1][1];
    if (*owner != NULL)
    {
        (*owner)->tx_complete_count++;
        *owner = NULL;
    }
    CANSendFIFOTransmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) { 
    CANInstance **owner = &tx_owner[hcan == &hcan1 ? 0 : 1][2];
    if (*owner != NULL)
    {
        (*owner)->tx_complete_count++;
        *owner = NULL;
    }
    CANSendFIFOTransmit(hcan == &hcan1 ? &CAN1_Send_FIFO : &CAN2_Send_FIFO);
}
