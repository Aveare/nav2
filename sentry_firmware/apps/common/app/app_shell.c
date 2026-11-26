/**
 * @file    app_shell.c
 * @brief   通用装配壳:板级初始化/任务创建/看门狗与兵种钩子调度
 * @layer   APP
 * @author  小猫大王
* @note    本文件不认识任何具体兵种;所有差异经 g_app_profile 注入。
*          这样切换兵种只需换 apps/<robot>/app/assembly.c 与 config/<robot>_<year>/,
*          共享代码里不出现 `#if SENTRY/HEAVY/INFANTRY`。
* @note    初始化顺序、任务频率、喂狗口径与原 robot.c/robot_task.c 逐项一致:
*          board → fault → 复位原因/鸣叫码 → calib → telemetry → 应用 Init
*          → 创建任务 → 开中断 → 启动 IWDG。
 */
#include "app_profile.h"
#include "robot.h"
#include "robot_def.h"
#include "robot_task.h"

#include "board_port.h"
#include "log_port.h"
#include "dwt_port.h"
#include "usart_port.h"
#include "iwdg_port.h"
#include "buzzer.h"
#include "tim.h"
#include "fault_port.h"
#include "calib.h"
#include "telemetry.h"
#include "diag.h"
#include "err.h"
#include "ht_motor.h"
#include "ins_task.h"
#include "motor_task.h"
#include "daemon.h"

#if defined(DG_TEST_HANG)
extern void dg_test_hang_init(void);
#endif

/* 整车工作状态:步兵由 cmd 强定义覆盖;骨架兵种用此弱定义。 */
__attribute__((weak)) robot_status_e robot_state = ROBOT_STATUS_STOP;
kernel_task_t ins_task_handle;
kernel_task_t robot_task_handle;
kernel_task_t motor_task_handle;
kernel_task_t daemon_task_handle;
kernel_task_t ui_task_handle;

static diag_watch_t diag_watch_ins, diag_watch_motor, diag_watch_robot, diag_watch_daemon;

/* ---------------- 开机复位原因与鸣叫码 ---------------- */

static uint8_t boot_beep_code;

/**
 * @brief 用蜂鸣器播报上一轮复位原因
 * @note  由 200Hz 应用任务驱动,buzzer_task(100Hz)执行;
 *        0=不鸣 1=一长(POR/BOR) 2..5=短鸣(软件/看门狗/HardFault/栈溢出)。
 */
static void robot_boot_beep_task(void)
{
    static float t0 = 0.0f;
    static uint8_t started = 0;

    if (boot_beep_code == 0u)
        return;
    if (buzzer_get_default() == NULL) // daemon 任务尚未完成 buzzer_init
        return;

    float now = dwt_get_timeline_ms();
    if (!started)
    {
        started = 1;
        t0 = now;
    }
    float t = now - t0;

    uint8_t on = 0;
    float end_t = 900.0f;
    if (boot_beep_code == 1u)
    {
        on = (t < 600.0f);
    }
    else
    {
        float slot = 240.0f;
        uint8_t n = boot_beep_code;
        end_t = (float)n * slot + 200.0f;
        on = (t < (float)n * slot) && (((uint32_t)(t / 120.0f) % 2u) == 0u);
    }
    buzzer_set_alarm_status(buzzer_get_default(), on ? ALARM_ON : ALARM_OFF);
    if (t > end_t)
        boot_beep_code = 0u;
}

/**
 * @brief 读取 RCC 复位标志并转成鸣叫码
 * @note  致命故障(HardFault/栈溢出)优先于普通复位原因,与 DEV-20261121-08 一致。
 */
