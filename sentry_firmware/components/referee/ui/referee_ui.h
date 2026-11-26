/**
 * @file    referee_ui.h
 * @brief   UI 绘图接口
 * @layer   COMPONENT
 * @author  小猫大王
* @note    源自跃鹿 basic_framework;本工程未改逻辑,仅统一注释格式。
 */
#ifndef REFEREE_UI_H
#define REFEREE_UI_H

#include "stdarg.h"
#include "stdint.h"
#include "referee_protocol.h"
#include "referee.h"

#pragma pack(1) // 按1字节对齐

/* 此处的定义只与UI绘制有关 */
typedef struct
{
    referee_frame_header_t FrameHeader;
    uint16_t CmdID;
    ext_student_interactive_header_data_t datahead;
    uint8_t Delete_Operate; // 删除操作
    uint8_t Layer;
    uint16_t frametail;
} referee_ui_delete_t;

typedef struct
{
    referee_frame_header_t FrameHeader;
    uint16_t CmdID;
    ext_student_interactive_header_data_t datahead;
    uint16_t frametail;
} referee_ui_graph_refresh_t;

typedef struct
{
    referee_frame_header_t FrameHeader;
    uint16_t CmdID;
    ext_student_interactive_header_data_t datahead;
    referee_string_data_t String_Data;
    uint16_t frametail;
} referee_ui_char_refresh_t; // 打印字符串数据

#pragma pack()

void referee_ui_delete(referee_id_t *_id, uint8_t Del_Operate, uint8_t Del_Layer);

void referee_ui_line_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width, uint32_t Start_x,
                uint32_t Start_y, uint32_t End_x, uint32_t End_y);

void referee_ui_rectangle_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                     uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                     uint32_t Start_x, uint32_t Start_y, uint32_t End_x, uint32_t End_y);

void referee_ui_circle_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                  uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width,
                  uint32_t Start_x, uint32_t Start_y, uint32_t Graph_Radius);

void referee_ui_oval_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Width, uint32_t Start_x,
                uint32_t Start_y, uint32_t end_x, uint32_t end_y);

void referee_ui_arc_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate, uint32_t Graph_Layer,
               uint32_t Graph_Color, uint32_t Graph_StartAngle, uint32_t Graph_EndAngle,
               uint32_t Graph_Width, uint32_t Start_x, uint32_t Start_y, uint32_t end_x,
               uint32_t end_y);

void referee_ui_float_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                 uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Size,
                 uint32_t Graph_Digit, uint32_t Graph_Width, uint32_t Start_x, uint32_t Start_y,
                 int32_t Graph_Float);

void referee_ui_int_draw(referee_graph_data_t *graph, char graphname[3], uint32_t Graph_Operate, uint32_t Graph_Layer,
               uint32_t Graph_Color, uint32_t Graph_Size, uint32_t Graph_Width, uint32_t Start_x,
               uint32_t Start_y, int32_t Graph_Integer);

void referee_ui_char_draw(referee_string_data_t *graph, char graphname[3], uint32_t Graph_Operate,
                uint32_t Graph_Layer, uint32_t Graph_Color, uint32_t Graph_Size,
                uint32_t Graph_Width, uint32_t Start_x, uint32_t Start_y, const char *fmt, ...);

void referee_ui_graph_refresh(referee_id_t *_id, int cnt, ...);

void referee_ui_char_refresh(referee_id_t *_id, referee_string_data_t string_Data);

#endif
