/**
 * @file    chassis_spin.c
 * @brief   小陀螺抗自瞄轨迹:双尺度正弦调制(无反转)
 * @layer   APP
 * @author  小猫大王
 * @note    敌方自瞄的状态估计是 [角度, 角速度] 的匀速模型,反小陀螺靠锁旋转中心、
 *          按 T = 2*pi/omega 预测装甲板经过时刻。本文件只做一件事:让 omega(t) 在
 *          0.5~1.5s 的观测窗口内持续存在不可短窗辨识的角加速度(DEV-20261124-06)。
 * @note    2026-11-25(DEV-20261125-09):反转段实车超功率,BOOST/GLIDE/BRAKE 的
 *          "加速-滑行"节拍也抬高瞬时功率;整段状态机已移除,改为在巡航转速上叠加
 *          两路不可通约的正弦。输出恒正,功率峰值只由正弦幅值与 α 限幅决定。
 * @note    随机只作用于进入相位,不随机角速度 —— 后者会被底盘惯量与速度环滤掉,
 *          只留下电流冲击。
 * @note    2026-11-26(DEV-20261126-17):每次进入随机缩放两路幅度(0.7~1.0)与
 *          频率(±15%),并在旋内每 500ms 做一次相位随机游走。峰值仍受 MOD_LIMIT/
 *          HIGH 约束,只增加"不可短窗拟合"的随机性,不抬高功率。
 */
#include "chassis_spin.h"
#include "chassis_internal.h"

#include <math.h>

#define CHASSIS_SPIN_TWO_PI (2.0f * FW_PI)

/* ---------------- 调制状态(单写者=chassis_task) ---------------- */
static uint32_t s_rng;      // xorshift32 状态
static float s_time_ms;     // 进入 ROTATE 后的累计时间,ms(调制相位基准)
static float s_phase1;      // 调制相位1,rad
static float s_phase2;      // 调制相位2,rad
static float s_a1, s_a2;    // 本次进入的调制幅度(已乘随机缩放)
static float s_f1, s_f2;    // 本次进入的调制频率(已乘随机缩放)
static float s_phase_walk_ms; // 相位游走计时,ms
static float s_wz_out;      // 上一拍输出,deg/s(限加速度用)
static float s_alpha;       // 当前角加速度,deg/s^2(jerk 限幅状态,DEV-20261124-11)

/**
 * @brief  xorshift32:周期 2^32-1,无除法,适合 200Hz 任务
 * @note   种子由调用方混入进入时刻;状态为 0 时输出恒 0,reset 已兜底。
 */
static uint32_t spin_rand(void)
{
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}

/** @brief 返回 [0,1) 的定点随机数(取高 24 位,浮点精度足够) */
static float spin_rand_unit(void)
{
    return (float)(spin_rand() >> 8) * (1.0f / 16777215.0f);
}

/** @brief 返回 [min,max] 的随机浮点 */
static float spin_rand_range(float min, float max)
{
    return min + (max - min) * spin_rand_unit();
}

/**
 * @brief 带 jerk 限幅的速度跟踪
 * @param cur       当前输出,deg/s
 * @param target    目标角速度,deg/s
 * @param alpha_max 角加速度上限,deg/s^2
 * @param jerk_max  角加速度变化率上限,deg/s^3
 * @param dt_sec    控制周期,s
 * @return 下一拍输出,deg/s
 * @note  原来按 α 直接限幅,加速度在起步/到位时阶跃,扭矩冲击会激发车体
 *        roll/pitch 与 yaw 轴振动。这里让 α 以 jerk 爬升/卸掉,并用
 *        "当前 α 卸到 0 还会走多远"(a^2/(2*jerk))预测刹车点,避免过冲。
 *        夹到目标时不清 α,让 jerk 自然卸掉,避免加速度二次阶跃(DEV-20261124-11)。
 */
static float spin_track(float cur, float target, float alpha_max, float jerk_max, float dt_sec)
{
    /* jerk=0 会除零;退回旧恒 α 行为,便于 A/B */
    if (!(jerk_max > 0.0f))
    {
        const float step = alpha_max * dt_sec;
        const float delta = target - cur;
        if (delta > step)
            return cur + step;
        if (delta < -step)
            return cur - step;
        return target;
    }

    const float err = target - cur;
    float a_des;

    if (fabsf(err) < 2.0f)
    {
        a_des = 0.0f; // 目标附近不再给加速度,避免正负反复换向
    }
    else
    {
        const float dv_stop = (s_alpha * s_alpha) / (2.0f * jerk_max);
        const float v_pred = cur + ((s_alpha > 0.0f) ? dv_stop : -dv_stop);
        /* 预测点仍没越过目标 → 继续加速;会越过 → 现在开始卸 α */
        a_des = ((target - v_pred) * err > 0.0f) ? ((err > 0.0f) ? alpha_max : -alpha_max) : 0.0f;
    }

    float da = a_des - s_alpha;
    const float da_max = jerk_max * dt_sec;
    if (da > da_max)
        da = da_max;
    if (da < -da_max)
        da = -da_max;
    s_alpha += da;

    float next = cur + s_alpha * dt_sec;
    if ((err > 0.0f && next > target) || (err < 0.0f && next < target))
        next = target; // 不越过目标;α 由 next 拍的 a_des 继续按 jerk 卸掉

    return next;
}

