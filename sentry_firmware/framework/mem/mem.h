/**
 * @file    mem.h
 * @brief   内存池接口
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    T07:稳态零 malloc;启动期豁免按**调用点**登记;链接期
*          --wrap=malloc/pvPortMalloc/calloc/realloc + 启动完成标志 + 遥测
*          rt_malloc_cnt==0(迁移批落)。本模块先立"唯一入口"与静态池语义:
*          池 = 调用方注入的缓冲(mem_pool_init),bump 分配、无释放(启动期
*          一次性分配与现有 kalman 豁免语义一致);运行期分配 = OOM。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void mem_pool_init(void *buf, size_t size); /* 幂等:仅首个调用生效 */
    void *mem_alloc(size_t size, size_t align); /* OOM 返回 NULL 并计 err */
    size_t mem_used(void);
    size_t mem_capacity(void);

#ifdef __cplusplus
}
#endif
