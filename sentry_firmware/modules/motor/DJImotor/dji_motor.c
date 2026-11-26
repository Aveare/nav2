/**
 * @file    dji_motor.c
 * @brief   DJI电机驱动:CAN分组发送与闭环调度
 * @author  neozng
 * @author  modified by JIT CR (电调ID校验/GM6020电流帧/残帧过滤)
 * @date    2026-10-29
 */
#include "dji_motor.h"
#include "general_def.h"
#include "bsp_dwt.h"
#include "bsp_log.h"

static uint8_t idx = 0; // 注册时的全局电机索引
/* 电机实例指针数组,control任务中遍历计算PID */
static DJIMotorInstance *dji_motor_instance[DJI_MOTOR_CNT] = {NULL};

/* CAN1 电机总闸:0 时该总线上所有电机在最后一级被压成 0 输出(见 DJIMotorControl)。
 * 初值由 dji_motor.h 的 DJI_MOTOR_CAN1_ENABLE_BOOT 决定,运行期可被调试器/MCP 直接改写。
 * 非 static 是刻意的:台架上不重烧固件就能切。
 * DJI_MOTOR_CAN1_FORCE_DISABLE=1 时本变量恒为 0 且写不进(见下面的 setter),防疯车用。 */
#if DJI_MOTOR_CAN1_FORCE_DISABLE
#pragma message("!!! DJI_MOTOR_CAN1_FORCE_DISABLE=1: CAN1 motors (4 wheels + yaw) are force-disabled, no driving possible !!!")
#endif

uint8_t g_dji_can1_enable = (DJI_MOTOR_CAN1_FORCE_DISABLE || !DJI_MOTOR_CAN1_ENABLE_BOOT) ? 0 : 1;

void DJIMotorSetCAN1Enable(uint8_t enable)
{
#if DJI_MOTOR_CAN1_FORCE_DISABLE
    /* 硬锁版:任何使能请求都拒绝,只记一次警告,避免被 1kHz 调用刷屏 */
    static uint8_t enable_refused_logged = 0;
    if (enable && !enable_refused_logged)
    {
        LOGWARNING("[dji_motor] CAN1 force-disable build: enable request refused");
        enable_refused_logged = 1;
    }
    (void)enable;
    g_dji_can1_enable = 0;
#else
    g_dji_can1_enable = enable ? 1 : 0;
#endif
}

uint8_t DJIMotorGetCAN1Enable(void)
{
    return g_dji_can1_enable;
}

/**
 * @brief DJI电机以4个一组发送,用6个can_instance(2总线x3组)专门负责组帧发送
 *        分组关系在 MotorSenderGrouping() 中建立,此处给出各组帧ID
 *
 * @note  仅用于发送,不在bsp_can注册
 *
 * C610(m2006)/C620(m3508):0x1ff,0x200; GM6020电压:0x1ff,0x2ff;电流:0x1fe,0x2fe
 * 反馈(rx_id): GM6020: 0x204+id ; C610/C620: 0x200+id
 * can1: [0]:0x1FF/0x1FE,[1]:0x200,[2]:0x2FF/0x2FE
 * can2: [3]:0x1FF/0x1FE,[4]:0x200,[5]:0x2FF/0x2FE
 */
static CANInstance sender_assignment[6] = {
    [0] = {.can_handle = &hcan1, .tx_id = 0x1ff, .tx_buff = {0}},
    [1] = {.can_handle = &hcan1, .tx_id = 0x200, .tx_buff = {0}},
    [2] = {.can_handle = &hcan1, .tx_id = 0x2ff, .tx_buff = {0}},
    [3] = {.can_handle = &hcan2, .tx_id = 0x1ff, .tx_buff = {0}},
    [4] = {.can_handle = &hcan2, .tx_id = 0x200, .tx_buff = {0}},
    [5] = {.can_handle = &hcan2, .tx_id = 0x2ff, .tx_buff = {0}},
};

/* 各分组是否有电机注册的标志,防止发送空帧 */
static uint8_t sender_enable_flag[6] = {0};

/**
 * @brief 根据电调拨码ID计算发送/接收ID,并对电机分组以便4个一组发送
 */
