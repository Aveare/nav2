/**
 * @file    daemon.c
 * @brief   守护进程:模块离线检测与超时回调
 * @author  JIT CR
 * @date    2026-10-27
 * @note    相对上游修复:init_count被reload_count覆盖导致上线等待失效(已修);
 */
#include "daemon.h"
#include "bsp_dwt.h" // 后续通过定时器来计时?
#include "stdlib.h"
#include "memory.h"
#include "buzzer.h"

// 用于保存所有的daemon instance
static DaemonInstance *daemon_instances[DAEMON_MX_CNT] = {NULL};
static uint8_t idx; // 用于记录当前的daemon instance数量,配合回调使用

DaemonInstance *DaemonRegister(Daemon_Init_Config_s *config)
{
    DaemonInstance *instance = (DaemonInstance *)malloc(sizeof(DaemonInstance));
    memset(instance, 0, sizeof(DaemonInstance));

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
void DaemonReload(DaemonInstance *instance)
{
    instance->temp_count = instance->reload_count;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    return instance->temp_count > 0;
}

void DaemonTask()
{
    DaemonInstance *dins; // 提高可读性同时降低访存开销
    for (size_t i = 0; i < idx; ++i)
    {

        dins = daemon_instances[i];
        if (dins->temp_count > 0) // 如果计数器还有值,说明上一次喂狗后还没有超时,则计数器减一
            dins->temp_count--;
        else if (dins->callback) // 等于零说明超时了,调用回调函数(如果有的话)
        {
            // 注意: 超时后回调会在每个周期重复触发,直到下一次喂狗;
            // 回调实现必须幂等(只置标志/重启接收/重发指令等),
            // 不要做累计、切换状态等非幂等动作.
            dins->callback(dins->owner_id); // module内可以将owner_id强制类型转换成自身类型从而调用特定module的offline callback
            // @todo 为蜂鸣器/led等增加离线报警的功能,非常关键!
        }
    }
}
