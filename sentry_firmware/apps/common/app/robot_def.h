/**
 * @file    robot_def.h
 * @brief   应用间消息协议合同:枚举与 pack(1) 结构布局守卫
 * @layer   APP
 * @author  modified by 小猫大王
* @note    本文件只放类型与结构体,数值一律在 config/infantry_2026/config.h。
* @note    所有消息结构体位于 #pragma pack(1) 内,布局受文件尾 _Static_assert 守卫:
*          增删字段 → 先同步对端板卡固件 → 再更新断言期望值。只改断言不改固件 = 静默错位。
* @note    本文件源自跃鹿 basic_framework(作者 NeoZeng,版权 HNU YueLu EC,
*          上游版本 0.1 / 2022-12-02),由 JIT CR 改造:双板协议布局守卫、
*          config 引用、三发与键鼠相关字段。
 */
#ifndef ROBOT_DEF_H
#define ROBOT_DEF_H

#include "config.h" // 本车参数(板型/零位/限位/几何/速度),换车只改 config
#include "ins_task.h"
#include "types.h" // 控制模式枚举等共享类型(模块可包含)
#include "stdint.h"

/* 板型冲突检查:ONE_BOARD / CHASSIS_BOARD / GIMBAL_BOARD 只允许定义一个。
 * 该检查在预编译期完成,配错板型会直接编译失败,不会烧出行为异常的固件。 */
#if (defined(ONE_BOARD) && defined(CHASSIS_BOARD)) ||                                              \
    (defined(ONE_BOARD) && defined(GIMBAL_BOARD)) ||                                               \
    (defined(CHASSIS_BOARD) && defined(GIMBAL_BOARD))
#error Conflict board definition! You can only define one board type.
#endif

#pragma pack(1) // 取消字节对齐:以下结构体可能经 CAN 原样透传,不允许填充字节

/* ---------------- 基本状态枚举 ---------------- */

typedef enum
{
    ROBOT_STATUS_STOP = 0, // 停机:零力,不接受控制输入,需重新走解锁流程
    ROBOT_STATUS_READY,    // 就绪:已解锁,执行控制指令
} robot_status_e;

typedef enum
{
    APP_STATUS_OFFLINE = 0, // 应用离线:守护线程判定超时
    APP_STATUS_ONLINE,      // 应用在线
    APP_STATUS_ERROR,       // 应用报错:通常伴随安全态
} app_status_e;

/* AX650自瞄挡遥控发射功能开关：
 * 1=拨轮压入触发区后开启摩擦轮，并在每次有效压入时单发一发；
 * 0=自瞄挡关闭整个发射链，只允许yaw/pitch跟随。 */
#ifndef SHOOT_AUTO_AIM_REMOTE_ENABLE
#define SHOOT_AUTO_AIM_REMOTE_ENABLE 1
#endif

/* 拨盘模式。注意语义差异:
 * 连发是"持续"语义(参照非零则按射速持续出弹);
 * 单发/三发是"沿"语义(shoot 端检测模式切换沿,只登记一次弹数)。 */
typedef enum
{
    LOADER_STOP = 0,  // 停止拨盘:速度环归零
    LOADER_REVERSE,   // 反转(退卡弹):占位,当前实现为停止
    LOADER_SINGLE,  // 单发:登记 1 发,角度环逐发推进
    LOADER_BURST3,  // 三发:登记 3 发,逐发推进
    LOADER_BURST, // 连发:按 shoot_rate 持续拨弹(会清空未完成的单发队列)
} loader_mode_e;

/* ---------------- cmd 发布 → gimbal/chassis/shoot 订阅 ---------------- */

/* 底盘控制指令。注意 vx/vy 是"云台坐标系",chassis 内部会按 offset_angle 旋转到底盘系;
 * 因此云台跟着人转动时,推 W 仍然是"朝枪口方向"前进。 */
typedef struct
{
    float vx;           // 前进速度,mm/s(云台坐标系)
    float vy;           // 横移速度,mm/s(云台坐标系)
    float wz;           // 自转角速度,deg/s(物理域,非转子域)
    float offset_angle; // 云台相对底盘的夹角,deg,底盘系逆时针为正
    float yaw_rate_cmd; // 云台目标角速度(操作手意图),deg/s;松手时≈0,给底盘做提前刹车
                        // **必须放 chassis_mode 之前**:本结构体紧凑排布,
                        // 放在 u8 之后会落在奇地址,+17 处 float 读到的是垃圾(2026-11-21 实测)
    chassis_mode_e chassis_mode; // 决定 wz 来源,见 chassis_mode_e
} chassis_ctrl_cmd_t;

/* 云台控制指令。yaw 是增量累加的世界系目标角,cmd 每周期在其上叠加; */
typedef struct
{
    float yaw;                 // yaw 目标角,deg(世界系,增量累加)
    float pitch;               // pitch 目标角,deg(IMU 反馈域,受 PITCH_MIN/MAX_ANGLE 限幅)
    float chassis_rotate_wz;   // 预留:自转速度,当前无人读取
    gimbal_mode_e gimbal_mode; // 反馈源选择,见 gimbal_mode_e
} gimbal_ctrl_cmd_t;

