/**
 * @file    chassis_spin.h
 * @brief   小陀螺抗自瞄轨迹接口:双尺度正弦调制(无反转)
 * @layer   APP
 * @author  小猫大王
 * @note    只在 CHASSIS_ROTATE 变速档调用;输出由 chassis_task 写入 chassis_cmd_recv.wz。
 * @note    2026-11-25(DEV-20261125-09):反转段实车超功率,移除 REVERSE/BOOST/GLIDE/BRAKE
 *          状态机,改为恒正的双尺度正弦调制;输出不过零。
 *          2026-11-26(DEV-20261126-25):输出上下限随中心同比例缩放,平移占轮速时
 *          整体下移而不是削顶;满中心时仍为 [CHASSIS_SPIN_LOW_DPS, CHASSIS_SPIN_HIGH_DPS]。
 *          设计依据、开源对照与调参判据见 .Doc/robots/infantry/plans/小陀螺抗自瞄改造计划.md。
 */
#ifndef CHASSIS_SPIN_H
#define CHASSIS_SPIN_H

#include <stdint.h>

/**
 * @brief  重置调制相位与随机源
 * @param  seed     随机种子;传 0 时使用固定非零兜底值(仅台架复现用)
 * @param  wz_start 起始输出,deg/s;传底盘实测角速度,使匀速↔变速切换时速度连续
 * @note   进入 CHASSIS_ROTATE 变速档的上升沿调用。种子应混入上电/进入时刻,
 *         固定种子会让调制相位序列可被对手录制复现。
 */
void chassis_spin_reset(uint32_t seed, float wz_start);

/**
 * @brief  平移→纯自转交接:从指定端点相位重启纯自转正弦
 * @param  seed     随机种子;传 0 用固定非零兜底
 * @param  wz_start 起始输出,deg/s;调用方已把它保持在 180 或 350
 * @param  at_high  1=从高端点(350)开始下降;0=从低端点(180)开始上升
 * @note   两路正弦相位都置 ±π/2、幅度缩放固定 1.0,使 t=0 的调制正好落在端点、
 *         导数为 0(先稳一下再自然分开)。保持段的计时由调用方完成。
 */
void chassis_spin_handoff(uint32_t seed, float wz_start, uint8_t at_high);

/**
 * @brief  推进一步小陀螺轨迹
 * @param  dt_ms     距上一拍的毫秒数;<=0 或 >50ms 时按 5ms 兜底
 * @param  center_dps 本拍轨迹中心转速,deg/s;调用方按平移占用的轮速余量给出,
 *                    内部夹到 [CHASSIS_SPIN_CENTER_MIN_DPS, CHASSIS_SPIN_CRUISE_DPS]
 * @param  mod_limit  本拍调制限幅(0~1);纯自转用 CHASSIS_SPIN_MOD_LIMIT(0.32),
 *                    平移段由调用方按 TRANS_LOW/HIGH_DPS 算出的 center/mod;
 *                    上下限 = center×(1±mod_limit),正弦按比例放大,不削顶
 * @return 本拍输出,deg/s;已按 CHASSIS_SPIN_ALPHA_MAX/JERK_MAX 限加速度
 */
float chassis_spin_update(float dt_ms, float center_dps, float mod_limit);

#endif // CHASSIS_SPIN_H
