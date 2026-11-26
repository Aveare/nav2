/**
 * @file    robot_def.h
 * @brief   应用间消息协议合同:枚举、消息结构体与双板布局守卫
 * @layer   APP
 * @note    本文件只放类型与结构体,数值一律在 config/robot_infantry_2026.h。
 * @note    所有消息结构体位于 #pragma pack(1) 内,布局受文件尾 _Static_assert 守卫:
 *          增删字段 → 先同步对端板卡固件 → 再更新断言期望值。只改断言不改固件 = 静默错位。
 * @note    本文件源自跃鹿 basic_framework(作者 NeoZeng,版权 HNU YueLu EC,
 *          上游版本 0.1 / 2022-12-02),由 JIT CR 改造:双板协议布局守卫、
 *          config 引用、三发与键鼠相关字段。
 */
#ifndef ROBOT_DEF_H
#define ROBOT_DEF_H

#include "robot_infantry_2026.h" // 本车参数(板型/零位/限位/几何/速度),换车只改 config
#include "ins_task.h"
#include "master_process.h"
#include "stdint.h"

/* 板型冲突检查:ONE_BOARD / CHASSIS_BOARD / GIMBAL_BOARD 只允许定义一个。
 * 该检查在预编译期完成,配错板型会直接编译失败,不会烧出行为异常的固件。 */
#if (defined(ONE_BOARD) && defined(CHASSIS_BOARD)) || \
    (defined(ONE_BOARD) && defined(GIMBAL_BOARD)) ||  \
    (defined(CHASSIS_BOARD) && defined(GIMBAL_BOARD))
#error Conflict board definition! You can only define one board type.
#endif

#pragma pack(1) // 取消字节对齐:以下结构体可能经 CAN 原样透传,不允许填充字节

/* ---------------- 基本状态枚举 ---------------- */

typedef enum
{
    ROBOT_STOP = 0, // 停机:零力,不接受控制输入,需重新走解锁流程
    ROBOT_READY,    // 就绪:已解锁,执行控制指令
} Robot_Status_e;

typedef enum
{
    APP_OFFLINE = 0, // 应用离线:守护线程判定超时
    APP_ONLINE,      // 应用在线
    APP_ERROR,       // 应用报错:通常伴随安全态
} App_Status_e;

/* 底盘模式:由 cmd 每周期发布,chassis 据此决定 wz 的来源,不保存历史状态。 */
typedef enum
{
    CHASSIS_ZERO_FORCE = 0,    // 零力:四轮电流置零
    CHASSIS_ROTATE,            // 小陀螺:wz 恒为 CHASSIS_ROTATE_SPEED,同时保持全向机动
    CHASSIS_NO_FOLLOW,         // 不跟随:wz 恒 0,允许全向平移
    CHASSIS_FOLLOW_GIMBAL_YAW, // 跟随云台:wz = 位置项 + 速度阻尼 + 陀螺仪前馈
    CHASSIS_RC_CONTROL,        // 底盘调试手动转向
    CHASSIS_NAVIGATION,        // USB NV v1 上位机底盘导航(保留 wz,不二次旋云台角)
} chassis_mode_e;

/* 云台模式 */
typedef enum
{
    GIMBAL_ZERO_FORCE = 0, // 零力:yaw/pitch 电流置零
    GIMBAL_FREE_MODE,      // 自由模式:与底盘分离,反馈用电机编码器相对角
    GIMBAL_GYRO_MODE,      // 陀螺仪反馈:yaw 世界系、pitch IMU 角,小陀螺与跟随均用此
} gimbal_mode_e;

/* 发射总开关:关闭时 shoot 任务停摩擦轮与拨盘,并清空待发队列 */
typedef enum
{
    SHOOT_OFF = 0,
    SHOOT_ON,
} shoot_mode_e;

typedef enum
{
    FRICTION_OFF = 0, // 摩擦轮关闭:目标转速 0
    FRICTION_ON,      // 摩擦轮开启:目标 FRICTION_TARGET_RPM
} friction_mode_e;

typedef enum
{
    LID_OPEN = 0, // 弹舱盖打开(舵机未接,当前无实际效果)
    LID_CLOSE,    // 弹舱盖关闭
} lid_mode_e;

/* 拨盘模式。注意语义差异:
 * 连发是"持续"语义(参照非零则按射速持续出弹);
 * 单发/三发是"沿"语义(shoot 端检测模式切换沿,只登记一次弹数)。 */
typedef enum
{
    LOAD_STOP = 0,  // 停止拨盘:速度环归零
    LOAD_REVERSE,   // 反转(退卡弹):占位,当前实现为停止
    LOAD_1_BULLET,  // 单发:登记 1 发,角度环逐发推进
    LOAD_3_BULLET,  // 三发:登记 3 发,逐发推进
    LOAD_BURSTFIRE, // 连发:按 shoot_rate 持续拨弹(会清空未完成的单发队列)
} loader_mode_e;

/* 功率控制数据(rm_referee 模块引用),当前尚未接入 */
typedef struct
{
    float chassis_power_mx; // 底盘功率上限,W
} Chassis_Power_Data_s;

