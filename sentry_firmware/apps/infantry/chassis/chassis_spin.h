/**
 * @file    chassis_spin.h
 * @brief   小陀螺抗自瞄轨迹接口:双尺度正弦调制(无反转)
 * @layer   APP
 * @author  小猫大王
 * @note    只在 CHASSIS_ROTATE 变速档调用;输出由 chassis_task 写入 chassis_cmd_recv.wz。
 * @note    2026-11-25(DEV-20261125-09):反转段实车超功率,移除 REVERSE/BOOST/GLIDE/BRAKE
 *          状态机,改为恒正的双尺度正弦调制;输出始终在 [CHASSIS_SPIN_LOW_DPS,
 *          CHASSIS_SPIN_HIGH_DPS] 内,不再过零。
 *          设计依据、开源对照与调参判据见 .Doc/plan/小陀螺抗自瞄改造计划.md。
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
 * @brief  推进一步小陀螺轨迹
 * @param  dt_ms 距上一拍的毫秒数;<=0 或 >50ms 时按 5ms 兜底
 * @return 本拍输出,deg/s;已按 CHASSIS_SPIN_ALPHA_MAX/JERK_MAX 限加速度
 */
float chassis_spin_update(float dt_ms);

#endif // CHASSIS_SPIN_H
