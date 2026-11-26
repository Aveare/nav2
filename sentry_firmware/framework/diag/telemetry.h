/**
 * @file    telemetry.h
 * @brief   遥测接口与环配置
 * @layer   FRAMEWORK
 * @author  JIT CR
 * @author  modified by 小猫大王
* @date 2026-10-27
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "log_port.h"
#include "dwt_port.h"

#define TELEMETRY_RING_SIZE 256 // 环容量(2的幂),单环 256x~21B ≈ 5.4KB
#define TELEMETRY_LOG_DIV   100 // 每写入N条输出一行结构化日志,置0关闭

/* CCM 零初始化段(链接脚本 .ccmbss,NOLOAD):启动不清零、不拷贝、不占 Flash。
 * 放入的对象必须自带初始化(遥测环走 telemetry_init)且绝不能是 DMA 目标。
 * 命名不能带 .ccmram_ 前缀(会被 .ccmram 段的 *(.ccmram*) 通配吞进可加载段)。
 * 主机单元测试(x86)没有 CCM,宏置空,避免 PE 目标 8 字符段名截断告警。 */
#if defined(__arm__) || defined(__ARM_EABI__)
#define TELEMETRY_CCM __attribute__((section(".ccmbss")))
#else
#define TELEMETRY_CCM
#endif

#pragma pack(1)
typedef struct
{
    uint32_t tick_ms; // dwt_get_timeline_ms()
    float ref;        // 目标
    float fb;         // 反馈
    float out;        // 控制输出(该通道最终环输出)
    float extra;      // 附加量(误差/角速度等,通道语义自定义)
    uint8_t mode;     // 模式/状态标记(通道语义自定义)
} telemetry_sample_t;

typedef struct
{
    telemetry_sample_t samples[TELEMETRY_RING_SIZE];
    volatile uint16_t write_idx; // 下一个写入位置,单写者推进
    uint32_t count;              // 累计写入条数,不清零
    const char *tag;             // 通道名,日志标识
} telemetry_ring_t;
#pragma pack()

/* 四个全局通道实例,Ozone live watch直接观察数组即可示波.
 * 单写者: yaw/pitch <- gimbal_task(200Hz); chassis_cmd <- cmd_task(200Hz);
 *         chassis_fb <- chassis_task(200Hz,轮速逆解结果); shoot <- shoot_task(200Hz). */
extern telemetry_ring_t telemetry_yaw;
extern telemetry_ring_t telemetry_pitch;
extern telemetry_ring_t telemetry_chassis_cmd;
extern telemetry_ring_t telemetry_chassis_fb;
extern telemetry_ring_t telemetry_shoot;

/**
 * @brief 遥测环上电初始化:清零各环并赋通道名
 * @note  环在 CCMRAM 的 .ccmbss(NOLOAD) 段,启动代码不清零;
 *        必须在任务启动前调用一次(robot_init 已调用),tag/write_idx 不初始化会
 *        导致首次写入越界或日志打出野指针。
 */
void telemetry_init(void);
void diag_arena_assign_tags(void); /* port 五环 tag 赋值(B6b arena 注入,telemetry_init 调) */

/**
 * @brief 写入一条遥测样本(时间戳由内部取dwt_get_timeline_ms)
 *
 * @param ring  目标环(调用方必须保证单写者)
 * @param ref   目标
 * @param fb    反馈
 * @param out   控制输出
 * @param extra 附加量
 * @param mode  模式/状态标记
 */
void telemetry_push(telemetry_ring_t *ring, float ref, float fb, float out, float extra,
                   uint8_t mode);

/**
 * @brief 整环回放打印(从最老存活样本按时间序),调试时手动调用
 *
 * @param ring 目标环
 */
void telemetry_dump(const telemetry_ring_t *ring);

#endif // !TELEMETRY_H
