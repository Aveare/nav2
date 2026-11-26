/**
 * @file    kernel_posix.h
 * @brief   POSIX 桩的测试控制面(仅主机)
 * @author  小猫大王
 * @layer   TESTS
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
    void kernel_posix_advance_us(uint64_t us);
    unsigned kernel_posix_task_count(void);
    typedef unsigned (*kernel_posix_thread_fn)(void *);
    unsigned kernel_posix_spawn(kernel_posix_thread_fn fn,
                                void *arg); /* Win32 线程(本机无 pthread) */
    void kernel_posix_join(unsigned handle);
#ifdef __cplusplus
}
#endif
