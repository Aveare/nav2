/**
 * @file    gimbal_alloc.h
 * @brief   哨兵双 yaw 控制分配纯函数(无硬件依赖,可主机单测)
 * @layer   APP
 * @author  小猫大王
 * @note    分配律:小 yaw 先吃掉世界误差中它力所能及的部分,大 yaw 只承担
 *          小 yaw 饱和后的剩余误差。小误差只动小 yaw,大角度转火才动大 yaw。
 */
#ifndef SENTRY_GIMBAL_ALLOC_H
#define SENTRY_GIMBAL_ALLOC_H

/**
 * @brief 角度折回 [-180, 180],deg
 */
static inline float sentry_gimbal_wrap180(float deg)
{
    while (deg > 180.0f)
        deg -= 360.0f;
    while (deg < -180.0f)
        deg += 360.0f;
    return deg;
}

/**
 * @brief 双 yaw 目标分配
 * @param target_world 世界 yaw 目标,deg
 * @param big_world    大 yaw 当前世界角,deg
 * @param small_limit  小 yaw 相对角可用范围(已扣限位余量),deg
 * @param small_target 输出:小 yaw 相对目标,deg
 * @param big_target   输出:大 yaw 世界目标,deg(已展开到 big_world 最近的整圈分支)
 * @note 纯函数,不读写全局状态;gimbal_task 每周期调用一次。
 */
static inline void sentry_gimbal_allocate(float target_world, float big_world, float small_limit,
                                          float *small_target, float *big_target)
{
    float err = sentry_gimbal_wrap180(target_world - big_world);
    if (err > small_limit)
        err = small_limit;
    else if (err < -small_limit)
        err = -small_limit;
    *small_target = err;
    /* 目标角展开到当前大 yaw 最近的整圈分支:避免跨越 ±180 时目标跳到另一圈,
     * 也保证输出对上层是可连续观察的。 */
    *big_target = big_world + sentry_gimbal_wrap180((target_world - err) - big_world);
}

/**
 * @brief 用调用方给定的小 yaw 目标(慢配平)反推大 yaw 世界目标
 * @param target_world 世界 yaw 目标,deg
 * @param big_world    大 yaw 当前世界角,deg
 * @param small_cmd    小 yaw 期望相对角,deg;内部夹到 ±small_limit
 * @param small_limit  小 yaw 可用范围,deg
 * @param small_target 输出:夹后的小 yaw 目标,deg
 * @param big_target   输出:大 yaw 世界目标,deg
 * @note 大 yaw 负责快速世界环,小 yaw 只做慢配平:两者目标之和恒等于世界目标,
 *       不会出现"两轴追同一误差"。
 */
static inline void sentry_gimbal_split(float target_world, float big_world, float small_cmd,
                                       float small_limit, float *small_target, float *big_target)
{
    const float small = (small_cmd > small_limit) ? small_limit
                        : (small_cmd < -small_limit) ? -small_limit
                                                     : small_cmd;
    *small_target = small;
    *big_target = big_world + sentry_gimbal_wrap180((target_world - small) - big_world);
}

#endif // SENTRY_GIMBAL_ALLOC_H