/* 视觉/自瞄协议枚举:原 components/ax650/master_process.h,2026-11-23 收敛到协议合同。
 * 数值即协议值,禁止改动;旧视觉收发实现已删除(RM-011),此处仅保留类型契约。 */
typedef enum
{
    ENEMY_COLOR_NONE = 0,
    ENEMY_COLOR_BLUE = 1,
    ENEMY_COLOR_RED = 2,
} enemy_color_e;

typedef enum
{
    BULLET_SPEED_NONE = 0,
    BULLET_SPEED_BIG_10 = 10,
    BULLET_SPEED_SMALL_15 = 15,
    BULLET_SPEED_BIG_16 = 16,
    BULLET_SPEED_SMALL_18 = 18,
    BULLET_SPEED_SMALL_30 = 30,
} bullet_speed_e;

/* 战术射速档位(DEV-20261121-11):操作手按 X 循环切换,UI 显示当前档位。
 * 档位只决定"请求射速",真正的节奏仍由热量守卫裁决 —— 爆发档不会被允许超热量。 */
typedef enum
{
    FIRE_TIER_ECO = 0, // 省热:按散热平衡走(230/14 下 = 1.4 发/s)
    FIRE_TIER_NORMAL,  // 常规:6 发/s
    FIRE_TIER_BURST,   // 爆发:12 发/s,由守卫按预算律压速
} fire_tier_e;

/* 发射控制指令 */
typedef struct
{
    shoot_mode_e shoot_mode;       // 发射总开关
    loader_mode_e load_mode;       // 拨盘模式,语义见 loader_mode_e
    lid_mode_e lid_mode;           // 弹舱盖(舵机未接,当前无效)
    friction_mode_e friction_mode; // 摩擦轮开关
    bullet_speed_e bullet_speed;   // 弹速挡位,枚举定义见本文件
    uint8_t rest_heat;             // 剩余枪口热量(裁判系统)
    float shoot_rate;              // 连发射速,发/s(由档位映射,见 cmd_fire_tier_rate)
    fire_tier_e fire_tier;         // 当前战术档位,供 UI 显示与日志
} shoot_ctrl_cmd_t;

/* ---------------- gimbal/shoot/chassis 发布 → cmd 订阅 ---------------- */

/* 底盘回传(单板时 cmd 也用它中转裁判数据) */
typedef struct
{
    uint8_t rest_heat;           // 剩余枪口热量
    bullet_speed_e bullet_speed; // 弹速上限(裁判系统下发)
    enemy_color_e enemy_color;   // 敌方颜色:0 = blue,1 = red
} chassis_upload_t;

/* ---------------- 双板协议布局守卫 ----------------
 *
 * can_comm 按 pack(1) 原样透传,两条板卡的固件必须使用完全一致的布局。
 * 字段增删会在此触发编译错误,此时必须:①同步对端板卡固件 → ②按实际 sizeof 更新期望值。
 * 只改断言不改固件,得到的是"编译通过但数据错位"的静默故障。
 *
 * 期望值来自工具链实测,不是理论值:
 *   ARM(arm-none-eabi,enum 占 1 字节):chassis_ctrl_cmd_t = 21,chassis_upload_t = 3
 *   主机 x86(enum 占 4 字节):          = 24,                  = 9
 * 换工具链或改编译选项后按实测值更新,不要沿用旧数字。 */
#if defined(__arm__) || defined(__thumb__)
_Static_assert(
    sizeof(chassis_ctrl_cmd_t) == 21,
    "Chassis_Ctrl_Cmd_s layout changed -> sync peer board firmware, then update this assert");
_Static_assert(
    sizeof(chassis_upload_t) == 3,
    "Chassis_Upload_Data_s layout changed -> sync peer board firmware, then update this assert");
#else
_Static_assert(sizeof(chassis_ctrl_cmd_t) == 24,
               "Chassis_Ctrl_Cmd_s layout changed (host ABI: enum=4B) -> update this assert");
_Static_assert(sizeof(chassis_upload_t) == 9,
               "Chassis_Upload_Data_s layout changed (host ABI: enum=4B) -> update this assert");
#endif

/* 云台回传 */
typedef struct
{
    attitude_t gimbal_imu_data;         // 云台 IMU 姿态(注意:Gyro 为芯片原始轴,非欧拉轴)
    float yaw_motor_single_round_angle; // yaw 电机单圈角度,deg,供跟随律计算 offset
} gimbal_upload_t;

/* 发射回传(预留:后续可加应用离线监测与卡弹反馈) */
typedef struct
{
    // 后续增加应用离线监测/卡弹反馈
} shoot_upload_t;

/* ---------------- UI 绘制所需 ---------------- */

typedef enum
{
    UI_KEEP,    // 不重绘,复用上帧
    UI_REFRESH, // 强制重绘
} ui_mode_e;

#pragma pack() // 恢复字节对齐

#endif // !ROBOT_DEF_H
