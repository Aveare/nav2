/**
 * @file    referee_vt_task.h
 * @brief   图传链路数据任务
 * @layer   MODULE
 * @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */

#ifndef REFEREE_VT_TASK_H
#define REFEREE_VT_TASK_H

#include "referee_vt.h"
#include "robot_def.h"

referee_vt_info_t *VTTaskInit(UART_HandleTypeDef *vt_usart_handle);

#endif // REFEREE_VT_TASK_H
