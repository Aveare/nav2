/**
 * @file    referee_task.c
 * @brief   裁判任务:接收解析与 UI 调度
 * @layer   COMPONENT
 * @author  kidneygood
 * @author  modified by 小猫大王
 * @author  modified by JIT CR (2026-11-23 USART1/USART6 路由收口)
* @version 0.1
* @date 2022-11-18
* @copyright Copyright (c) 2022
 */
#include "referee_task.h"
#include "referee.h"
#include "referee_ui.h"
#include "referee_app_status.h" // 热量/档位由 app 注册提供者(T01 反向依赖消除)
#include "dwt_port.h"
#include "string.h"
#include "kernel.h"
#include "log_port.h"

static referee_interactive_info_t *Interactive_data; // UI绘制需要的机器人状态数据
static referee_info_t *referee_recv_info;            // 接收到的裁判系统数据
uint8_t referee_ui_seq;                                      // 包序号，供整个referee文件使用
// @todo 不应该使用全局变量

/**
 * @brief  判断各种ID，选择客户端ID
 * @param  referee_info_t *referee_recv_info
 * @retval none
 * @attention
 */
static void referee_determine_robot_id()
{
    // id小于7是红色,大于7是蓝色,0为红色，1为蓝色   #define REFEREE_ROBOT_RED 0    #define REFEREE_ROBOT_BLUE 1
    referee_recv_info->referee_id.Robot_Color =
        referee_recv_info->GameRobotState.robot_id > 7 ? REFEREE_ROBOT_BLUE : REFEREE_ROBOT_RED;
    referee_recv_info->referee_id.Robot_ID = referee_recv_info->GameRobotState.robot_id;
    referee_recv_info->referee_id.Cilent_ID =
        0x0100 + referee_recv_info->referee_id.Robot_ID; // 计算客户端ID
    referee_recv_info->referee_id.Receiver_Robot_ID = 0;
}

static void referee_my_ui_refresh(referee_info_t *referee_recv_info,
                        referee_interactive_info_t *_Interactive_data);
static void referee_ui_change_check(referee_interactive_info_t *_Interactive_data); // 模式切换检测
static void
RobotModeTest(referee_interactive_info_t *_Interactive_data); // 测试用函数，实现模式自动变化

referee_info_t *referee_ui_task_init(UART_HandleTypeDef *referee_usart_handle,
                           referee_interactive_info_t *UI_data)
{
    referee_recv_info =
        referee_init(referee_usart_handle); // 初始化裁判系统的串口,并返回裁判系统反馈数据指针
    Interactive_data = UI_data;            // 获取UI绘制需要的机器人状态数据
    referee_recv_info->init_flag = 1;
    return referee_recv_info;
}

void referee_ui_task()
{
    referee_my_ui_refresh(referee_recv_info, Interactive_data);
}

static referee_graph_data_t referee_ui_shoot_line[10]; // 射击准线
static referee_graph_data_t referee_ui_energy[3];      // 电容能量条
static referee_string_data_t referee_ui_state_static[6];  // 机器人状态,静态只需画一次
static referee_string_data_t referee_ui_state_dynamic[6];  // 机器人状态,动态先add才能change
static referee_graph_data_t referee_ui_heat_bar[2];    // 热量条:0=框 1=填充(DEV-20261121-11)
static referee_string_data_t referee_ui_heat_text[2];   // 0=战术档位 1=守卫状态
static referee_graph_data_t referee_ui_heat_number;       // 当前热量数字
static referee_graph_data_t referee_ui_aim_circle;        // 准星中心自瞄指示圆环
static uint32_t shoot_line_location[10] = {540, 960, 490, 515, 565};

/* 状态栏数值列统一 18 字符:UI_Graph_Change 按新串长度覆盖,短串会残留旧字符。
 * 18 字符 + NUL 仍落在 referee_string_data_t.show_Data[30] 内。
 * 不用 vsnprintf:UI 任务栈只有 512 words,且 newlib-nano 的格式化库会额外吃栈;
 * 这里只做拷贝 + 补空格,数值文本用固定常量(与 config SHOOT_RATE_* 同步维护)。 */
