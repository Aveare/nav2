/**
 * @file    ax650_pose.h
 * @brief   AX650 帧结构与编解码接口
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    纯字节协议，不访问硬件；多字节字段使用小端序。
 */
#ifndef AX650_POSE_H
#define AX650_POSE_H

#include <stddef.h>
#include <stdint.h>

#define AX650_POSE_FRAME_SIZE      31u  // [协议] 0x1021 整帧字节数
#define AX650_FULL_POSE_FRAME_SIZE 61u  // [协议] 0x1022 整帧字节数，可装入 USB FS 64 B 包
#define AX650_RX_BUFFER_SIZE       256u // [协议] 流式接收缓存，容纳粘包和分片

#define AX650_POSE_VALID_IMU              (1u << 0)
#define AX650_POSE_VALID_BIG_YAW          (1u << 1)
#define AX650_POSE_VALID_SMALL_YAW        (1u << 2)
#define AX650_POSE_VALID_PITCH            (1u << 3)
#define AX650_POSE_CALIBRATED_SMALL_YAW   (1u << 4)
#define AX650_POSE_CALIBRATED_PITCH       (1u << 5)
#define AX650_POSE_VALID_PROJECTILE_SPEED (1u << 6)
#define AX650_POSE_VALID_ROBOT_ID         (1u << 7)
#define AX650_POSE_AUTO_AIM_ALLOWED       (1u << 8)
#define AX650_POSE_GIMBAL_ONLINE          (1u << 9)

typedef struct
{
    float yaw;         // 目标 yaw，deg，世界系绝对角
    float pitch;       // 目标 pitch，deg，IMU 反馈域绝对角
    float yaw_speed;   // yaw 目标角速度，rad/s
    float pitch_speed; // pitch 目标角速度，rad/s
    float yaw_acc;     // yaw 目标角加速度，rad/s²
    float pitch_acc;   // pitch 目标角加速度，rad/s²
    float distance;    // 目标距离，m
    uint8_t shoot;     // AX650 原始发射标志；本次不接入发射控制
    uint8_t target_id; // 目标编号，0 表示当前无目标
} ax650_command_t;

/** 0x1022 完整云台姿态的主机无关输入快照。 */
typedef struct
{
    float q_wxyz[4];                  // 大 yaw 平台 IMU 到世界系主动旋转,无量纲
    float big_yaw_rel_rad;            // 大 yaw 相对底盘机械零位,rad
    float small_yaw_rad;              // 小 yaw 相对机械中位,rad
    float pitch_rad;                  // pitch 相对本次机械参考位,rad
    float projectile_speed_mps;       // 弹丸初速,m/s
    uint32_t imu_sample_time_ms;       // IMU 样本 MCU tick,ms
    uint32_t big_yaw_sample_time_ms;   // 大 yaw CAN 样本 MCU tick,ms
    uint32_t small_yaw_sample_time_ms; // 小 yaw 快照 MCU tick,ms
    uint32_t pitch_sample_time_ms;     // pitch CAN 样本 MCU tick,ms
    uint16_t valid_flags;              // AX650_POSE_* 位标志
    uint8_t program_mode;              // RMCV ProgramMode 原始枚举,无量纲
    uint8_t robot_id;                  // 裁判系统本车 ID,无量纲
} ax650_full_pose_t;

typedef struct
{
    uint8_t buffer[AX650_RX_BUFFER_SIZE];
    size_t used;
} ax650_parser_t;

typedef void (*Ax650CommandHandler)(void *context, const ax650_command_t *command);

/**
 * @brief  计算 RMCV 帧头 CRC8
 * @param  data  字节数组，无量纲
 * @param  count  参与校验的字节数；帧头取前 4 字节
 * @return 反射多项式 0x8C、初值 FF、无最终异或的校验值。
 */
uint8_t ax650_pose_crc8(const uint8_t *data, size_t count);

/**
 * @brief  计算 RMCV 整帧 CRC16
 * @param  data  字节数组，无量纲
 * @param  count  参与校验的字节数；30 字节姿态帧取前 28 字节
 * @return 反射多项式 0x8408、初值 FFFF、无最终异或的校验值。
 */
uint16_t ax650_pose_crc16(const uint8_t *data, size_t count);

/** @brief 清空 AX650 下行流解析器。 */
void ax650_parser_init(ax650_parser_t *parser);

/**
 * @brief 流式解析 AX650 0x0503 控制帧，支持分片、粘包和错误后重同步
 * @param parser 解析器实例
 * @param data 本次收到的字节
 * @param count 本次字节数
 * @param handler 有效控制帧回调；可为 NULL
 * @param context 透传给回调的上下文
 */
void ax650_parser_feed(ax650_parser_t *parser, const uint8_t *data, size_t count,
                     Ax650CommandHandler handler, void *context);

/**
 * @brief  将真实四元数封装为 AX650 的 0x1021 姿态帧
 * @param  out  输出缓冲区，无量纲
 * @param  capacity  输出缓冲区容量，字节
 * @param  q_wxyz  机体到导航的主动旋转四元数 [w,x,y,z]，无量纲
 * @param  speed_mps  弹丸初速，m/s；手眼标定填 0
 * @param  mode  AX650 ProgramMode 原始枚举值，无量纲；标定填 0
 * @param  sequence  帧序号，无量纲；255 后允许回绕
 * @param  sample_time_ms  姿态 EKF 更新后的 MCU 单调时间，ms；uint32_t 回绕
 * @param  robot_id  裁判系统 0x0201 本车 ID；0 表示尚未收到有效裁判数据
 * @return 成功返回 31；输入非法、非有限或接近 ZXY 奇异点时返回 0。
 * @note   输出 yaw/pitch/roll 为 deg，满足 Rz(yaw)Rx(pitch)Ry(roll)。
 * @note   IT/DMA 发送完成前，调用方必须保持输出缓冲区不变。
 */
size_t ax650_pose_pack(uint8_t *out, size_t capacity, const float q_wxyz[4], float speed_mps,
                      uint8_t mode, uint8_t sequence, uint32_t sample_time_ms, uint8_t robot_id);

/**
 * @brief  按 RMCV 0x1022 打包完整云台姿态
 * @param  out 输出缓冲区,字节
 * @param  capacity 输出缓冲区容量,字节
 * @param  pose 已完成单位换算的姿态快照
 * @param  sequence 8 位帧序号,无量纲
 * @return 成功返回 AX650_FULL_POSE_FRAME_SIZE；非法输入返回 0。
 * @note   函数归一化四元数，不发送 C 结构体内存布局。
 */
size_t ax650_full_pose_pack(uint8_t *out, size_t capacity, const ax650_full_pose_t *pose,
                           uint8_t sequence);

#endif
