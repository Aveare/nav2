/**
 * @file    dji_motor.h
 * @brief   DJI 电机驱动接口:实例注册、闭环切换与参考值下发
 * @layer   MODULE
 * @note    反馈与设定值都在"电机转子"域:角度 deg、角速度 deg/s、电流为电调原始码。
 *          需要输出轴量时自行除以减速比(3508 / 2006 分别为 19 / 36,6020 无减速)。
 * @note    源自跃鹿 basic_framework(作者 neozng,上游版本 0.2 / 2022-11-01,
 *          版权 HNU YueLu EC);JIT CR 增加 feedback_received 首次反馈标志。
 */

#ifndef DJI_MOTOR_H
#define DJI_MOTOR_H

#include "bsp_can.h"
#include "controller.h"
#include "motor_def.h"
#include "stdint.h"
#include "daemon.h"
#include "robot_infantry_2026.h"

#define DJI_MOTOR_CNT 12

/* 滤波系数为 1 时即关闭滤波 */
#define SPEED_SMOOTH_COEF 0.85f      // 速度一阶低通系数
#define CURRENT_SMOOTH_COEF 0.9f     // 电流一阶低通系数
#define ECD_ANGLE_COEF_DJI 0.043945f // 360/8192,编码器值 → 角度,deg

/* DJI 电机 CAN 反馈信息。所有角度/角速度均在电机转子侧,不是输出轴。 */
typedef struct
{
    uint16_t last_ecd;        // 上一次编码器值,0-8191
    uint16_t ecd;             // 当前编码器值,0-8191
    float angle_single_round; // 单圈角度,deg(电机轴)
    float speed_aps;          // 角速度,deg/s(电机转子,已低通)
    int16_t real_current;     // 实际电流,电调原始码(已低通;C620 满量程 16384,C610 为 10000)
    uint8_t temperature;      // 温度,℃

    float total_angle;   // 多圈累计角度,deg(电机轴);方向由安装方向与反转标志共同决定
    int32_t total_round; // 累计圈数,带符号
} DJI_Motor_Measure_s;

typedef struct
{
    DJI_Motor_Measure_s measure;            // 电机测量值
    Motor_Control_Setting_s motor_settings; // 电机设置
    Motor_Controller_s motor_controller;    // 电机控制器

    CANInstance *motor_can_instance; // 电机 CAN 实例
    // 分组发送设置
    uint8_t sender_group;
    uint8_t message_num;

    Motor_Type_e motor_type;        // 电机类型
    Motor_Working_Type_e stop_flag; // 启停标志

    DaemonInstance *daemon;
    uint32_t feed_cnt;
    float dt;
    volatile uint8_t feedback_received; // 首次反馈标志(会溢出的 DWT 计数不能作在线判据)
} DJIMotorInstance;

/**
 * @brief  注册一个 DJI 电机,在应用初始化时调用
 * @param  config 电机初始化配置(控制设置 + PID + 类型 + CAN 配置)
 * @return 电机实例指针
 * @note   反馈报文 ID:M3508 / M2006 为 0x200 + id,GM6020 为 0x204 + id。
 *         两组 ID 空间重叠,分配时不得冲突:1 与 5、2 与 6、3 与 7、4 与 8 不能同时使用。
 *         冲突或电调编号非法会在初始化时 LOGERROR 并挂起初始化。
 */
DJIMotorInstance *DJIMotorInit(Motor_Init_Config_s *config);

/**
 * @brief  给电机设定参考值。应用层可把电机视为传递函数为 1 的设备,无需关心底层闭环。
 * @param  motor 目标电机实例
 * @param  ref   参考值,**单位由外环类型决定**:
 *               角度环 → deg(电机轴);速度环 → deg/s(电机转子);电流环 → 电调原始码。
 *               单位给错不会报错,只会表现为"转得很怪",这是本工程最常见的坑之一。
 */
void DJIMotorSetRef(DJIMotorInstance *motor, float ref);

/**
 * @brief  切换闭环的反馈来源(例如云台把角度/角速度反馈换成 IMU)
 * @param  motor 目标电机实例
 * @param  loop  要切换的闭环
 * @param  type  目标反馈来源
 * @note   切换瞬间反馈值会跳变,若外环积分项不清零会产生输出突跳;
 *         云台切换前需确认目标角已与当前姿态对齐。
 */
void DJIMotorChangeFeed(DJIMotorInstance *motor, Closeloop_Type_e loop, Feedback_Source_e type);

/**
 * @brief  计算所有电机实例的三环 PID 并分组发送控制报文
 * @note   由 motor_task 以 1kHz 调用;**只能有一个调用者**,否则报文会重复发送。
 */
void DJIMotorControl();

/**
 * @brief  停止电机:直接将下发的电流/电压置零,而不是把设定值置零
 * @note   区别:SetRef(0) 仍走闭环,可能有保持力矩;Stop 是彻底断输出,电机自由。
 */
void DJIMotorStop(DJIMotorInstance *motor);

/**
 * @brief  启动电机,使其重新响应设定值(初始化后默认已使能)
 */
void DJIMotorEnable(DJIMotorInstance *motor);

