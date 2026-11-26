/**
 * @file    robot_task.c
 * @brief   FreeRTOS 任务创建与各周期任务的实现
 * @layer   APP
 * @note    任务频率:INS 1kHz(阻塞读传感器)/ MOTOR 1kHz / DAEMON 100Hz / ROBOT 200Hz / UI 按需。
 * @note    每个任务循环自测耗时,超期打 LOGERROR;仅暴露调度问题,不阻塞控制。
 * @note    **日志单位陷阱**:超期日志打印 (int)(dt × 1000),数值是微秒级但标签写作 ms。
 *          排障时按 µs 解读,不要被放大 1000 倍的数字误导。
 * @note    2026-10-29 建立,作者 JIT CR。
 */
#include "robot_task.h"
#include "robot_def.h"
#include "ax650_uart1.h"

osThreadId insTaskHandle;
osThreadId robotTaskHandle;
osThreadId motorTaskHandle;
osThreadId daemonTaskHandle;
osThreadId uiTaskHandle;

void StartINSTASK(void const *argument);
void StartMOTORTASK(void const *argument);
void StartDAEMONTASK(void const *argument);
void StartROBOTTASK(void const *argument);
void StartUITASK(void const *argument);

/**
 * @brief  创建全部 FreeRTOS 任务,并初始化可选的 HT 电机控制
 * @note   优先级:INS 高于 Normal,保证 1kHz 姿态解算不被同级任务抢占;其余同级。
 * @note   栈大小(单位 word)按峰值留余量:INS / ROBOT 1024,MOTOR 256,DAEMON 128,UI 512。
 * @note   仅由 RobotInit() 在 RTOS 启动前调用;任务是否创建由 TASK_*_ENABLE 宏决定。
 */
void OSTaskInit()
{
#if !CHASSIS_TUNING_ONLY || SENTRY_BIG_YAW_CONTROL_ENABLE || SENTRY_PITCH_CONTROL_ENABLE || SENTRY_SMALL_YAW_ENABLE
    osThreadDef(instask, StartINSTASK, osPriorityAboveNormal, 0, 1024);
    insTaskHandle = osThreadCreate(osThread(instask), NULL); // 姿态解算高优先级,确保1kHz
#endif

    osThreadDef(motortask, StartMOTORTASK, osPriorityNormal, 0, 256);
    motorTaskHandle = osThreadCreate(osThread(motortask), NULL);

    osThreadDef(daemontask, StartDAEMONTASK, osPriorityNormal, 0, 128);
    daemonTaskHandle = osThreadCreate(osThread(daemontask), NULL);

    osThreadDef(robottask, StartROBOTTASK, osPriorityNormal, 0, 1024);
    robotTaskHandle = osThreadCreate(osThread(robottask), NULL);

    // UI任务由TASK_UI_ENABLE控制接线(见robot_task.h)
#if TASK_UI_ENABLE && !CHASSIS_TUNING_ONLY
    osThreadDef(uitask, StartUITASK, osPriorityNormal, 0, 512);
    uiTaskHandle = osThreadCreate(osThread(uitask), NULL);
#endif

    HTMotorControlInit(); // 没有注册HT电机则不会执行
}

/* 姿态解算任务,1kHz */
__attribute__((noreturn)) void StartINSTASK(void const *argument)
{
    static float ins_start;
    static float ins_dt;
    INS_Init(); // 确保BMI088被正确初始化
#if AX650_UART1_ENABLE
    if (!Ax650Uart1Init()) LOGERROR("[ax650] USART1 init failed; retrying with INS updates");
#endif
    LOGINFO("[freeRTOS] INS Task Start");
    for (;;)
    {
        ins_start = DWT_GetTimeline_ms();
        INS_Task();
        ins_dt = DWT_GetTimeline_ms() - ins_start;
        if (ins_dt > 1)
            LOGERROR("[freeRTOS] INS Task is being DELAY! dt = [%d ms]", (int)(ins_dt * 1000));
        osDelay(1);
    }
}

/* 电机闭环与CAN发送任务,1kHz */
__attribute__((noreturn)) void StartMOTORTASK(void const *argument)
{
    static float motor_dt;
    static float motor_start;
    LOGINFO("[freeRTOS] MOTOR Task Start");
    for (;;)
    {
        motor_start = DWT_GetTimeline_ms();
        MotorControlTask();
        motor_dt = DWT_GetTimeline_ms() - motor_start;
        if (motor_dt > 1)
            LOGERROR("[freeRTOS] MOTOR Task is being DELAY! dt = [%d ms]", (int)(motor_dt * 1000));
        osDelay(1);
    }
}

/* 守护+蜂鸣器任务,100Hz */
__attribute__((noreturn)) void StartDAEMONTASK(void const *argument)
{
    static float daemon_dt;
    static float daemon_start;
    BuzzerInit();
    LOGINFO("[freeRTOS] Daemon Task Start");
    for (;;)
    {
        daemon_start = DWT_GetTimeline_ms();
        DaemonTask();
        BuzzerTask();
        daemon_dt = DWT_GetTimeline_ms() - daemon_start;
        if (daemon_dt > 10)
            LOGERROR("[freeRTOS] Daemon Task is being DELAY! dt = [%d ms]", (int)(daemon_dt * 1000));
        osDelay(10);
    }
}

/* 应用核心任务(chassis/gimbal/shoot/cmd),200Hz;平衡步兵等需提升至1kHz */
__attribute__((noreturn)) void StartROBOTTASK(void const *argument)
{
    static float robot_dt;
    static float robot_start;
    LOGINFO("[freeRTOS] ROBOT core Task Start");
    for (;;)
    {
        robot_start = DWT_GetTimeline_ms();
        RobotTask();
        robot_dt = DWT_GetTimeline_ms() - robot_start;
        /* 周期 5ms(200Hz);超期意味着本拍控制被拖慢,会直接影响手感与互锁时序 */
        if (robot_dt > 5)
            LOGERROR("[freeRTOS] ROBOT core Task is being DELAY! dt = [%d ms]", (int)(robot_dt * 1000));
        osDelay(5);
    }
}

#if TASK_UI_ENABLE && !CHASSIS_TUNING_ONLY
/* 裁判系统UI任务 */
__attribute__((noreturn)) void StartUITASK(void const *argument)
{
    LOGINFO("[freeRTOS] UI Task Start");
    MyUIInit();
    LOGINFO("[freeRTOS] UI Init Done, communication with ref has established");
    for (;;)
    {
        UITask();
        osDelay(1); // 无UI刷新时也挂起一次,防止卡死切换
    }
}
#endif