/**
 * @brief 双尺度正弦调制项
 * @note  两个频率不可通约(0.31/0.83Hz),避免形成肉眼和算法都能看出的固定节拍;
 *        幅度限幅防止极端相位把转速压进自瞄可稳定跟踪的区间。
 */
static float spin_modulation(void)
{
    const float t = s_time_ms * 0.001f;
    float m = s_a1 * sinf(CHASSIS_SPIN_TWO_PI * s_f1 * t + s_phase1) +
              s_a2 * sinf(CHASSIS_SPIN_TWO_PI * s_f2 * t + s_phase2);
    if (m > CHASSIS_SPIN_MOD_LIMIT)
        m = CHASSIS_SPIN_MOD_LIMIT;
    if (m < -CHASSIS_SPIN_MOD_LIMIT)
        m = -CHASSIS_SPIN_MOD_LIMIT;
    return m;
}

void chassis_spin_reset(uint32_t seed, float wz_start)
{
    s_rng = (seed != 0u) ? seed : 0x9E3779B9u;
    s_time_ms = 0.0f;
    /* 每次进入重掷幅度/频率缩放与相位:固定幅度谱+固定相位会被预先拟合。
     * 幅度整体缩放(0.7~1.0)不抬高峰值;频率缩放(±15%)打散固定谱线。 */
    {
        const float a_scale =
            spin_rand_range(CHASSIS_SPIN_MOD_A_SCALE_MIN, CHASSIS_SPIN_MOD_A_SCALE_MAX);
        const float f_scale =
            spin_rand_range(CHASSIS_SPIN_MOD_F_SCALE_MIN, CHASSIS_SPIN_MOD_F_SCALE_MAX);
        s_a1 = CHASSIS_SPIN_MOD_A1 * a_scale;
        s_a2 = CHASSIS_SPIN_MOD_A2 * a_scale;
        s_f1 = CHASSIS_SPIN_MOD_F1_HZ * f_scale;
        s_f2 = CHASSIS_SPIN_MOD_F2_HZ * f_scale;
    }
    s_phase1 = spin_rand_unit() * CHASSIS_SPIN_TWO_PI;
    s_phase2 = spin_rand_unit() * CHASSIS_SPIN_TWO_PI;
    s_phase_walk_ms = 0.0f;
    s_alpha = 0.0f;
    /* 初值夹到轨迹可达范围;NaN 比较恒假,同样退回 0(避免污染整条速度环) */
    if (!(wz_start >= -CHASSIS_SPIN_HIGH_DPS && wz_start <= CHASSIS_SPIN_HIGH_DPS))
        wz_start = 0.0f;
    s_wz_out = wz_start;
}

float chassis_spin_update(float dt_ms)
{
    /* NaN 也要落到兜底:!(dt>0) 对 NaN 为真 */
    if (!(dt_ms > 0.0f) || dt_ms > 50.0f)
        dt_ms = CHASSIS_SPIN_NOMINAL_DT_MS;

    s_time_ms += dt_ms;

    /* 旋内相位随机游走:瞬时频率持续抖动,破坏对方 1~2s 窗口内的正弦拟合。
     * 只改相位,不改峰值/平均转速;步长受 α/jerk 限幅约束,功率不增。 */
    if (CHASSIS_SPIN_PHASE_WALK_MS > 0.0f)
    {
        s_phase_walk_ms += dt_ms;
        while (s_phase_walk_ms >= CHASSIS_SPIN_PHASE_WALK_MS)
        {
            s_phase_walk_ms -= CHASSIS_SPIN_PHASE_WALK_MS;
            s_phase1 += spin_rand_range(-CHASSIS_SPIN_PHASE_WALK_STEP, CHASSIS_SPIN_PHASE_WALK_STEP);
            s_phase2 += spin_rand_range(-CHASSIS_SPIN_PHASE_WALK_STEP, CHASSIS_SPIN_PHASE_WALK_STEP);
            /* 周期折叠,避免长时间运行后 float 精度下降 */
            s_phase1 = fmodf(s_phase1, CHASSIS_SPIN_TWO_PI);
            s_phase2 = fmodf(s_phase2, CHASSIS_SPIN_TWO_PI);
        }
    }

    /* 恒正轨迹:基础巡航转速 × (1 + 调制),再按上下限兜底。
     * 不再有反转/加速/减速段,功率峰值只随正弦幅值缓慢变化。 */
    float target = CHASSIS_SPIN_CRUISE_DPS * (1.0f + spin_modulation());
    if (target > CHASSIS_SPIN_HIGH_DPS)
        target = CHASSIS_SPIN_HIGH_DPS;
    if (target < CHASSIS_SPIN_LOW_DPS)
        target = CHASSIS_SPIN_LOW_DPS;

    s_wz_out = spin_track(s_wz_out, target, CHASSIS_SPIN_ALPHA_MAX, CHASSIS_SPIN_JERK_MAX,
                          dt_ms * 0.001f);
    return s_wz_out;
}
