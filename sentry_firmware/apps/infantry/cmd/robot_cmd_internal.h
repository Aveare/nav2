/**
 * @file    robot_cmd_internal.h
 * @brief   cmd 内部共享状态与跨文件接口
 * @layer   APP
 * @author  小猫大王
* @note    2026-11-20 从 robot_cmd.c 拆分时建立。**过渡手段**:跨簇共享的状态仍集中在
*          robot_cmd.c 定义,这里只做 extern 声明 —— 不把每个变量包成 setter,
*          因为那会改变读写时序,而本次拆分的底线是零行为变化。
* @note    后续若要做真正的低耦合,应改为"每簇只暴露必要操作",那是功能批次的事。
 */
#ifndef ROBOT_CMD_INTERNAL_H
#define ROBOT_CMD_INTERNAL_H

#include "robot_def.h"
#include "remote_control.h"

/* 输入源:遥控杆 / 图传键鼠。定义在 robot_cmd.c,由 cmd_kbm.c 的 cmd_update_input_source 写。 */
typedef enum
{
    INPUT_RC = 0, // DBUS遥控杆
    INPUT_KBM,    // 图传键鼠
} kbm_input_source_e;

extern kbm_input_source_e input_source;

/* ---------------- 跨簇共享状态(定义在 robot_cmd.c) ---------------- */

extern remote_rc_ctrl_t *rc_data;                  // 遥控器数据(Init 里建立)
extern robot_status_e robot_state;          // 整机状态:仲裁写,其余簇读
extern chassis_ctrl_cmd_t chassis_cmd; // 三路命令:多簇写,Task 统一发布
/* 调试可写:平移最大速度(mm/s),键鼠/遥控两条路径共用。定义在 robot_cmd.c。 */
extern volatile float chassis_max_speed_mm_s;
extern gimbal_ctrl_cmd_t gimbal_cmd;
extern shoot_ctrl_cmd_t shoot_cmd;
extern gimbal_upload_t gimbal_fb_recv; // 云台反馈:Task 接收,偏移角/重锚定用
extern float ax650_raw_yaw_target;             // 最近一帧有效 0x0503 yaw 目标
extern uint8_t yaw_recentering;                // 解锁回中状态机:键鼠/仲裁/Task 三处置位
extern uint16_t recenter_cnt;

/* 统一自瞄状态(DEV-20261124-03):左中挡,或键鼠输入源下的 Shift 锁存,两者或运算。
 * 键鼠超时回落遥控时 Shift 锁存不参与(避免只剩急停能退出自瞄,DEV-20261124-04)。
 * 生产者 = cmd_task 每周期刷新,消费者 = cmd_remote_set / cmd_keyboard_mouse_set / 回中判据。 */
extern uint8_t auto_aim_active;

/* ---------------- 跨 TU 函数 ---------------- */

void cmd_update_input_source(void);
uint8_t cmd_kbm_auto_aim_held(void); // Shift 按住=自瞄,松手退出(cmd_task 每周期调用)
void cmd_keyboard_mouse_set(void);
void cmd_remote_set(void);
void cmd_authority_arbitrate(void);

/* AX650 0x0503 → yaw/pitch 目标的统一解算(左中挡与键鼠自瞄共用,DEV-20261124-03)。
 * 调用方只负责置 gimbal_mode= AUTO_AIM 与底盘模式,本函数只写目标角。 */
void cmd_auto_aim_set(void);

/* 战术射速档位(DEV-20261121-11):状态在 robot_cmd.c,输入层调用 Cycle、发布层调用 Get/Rate */
void cmd_fire_tier_cycle(void);      // 切换下一档(键鼠 X 键调用)
fire_tier_e cmd_fire_tier_get(void); // 当前档位(UI/日志用)
float cmd_fire_tier_rate(void);      // 当前档位对应的请求射速,发/s

/* 小陀螺档位(DEV-20261125-08):状态在 robot_cmd.c,键鼠 C 键调用 Toggle;
 * 档位随 chassis_cmd.spin_vary 每周期下发给底盘。 */
void cmd_spin_vary_toggle(void);

#if YAW_AUTO_KF_ENABLE
void cmd_yaw_auto_kalman_reset(void);
float cmd_yaw_auto_kalman_update(float measured_angle, float measured_rate, uint8_t target_id,
                          uint32_t command_count, float now_ms);
#endif
#if YAW_AUTO_SMOOTH_ENABLE
void cmd_yaw_auto_smooth_reset(void);
float cmd_yaw_auto_smooth_update(float target, float initial_angle, float now_ms);
#endif

#endif // ROBOT_CMD_INTERNAL_H