static void robot_log_reset_cause(void)
{
    uint32_t csr = RCC->CSR;
    LOG_INFO("[boot] RST cause: IWDG=%u WWDG=%u SW=%u POR=%u PIN=%u BOR=%u LPWR=%u",
             (unsigned)((csr & RCC_CSR_IWDGRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_WWDGRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_SFTRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_PORRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_PINRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_BORRSTF) ? 1u : 0u),
             (unsigned)((csr & RCC_CSR_LPWRRSTF) ? 1u : 0u));

    if (csr & RCC_CSR_IWDGRSTF)
        boot_beep_code = 3;
    else if (csr & (RCC_CSR_PORRSTF | RCC_CSR_BORRSTF))
        boot_beep_code = 1;
    else if (csr & RCC_CSR_SFTRSTF)
        boot_beep_code = 2;
    else
        boot_beep_code = 0;

    uint32_t fault = fault_consume();
    if (fault == FAULT_CODE_STACKOVF)
        boot_beep_code = 5;
    else if (fault != 0u)
        boot_beep_code = 4;

    RCC->CSR |= RCC_CSR_RMVF;
}

/* ---------------- 任务体 ---------------- */

__attribute__((noreturn)) static void robot_task_start_ins(void *argument)
{
    static float ins_start;
    static float ins_dt;
    (void)argument;

    ins_init();
    if (g_app_profile.ins_init != NULL)
        g_app_profile.ins_init();

    LOG_INFO("[freeRTOS] INS Task Start");
    for (;;)
    {
        ins_start = dwt_get_timeline_ms();
        ins_task();
        ins_dt = dwt_get_timeline_ms() - ins_start;
        if (ins_dt > 1)
            LOG_ERROR("[freeRTOS] INS Task is being DELAY! dt = [%d us]", (int)(ins_dt * 1000));
        diag_watch_kick(diag_watch_ins);
        kernel_task_delay(1);
    }
}

__attribute__((noreturn)) static void robot_task_start_motor(void *argument)
{
    static float motor_dt;
    static float motor_start;
    (void)argument;

    LOG_INFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        motor_start = dwt_get_timeline_ms();
        motor_control_task();
        motor_dt = dwt_get_timeline_ms() - motor_start;
        if (motor_dt > 1)
            LOG_ERROR("[freeRTOS] MOTOR Task is being DELAY! dt = [%d us]",
                      (int)(motor_dt * 1000));
        diag_watch_kick(diag_watch_motor);
        kernel_task_delay(1);
    }
}

__attribute__((noreturn)) static void robot_task_start_daemon(void *argument)
{
    static float daemon_dt;
    static float daemon_start;
    (void)argument;

    buzzer_init(&htim4, TIM_CHANNEL_3);
    LOG_INFO("[freeRTOS] Daemon Task Start");
    for (;;)
    {
        daemon_start = dwt_get_timeline_ms();
        daemon_task();
        buzzer_task();
        daemon_dt = dwt_get_timeline_ms() - daemon_start;
        if (daemon_dt > 10)
            LOG_ERROR("[freeRTOS] Daemon Task is being DELAY! dt = [%d us]",
                      (int)(daemon_dt * 1000));
        diag_watch_tick();
        diag_watch_kick(diag_watch_daemon);
        kernel_task_delay(10);
    }
}

__attribute__((noreturn)) static void robot_task_start_robot(void *argument)
{
    static float robot_dt;
    static float robot_start;
    (void)argument;

    LOG_INFO("[freeRTOS] ROBOT core Task Start");
    for (;;)
    {
        robot_start = dwt_get_timeline_ms();
        robot_task();
        robot_dt = dwt_get_timeline_ms() - robot_start;
        if (robot_dt > 5)
            LOG_ERROR("[freeRTOS] ROBOT core Task is being DELAY! dt = [%d us]",
                      (int)(robot_dt * 1000));
        diag_watch_kick(diag_watch_robot);
        kernel_task_delay(5);
    }
}

#if ROBOT_TASK_UI_ENABLE
__attribute__((noreturn)) static void robot_task_start_ui(void *argument)
{
    (void)argument;
    LOG_INFO("[freeRTOS] UI Task Start");
    referee_my_ui_init();
    LOG_INFO("[freeRTOS] UI Init Done, communication with ref has established");
    for (;;)
    {
        referee_ui_task();
        kernel_task_delay(1);
    }
}
#endif

