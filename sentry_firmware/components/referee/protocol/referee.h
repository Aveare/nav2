/**
 * @file    referee.h
 * @brief   裁判数据结构与收发接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    只保留最新一帧:各 ext_* 字段是"最近一次收到该 CMD"的缓存,不是队列。
*          某条 CMD 停止下发后,对应字段会停留在旧值,不会自动清零 —— 判断有效性
*          必须靠 daemon 或下发频率,不能靠字段内容。
* @note    0x0202 的底盘功率曾被命名为 reserved3 且从未解析,下位机读不到真值;
*          2026-11-20 已按官方协议更正为 voltage / current / power(float) 三个具名字段,
*          布局仍为 14 字节并加编译期守卫。功率控制与 UI 直接取 PowerHeatData.chassis_power。
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式并记录上述缺口。
 */
#ifndef RM_REFEREE_H
#define RM_REFEREE_H

#include "usart.h"
#include "referee_protocol.h"
#include "usart_port.h"
#include "types.h" // UI 交互结构需要的模式枚举(模块共享类型)

extern uint8_t referee_ui_seq;

#pragma pack(1)
typedef struct
{
    uint8_t Robot_Color;        // 机器人颜色:0 = 红,1 = 蓝
    uint16_t Robot_ID;          // 本机器人 ID(红蓝方由 Robot_Color 区分,蓝方编号整体偏移)
    uint16_t Cilent_ID;         // 本机器人对应的客户端 ID(原文拼写如此,勿改字段名以免破坏协议)
    uint16_t Receiver_Robot_ID; // 车间通信接收者 ID,必须与本机器人同色
} referee_id_t;

// 裁判系统接收数据总表 + UI 绘制与车间通信信息
typedef struct
{
    referee_id_t referee_id;

    referee_frame_header_t FrameHeader;        // 最近一帧的帧头
    uint16_t CmdID;                  // 最近一帧的指令 ID
    ext_game_state_t GameState;      // 0x0001 比赛状态
    ext_game_result_t GameResult;    // 0x0002 比赛结果
    referee_game_robot_hp_t GameRobotHP; // 0x0003 机器人血量
    ext_event_data_t EventData;      // 0x0101 场地事件

    ext_referee_warning_t WarnData; // 0x0104 裁判警告
    ext_dart_info_t DartData;       // 0x0105 飞镖状态

    ext_game_robot_state_t GameRobotState; // 0x0201 机器人状态(等级/血量上限/底盘功率上限)
    ext_power_heat_data_t PowerHeatData;   // 0x0202 功率与热量(功率字段未解析,见文件头)
    ext_game_robot_pos_t GameRobotPos;     // 0x0203 机器人位置
    ext_buff_musk_t BuffMusk;              // 0x0204 增益状态

    ext_robot_hurt_t RobotHurt; // 0x0206 伤害事件
    ext_shoot_data_t ShootData; // 0x0207 射击数据

    ext_projectile_allowance_t AllowanceData; // 0x0208 弹丸允许量
    ext_rfid_status_t RFIDdata;               // 0x0209 RFID 状态
    ext_map_command_t MapData;                // 0x0303 地图交互指令

    // 自定义交互数据(如自定义控制器 0x0302)
    referee_communicate_receive_t ReceiveData;

    uint8_t init_flag; // 是否已完成首帧解析

    uint32_t PowerHeatTick; // 0x0202 累计帧号,每收到一帧 +1
    /* 帧号存在的理由:热量值在两帧之间可能一字不变,上层若靠"数值是否变化"判断
     * 新帧,会把同一帧当成新数据反复处理(向下校正会每周期误触发一次)。
     * 判新帧必须用本计数,不能用 PowerHeatData 的内容。 */

} referee_info_t;

// 模式切换标志:置 1 表示该模式本帧发生了变化,供 UI 判断是否需要重绘
typedef struct
{
    uint32_t chassis_flag : 1;
    uint32_t gimbal_flag : 1;
    uint32_t shoot_flag : 1;
    uint32_t lid_flag : 1;
    uint32_t friction_flag : 1;
    uint32_t Power_flag : 1;
} referee_interactive_flag_t;

// UI 绘制与车间通信需要的本机状态(非裁判系统下发数据)
typedef struct
{
    referee_interactive_flag_t Referee_Interactive_Flag;
    // 当前模式,供 UI 绘制与交互数据使用
    chassis_mode_e chassis_mode;             // 底盘模式
    gimbal_mode_e gimbal_mode;               // 云台模式
    shoot_mode_e shoot_mode;                 // 发射模式
    friction_mode_e friction_mode;           // 摩擦轮开关
    lid_mode_e lid_mode;                     // 弹舱盖
    chassis_power_data_t Chassis_Power_Data; // 功率控制数据(当前未接入)

    // 上一次的模式,用于生成上面的 flag
    chassis_mode_e chassis_last_mode;
    gimbal_mode_e gimbal_last_mode;
    shoot_mode_e shoot_last_mode;
    friction_mode_e friction_last_mode;
    lid_mode_e lid_last_mode;
    chassis_power_data_t Chassis_last_Power_Data;

} referee_interactive_info_t;

#pragma pack()

/**
 * @brief  初始化裁判系统通信:配置串口并开启接收中断
 * @param  referee_usart_handle 裁判系统串口句柄(C 板一般为 USART6)
 * @return 裁判系统数据总表指针;之后由裁判系统任务持续填充
 * @note   首帧到达前各字段内容不可用,依赖 init_flag 或 daemon 判断。
 */
referee_info_t *referee_init(UART_HandleTypeDef *referee_usart_handle);

/**
 * @brief  发送 UI 绘制数据或车间通信数据
 * @param  send   发送数据首地址
 * @param  tx_len 发送长度,字节
 * @note   函数内部含 RTOS 延时,因为裁判系统对 CMD 数据的接收上限约 10Hz。
 *         因此 UI 刷新率上限约 10Hz,**禁止在 200Hz 控制周期内调用**,否则会拖垮调度。
 */
void referee_send(uint8_t *send, uint16_t tx_len);

#endif // !REFEREE_H