static void MotorSenderGrouping(DJIMotorInstance *motor, CAN_Init_Config_s *config)
{
    // 电调拨码号合法范围1-8;配错会导致分组槽位越界写内存,初始化期fail-fast
    if (config->tx_id < 1 || config->tx_id > 8)
    {
        LOGERROR("[dji_motor] invalid ESC id %u, expect 1-8", config->tx_id);
        while (1)
            ;
    }
    uint8_t motor_id = config->tx_id - 1; // 下标从零开始
    uint8_t motor_send_num;
    uint8_t motor_grouping;

    switch (motor->motor_type)
    {
    case M2006:
    case M3508:
        if (motor_id < 4)
        {
            motor_send_num = motor_id;
            motor_grouping = config->can_handle == &hcan1 ? 1 : 4;
        }
        else
        {
            motor_send_num = motor_id - 4;
            motor_grouping = config->can_handle == &hcan1 ? 0 : 3;
        }

        config->rx_id = 0x200 + motor_id + 1;
        sender_enable_flag[motor_grouping] = 1;
        motor->message_num = motor_send_num;
        motor->sender_group = motor_grouping;

        // ID冲突检查:6020的id1-4与2006/3508的id5-8反馈帧冲突(1!5,2!6,3!7,4!8)
        for (size_t i = 0; i < idx; ++i)
        {
            if (dji_motor_instance[i]->motor_can_instance->can_handle == config->can_handle && dji_motor_instance[i]->motor_can_instance->rx_id == config->rx_id)
            {
                uint16_t can_bus = config->can_handle == &hcan1 ? 1 : 2;
                LOGERROR("[dji_motor] ID crash, id [%d], can_bus [%d]", config->rx_id, can_bus);
                while (1)
                    ;
            }
        }
        break;

    case GM6020:
        if (motor_id < 4)
        {
            motor_send_num = motor_id;
            motor_grouping = config->can_handle == &hcan1 ? 0 : 3;
        }
        else
        {
            motor_send_num = motor_id - 4;
            motor_grouping = config->can_handle == &hcan1 ? 2 : 5;
        }

        /* GM6020电流模式帧ID:0x1FE(ID1-4)/0x2FE(ID5-7),±16384↔±3A;
         * 电压模式0x1FF/0x2FF(±25000)。反馈帧两种模式相同。
         * 同分组内所有电机必须同模式,混用会互踩命令帧,fail-fast。 */
        if (motor->motor_settings.control_mode == MOTOR_CONTROL_CURRENT)
        {
            uint16_t current_tx_id = (motor_grouping == 0 || motor_grouping == 3) ? 0x1fe : 0x2fe;
            if (sender_enable_flag[motor_grouping] &&
                sender_assignment[motor_grouping].tx_id != current_tx_id)
            {
                LOGERROR("[dji_motor] mixed control mode in one sender group, check control_mode");
                while (1)
                    ;
            }
            sender_assignment[motor_grouping].tx_id = current_tx_id;
        }

        config->rx_id = 0x204 + motor_id + 1;
        sender_enable_flag[motor_grouping] = 1;
        motor->message_num = motor_send_num;
        motor->sender_group = motor_grouping;

        for (size_t i = 0; i < idx; ++i)
        {
            if (dji_motor_instance[i]->motor_can_instance->can_handle == config->can_handle && dji_motor_instance[i]->motor_can_instance->rx_id == config->rx_id)
            {
                uint16_t can_bus = config->can_handle == &hcan1 ? 1 : 2;
                LOGERROR("[dji_motor] ID crash, id [%d], can_bus [%d]", config->rx_id, can_bus);
                while (1)
                    ;
            }
        }
        break;

    default: // 其他电机不应使用DJI电机接口注册
        LOGERROR("[dji_motor] You must not register other motors using the API of DJI motor.");
        while (1)
            ;
    }
}

/**
 * @brief 解析电机反馈报文(由CAN接收回调调用)
 *
 * @param _instance 收到数据的can instance,其id字段存有电机实例地址
 */