static const size_t referee_ui_status_len = 18;

static void referee_ui_status_text(char *dst, const char *src)
{
    size_t n = 0;
    while (n < referee_ui_status_len && src[n] != '\0')
    {
        dst[n] = src[n];
        n++;
    }
    while (n < referee_ui_status_len)
        dst[n++] = ' ';
    dst[referee_ui_status_len] = '\0';
}

void referee_my_ui_init()
{
    if (!referee_recv_info->init_flag)
    {
        kernel_task_delete(NULL); // 如果没有初始化裁判系统则结束当前 UI 任务
        return;
    }
    while (referee_recv_info->GameRobotState.robot_id == 0)
    {
        // 硬件未接/裁判系统未上电时停留于此(1Hz提示,不阻塞其他任务);接好USART6并上电后自动继续
        LOG_INFO("[referee] waiting for robot_id (check USART6 wiring & referee power)");
        /* USART6 是裁判系统唯一接收口;USART1 只承载 AX650 或 VOFA 调试。
         * 这里不能再向 USART1 写 ASCII,否则会绕过宏所有权并污染另一协议。 */
        kernel_task_delay(1000);
    }
    LOG_INFO("[referee] robot_id received (%d), start UI init",
            (int)referee_recv_info->GameRobotState.robot_id);

    referee_determine_robot_id();                                            // 确定ui要发送到的目标客户端
    referee_ui_delete(&referee_recv_info->referee_id, UI_Data_Del_ALL, 0); // 清空UI

    // 绘制发射基准线
    referee_ui_line_draw(&referee_ui_shoot_line[0], "sl0", UI_Graph_ADD, 7, UI_Color_White, 3, 710,
               shoot_line_location[0], 1210, shoot_line_location[0]);
    referee_ui_line_draw(&referee_ui_shoot_line[1], "sl1", UI_Graph_ADD, 7, UI_Color_White, 3, shoot_line_location[1],
               340, shoot_line_location[1], 740);
    referee_ui_line_draw(&referee_ui_shoot_line[2], "sl2", UI_Graph_ADD, 7, UI_Color_Yellow, 2, 810,
               shoot_line_location[2], 1110, shoot_line_location[2]);
    referee_ui_line_draw(&referee_ui_shoot_line[3], "sl3", UI_Graph_ADD, 7, UI_Color_Yellow, 2, 810,
               shoot_line_location[3], 1110, shoot_line_location[3]);
    referee_ui_line_draw(&referee_ui_shoot_line[4], "sl4", UI_Graph_ADD, 7, UI_Color_Yellow, 2, 810,
               shoot_line_location[4], 1110, shoot_line_location[4]);
    referee_ui_graph_refresh(&referee_recv_info->referee_id, 5, referee_ui_shoot_line[0], referee_ui_shoot_line[1],
                   referee_ui_shoot_line[2], referee_ui_shoot_line[3], referee_ui_shoot_line[4]);

    /* 自瞄指示(DEV-20261124-15):准星中心圆环,绿色=自瞄开,白色=关。
     * 半径 120/线宽 6;换色用整层删除重画,不依赖 Change 的 color 字段。 */
    referee_ui_circle_draw(&referee_ui_aim_circle, "ac0", UI_Graph_ADD, 9, UI_Color_White, 6, 960, 540, 120);
    referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_aim_circle);

    // 状态栏:左侧中上,标签白色,值按状态变色(DEV-20261124-15)
    referee_ui_char_draw(&referee_ui_state_static[0], "ss0", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 150, 750,
               "SPIN");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[0]);
    referee_ui_char_draw(&referee_ui_state_static[1], "ss1", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 150, 700,
               "FRICT");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[1]);
    referee_ui_char_draw(&referee_ui_state_static[2], "ss2", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 150, 650,
               "LOADER");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[2]);
    referee_ui_char_draw(&referee_ui_state_static[3], "ss3", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 150, 600, "LOAD");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[3]);
    referee_ui_char_draw(&referee_ui_state_static[4], "ss4", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 150, 550, "TIER");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[4]);

    // 数值列:统一 18 字符,去掉与标签重复的前缀,行尾带专属按键
    char ui_text[24];
    referee_ui_status_text(ui_text, "OFF [Q]");
    referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 240, 750,
               "%s", ui_text);
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[0]);
    referee_ui_status_text(ui_text, "OFF [RMB]");
    referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 240, 700,
               "%s", ui_text);
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[1]);
    referee_ui_status_text(ui_text, "OFF");
    referee_ui_char_draw(&referee_ui_state_dynamic[2], "sd2", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 240, 650,
               "%s", ui_text);
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[2]);
    referee_ui_status_text(ui_text, "AUTO [R/E][LMB]");
    referee_ui_char_draw(&referee_ui_state_dynamic[3], "sd3", UI_Graph_ADD, 8, UI_Color_Cyan, 15, 2, 240, 600,
               "%s", ui_text);
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[3]);
    referee_ui_status_text(ui_text, "NORMAL 6.0/s [X]");
    referee_ui_char_draw(&referee_ui_state_dynamic[4], "sd4", UI_Graph_ADD, 8, UI_Color_White, 15, 2, 240, 550,
               "%s", ui_text);
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[4]);

    /* 功率条已删除(DEV-20261124-15):上游遗留,本车无功率闭环,显示无意义。 */

    /* 热量面板(DEV-20261121-11):热量条 + 数字 + 战术档位 + 守卫状态。
     * 布局放在左下方(避开裁判默认的中央血条与现有准星线),坐标系 1920×1080。 */
    referee_ui_rectangle_draw(&referee_ui_heat_bar[0], "hb0", UI_Graph_ADD, 7, UI_Color_White, 2, 150, 455, 650, 495);
    referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_bar[0]);
    referee_ui_line_draw(&referee_ui_heat_bar[1], "hb1", UI_Graph_ADD, 8, UI_Color_Green, 30, 156, 475, 156, 475);
    referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_bar[1]);
    referee_ui_char_draw(&referee_ui_heat_text[1], "hx1", UI_Graph_ADD, 8, UI_Color_Yellow, 15, 2, 150, 380,
               "HEAT:NORMAL ");
    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_heat_text[1]);
    referee_ui_float_draw(&referee_ui_heat_number, "hx2", UI_Graph_ADD, 8, UI_Color_Green, 15, 2, 2, 700, 505, 0);
    referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_number);
}

