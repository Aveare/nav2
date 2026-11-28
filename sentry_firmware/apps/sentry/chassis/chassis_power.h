/**
 * @file    chassis_power.h
 * @brief   哨兵底盘功率控制:电机模型预测 → 电流指令衰减系数
 * @layer   APP
 * @author  小猫大王
 * @note    与步兵 chassis_power 同源(304 PowerManager 口径);参数在 config/sentry_2026。
 *          本文件无硬件依赖,只做纯计算,可主机单测。
 */
#ifndef SENTRY_CHASSIS_POWER_H
#define SENTRY_CHASSIS_POWER_H

#include <stdint.h>

/** @brief 复位衰减系数低通状态到 1.0(初始化/重新使能时调用)。 */
void chassis_power_reset(void);

/**
 * @brief 计算电流指令衰减系数
 * @param current_cmd 四轮未限幅电流指令(C620 原始码,乘 output_scale 之前)
 * @param w_dps       四轮转子转速,deg/s
 * @param limit_w     本拍有效功率上限,W
 * @param min_scale   输出下限;夹到 [0,1]
 * @param p_pred_out  可选:scale=1 时的总预测功率,W;不需要传 0
 * @param p_post_out  可选:乘上本拍 scale 后的模型功率,W;不需要传 0
 * @return 0~1,乘到电机输出电流并同步压低速度环积分限幅
 * @note  模型取电流/转速的绝对值(304 口径),但**只统计电动轮**:
 *        i 与 ω 反号的制动轮跳过(回馈功率为负,不占电池预算,步兵 DEV-20261127-200745);
 *        配套 chassis_limit_output 对制动轮保持 output_scale=1。全轮制动时返回 1.0。
 */
float chassis_power_scale(const float current_cmd[4], const float w_dps[4], float limit_w,
                          float min_scale, float *p_pred_out, float *p_post_out);

#endif // SENTRY_CHASSIS_POWER_H
