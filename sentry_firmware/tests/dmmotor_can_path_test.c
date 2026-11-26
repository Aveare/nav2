/* 编译真实 BSP + 达妙驱动，仅模拟 HAL 外设边界。 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dmmotor.h"

static CAN_TypeDef registers[2];
CAN_HandleTypeDef hcan1 = {&registers[0]}, hcan2 = {&registers[1]};
static CAN_HandleTypeDef *rx_can = &hcan1, *sent_can;
static CAN_FilterTypeDef filters[28];
static uint32_t notices[2], baspri, sent, rx_pending, rx_fifo;
static CAN_RxHeaderTypeDef rx_header;
static uint8_t rx_bytes[8], sent_bytes[8];
static CAN_TxHeaderTypeDef sent_header;
static HAL_StatusTypeDef injected_submit = HAL_OK;
static HAL_StatusTypeDef injected_start = HAL_OK, injected_notice = HAL_OK;
static HAL_StatusTypeDef injected_filter = HAL_OK;
static unsigned selected_slot, submitted_slot;

uint32_t __get_BASEPRI(void) { return baspri; }
void __set_BASEPRI(uint32_t value) { baspri = value; }
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *can) { (void)can; return injected_start; }
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *can, uint32_t mask)
{
    notices[can == &hcan1 ? 0 : 1] |= mask;
    return injected_notice;
}
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *can, CAN_FilterTypeDef *filter)
{
    (void)can; filters[filter->FilterBank] = *filter; return injected_filter;
}
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *can)
{
    return (can->Instance->TSR >> 26) & 7u;
}
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *can, const CAN_TxHeaderTypeDef *header,
                                      const uint8_t data[], uint32_t *mailbox)
{
    if (injected_submit != HAL_OK) return injected_submit;
    assert(baspri == 0x50 && (can->Instance->TSR & (CAN_TSR_TME0 << selected_slot)));
    sent_header = *header; memcpy(sent_bytes, data, 8); ++sent;
    sent_can=can;
    submitted_slot=selected_slot;
    *mailbox = CAN_TX_MAILBOX0 << selected_slot;
    can->Instance->TSR &= ~(CAN_TSR_TME0 << selected_slot);
    return HAL_OK;
}
uint32_t HAL_CAN_GetRxFifoFillLevel(CAN_HandleTypeDef *can, uint32_t fifo)
{
    return can == rx_can && fifo == rx_fifo ? rx_pending : 0;
}
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *can, uint32_t fifo,
                                      CAN_RxHeaderTypeDef *header, uint8_t data[])
{
    assert(can == rx_can && fifo == rx_fifo && rx_pending);
    *header = rx_header; memcpy(data, rx_bytes, 8); rx_pending = 0;
    return HAL_OK;
}
void PIDInit(PIDInstance *pid, PID_Init_Config_s *cfg) { (void)pid; (void)cfg; }
float PIDCalculate(PIDInstance *pid, float measure, float ref) { (void)pid; return ref - measure; }
void PIDClear(PIDInstance *pid) { (void)pid; }
void DWT_Delay(float delay) { (void)delay; }

#include "../bsp/can/bsp_can.c"
#include "../modules/motor/DMmotor/dmmotor.c"

static void CompleteOn(CAN_HandleTypeDef *bus)
{
    /* HAL 清 RQCP/TXOK 后才调用成功回调。 */
    bus->Instance->TSR = 7u << 26;
    switch(submitted_slot) {
    case 0: HAL_CAN_TxMailbox0CompleteCallback(bus); break;
    case 1: HAL_CAN_TxMailbox1CompleteCallback(bus); break;
    case 2: HAL_CAN_TxMailbox2CompleteCallback(bus); break;
    }
}
static void Complete(void) { CompleteOn(&hcan1); }
static void Receive(uint32_t id, uint8_t device, uint32_t length)
{
    rx_header = (CAN_RxHeaderTypeDef){.StdId=id, .IDE=CAN_ID_STD, .RTR=CAN_RTR_DATA, .DLC=length};
    const uint8_t data[8] = {(uint8_t)(0x10u | device), 0x9c, 0x40, 0x7f, 0xf7, 0xff, 30, 31};
    memcpy(rx_bytes, data, 8); rx_fifo = CAN_RX_FIFO1; rx_pending = 1;
    HAL_CAN_RxFifo1MsgPendingCallback(rx_can);
}