// 测试用函数，实现模式自动变化,用于检查该任务和裁判系统是否连接正常
static uint8_t count = 0;
static uint16_t count1 = 0;
/* 上游遗留的联调自检函数:已从 referee_ui_task 调用链移除(它会自动改写模式,干扰真实状态显示),
 * 保留代码备查;标记 unused 以免编译告警。 */
static void __attribute__((unused)) RobotModeTest(referee_interactive_info_t *_Interactive_data)
{
    count++;
    if (count >= 50)
    {
        count = 0;
        count1++;
    }
    switch (count1 % 4)
    {
    case 0:
    {
        _Interactive_data->chassis_mode = CHASSIS_ZERO_FORCE;
        _Interactive_data->gimbal_mode = GIMBAL_ZERO_FORCE;
        _Interactive_data->shoot_mode = SHOOT_ON;
        _Interactive_data->friction_mode = FRICTION_ON;
        _Interactive_data->lid_mode = LID_OPEN;
        _Interactive_data->Chassis_Power_Data.chassis_power_mx += 3.5;
        if (_Interactive_data->Chassis_Power_Data.chassis_power_mx >= 18)
            _Interactive_data->Chassis_Power_Data.chassis_power_mx = 0;
        break;
    }
    case 1:
    {
        _Interactive_data->chassis_mode = CHASSIS_ROTATE;
        _Interactive_data->gimbal_mode = GIMBAL_FREE_MODE;
        _Interactive_data->shoot_mode = SHOOT_OFF;
        _Interactive_data->friction_mode = FRICTION_OFF;
        _Interactive_data->lid_mode = LID_CLOSE;
        break;
    }
    case 2:
    {
        _Interactive_data->chassis_mode = CHASSIS_NO_FOLLOW;
        _Interactive_data->gimbal_mode = GIMBAL_GYRO_MODE;
        _Interactive_data->shoot_mode = SHOOT_ON;
        _Interactive_data->friction_mode = FRICTION_ON;
        _Interactive_data->lid_mode = LID_OPEN;
        break;
    }
    case 3:
    {
        _Interactive_data->chassis_mode = CHASSIS_FOLLOW_GIMBAL_YAW;
        _Interactive_data->gimbal_mode = GIMBAL_ZERO_FORCE;
        _Interactive_data->shoot_mode = SHOOT_OFF;
        _Interactive_data->friction_mode = FRICTION_OFF;
        _Interactive_data->lid_mode = LID_CLOSE;
        break;
    }
    default:
        break;
    }
}