/* ---------------- cmd 发布 → gimbal/chassis/shoot 订阅 ---------------- */

/* 底盘控制指令。注意 vx/vy 是"云台坐标系",chassis 内部会按 offset_angle 旋转到底盘系;
 * 因此云台跟着人转动时,推 W 仍然是"朝枪口方向"前进。 */
typedef struct
{
    float vx;                    // 前进速度,mm/s(云台坐标系)
    float vy;                    // 横移速度,mm/s(云台坐标系)
    float wz;                    // 自转角速度,deg/s(物理域,非转子域)
    float offset_angle;          // 云台相对底盘的夹角,deg,底盘系逆时针为正
    chassis_mode_e chassis_mode; // 决定 wz 来源,见 chassis_mode_e
    int chassis_speed_buff;      // 预留:当前全工程无引用,勿依赖
} Chassis_Ctrl_Cmd_s;

/* 云台控制指令。yaw 是增量累加的世界系目标角,cmd 每周期在其上叠加; */
typedef struct
{
    float yaw;                  // yaw 目标角,deg(世界系,增量累加)
    float pitch;                // deg；哨兵为4310输出轴的解锁增量，连杆实际俯仰角尚未换算
    float small_yaw_rate_deg_s; // 小 yaw 手动角速度指令,deg/s(原预留字段,布局未变)
    gimbal_mode_e gimbal_mode;  // 反馈源选择,见 gimbal_mode_e
} Gimbal_Ctrl_Cmd_s;

/* 发射控制指令 */
typedef struct
{
    shoot_mode_e shoot_mode;       // 发射总开关
    loader_mode_e load_mode;       // 拨盘模式,语义见 loader_mode_e
    lid_mode_e lid_mode;           // 弹舱盖(舵机未接,当前无效)
    friction_mode_e friction_mode; // 摩擦轮开关
    Bullet_Speed_e bullet_speed;   // 弹速挡位,枚举定义见 master_process.h
    uint8_t rest_heat;             // 剩余枪口热量(裁判系统)
    float shoot_rate;              // 连发射速,发/s
} Shoot_Ctrl_Cmd_s;

/* ---------------- gimbal/shoot/chassis 发布 → cmd 订阅 ---------------- */

/* 底盘回传(单板时 cmd 也用它中转裁判数据) */
typedef struct
{
    uint8_t rest_heat;           // 剩余枪口热量
    Bullet_Speed_e bullet_speed; // 弹速上限(裁判系统下发)
    Enemy_Color_e enemy_color;   // 敌方颜色:0 = blue,1 = red
} Chassis_Upload_Data_s;

/* ---------------- 双板协议布局守卫 ----------------
 *
 * can_comm 按 pack(1) 原样透传,两条板卡的固件必须使用完全一致的布局。
 * 字段增删会在此触发编译错误,此时必须:①同步对端板卡固件 → ②按实际 sizeof 更新期望值。
 * 只改断言不改固件,得到的是"编译通过但数据错位"的静默故障。
 *
 * 期望值来自工具链实测,不是理论值:
 *   ARM(arm-none-eabi,enum 占 1 字节):Chassis_Ctrl_Cmd_s = 21,Chassis_Upload_Data_s = 3
 *   主机 x86(enum 占 4 字节):          = 24,                  = 9
 * 换工具链或改编译选项后按实测值更新,不要沿用旧数字。 */
#if defined(__arm__) || defined(__thumb__)
_Static_assert(sizeof(Chassis_Ctrl_Cmd_s) == 21, "Chassis_Ctrl_Cmd_s layout changed -> sync peer board firmware, then update this assert");
_Static_assert(sizeof(Chassis_Upload_Data_s) == 3, "Chassis_Upload_Data_s layout changed -> sync peer board firmware, then update this assert");
#else
_Static_assert(sizeof(Chassis_Ctrl_Cmd_s) == 24, "Chassis_Ctrl_Cmd_s layout changed (host ABI: enum=4B) -> update this assert");
_Static_assert(sizeof(Chassis_Upload_Data_s) == 9, "Chassis_Upload_Data_s layout changed (host ABI: enum=4B) -> update this assert");
#endif

/* 云台回传 */
typedef struct
{
    attitude_t gimbal_imu_data;         // 云台 IMU 姿态(注意:Gyro 为芯片原始轴,非欧拉轴)
    float yaw_motor_single_round_angle; // deg；旧DJI单圈角，哨兵连续关节角（底盘调试使用车体系）
} Gimbal_Upload_Data_s;

/* 发射回传(预留:后续可加应用离线监测与卡弹反馈) */
typedef struct
{
    // 后续增加应用离线监测/卡弹反馈
} Shoot_Upload_Data_s;

/* ---------------- UI 绘制所需 ---------------- */

typedef enum
{
    UI_KEEP,    // 不重绘,复用上帧
    UI_REFRESH, // 强制重绘
} ui_mode_e;

#pragma pack() // 恢复字节对齐

#endif // !ROBOT_DEF_H
