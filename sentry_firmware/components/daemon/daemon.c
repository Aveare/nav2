/**
 * @file    daemon.c
 * @brief   模块在线守护:注册/喂狗/超时回调
 * @layer   COMPONENT
 * @author  JIT CR
 * @author  modified by 小猫大王
* @date    2026-10-27
* @note    相对上游修复:init_count被reload_count覆盖导致上线等待失效(已修);
 */
#include "daemon.h"
#include "dwt_port.h" // 后续通过定时器来计时?
#include "log_port.h" // LOG_ERROR:注册失败告警(DEV-20261121-09)
#include "stdlib.h"
#include "memory.h"
#include "buzzer.h"

// 用于保存所有的daemon instance
static daemon_t *daemon_instances[DAEMON_MAX_COUNT] = {NULL};
/* 静态实例池(DEV-20261121-09):每台车要注册 10+ 个守护(每个电机一个),
 * 走 malloc 在堆紧张时返回 NULL → 调用方解引用即 HardFault。静态池零失败路径。 */
static daemon_t daemon_pool[DAEMON_MAX_COUNT];
static uint8_t idx; // 用于记录当前的daemon instance数量,配合回调使用

daemon_t *daemon_register(daemon_init_config_t *config)
{
    if (config == NULL || idx >= DAEMON_MAX_COUNT) // 越界写会破坏相邻静态数据,必须挡
    {
        LOG_ERROR("[daemon] register failed: pool full or null config");
        return NULL;
    }
    daemon_t *instance = &daemon_pool[idx];
    memset(instance, 0, sizeof(daemon_t));

    instance->owner_id = config->owner_id;
    instance->reload_count = config->reload_count == 0 ? 100 : config->reload_count; // 默认值为100
    instance->callback = config->callback;
    // 修复: 原实现此处的init_count赋值会立即被下一行reload_count覆盖,
    // "上线等待时间"从未生效. 现语义: 未填init_count时与reload_count一致,
    // 填了则作为上线等待窗口(首个有效数据到来前的宽限时间,与reload_count独立).
    instance->temp_count = config->init_count == 0 ? config->reload_count : config->init_count;
    daemon_instances[idx++] = instance;
    return instance;
}

/* "喂狗"函数 */
void daemon_reload(daemon_t *instance)
{
    if (instance == NULL)
        return;
    instance->temp_count = instance->reload_count;
}

uint8_t daemon_is_online(daemon_t *instance)
{
    if (instance == NULL)
        return 0;
    return instance->temp_count > 0;
}

void daemon_task()
{
    daemon_t *daemon_instance; // 提高可读性同时降低访存开销
    for (size_t i = 0; i < idx; ++i)
    {
        daemon_instance = daemon_instances[i];
        if (daemon_instance == NULL) // 防御:注册失败/槽位未用
            continue;
        if (daemon_instance->temp_count > 0) // 如果计数器还有值,说明上一次喂狗后还没有超时,则计数器减一
            daemon_instance->temp_count--;
        else if (daemon_instance->callback) // 等于零说明超时了,调用回调函数(如果有的话)
        {
            // 注意: 超时后回调会在每个周期重复触发,直到下一次喂狗;
            // 回调实现必须幂等(只置标志/重启接收/重发指令等),
            // 不要做累计、切换状态等非幂等动作.
            daemon_instance->callback(
                daemon_instance->owner_id); // module内可以将owner_id强制类型转换成自身类型从而调用特定module的offline
                                 // callback
            // @todo 为蜂鸣器/led等增加离线报警的功能,非常关键!
        }
    }
}