/* 静态图元周期重发(DEV-20261125-02):裁判客户端重启/切视角/重进比赛会清空本地 UI,
 * 机器人无法感知,初始化时只画一次的标签与准星线就永久消失(圆环因为随自瞄状态
 * 重画而幸存)。这里每 300ms 重发 1 个静态图元,8 步约 2.4s 轮完一圈。
 * 不能整批重发:referee_send 每帧固定 34ms 延时,19 帧会阻塞 UI 任务约 646ms。 */
#define REFEREE_UI_REPLAY_PERIOD_MS 300.0f

static void referee_ui_replay_static(void)
{
    static float last_ms;
    static uint8_t step;
    const float now = dwt_get_timeline_ms();
    if (now - last_ms < REFEREE_UI_REPLAY_PERIOD_MS)
        return;
    last_ms = now;

    switch (step)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_static[step]);
        break;
    case 5:
        referee_ui_graph_refresh(&referee_recv_info->referee_id, 5, referee_ui_shoot_line[0],
                     referee_ui_shoot_line[1], referee_ui_shoot_line[2], referee_ui_shoot_line[3],
                     referee_ui_shoot_line[4]);
        break;
    case 6:
        referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_bar[0]);
        break;
    default:
        /* 热量档位文字是静态的,和 HEAT 标签一起补发 */
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_heat_text[1]);
        break;
    }

    step = (uint8_t)((step + 1u) % 8u);
}