static void DecodeDJIMotor(CANInstance *_instance)
{
    if (_instance->rx_len != 8) // DJI电机反馈固定8字节,残帧不刷新在线状态
        return;
    uint8_t *rxbuff = _instance->rx_buff;
    DJIMotorInstance *motor = (DJIMotorInstance *)_instance->id;
    DJI_Motor_Measure_s *measure = &motor->measure;

    DaemonReload(motor->daemon);
    motor->dt = DWT_GetDeltaT(&motor->feed_cnt);

    // 解析并对速度/电流做一阶低通,报文格式见电机手册
    measure->last_ecd = measure->ecd;
    measure->ecd = ((uint16_t)rxbuff[0]) << 8 | rxbuff[1];
    measure->angle_single_round = ECD_ANGLE_COEF_DJI * (float)measure->ecd;
    measure->speed_aps = (1.0f - SPEED_SMOOTH_COEF) * measure->speed_aps +
                         RPM_2_ANGLE_PER_SEC * SPEED_SMOOTH_COEF * (float)((int16_t)(rxbuff[2] << 8 | rxbuff[3]));
    measure->real_current = (1.0f - CURRENT_SMOOTH_COEF) * measure->real_current +
                            CURRENT_SMOOTH_COEF * (float)((int16_t)(rxbuff[4] << 8 | rxbuff[5]));
    measure->temperature = rxbuff[6];

    // 多圈角度计算:假设两次采样间转角小于180°,跨半圈时圈数±1
    if (measure->ecd - measure->last_ecd > 4096)
        measure->total_round--;
    else if (measure->ecd - measure->last_ecd < -4096)
        measure->total_round++;
    measure->total_angle = measure->total_round * 360 + measure->angle_single_round;
    motor->feedback_received = 1;
}

static void DJIMotorLostCallback(void *motor_ptr)
{
    DJIMotorInstance *motor = (DJIMotorInstance *)motor_ptr;
    uint16_t can_bus = motor->motor_can_instance->can_handle == &hcan1 ? 1 : 2;
    LOGWARNING("[dji_motor] Motor lost, can bus [%d] , id [%d]", can_bus, motor->motor_can_instance->tx_id);
}

// 电机初始化,返回电机实例
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config)
{
    DJIMotorInstance *instance = (DJIMotorInstance *)malloc(sizeof(DJIMotorInstance));
    memset(instance, 0, sizeof(DJIMotorInstance));

    instance->motor_type = config->motor_type;
    instance->motor_settings = config->controller_setting_init_config;

    PIDInit(&instance->motor_controller.current_PID, &config->controller_param_init_config.current_PID);
    PIDInit(&instance->motor_controller.speed_PID, &config->controller_param_init_config.speed_PID);
    PIDInit(&instance->motor_controller.angle_PID, &config->controller_param_init_config.angle_PID);
    instance->motor_controller.other_angle_feedback_ptr = config->controller_param_init_config.other_angle_feedback_ptr;
    instance->motor_controller.other_speed_feedback_ptr = config->controller_param_init_config.other_speed_feedback_ptr;
    instance->motor_controller.current_feedforward_ptr = config->controller_param_init_config.current_feedforward_ptr;
    instance->motor_controller.speed_feedforward_ptr = config->controller_param_init_config.speed_feedforward_ptr;

    // 电机分组(至多4个电机共用一帧CAN控制报文)
    MotorSenderGrouping(instance, &config->can_init_config);

    // 注册到CAN总线,回调id存电机实例地址
    config->can_init_config.can_module_callback = DecodeDJIMotor;
    config->can_init_config.id = instance;
    instance->motor_can_instance = CANRegister(&config->can_init_config);

    // 注册守护:20ms未收到反馈判定离线
    Daemon_Init_Config_s daemon_config = {
        .callback = DJIMotorLostCallback,
        .owner_id = instance,
        .reload_count = 2,
    };
    instance->daemon = DaemonRegister(&daemon_config);

    DJIMotorEnable(instance);
    dji_motor_instance[idx++] = instance;
    return instance;
}

void DJIMotorChangeFeed(DJIMotorInstance *motor, Closeloop_Type_e loop, Feedback_Source_e type)
{
    if (loop == ANGLE_LOOP)
        motor->motor_settings.angle_feedback_source = type;
    else if (loop == SPEED_LOOP)
        motor->motor_settings.speed_feedback_source = type;
    else
        LOGERROR("[dji_motor] loop type error, check memory access and func param");
}

void DJIMotorStop(DJIMotorInstance *motor)
{
    motor->stop_flag = MOTOR_STOP;
}

void DJIMotorEnable(DJIMotorInstance *motor)
{
    motor->stop_flag = MOTOR_ENABLED;
}

void DJIMotorOuterLoop(DJIMotorInstance *motor, Closeloop_Type_e outer_loop)
{
    motor->motor_settings.outer_loop_type = outer_loop;
}

void DJIMotorSetRef(DJIMotorInstance *motor, float ref)
{
    motor->motor_controller.pid_ref = ref;
}

/**
 * @brief 为所有电机实例计算串级PID并分组发送控制报文(1kHz,由motor_task调用)
 */