int main(int argc, char **argv)
{
    (void)argv;
    registers[0].TSR = registers[1].TSR = 7u << 26;
    /* 单独进程故障注入，覆盖初始化失败被记录而非误报成功。 */
    if (argc > 1) {
        injected_start=HAL_TIMEOUT; injected_notice=HAL_ERROR; injected_filter=HAL_ERROR;
    }
    MIT_Init_Config_s cfg = {
        .can_init_config={.can_handle=&hcan1,.tx_id=5,.rx_id=0},
        .controller_param_init_config={.Kp=54,.Kd=0.5f},
    };
    DMMotorInstance *motor=MITMotorInit(&cfg);
    CANInstance *can=motor->motor_can_instance;
    assert(can->start_status==injected_start && can->notification_status==injected_notice);
    assert(can->filter_status==injected_filter);
    if (argc > 1) { puts("PASS: CAN initialization failure status retained"); free(can); return 0; }
    assert(notices[0]==(CAN_IT_RX_FIFO0_MSG_PENDING|CAN_IT_RX_FIFO1_MSG_PENDING|CAN_IT_TX_MAILBOX_EMPTY));
    assert(filters[0].FilterFIFOAssignment==CAN_RX_FIFO1 && filters[0].FilterBank==0);
#if CAN1_DIAGNOSTIC_CAPTURE_ENABLE
    assert(filters[0].FilterMode==CAN_FILTERMODE_IDMASK && filters[0].FilterScale==CAN_FILTERSCALE_32BIT);
    assert(filters[0].FilterMaskIdLow==(CAN_ID_EXT|CAN_RTR_REMOTE));
#else
    assert(filters[0].FilterMode==CAN_FILTERMODE_IDLIST && filters[0].FilterScale==CAN_FILTERSCALE_16BIT);
    assert(filters[0].FilterIdLow==0 && filters[0].FilterIdHigh==0 &&
           filters[0].FilterMaskIdLow==0 && filters[0].FilterMaskIdHigh==0);
#endif
    assert(sent==1 && sent_header.StdId==5 && sent_header.DLC==8 && sent_header.IDE==CAN_ID_STD);
    for (unsigned i=0;i<7;++i) assert(sent_bytes[i]==0xff);
    assert(sent_bytes[7]==0xfc && can->tx_submit_count==1 && can->tx_complete_count==0);
    Complete(); assert(can->tx_complete_count==1);

    DMMotorStop(motor);
    for (unsigned i=0;i<32;++i) { DMMotorTask(); Complete(); }
    assert(sent>1 && sent_bytes[4]==0 && sent_bytes[5]==0);
    assert(can->rx_count==0 && motor->rx_init_flag==0);
    Receive(1,5,8); assert(can->rx_count==0 && motor->rx_init_flag==0);
    Receive(0,6,8); assert(can->rx_count==1 && motor->rx_init_flag==0);
    Receive(0,5,7); assert(can->rx_count==2 && motor->rx_init_flag==0);
    Receive(0,5,8); assert(can->rx_count==3 && motor->rx_init_flag==1);
    assert(motor->measure.position_raw==40000 && motor->measure.state==DM_STATE_ENABLE);
#if CAN1_DIAGNOSTIC_CAPTURE_ENABLE
    assert(can1_rx_capture[0].id==1 && can1_rx_capture[0].count==1);
    assert(can1_rx_capture[1].id==0 && can1_rx_capture[1].count==3);
    assert(can1_rx_capture[1].data[0]==0x15 && can1_rx_capture[1].length==8);
    assert(can1_rx_capture_overflow==0);
#endif

    CAN_Init_Config_s wheel_config={.can_handle=&hcan1,.tx_id=0x200,.rx_id=0x201};
    CANInstance *wheel=CANRegister(&wheel_config);
    assert(filters[1].FilterFIFOAssignment==CAN_RX_FIFO0);
    assert(filters[1].FilterIdLow==0x4020 && filters[1].FilterIdHigh==0x4020 &&
           filters[1].FilterMaskIdLow==0x4020 && filters[1].FilterMaskIdHigh==0x4020);
    for (selected_slot=0;selected_slot<3;++selected_slot) {
        const uint32_t dm_done=can->tx_complete_count, wheel_done=wheel->tx_complete_count;
        CANTransmit(can,8); CANTransmit(wheel,8);
        assert(wheel->tx_complete_count==wheel_done);
        Complete(); assert(can->tx_complete_count==dm_done+1 && sent_header.StdId==0x200);
        Complete(); assert(wheel->tx_complete_count==wheel_done+1);
    }
    selected_slot=0;

    /* 邮箱完成待处理中不能复用，避免旧帧完成归属被覆盖。 */
    DM_Motor_Command(DM_CMD_MOTOR_MODE,motor);
    const unsigned before=sent;
    registers[0].TSR=CAN_TSR_TME0|CAN_TSR_RQCP0;
    CANTransmit(can,8); assert(sent==before && CAN1_Send_FIFO.current_size==1);
    Complete(); assert(sent==before+1 && CAN1_Send_FIFO.current_size==0); Complete();

    injected_submit=HAL_ERROR;
    CANTransmit(can,8); assert(can->tx_error_count==1 && CAN1_Send_FIFO.current_size==1);
    injected_submit=HAL_OK;
    CANTransmit(can,8); Complete(); Complete();
    assert(CAN1_Send_FIFO.current_size==0);
    /* HAL处理发送失败后无成功回调，下次生产者调用应释放旧owner并续发。 */
    CANTransmit(can,8); registers[0].TSR=CAN_TSR_TME0;
    CANTransmit(can,8); assert(can->tx_error_count==2); Complete();

    CANTransmit(can,8);
    for (unsigned i=0;i<CAN_Send_FIFO_SIZE+2;++i) CANTransmit(can,8);
    assert(can->tx_drop_count==2 && CAN1_Send_FIFO.current_size==CAN_Send_FIFO_SIZE);
    for (unsigned i=0;i<CAN_Send_FIFO_SIZE+1;++i) Complete();
    assert(CAN1_Send_FIFO.current_size==0 && baspri==0);

    MIT_Init_Config_s pitch_cfg = {
        .can_init_config={.can_handle=&hcan2,.tx_id=5,.rx_id=SENTRY_PITCH_DM_MASTER_ID},
        .controller_param_init_config={.Kp=0,.Kd=0},
    };
    DMMotorInstance *pitch=MITMotorInit(&pitch_cfg);
    CANInstance *pitch_can=pitch->motor_can_instance;
    assert(sent_can==&hcan2 && sent_header.StdId==5 && sent_bytes[7]==0xfc);
    assert(filters[14].FilterFIFOAssignment==CAN_RX_FIFO1);
    assert(filters[14].FilterIdLow==(SENTRY_PITCH_DM_MASTER_ID<<5) &&
           filters[14].FilterIdHigh==(SENTRY_PITCH_DM_MASTER_ID<<5));
    assert(pitch_can->start_status==0 && pitch_can->notification_status==0);
    CompleteOn(&hcan2);
    DMMotorStop(pitch);
    rx_can=&hcan2;
    Receive(SENTRY_PITCH_DM_MASTER_ID ^ 1u,5,8); assert(pitch->rx_init_flag==0);
    Receive(SENTRY_PITCH_DM_MASTER_ID,5,8); assert(pitch->rx_init_flag==1 && pitch->measure.position_raw==40000);
    assert(pitch_can->rx_count==1 && can->rx_count==3);
    for (unsigned i=0;i<32;++i) { DMMotorTask(); Complete(); CompleteOn(&hcan2); }
    assert(pitch_can->tx_complete_count>1);
    assert(sent_can==&hcan2 && sent_header.StdId==5 && sent_bytes[4]==0);
    DMMotorSetMITTorque(pitch,0.3f);
    DMMotorEnable(pitch);
    for (unsigned i=0;i<DM_TASK_CYCLE;++i) { DMMotorTask(); Complete(); CompleteOn(&hcan2); }
    assert(sent_can==&hcan2 && sent_bytes[4]==0 && sent_bytes[5]==0);
    assert((((sent_bytes[6]&0x0f)<<8)|sent_bytes[7])>2047); // 零增益外部力矩帧
    free(pitch_can);
    free(can);
    free(wheel);
    puts("PASS: real CAN queue/FC-ID5/filters-ID0-ID201/DM decode/3-mailboxes/owner/error retry/overflow");
    puts("PASS: CAN2 pitch-ID5 configured reply-ID, separate buses, feedback, zero-force and MIT torque output");
    return 0;
}