/**
 * @brief  修改电机外层闭环类型(角度/速度/电流)
 * @param  motor      目标电机实例
 * @param  outer_loop 目标外环类型
 * @note   切换瞬间参考值单位随之改变;若不清积分,切换后首个周期可能输出异常。
 */
void DJIMotorOuterLoop(DJIMotorInstance *motor, Closeloop_Type_e outer_loop);

/* ==================== CAN1 电机总闸(台架/整备用,2026-11-15) ====================
 *
 * 作用:一条开关同时失能/使能挂在 **CAN1** 上的全部 DJI 电机(四轮 M3508 + yaw GM6020)。
 *       失能时输出电流/电压恒为 0,但**反馈照常更新**(电调照发反馈帧、编码器/温度/在线判据都还在),
 *       所以推车、架空测试、手转云台都不受电机反抗;使能后立即恢复闭环,无需重新初始化。
 *
 * 与 DJIMotorStop() 的区别:
 *   - DJIMotorStop() 是**单个电机**的停机,并且会被同一周期内的 DJIMotorEnable() 覆盖(上层每帧都会重新下指令);
 *   - 总闸是**总线级、最后一级**的强制零输出,与上层算出的指令无关,因此不会被底盘/云台任务"顶回去"。
 *   两者可叠加:总闸关闭时连停机的 PID 清零逻辑也会走(见 dji_motor.c 的停机分支)。
 *
 * 用法:
 *   1) 编译期决定上电初值:DJI_MOTOR_CAN1_ENABLE_BOOT 1=正常(默认),0=上电即失能。
 *   2) 运行期切换(推荐,不用重烧):
 *        canvas 里直接改全局变量 g_dji_can1_enable = 0/1,
 *        或用 MCP:variables_write {"name":"g_dji_can1_enable","value":0},
 *        或调用 DJIMotorSetCAN1Enable(0/1) / 宏 DJI_MOTOR_CAN1_DISABLE_ALL()、DJI_MOTOR_CAN1_ENABLE_ALL()。
 *   3) 只影响 CAN1:CAN2 上的 pitch / 摩擦轮 / 拨盘不受影响,反之亦然。
 *
 * 为什么不做成"停止发送控制帧":C620/C610 在丢帧约 1s 后会自行停机并进入保护,
 * 想重新使能就得断电重来;持续发 0 电流帧则随时可以恢复。
 * ============================================================================ */
#ifndef DJI_MOTOR_CAN1_ENABLE_BOOT
#define DJI_MOTOR_CAN1_ENABLE_BOOT 0
#endif

/* ---------------------------- 防疯车硬锁(编译期,2026-11-15 加) ----------------------------
 * 1 = 这一版固件**永久失能 CAN1 上全部 DJI 电机**(四轮 + yaw),输出恒 0,并且**运行期无法打开**:
 *     改 g_dji_can1_enable、调 DJIMotorSetCAN1Enable(1)、上层算多大指令 — 统统无效。
 *     烧这一版就绝对不会有底盘自走。撞过东西的场合、理线/焊云台/推车找位置时用。
 * 0 = 正常,总闸按 DJI_MOTOR_CAN1_ENABLE_BOOT 定初值、运行期可随意切。
 *
 * 与 DJI_MOTOR_CAN1_ENABLE_BOOT 的分工:
 *   ENABLE_BOOT 只定**上电初值**,一句 variables_write 就能打开(台架方便,防不住误操作);
 *   FORCE_DISABLE 是**编译期焊死**的,追求"绝对不出力",代价是这一版跑不了任何驱动测试。
 *   两者共存时 FORCE_DISABLE 优先。开了硬锁构建日志会打 #pragma message 提醒。
 *
 * 切换节奏:焊/推/调机械 → =1 安全版;要跑 S1/S2/E1(需要电机出力)→ 改回 0 重烧。
 * ---------------------------------------------------------------------------------------- */
#ifndef DJI_MOTOR_CAN1_FORCE_DISABLE
#define DJI_MOTOR_CAN1_FORCE_DISABLE 1 /* 当前=1:安全版(防疯车)。跑 S1/S2/E1 等驱动测试前改回 0 重烧 */
#endif

/* 1=CAN1 电机正常出力,0=全部强制零输出。非 static,便于调试器/MCP 直接读写。 */
extern uint8_t g_dji_can1_enable;

/**
 * @brief  失能/使能 CAN1 上的全部 DJI 电机(总线级总闸,最后一级生效)
 * @param  enable 1=使能(正常),0=失能(输出恒 0,反馈照常)
 * @note   运行期立即生效,下一个 1kHz 控制周期开始作用;失能期间三环积分会被连续清零,
 *         使能后从零状态起步,不会带出停机前的残余力矩。
 */
void DJIMotorSetCAN1Enable(uint8_t enable);

/**
 * @brief  查询 CAN1 电机总闸当前状态
 * @return 1=使能,0=失能
 */
uint8_t DJIMotorGetCAN1Enable(void);

/* 便于在代码里当语句用的两个宏 */
#define DJI_MOTOR_CAN1_DISABLE_ALL() DJIMotorSetCAN1Enable(0)
#define DJI_MOTOR_CAN1_ENABLE_ALL() DJIMotorSetCAN1Enable(1)

#endif // !DJI_MOTOR_H