void DJIMotorControl()
{
    uint8_t group, num;
    uint8_t bus_disabled;
    int16_t set;
    DJIMotorInstance *motor;
    Motor_Control_Setting_s *motor_setting;
    Motor_Controller_s *motor_controller;
    DJI_Motor_Measure_s *measure;
    float pid_measure, pid_ref;

    for (size_t i = 0; i < idx; ++i)
    {
        motor = dji_motor_instance[i];
        motor_setting = &motor->motor_settings;
        motor_controller = &motor->motor_controller;
        measure = &motor->measure;
        /* 总线级总闸:本电机在 CAN1 且总闸关闭时,本周期输出恒 0。
         * 判断放在每一帧而不是进入函数时算一次:总闸可能被调试器随时改写。
         * 硬锁版(FORCE_DISABLE)不看 g_dji_can1_enable,连调试器改写也绕不过去。 */
#if DJI_MOTOR_CAN1_FORCE_DISABLE
        bus_disabled = (motor->motor_can_instance->can_handle == &hcan1) ? 1 : 0;
#else
        bus_disabled = (!g_dji_can1_enable && motor->motor_can_instance->can_handle == &hcan1) ? 1 : 0;
#endif
        pid_ref = motor_controller->pid_ref;
        if (motor_setting->motor_reverse_flag == MOTOR_DIRECTION_REVERSE)
            pid_ref *= -1;

        // pid_ref 顺次作为各闭环的载体:角度环->速度环->电流环
        if ((motor_setting->close_loop_type & ANGLE_LOOP) && motor_setting->outer_loop_type == ANGLE_LOOP)
        {
            if (motor_setting->angle_feedback_source == OTHER_FEED)
                pid_measure = *motor_controller->other_angle_feedback_ptr;
            else
                pid_measure = measure->total_angle; // 对total_angle闭环,避免单圈边界突跃
            pid_ref = PIDCalculate(&motor_controller->angle_PID, pid_measure, pid_ref);
        }

        if ((motor_setting->close_loop_type & SPEED_LOOP) && (motor_setting->outer_loop_type & (ANGLE_LOOP | SPEED_LOOP)))
        {
            if (motor_setting->feedforward_flag & SPEED_FEEDFORWARD)
                pid_ref += *motor_controller->speed_feedforward_ptr;

            if (motor_setting->speed_feedback_source == OTHER_FEED)
                pid_measure = *motor_controller->other_speed_feedback_ptr;
            else
                pid_measure = measure->speed_aps;
            pid_ref = PIDCalculate(&motor_controller->speed_PID, pid_measure, pid_ref);
        }

        // 输出侧前馈与电流环
        if (motor_setting->feedforward_flag & CURRENT_FEEDFORWARD)
            pid_ref += *motor_controller->current_feedforward_ptr;
        if (motor_setting->close_loop_type & CURRENT_LOOP)
        {
            pid_ref = PIDCalculate(&motor_controller->current_PID, measure->real_current, pid_ref);
        }

        if (motor_setting->feedback_reverse_flag == FEEDBACK_DIRECTION_REVERSE)
            pid_ref *= -1;

        set = (int16_t)pid_ref;

        // CAN1 总闸关闭时强制零力矩:反馈(ecd/温度/在线判据)照常,只是不出力
        if (bus_disabled)
        {
            set = 0;
        }

        // 分组填入发送缓冲
        group = motor->sender_group;
        num = motor->message_num;
        sender_assignment[group].tx_buff[2 * num] = (uint8_t)(set >> 8);
        sender_assignment[group].tx_buff[2 * num + 1] = (uint8_t)(set & 0x00ff);

        // 停止状态的电机槽位直接置零(仍发帧,保持组同步);CAN1 总闸关闭时同走这条路径
        if (motor->stop_flag == MOTOR_STOP || bus_disabled)
        {
            memset(sender_assignment[group].tx_buff + 2 * num, 0, sizeof(uint16_t));
            /* 停机期间连续清 PID 状态:否则积分会跨停机保留,再次使能瞬间带出残余力矩
             * (云台零力->再解锁、急停后恢复都会走这条路)。DM 电机在模式切换处已有同样先例。
             * 依据:.Doc/yaw轴深化改造计划.md §3.6 / P0-2(2026-11-12)。 */
            PIDClear(&motor_controller->angle_PID);
            PIDClear(&motor_controller->speed_PID);
            PIDClear(&motor_controller->current_PID);
        }
    }

    // 只发送有电机注册的分组
    for (size_t i = 0; i < 6; ++i)
    {
        if (sender_enable_flag[i])
        {
            CANTransmit(&sender_assignment[i], 8);
        }
    }
}
