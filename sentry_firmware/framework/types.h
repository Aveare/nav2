/**
 * @file    types.h
 * @brief   跨层共享枚举与类型
 * @layer   FRAMEWORK
 * @author  modified by 小猫大王
* @note    2026-11-20 从 apps/infantry/app/robot_def.h 下移。理由:referee 的 UI 交互结构
*          (referee_interactive_info_t)需要这些模式枚举,但 components/ 不应依赖 apps/。
* @note    **枚举值即语义**(与协议/寄存器/UI 约定绑定),改值等同于改行为。
* @note    这些类型原本位于 robot_def.h 的 #pragma pack(1) 区内,但均为枚举与单字段结构,
*          不受打包影响;布局守卫(chassis_ctrl_cmd_t == 21)由编译期断言继续把关。
 */
#ifndef APP_TYPES_H
#define APP_TYPES_H

#include "stdint.h"

/* 底盘模式:由 cmd 每周期发布,chassis 据此决定 wz 的来源,不保存历史状态。 */
typedef enum
{
    CHASSIS_ZERO_FORCE = 0,    // 零力:四轮电流置零
    CHASSIS_ROTATE,            // 小陀螺:wz 恒为 CHASSIS_ROTATE_SPEED,同时保持全向机动
    CHASSIS_NO_FOLLOW,         // 不跟随:wz 恒 0,允许全向平移
    CHASSIS_FOLLOW_GIMBAL_YAW, // 跟随云台:wz = 线性位置项 + 陀螺仪前馈(2026-11-20
                               // 重写,不再有阻尼/平方律)
    CHASSIS_ALIGN,             // 回中:只旋底盘把关节角 offset 收到 0,云台由自身闭环保持世界方向
                               // (2026-11-17 新增;与跟随的区别是恒速+末端线性收敛,不走死区)
    CHASSIS_RC_CONTROL,        // 手动转向:wz 直接来自遥控器/鼠标并由 chassis 限幅
    CHASSIS_NAVIGATION,        // 底盘物理系输入,保留上位机 wz,不做云台系旋转
} chassis_mode_e;

/* 云台模式 */
typedef enum
{
    GIMBAL_ZERO_FORCE = 0, // 零力:yaw/pitch 电流置零
    GIMBAL_FREE_MODE,      // 自由模式:与底盘分离,反馈用电机编码器相对角
    GIMBAL_GYRO_MODE,      // 陀螺仪反馈:yaw 世界系、pitch IMU 角,小陀螺与跟随均用此
    GIMBAL_AUTO_AIM_MODE,  // AX650 自瞄:反馈源不变，yaw 使用独立 PID 参数
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
    FRICTION_ON,      // 摩擦轮开启:目标 SHOOT_FRICTION_TARGET_RPM
} friction_mode_e;

typedef enum
{
    LID_OPEN = 0, // 弹舱盖打开(舵机未接,当前无实际效果)
    LID_CLOSE,    // 弹舱盖关闭
} lid_mode_e;

/* 功率控制数据(rm_referee 模块引用),当前尚未接入 */
typedef struct
{
    float chassis_power_mx; // 底盘功率上限,W
} chassis_power_data_t;

#endif // APP_TYPES_H
