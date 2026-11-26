/**
 * @file    mem.c
 * @brief   静态内存池:bump 分配(启动期)
 * @layer   FRAMEWORK
 * @author  小猫大王
 */
#include "mem.h"
#include "err.h"
#include <string.h>

static uint8_t *pool_base;
static size_t pool_size;
static size_t pool_used;
static int pool_ready;

void mem_pool_init(void *buf, size_t size)
{
    if (pool_ready)
        return; /* 幂等:仅首个调用生效,防重复注入 */
    pool_base = (uint8_t *)buf;
    pool_size = size;
    pool_used = 0;
    pool_ready = 1;
}

void *mem_alloc(size_t size, size_t align)
{
    if (!pool_ready || align == 0 || (align & (align - 1)))
        return NULL;
    uintptr_t next = (uintptr_t)pool_base + pool_used;
    uintptr_t aligned = (next + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t pad = (size_t)(aligned - next);
    if (pad > pool_size - pool_used || size > pool_size - pool_used - pad)
    {
        err_report(ERR_CODE_MEM_OOM, ERR_POLICY_LOG_ONLY);
        return NULL;
    }
    pool_used = pad + pool_used + size;
    return (void *)aligned;
}

size_t mem_used(void)
{
    return pool_used;
}
size_t mem_capacity(void)
{
    return pool_size;
}