static void referee_my_ui_refresh(referee_info_t *referee_recv_info,
                        referee_interactive_info_t *_Interactive_data)
{
    referee_ui_replay_static();
    referee_ui_change_check(_Interactive_data);
    // chassis
    if (_Interactive_data->Referee_Interactive_Flag.chassis_flag == 1)
    {
        switch (_Interactive_data->chassis_mode)
        {
        case CHASSIS_ZERO_FORCE:
            referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_Change, 8, UI_Color_Main, 15, 2, 270, 750,
                       "zeroforce");
            break;
        case CHASSIS_ROTATE:
            referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_Change, 8, UI_Color_Main, 15, 2, 270, 750,
                       "rotate   ");
            // 此处注意字数对齐问题，字数相同才能覆盖掉
            break;
        case CHASSIS_NO_FOLLOW:
            referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_Change, 8, UI_Color_Main, 15, 2, 270, 750,
                       "nofollow ");
            break;
        case CHASSIS_FOLLOW_GIMBAL_YAW:
            referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_Change, 8, UI_Color_Main, 15, 2, 270, 750,
                       "follow   ");
            break;
        default: // CHASSIS_ALIGN 由底盘侧执行,UI 不切换状态文本
            break;
        }
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[0]);
        _Interactive_data->Referee_Interactive_Flag.chassis_flag = 0;
    }
    // gimbal
    if (_Interactive_data->Referee_Interactive_Flag.gimbal_flag == 1)
    {
        switch (_Interactive_data->gimbal_mode)
        {
        case GIMBAL_ZERO_FORCE:
        {
            referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270,
                       700, "zeroforce");
            break;
        }
        case GIMBAL_FREE_MODE:
        {
            referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270,
                       700, "free     ");
            break;
        }
        case GIMBAL_GYRO_MODE:
        {
            referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270,
                       700, "gyro     ");
            break;
        }
        case GIMBAL_AUTO_AIM_MODE:
        {
            referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 270,
                       700, "autoaim  ");
            break;
        }
        }
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[1]);
        _Interactive_data->Referee_Interactive_Flag.gimbal_flag = 0;
    }
    // shoot
    if (_Interactive_data->Referee_Interactive_Flag.shoot_flag == 1)
    {
        referee_ui_char_draw(&referee_ui_state_dynamic[2], "sd2", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 650,
                   _Interactive_data->shoot_mode == SHOOT_ON ? "on " : "off");
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[2]);
        _Interactive_data->Referee_Interactive_Flag.shoot_flag = 0;
    }
    // friction
    if (_Interactive_data->Referee_Interactive_Flag.friction_flag == 1)
    {
        referee_ui_char_draw(&referee_ui_state_dynamic[3], "sd3", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 600,
                   _Interactive_data->friction_mode == FRICTION_ON ? "on " : "off");
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[3]);
        _Interactive_data->Referee_Interactive_Flag.friction_flag = 0;
    }
    // lid
    if (_Interactive_data->Referee_Interactive_Flag.lid_flag == 1)
    {
        referee_ui_char_draw(&referee_ui_state_dynamic[4], "sd4", UI_Graph_Change, 8, UI_Color_Pink, 15, 2, 270, 550,
                   _Interactive_data->lid_mode == LID_OPEN ? "open " : "close");
        referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[4]);
        _Interactive_data->Referee_Interactive_Flag.lid_flag = 0;
    }
    // power
    if (_Interactive_data->Referee_Interactive_Flag.Power_flag == 1)
    {
        referee_ui_float_draw(&referee_ui_energy[1], "sd5", UI_Graph_Change, 8, UI_Color_Green, 18, 2, 2, 750, 230,
                    _Interactive_data->Chassis_Power_Data.chassis_power_mx * 1000);
        referee_ui_line_draw(&referee_ui_energy[2], "sd6", UI_Graph_Change, 8, UI_Color_Pink, 30, 720, 160,
                   (uint32_t)750 + _Interactive_data->Chassis_Power_Data.chassis_power_mx * 30,
                   160);
        referee_ui_graph_refresh(&referee_recv_info->referee_id, 2, referee_ui_energy[1], referee_ui_energy[2]);
        _Interactive_data->Referee_Interactive_Flag.Power_flag = 0;
    }

    /* 热量面板(DEV-20261121-11):10Hz 刷新。
     * referee_send 内部按裁判接收上限(~10Hz)做延时,不能每拍发送;这里用 100ms 节流,
     * 且只在"数值变化"时才发,减少 UI 串口占用。 */
    {
        static float last_ms;
        static int last_q = -1;
        static int last_tier = 1; // 与初始化文本 TIER:NORMAL 一致,避免启动重复发帧
        static int last_state = -1;
        float now = dwt_get_timeline_ms();
        if (now - last_ms >= 100.0f)
        {
            last_ms = now;

            referee_app_status_t app = {0};
            bool has_app = referee_app_status_get(&app);
            char ui_text[24];
            float q = has_app ? app.heat : 0.0f;
            float lim = has_app ? app.heat_limit : 0.0f;
            (void)(has_app ? app.cool_per_s : 0.0f); /* 冷却值当前未上屏 */
            if (lim < 1.0f)
                lim = 230.0f; // 裁判未上线时按爆发优先档显示满量程
            int qi = (int)(q + 0.5f);
            if (qi < 0)
                qi = 0;
            if ((float)qi > lim)
                qi = (int)lim;

            /* 填充长度按比例映射到 494px(156 → 650) */
            uint32_t fill_w = (uint32_t)(494.0f * ((float)qi / lim));
            referee_ui_line_draw(&referee_ui_heat_bar[1], "hb1", UI_Graph_Change, 8, UI_Color_Green, 30, 156, 475,
                       156 + fill_w, 475);
            referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_bar[1]);

            if (qi != last_q)
            {
                last_q = qi;
                referee_ui_float_draw(&referee_ui_heat_number, "hx2", UI_Graph_Change, 8, UI_Color_Green, 15, 2, 2, 700,
                            505, qi);
                referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_heat_number);
            }

            int tier = has_app ? (int)app.fire_tier : 1;
            if (tier != last_tier)
            {
                last_tier = tier;
                /* 数值与 config 的 SHOOT_RATE_ECO / DEFAULT / BURST 同步:1.4 / 6.0 / 12.0 发/s */
                const char *t = (tier == 0)   ? "ECO 1.4/s [X]"
                                : (tier == 2) ? "BURST 12.0/s [X]"
                                              : "NORMAL 6.0/s [X]";
                const uint32_t color = (tier == 0)   ? UI_Color_Green
                                       : (tier == 2) ? UI_Color_Yellow
                                                     : UI_Color_White;
                referee_ui_status_text(ui_text, t);
                referee_ui_char_draw(&referee_ui_state_dynamic[4], "sd4", UI_Graph_Change, 8, color, 15, 2,
                           240, 550, "%s", ui_text);
                referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[4]);
            }

            /* 小陀螺/摩擦轮/拨弹盘/发射档(DEV-20261124-15):10Hz,只在变化时重绘。
             * 自瞄不占文字行,只改准星中心圆环颜色。 */
            {
                static int last_spin = 0;
                static int last_aim = 0;
                static int last_frict = 0;
                static int last_loader = 0;
                static int last_load = 3; // 与初始化 LOAD:AUTO 一致,避免启动重复发帧
                int spin = has_app ? (int)app.spin_mode : 0;
                int aim = has_app ? (int)app.auto_aim : 0;
                int frict = has_app ? (int)app.friction_on : 0;
                int loader = has_app ? (int)app.loader_on : 0;
                int load = has_app ? (int)app.load_mode : 3;

                /* 自瞄指示:绿=开,白=关。客户端对 Change 的 color 不敏感,
                 * 这里删除 layer 9(仅圆环)后重画,保证颜色一定生效。 */
                if (aim != last_aim)
                {
                    last_aim = aim;
                    referee_ui_delete(&referee_recv_info->referee_id, UI_Data_Del_Layer, 9);
                    referee_ui_circle_draw(&referee_ui_aim_circle, "ac0", UI_Graph_ADD, 9,
                                aim ? UI_Color_Green : UI_Color_White, 6, 960, 540, 120);
                    referee_ui_graph_refresh(&referee_recv_info->referee_id, 1, referee_ui_aim_circle);
                }
                if (spin != last_spin)
                {
                    last_spin = spin;
                    const char *t = (spin == REFEREE_SPIN_VARY)  ? "VARY [Q]"
                                    : (spin == REFEREE_SPIN_CONST) ? "CONST [Q]"
                                                                   : "OFF [Q]";
                    const uint32_t color = (spin == REFEREE_SPIN_OFF) ? UI_Color_White : UI_Color_Green;
                    referee_ui_status_text(ui_text, t);
                    referee_ui_char_draw(&referee_ui_state_dynamic[0], "sd0", UI_Graph_Change, 8, color, 15, 2,
                               240, 750, "%s", ui_text);
                    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[0]);
                }
                if (frict != last_frict)
                {
                    last_frict = frict;
                    referee_ui_status_text(ui_text, frict ? "ON [RMB]" : "OFF [RMB]");
                    referee_ui_char_draw(&referee_ui_state_dynamic[1], "sd1", UI_Graph_Change, 8,
                               frict ? UI_Color_Green : UI_Color_White, 15, 2, 240, 700, "%s", ui_text);
                    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[1]);
                }
                if (loader != last_loader)
                {
                    last_loader = loader;
                    referee_ui_status_text(ui_text, loader ? "ON" : "OFF");
                    referee_ui_char_draw(&referee_ui_state_dynamic[2], "sd2", UI_Graph_Change, 8,
                               loader ? UI_Color_Green : UI_Color_White, 15, 2, 240, 650, "%s", ui_text);
                    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[2]);
                }
                if (load != last_load)
                {
                    last_load = load;
                    const char *t = (load == REFEREE_LOAD_AUTO)    ? "AUTO [R/E][LMB]"
                                    : (load == REFEREE_LOAD_BURST3) ? "BURST3 [E][LMB]"
                                    : (load == REFEREE_LOAD_SINGLE) ? "SINGLE [R][LMB]"
                                                                    : "OFF [R][LMB]";
                    const uint32_t color = (load == REFEREE_LOAD_AUTO) ? UI_Color_Cyan : UI_Color_Green;
                    referee_ui_status_text(ui_text, t);
                    referee_ui_char_draw(&referee_ui_state_dynamic[3], "sd3", UI_Graph_Change, 8, color, 15, 2,
                               240, 600, "%s", ui_text);
                    referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_state_dynamic[3]);
                }
            }

            int st = has_app ? (int)app.heat_state : (int)REFEREE_HEAT_STATE_OFF;
            if (st != last_state)
            {
                last_state = st;
                const char *s = (st == REFEREE_HEAT_STATE_HOLD)     ? "HEAT:HOLD   "
                                : (st == REFEREE_HEAT_STATE_SINGLE) ? "HEAT:SINGLE "
                                : (st == REFEREE_HEAT_STATE_OFF)    ? "HEAT:OFF    "
                                                            : "HEAT:NORMAL ";
                referee_ui_char_draw(&referee_ui_heat_text[1], "hx1", UI_Graph_Change, 8, UI_Color_Yellow, 15, 2, 150,
                           380, s);
                referee_ui_char_refresh(&referee_recv_info->referee_id, referee_ui_heat_text[1]);
            }
        }
    }
}

