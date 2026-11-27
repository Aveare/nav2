/**
 * @file    chassis_power.h
 * @brief   底盘功率控制:电机模型预测 → 电流指令衰减系数(二次方程精确解)
 * @layer   APP
 * @author  小猫大王
 * @note    2026 协议删除 0x0202 的 chassis_power 后改为模型预测;
 *          系数形式与天工大 304 PowerManager(MIT)一致,控制律参考其 limiter():
 *          用**指令电流**和**反馈转速**预测,超限时解二次方程求电流衰减系数。
 */
#ifndef CHASSIS_POWER_H
#define CHASSIS_POWER_H

#include <stdint.h>

/**
 * @brief 复位衰减系数低通状态到 1.0
 * @note  初始化/重新使能时调用,避免上一局限幅残留。
 */
void chassis_power_reset(void);

/**
 * @brief 计算电流指令衰减系数
 * @param current_cmd 四轮未限幅电流指令(C620 原始码,乘 output_scale 之前)
 * @param w_dps       四轮转子转速,deg/s
 * @param limit_w     本拍有效功率上限,W(已含余量与缓冲衰减)
 * @param min_scale   输出下限(如 0.20),保证受限时仍有电流权限;夹到 [0,1]
 * @param p_pred_out  可选:输出 scale=1 时的总预测功率,W;不需要时传 0
 * @param p_post_out  可选:输出乘上本拍 scale 后的模型功率,W;不需要时传 0
 * @return 0~1,乘到电机输出电流(同时按比例压低速度环积分限幅)
 * @note  模型取电流/转速的绝对值(304 默认口径),但**只统计电动轮**:
 *        i 与 ω 反号的制动轮跳过(回馈功率为负,不占电池预算,DEV-20261127-200745)。
 * @note  超限时解 `S2*s^2 + S1*s + (S0 - limit) = 0`,取 [0,1] 内较大根:
 *        S0 = Σ(K0 + K2*w + K5*w^2),S1 = Σ(K1 + K3*w)*|i|,S2 = Σ K4*i^2。
 *        求和范围 = 电动轮子集;全轮制动时 S0=S1=S2=0,返回 1.0。
 */
float chassis_power_scale(const float current_cmd[4], const float w_dps[4], float limit_w,
                          float min_scale, float *p_pred_out, float *p_post_out);

#endif // CHASSIS_POWER_H
