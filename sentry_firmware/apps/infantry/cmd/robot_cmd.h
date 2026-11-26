/**
 * @file    robot_cmd.h
 * @brief   cmd 应用接口:初始化/任务/档位查询
 * @layer   APP
 * @author  小猫大王
 */
#ifndef ROBOT_CMD_H
#define ROBOT_CMD_H

/**
 * @brief  控制命令应用初始化:注册遥控/图传输入,订阅各应用反馈
 * @note   须在 RTOS 启动前调用,由 robot_init() 统一调度;只能调用一次。
 */
void cmd_init();

/**
 * @brief  控制命令应用任务:裁决唯一控制源,把输入翻译成底盘/云台/发射指令
 * @note   由 robot_task() 以 200Hz 调用。频率必须高于视觉发送频率,
 *         否则视觉目标更新率会超过指令下行能力。内部持有仲裁状态,不可并发调用。
 */
void cmd_task();

/**
 * @brief 最近一帧有效 0x0503 中 AX650 下发的原始 yaw 目标角
 * @return 世界系绝对目标角，deg；尚未收到有效目标时为 0。
 */
float cmd_get_ax650_raw_yaw(void);

/**
 * @brief 当前战术射速档位(0=ECO 1=NORMAL 2=BURST)
 * @return 档位枚举值;UI 显示与日志用,未初始化时为 NORMAL(1)
 * @note  对外暴露 uint8_t 而不是 fire_tier_e,避免 UI 模块反向依赖 APP 头文件
 */
uint8_t cmd_get_fire_tier(void);

/**
 * @brief 自瞄是否开启(左中挡或键鼠 Shift 锁存)
 * @return 1=开启;0=关闭
 */
uint8_t cmd_get_auto_aim_active(void);

/**
 * @brief 小陀螺显示模式
 * @return 0=未开启 1=匀速 2=变速
 * @note  匀速/变速为运行期档位(DEV-20261125-08),键鼠 C 沿切换;
 *        编译宏 CHASSIS_SPIN_VARY_EN=0 时恒为匀速。
 */
uint8_t cmd_get_spin_mode(void);

/**
 * @brief 摩擦轮是否开启(请求侧,不是测速)
 * @return 1=开启;0=关闭
 */
uint8_t cmd_get_friction_on(void);

/**
 * @brief 最近一次发射档位(键鼠模式切换后的锁存值)
 * @return 1=单发 2=三发 3=连发
 * @note  单发/三发/连发由键鼠 R/E 切换,左键按当前模式发射;遥控路径不改变该值。
 */
uint8_t cmd_get_fire_mode(void);

/**
 * @brief 拨弹盘是否正在拨弹(有非 STOP 的发射请求)
 * @return 1=正在拨弹/有待发请求;0=停止
 */
uint8_t cmd_get_loader_on(void);

/**
 * @brief 当前战术档位对应的请求射速
 * @return 发/s(ECO=1.4 / NORMAL=6.0 / BURST=12.0)
 * @note  供裁判 UI 显示具体数值,数值来源仍是 config 的 SHOOT_RATE_*。
 */
float cmd_get_fire_rate(void);

#endif // !ROBOT_CMD_H