/**
 * @brief  模式切换检测,模式发生切换时，对flag置位
 * @param  referee_interactive_info_t *_Interactive_data
 * @retval none
 * @attention
 */
static void referee_ui_change_check(referee_interactive_info_t *_Interactive_data)
{
    if (_Interactive_data->chassis_mode != _Interactive_data->chassis_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.chassis_flag = 1;
        _Interactive_data->chassis_last_mode = _Interactive_data->chassis_mode;
    }

    if (_Interactive_data->gimbal_mode != _Interactive_data->gimbal_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.gimbal_flag = 1;
        _Interactive_data->gimbal_last_mode = _Interactive_data->gimbal_mode;
    }

    if (_Interactive_data->shoot_mode != _Interactive_data->shoot_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.shoot_flag = 1;
        _Interactive_data->shoot_last_mode = _Interactive_data->shoot_mode;
    }

    if (_Interactive_data->friction_mode != _Interactive_data->friction_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.friction_flag = 1;
        _Interactive_data->friction_last_mode = _Interactive_data->friction_mode;
    }

    if (_Interactive_data->lid_mode != _Interactive_data->lid_last_mode)
    {
        _Interactive_data->Referee_Interactive_Flag.lid_flag = 1;
        _Interactive_data->lid_last_mode = _Interactive_data->lid_mode;
    }

    if (_Interactive_data->Chassis_Power_Data.chassis_power_mx !=
        _Interactive_data->Chassis_last_Power_Data.chassis_power_mx)
    {
        _Interactive_data->Referee_Interactive_Flag.Power_flag = 1;
        _Interactive_data->Chassis_last_Power_Data.chassis_power_mx =
            _Interactive_data->Chassis_Power_Data.chassis_power_mx;
    }
}

/**
 * @brief 取裁判系统数据总表,供应用层只读访问
 * @return 数据总表指针;referee_ui_task_init 之前或该任务被配置宏关闭时返回 NULL
 * @note  见 referee_task.h 的只读与一致性说明。
 */
referee_info_t *referee_get_info(void)
{
    return referee_recv_info;
}
