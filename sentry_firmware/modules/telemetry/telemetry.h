/**
 * @file telemetry.h
 * @brief 轻量控制遥测环形缓冲
 * @author JIT CR
 * @date 2026-10-27
 *
 * 用途:每控制周期记录(时间,目标,反馈,输出,附加,模式),供Ozone live watch直接采样
 * 全局环实例,或通过TelemetryDump()整环回放打印;调试窗口稀缺时用于现场定位与事后回放.
 *
 * 使用约定:
 *  - 单写者:每个环实例只允许一个任务调用TelemetryPush(读侧可并发,单条样本可能撕裂,调试数据可容忍);
 *  - 无锁无malloc:静态.bss,共3环 x 256 x 24B ≈ 18KB RAM,注意构建输出的RAM占用;
 *  - 日志:每写入TELEMETRY_LOG_DIV条自动输出一行结构化行,DISABLE_LOG_SYSTEM时自动静默;
 *  - 通道语义:ref/fb/out/extra/mode的含义由调用方定义,见各接入点注释.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "bsp_log.h"
#include "bsp_dwt.h"

#define TELEMETRY_RING_SIZE 256 // 环容量(2的幂),单环 256x24B = 6KB
#define TELEMETRY_LOG_DIV 100   // 每写入N条输出一行结构化日志,置0关闭

#pragma pack(1)
typedef struct
{
    uint32_t tick_ms; // DWT_GetTimeline_ms()
    float ref;        // 目标
    float fb;         // 反馈
    float out;        // 控制输出(该通道最终环输出)
    float extra;      // 附加量(误差/角速度等,通道语义自定义)
    uint8_t mode;     // 模式/状态标记(通道语义自定义)
} Telemetry_Sample_s;

typedef struct
{
    Telemetry_Sample_s samples[TELEMETRY_RING_SIZE];
    volatile uint16_t write_idx; // 下一个写入位置,单写者推进
    uint32_t count;              // 累计写入条数,不清零
    const char *tag;             // 通道名,日志标识
} Telemetry_Ring_s;
#pragma pack()

/* 四个全局通道实例,Ozone live watch直接观察数组即可示波.
 * 单写者: yaw/pitch <- GimbalTask(200Hz); chassis_cmd <- RobotCMDTask(200Hz);
 *         chassis_fb <- ChassisTask(200Hz,轮速逆解结果); shoot <- ShootTask(200Hz). */
extern Telemetry_Ring_s telemetry_yaw;
extern Telemetry_Ring_s telemetry_pitch;
extern Telemetry_Ring_s telemetry_chassis_cmd;
extern Telemetry_Ring_s telemetry_chassis_fb;
extern Telemetry_Ring_s telemetry_shoot;

/**
 * @brief 写入一条遥测样本(时间戳由内部取DWT_GetTimeline_ms)
 *
 * @param ring  目标环(调用方必须保证单写者)
 * @param ref   目标
 * @param fb    反馈
 * @param out   控制输出
 * @param extra 附加量
 * @param mode  模式/状态标记
 */
void TelemetryPush(Telemetry_Ring_s *ring, float ref, float fb, float out, float extra, uint8_t mode);

/**
 * @brief 整环回放打印(从最老存活样本按时间序),调试时手动调用
 *
 * @param ring 目标环
 */
void TelemetryDump(const Telemetry_Ring_s *ring);

#endif // !TELEMETRY_H
