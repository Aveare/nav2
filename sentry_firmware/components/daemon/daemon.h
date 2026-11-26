/**
 * @file    daemon.h
 * @brief   守护接口:注册与在线查询
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef MONITOR_H
#define MONITOR_H

#include "stdint.h"
#include "string.h"

#define DAEMON_MAX_COUNT 64

/* 模块离线处理函数指针 */
typedef void (*offline_callback)(void *);

/* daemon 运行实例 */
typedef struct daemon_ins
{
    uint16_t reload_count;     // 重载值,**单位是 daemon_task 调用次数,不是毫秒**
    offline_callback callback; // 离线回调,判定离线时触发一次

    uint16_t temp_count; // 当前倒计数;减到 0 判定模块离线
    void *owner_id;      // 实例地址,初始化时填入,回调时原样传回
} daemon_t;

/* daemon 初始化配置 */
typedef struct
{
    uint16_t reload_count;     // 超时换算:次数 = 期望超时(ms) / daemon_task 周期(ms)
    uint16_t init_count;       // 上线等待时间,单位同上;有些模块要先收到主控指令才回报文
    offline_callback callback; // 离线回调

    void *owner_id; // 拥有 daemon 的实例地址,如 dji_motor_t*,cast 成 void*;
                    // 同一 module 存在多个实例时,离线回调靠它区分是哪一个
} daemon_init_config_t;

/**
 * @brief  注册一个 daemon 实例
 * @param  config 初始化配置
 * @return daemon 实例指针;数量超过 DAEMON_MAX_COUNT 时返回 NULL
 * @note   reload_count 与 init_count 都按 **daemon_task 调用次数** 计,不是毫秒。
 *         本工程 daemon_task 以 100Hz 运行,故 100ms 超时填 10;
 *         若改了任务周期而没同步这两个数,在线判定会整体偏移。
 */
daemon_t *daemon_register(daemon_init_config_t *config);

/**
 * @brief  喂狗:模块收到新数据或完成一次有效动作时调用,把 temp_count 重置为 reload_count
 * @param  instance daemon 实例指针
 * @note   漏喂一次不会立即判离线,只有连续 reload_count 次没喂才会触发。
 */
void daemon_reload(daemon_t *instance);

/**
 * @brief  查询模块是否在线
 * @param  instance daemon 实例指针
 * @return 1 = 在线且正常;0 = 离线或尚未完成首次上线
 * @note   上电初期 init_count 未走完时返回 0,调用方需把"未上线"当作"不可用"处理,
 *         否则可能用陈旧反馈做出误判(本工程 shoot 互锁即依赖此语义)。
 */
uint8_t daemon_is_online(daemon_t *instance);

/**
 * @brief  守护线程主体:对所有实例的 temp_count 递减,减到 0 的触发离线回调
 * @note   必须固定周期调用(本工程 100Hz)。周期变化会改变所有 reload_count 的实际超时,
 *         改任务频率时必须同步复查各模块的 daemon 配置。
 */
void daemon_task();

#endif // !MONITOR_H