/* ---------------- 对外入口 ---------------- */

void robot_task_init(void)
{
    ins_task_handle = kernel_task_create(&(kernel_task_cfg_t){
        .fn = robot_task_start_ins, .arg = NULL, .name = "instask",
        .stack_words = 1024, .priority = KERNEL_PRIO_HIGH});

    motor_task_handle = kernel_task_create(&(kernel_task_cfg_t){
        .fn = robot_task_start_motor, .arg = NULL, .name = "motortask",
        .stack_words = 256, .priority = KERNEL_PRIO_NORMAL});

    daemon_task_handle = kernel_task_create(&(kernel_task_cfg_t){
        .fn = robot_task_start_daemon, .arg = NULL, .name = "daemontask",
        .stack_words = 128, .priority = KERNEL_PRIO_NORMAL});

    robot_task_handle = kernel_task_create(&(kernel_task_cfg_t){
        .fn = robot_task_start_robot, .arg = NULL, .name = "robottask",
        .stack_words = 1024, .priority = KERNEL_PRIO_NORMAL});

#if ROBOT_TASK_UI_ENABLE
    if (g_app_profile.ui_enable)
        ui_task_handle = kernel_task_create(&(kernel_task_cfg_t){
            .fn = robot_task_start_ui, .arg = NULL, .name = "uitask",
            .stack_words = 512, .priority = KERNEL_PRIO_NORMAL});
#endif

    ht_motor_control_init();

    diag_watch_ins = diag_watch_register("INS", 30);
    diag_watch_motor = diag_watch_register("MOTOR", 30);
    diag_watch_robot = diag_watch_register("ROBOT", 20);
    diag_watch_daemon = diag_watch_register("DAEMON", 10);
#if defined(DG_TEST_HANG)
    dg_test_hang_init();
#endif
}

void robot_init(void)
{
    __disable_irq();

    board_init();

    fault_init();
    fault_dump_to_uart();
    robot_log_reset_cause();

    if (g_app_profile.calib_enable)
        calib_init();

    telemetry_init();

    robot_state = ROBOT_STATUS_STOP;
    if (g_app_profile.init != NULL)
        g_app_profile.init();

    robot_task_init();

    __enable_irq();

    /* IWDG 最后启动:初始化已完成,距首拍 200Hz 任务远小于 2s 超时。 */
    iwdg_init();
}

void robot_task(void)
{
    /* 串口消费必须最先做:各执行单元本拍读到的就是最新收到的字节。 */
    usart_process_all();

    if (diag_watch_required_alive())
        iwdg_feed();
    else
    {
        static uint32_t last_log = 0;
        uint32_t now = dwt_get_timeline_ms();
        if (now - last_log >= 1000u)
        {
            char dead[48];
            diag_watch_dead_names(dead, sizeof(dead));
            LOG_ERROR("[diag] required task dead [%s], IWDG feed stopped", dead);
            last_log = now;
        }
    }

    if (err_safe_state_pending())
    {
        err_safe_state_consume();
        robot_state = ROBOT_STATUS_STOP;
        LOG_ERROR("[err] SAFE_STATE consumed -> ROBOT_STOP");
    }
    if (err_restart_pending())
    {
        err_restart_consume();
        LOG_ERROR("[err] RESTART policy: controlled reset");
        NVIC_SystemReset();
    }
    {
        err_log_entry_t drain[8];
        unsigned n = err_log_drain(drain, 8);
        for (unsigned i = 0; i < n; ++i)
            LOG_ERROR("[err] code=0x%04x tick=%u", (unsigned)drain[i].code,
                      (unsigned)drain[i].tick_ms);
    }

    robot_boot_beep_task();

    if (g_app_profile.run_200hz != NULL)
        g_app_profile.run_200hz();
}
