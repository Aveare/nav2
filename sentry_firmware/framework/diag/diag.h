/**
 * @file    diag.h
 * @brief   心跳监督接口:必活/可选集合
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    必活/可选集合(方案 §4,修"任务卡死不触发 IWDG"的真实安全差距):
*          - 必活(默认):{ins_state, MOTOR, ROBOT, DAEMON}——任一失活,IWDG 停喂 → 复位;
*          - 可选:diag_watch_register_optional(注册即豁免)+ diag_watch_unregister
*            (UI 合法永久不活/合法离线,不喂狗不算死——修"不接裁判 2s 复位循环"毁车缺陷);
*          - **超时单位 = 拍数**(不变量,与 daemon 一致;tick 由 daemon_task 100Hz 推进);
*          - 喂狗只看必活:robot.c robot_task `if (diag_watch_required_alive()) iwdg_feed();`。
*
* @note    G9 注入钩子:diag_watch_simulate_hang(name) 使该任务的 kick 失效
*          (模拟任务挂死);G9 构建(-DDG_TEST_HANG)在启动延时后自动挂死指定任务,
*          验收 = IWDG 复位 ≤3.0s + RST cause IWDG=1 + 复位后 ROBOT_STATUS_STOP 需解锁。
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct diag_watch *diag_watch_t;

/* 注册必活监督(超时=拍数,拍源=daemon_task 100Hz);注册即要求存活 */
diag_watch_t diag_watch_register(const char *name, uint32_t timeout_ticks);
/* 注册可选监督(UI 等合法离线对象;不进必活掩码) */
diag_watch_t diag_watch_register_optional(const char *name, uint32_t timeout_ticks);
/* 喂心跳(任务每拍调用) */
void diag_watch_kick(diag_watch_t w);
/* 注销(UI 合法离线豁免,P0-3) */
void diag_watch_unregister(diag_watch_t w);

/* 拍推进(daemon_task 100Hz 调用;必活超时 → 失活) */
void diag_watch_tick(void);
/* 必活全活?1=全活(可喂 IWDG)0=有必活失活(停喂) */
int diag_watch_required_alive(void);
/* 失活必活任务名(G9 日志/复位前留痕);返回写入数 */
unsigned diag_watch_dead_names(char *out, unsigned cap);

/* G9 注入:使指定必活任务的 kick 失效(模拟挂死);仅 -DDG_TEST_HANG 构建调用 */
void diag_watch_simulate_hang(const char *name);

#ifdef __cplusplus
}
#endif
