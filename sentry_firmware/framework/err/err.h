/**
 * @file    err.h
 * @brief   错误接口与错误码
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    四档(方案 §4,分投递语义,**不是**统一 while(1)):
*          - ERR_POLICY_LOG_ONLY   计数 + 无锁环形缓存(ISR 安全);消费侧(diag 任务)
*                                  在 B3 接线,本模块只存不打印;
*          - ERR_POLICY_SAFE_STATE 只置原子标志,robot_task 首拍消费执行注入回调
*                                  (ISR 禁写任务数据面,双写者禁令 → 标志位而非直接动作);
*          - ERR_POLICY_RESTART    置待重启位(周期任务尾检查,B3 接线);
*          - ERR_POLICY_FAILFAST_BOOT  启动期 fail-fast:调用已注册的 failfast 钩子
*                                  (未注册 = 只置全局致命标志并返回,由调用方决定停机)。
*
* @note    **临界区内禁 err_report**(方案 §4:ISR 写任务数据面=双写者);
*          dbg 构建下 err_report 首行断言非临界区(is_in_critical 由 kernel 提供,
*          POSIX 桩恒为否)。
*
* @note    错误码分配:见 .Doc/history/archive/plan/自主研发框架升级总方案.md §4 err 节;
*          高字节进 BKP0R 低 8 位与蜂鸣码映射表在 B3 落地。
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        ERR_POLICY_LOG_ONLY = 0,  /* 只记录,不改变控制流 */
        ERR_POLICY_SAFE_STATE,    /* 置安全态标志,任务侧首拍消费 */
        ERR_POLICY_RESTART,       /* 请求受控重启(周期尾执行) */
        ERR_POLICY_FAILFAST_BOOT, /* 启动期 fail-fast(白名单 4 类 10 点的替代出口) */
    } err_policy_e;

    /* 已登记错误码(持续追加;高 8 位=子系统,低 8 位=序号) */
    typedef enum
    {
        ERR_CODE_KERNEL_ASSERT = 0x0101, /* configASSERT 改造出口(B1) */
        ERR_CODE_MEM_OOM = 0x0201,       /* mem 池耗尽(B1) */
        ERR_CODE_CAN_FILTER = 0x0301,    /* CAN 过滤器配置失败/预算越界(B4a) */
        ERR_CODE_PWM_POOL = 0x0401,      /* PWM 实例池满(ports/stm32f4/pwm_port) */
        ERR_CODE_FLASH_ADDR = 0x0402,    /* Flash 访问地址越界(ports/stm32f4/flash_port) */
    } err_code_e;

    void err_report(uint16_t code, err_policy_e policy);
    void err_assert_fail(const char *file, int line); /* configASSERT 出口(port 实现) */

    /* 消费/观测接口(diag 在 B3 接线;robot_task 首拍消费 SAFE_STATE) */
    uint32_t err_count(uint16_t code);
    int err_safe_state_pending(void); /* 1=有未消费的安全态请求 */
    void err_safe_state_consume(void);
    int err_restart_pending(void);
    void err_restart_consume(void);
    typedef void (*err_failfast_fn)(uint16_t code);
    void err_register_failfast(err_failfast_fn fn); /* 未注册时只置标志 */

    /* LOG_ONLY 环(单写多读位:ISR 写,任务取;容量 32,满则丢最旧并计溢出) */
    typedef struct
    {
        uint16_t code;
        uint32_t tick_ms;
    } err_log_entry_t;
    unsigned err_log_drain(err_log_entry_t *out, unsigned cap); /* 返回取出条数 */
    unsigned err_log_overflow(void);                            /* 丢最旧次数 */
    uint8_t err_hard_flag(void);                                /* 致命错误已发生 */
    uint32_t err_log_drain_tick(void);                          /* port 提供的 tick 源(弱符号) */

#ifdef __cplusplus
}
#endif
