/**
 * @file    referee_task.h
 * @brief   裁判任务接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef REFEREE_TASK_H
#define REFEREE_TASK_H

#include "referee.h"

/**
 * @brief 初始化裁判系统交互任务(UI和多机通信)
 *
 */
referee_info_t *referee_ui_task_init(UART_HandleTypeDef *referee_usart_handle,
                           referee_interactive_info_t *UI_data);

/**
 * @brief  取裁判系统数据总表,供应用层只读访问(热量/等级/增益等)
 * @return 数据总表指针;referee_ui_task_init 之前或该任务被配置宏关闭时返回 NULL
 * @note   只读。要写必须走裁判系统任务,本函数只暴露指针,不做同步 ——
 *         字段是单帧缓存,读到的可能是正在被中断改写的半帧,数值类用途可容忍,
 *         不要用它做逐字节一致性判断。
 */
referee_info_t *referee_get_info(void);

/**
 * @brief 在referee task之前调用,添加在freertos.c中
 *
 */
void referee_my_ui_init();

/**
 * @brief 裁判系统交互任务(UI和多机通信)
 *
 */
void referee_ui_task();

#endif // REFEREE_TASK_H
