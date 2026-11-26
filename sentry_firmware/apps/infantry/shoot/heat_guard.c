/**
 * @file    heat_guard.c
 * @brief   枪口热量守卫:估计器+前馈射速的两级闸门
 * @layer   APP
 * @author  小猫大王
* @note    估计器:200Hz 本地推算 + 10Hz 裁判硬校正。
*          推算 = 放行时 +10 点 / 冷却按 100ms 阶梯扣 (c/10) 点;
*          校正 = 裁判值更高立刻跟(向上瞬时),更低需连续 3 帧(300ms)才跟
*          (向下错是本设计唯一会往危险方向偏的误差,必须用时间换安全)。
* @note    射速不是"热量高了再刹车"的反馈环 —— 该对象死区 100~200ms、在途弹
*          1~2 发,反馈必然过冲。这里是把裁判热量拿来校模型,再由模型直接算射速。
* @note    2026-11-19 建立。算法原型来自同济/中科大公开代码的对照分析,
*          余量 30 点与 r_sustain=c/10 的取法在中科大 2024 全向步兵
*          crt_booster.cpp 中有同构实现,但本实现补了估计器、滞回与断流降级。
 */
#include "heat_guard.h"
#include "config.h"
#include "referee_task.h"
#include "dwt_port.h"
#include "log_port.h"

#include <math.h>

/* 内部常量:只在换算处使用,可调项一律在 config 中 */
#define HEAT_TICK_MS   100.0f // 裁判热量结算周期,ms(规则 10Hz)
#define HEAT_TICK_STEP 0.1f   // 每个结算周期的秒数,s
#define HEAT_LIMIT_MIN 1.0f   // 判定 0x0201 有效的热量上限下限,热量点
#define HEAT_COOL_MIN  1.0f   // 判定 0x0201 有效的冷却速率下限,点/s
#define HEAT_RATE_EPS  0.05f  // 低于此射速视为禁止,发/s
#define HEAT_UP_TOL    0.5f   // 向上校正死区,避免同值反复赋值,热量点

static heat_guard_state_e guard_state = HEAT_GUARD_OFF;

static float heat_q = 0.0f;     // Q̂,热量点
static float heat_limit = 0.0f; // Q0,热量上限,热量点(0x0201 每帧刷新)
static float cool_per_s = 0.0f; // 冷却速率,热量点/s(基础值 + 冷却增益)

static float cool_acc_ms = 0.0f;    // 冷却阶梯累加器,ms
static float last_upd_ms = 0.0f;    // 上周期时刻,ms
static float last_rx_ms = 0.0f;     // 最近一次收到 0x0202 的时刻,ms
static float last_single_ms = 0.0f; // 最近一次放行单发的时刻,ms

static uint32_t last_heat_tick = 0;   // 最近处理的 0x0202 帧号
static uint8_t down_cnt = 0;          // 连续"裁判值偏低"的帧数
static uint8_t heat_ready = 0;        // 是否已取得过有效的 Q0
static uint8_t degraded = 0;          // 1 = 曾上线但断流
static uint8_t warn_off_done = 0;     // 直通告警只打一次
static uint8_t allowance_blocked = 0; // 1 = 2026 允许发弹量耗尽(0x0208),硬禁发(DEV-20261121-05)

#if HEAT_GUARD_LOG
static float log_last_ms = 0.0f;
#endif

/* 余量按 Q0 的固定比例封顶:低等级车 Q0 很小(如 50 点)时,固定 30 点余量
 * 会把枪直接封死,此时按比例收窄,宁可少守一点也不锁枪。
 * 闸门阈值与射速律必须用同一个封顶结果,否则"切单发"和"降射速"两条线会打架。 */
static float heat_margin_burst(void)
{
    float cap = heat_limit * HEAT_GUARD_MARGIN_MAX_RATIO;
    return (HEAT_GUARD_MARGIN_BURST > cap) ? cap : HEAT_GUARD_MARGIN_BURST;
}

static float heat_margin_single(void)
{
    float cap = heat_limit * HEAT_GUARD_MARGIN_MAX_RATIO;
    float m_burst = heat_margin_burst();
    float m_single = (HEAT_GUARD_MARGIN_SINGLE > cap) ? cap : HEAT_GUARD_MARGIN_SINGLE;
    return (m_single > m_burst) ? m_burst : m_single; // 单发余量不得大于连发,否则永远进不去 SINGLE
}

/**
 * @brief 由 Q0 与配置余量算出两级闸门阈值
 * @param gate_single 输出:切单发的热量阈值,热量点
 * @param gate_hold   输出:禁发的热量阈值,热量点
 */
static void heat_calc_gates(float *gate_single, float *gate_hold)
{
    *gate_hold = heat_limit - heat_margin_single();
    *gate_single = heat_limit - heat_margin_burst();
}

/* 单发最小间隔:按"两发平衡间隔"给(10/c 秒是一发的平衡间隔,乘倍数即留恢复余量) */
static float heat_single_gap_ms(void)
{
    if (cool_per_s < HEAT_COOL_MIN)
        return 0.0f;
    return HEAT_GUARD_SINGLE_GAP_BULLETS * HEAT_GUARD_HEAT_PER_BULLET / cool_per_s * 1000.0f;
}

#if !LOG_DISABLE_SYSTEM
static const char *heat_state_name(heat_guard_state_e s)
{
    switch (s)
    {
    case HEAT_GUARD_OFF:
        return "OFF";
    case HEAT_GUARD_NORMAL:
        return "NORMAL";
    case HEAT_GUARD_SINGLE:
        return "SINGLE";
    case HEAT_GUARD_HOLD:
        return "HOLD";
    default:
        return "?";
    }
}
#endif // !LOG_DISABLE_SYSTEM

void heat_guard_init(void)
{
    guard_state = HEAT_GUARD_OFF;
    heat_q = 0.0f;
    heat_limit = 0.0f;
    cool_per_s = 0.0f;
    cool_acc_ms = 0.0f;
    last_upd_ms = dwt_get_timeline_ms();
    last_rx_ms = 0.0f;
    last_single_ms = 0.0f;
    last_heat_tick = 0;
    down_cnt = 0;
    heat_ready = 0;
    degraded = 0;
    warn_off_done = 0;
    allowance_blocked = 0;
#if HEAT_GUARD_LOG
    log_last_ms = 0.0f;
#endif
}

void heat_guard_update(void)
{
    float now = dwt_get_timeline_ms();
    float dt = now - last_upd_ms;
    last_upd_ms = now;
    if (dt < 0.0f || dt > 100.0f)
        dt = 0.0f;

    /* 冷却必须按裁判的 100ms 阶梯扣,不能用连续 dt 积分:
     * 裁判是每周期一次性减 c/10,连续衰减会在每个周期开头系统性低估最多 c/10。
     * 这是本模块唯一会往危险方向偏的建模误差,必须结构上消除。 */
    cool_acc_ms += dt;
    while (cool_acc_ms >= HEAT_TICK_MS)
    {
        cool_acc_ms -= HEAT_TICK_MS;
        heat_q -= cool_per_s * HEAT_TICK_STEP;
    }
    if (heat_q < 0.0f)
        heat_q = 0.0f;

    /* 关掉守卫时不做任何估计(连裁判系统都不读),但代码保持编译,便于随时改回 */
    if (!HEAT_GUARD_ENABLE)
    {
        guard_state = HEAT_GUARD_OFF;
        degraded = 0;
        return;
    }

    referee_info_t *ref = referee_get_info();
    uint8_t ref_alive = 0;

    if (ref != NULL)
    {
        /* 用帧号变化判"收到新帧",不能只比数值:热量在两帧之间可能一字不变,
         * 拿陈旧值做向下校正会每 5ms 误判一次。 */
        if (ref->PowerHeatTick != last_heat_tick)
        {
            last_heat_tick = ref->PowerHeatTick;
            last_rx_ms = now;

            /* Q0 与 c 每帧重取:升级瞬间上限会跳变,缓存就会算错阈值 */
            float q0 = (float)ref->GameRobotState.shooter_barrel_heat_limit;
            float c = (float)ref->GameRobotState.shooter_barrel_cooling_value;
#if HEAT_GUARD_COOLING_USE_BUFF
            c += (float)ref->BuffMusk.cooling_buff; // 冷却增益为直接值(点/s),不是百分比
#endif
            if (q0 > HEAT_LIMIT_MIN)
                heat_limit = q0;
            if (c > HEAT_COOL_MIN)
                cool_per_s = c;

            if (heat_limit > HEAT_LIMIT_MIN)
            {
                float qr = (float)ref->PowerHeatData.shooter_17mm_barrel_heat;

                if (!heat_ready)
                {
                    /* 首次取得有效上限:直接采用裁判值,不做推算起点 */
                    heat_ready = 1;
                    heat_q = qr;
                    down_cnt = 0;
                }
                else if (qr > heat_q + HEAT_UP_TOL)
                {
                    /* 向上瞬时跟随:我们漏记了,或者裁判多记了,两者都必须立刻采纳 */
                    heat_q = qr;
                    down_cnt = 0;
                }
                else if (qr < heat_q - HEAT_GUARD_DOWN_TOL)
                {
                    /* 向下要连续多帧:单帧偏低可能只是帧延迟(裁判值比我们落后一拍),
                     * 立刻跟随等于把在途弹的热量抹掉,是锁枪的直接成因。 */
                    down_cnt++;
                    if (down_cnt >= HEAT_GUARD_DOWN_FRAMES)
                    {
                        heat_q = qr;
                        down_cnt = 0;
                    }
                }
                else
                {
                    down_cnt = 0;
                }
            }
        }

        if (last_rx_ms > 0.0f && (now - last_rx_ms) <= HEAT_GUARD_REF_TIMEOUT_MS)
            ref_alive = 1;
    }

    /* 从未取得过有效 Q0 = 没有裁判系统(台架/未上线),直通 */
    if (!heat_ready)
    {
        if (!warn_off_done)
        {
            LOG_WARN("[heat] no referee heat data, guard bypassed (bench mode)");
            warn_off_done = 1;
        }
        guard_state = HEAT_GUARD_OFF;
        degraded = 0;
        return;
    }

    /* 曾上线但断流:不放弃估计,只关掉"花余热预算"的权限,射速压到散热速率。
     * 断流期间若冷却/上限字段本身还没拿到(极早期断流),用配置兜底值,
     * 否则 r_sustain 会算成 0,把枪直接封死 —— 那是比不守更糟的失效模式。 */
    degraded = (ref_alive == 0) ? 1 : 0;
    if (degraded)
    {
        if (cool_per_s < HEAT_COOL_MIN)
            cool_per_s = HEAT_GUARD_DEFAULT_COOLING;
        if (heat_limit < HEAT_LIMIT_MIN)
            heat_limit = HEAT_GUARD_DEFAULT_HEAT_LIMIT;
    }

    /* 2026 允许发弹量(0x0208)硬闸门(DEV-20261121-05):额度 <1 发即禁发,优先级高于
     * 热量模型 —— 额度归零后裁判直接锁发射机构(断电),热量算得再好看也发不出去。
     * 闸门做成独立标志而非改 guard_state:改状态会和热量闸门互相覆盖导致状态抖动。
     * 台架/未开赛额度恒为 0,由 HEAT_GUARD_ALLOWANCE_ENABLE 开关控制是否启用。 */
    if (HEAT_GUARD_ALLOWANCE_ENABLE)
    {
        uint8_t blocked_now =
            (ref != NULL && ref->AllowanceData.projectile_allowance_17mm < 1) ? 1 : 0;
        if (blocked_now != allowance_blocked)
        {
            allowance_blocked = blocked_now;
            LOG_WARN("[heat] allowance %s (17mm=%d)",
                       blocked_now ? "EXHAUSTED, hold fire" : "recovered",
                       (ref != NULL) ? (int)ref->AllowanceData.projectile_allowance_17mm : 0);
        }
    }
    else
        allowance_blocked = 0;

    /* 状态迁移:进入用当前阈值,退出用阈值 - 回差,构成滞回带 */
    float gate_single, gate_hold;
    heat_guard_state_e prev = guard_state;

    heat_calc_gates(&gate_single, &gate_hold);

    switch (guard_state)
    {
    case HEAT_GUARD_HOLD:
        if (heat_q < gate_hold - HEAT_GUARD_HYST)
            guard_state =
                (heat_q >= gate_single - HEAT_GUARD_HYST) ? HEAT_GUARD_SINGLE : HEAT_GUARD_NORMAL;
        break;
    case HEAT_GUARD_SINGLE:
        if (heat_q >= gate_hold)
            guard_state = HEAT_GUARD_HOLD;
        else if (heat_q < gate_single - HEAT_GUARD_HYST)
            guard_state = HEAT_GUARD_NORMAL;
        break;
    case HEAT_GUARD_NORMAL:
    case HEAT_GUARD_OFF:
    default:
        if (heat_q >= gate_hold)
            guard_state = HEAT_GUARD_HOLD;
        else if (heat_q >= gate_single)
            guard_state = HEAT_GUARD_SINGLE;
        else
            guard_state = HEAT_GUARD_NORMAL;
        break;
    }

    /* 闸门动作必须让操作手看得见,否则会被当成卡弹,越按越糟。
     * DEV-20261121-10:**RTT 的 printf 不支持 %f** —— 用了 %.0f 时该占位符不消费参数,
     * 后面的 %s(含 LOG_PROTO 自动追加的结尾 %s)会把浮点当 char* 读,实车即 HardFault
     * (BFAR=0x40240000 = heat_q 2.5625 的位模式)。故一律用整型 + %d 打印。 */
    if (guard_state != prev)
        LOG_WARN("[heat] %s -> %s (q=%d/%d c=%d/s)", heat_state_name(prev), heat_state_name(guard_state),
                   (int)heat_q, (int)heat_limit, (int)cool_per_s);

#if HEAT_GUARD_LOG
    if (now - log_last_ms >= 1000.0f)
    {
        log_last_ms = now;
        LOG_INFO("[heat] q=%d/%d c=%d/s state=%s%s", (int)heat_q, (int)heat_limit, (int)cool_per_s,
                heat_state_name(guard_state), degraded ? " (degraded)" : "");
    }
#endif
}

uint8_t heat_guard_allow_burst(float demand, float *rate)
{
    if (rate == NULL)
        return 0;

    if (!HEAT_GUARD_ENABLE)
    {
        *rate = demand;
        return (demand > HEAT_RATE_EPS) ? 1 : 0;
    }

    /* 未上线视为无热量约束:裁判就是判罚方,没有裁判就没有热量 */
    if (guard_state == HEAT_GUARD_OFF)
    {
        *rate = demand;
        return (demand > HEAT_RATE_EPS) ? 1 : 0;
    }

    /* 发弹额度硬闸门(2026):额度耗尽即禁发,与热量状态无关(DEV-20261121-05) */
    if (allowance_blocked)
    {
        *rate = 0.0f;
        return 0;
    }

    if (guard_state == HEAT_GUARD_SINGLE || guard_state == HEAT_GUARD_HOLD)
    {
        *rate = 0.0f;
        return 0;
    }

    /* 射速律(两种模式,config 宏 HEAT_GUARD_RATE_MODE 选择):
     * ① 指数衰减:稳态点 = 散热速率,余热按 SPEND_TIME_S 摊开 —— 前快后慢,数学上最贪心;
     * ② 恒定摊平(参考同济 RM2024 公开策略):把可用热量在固定窗口内**均摊**成恒定射速,
     *    手感可预期 —— 操作手按"这个射速能打多久"来规划,而不是感受一个逐渐变慢的曲线。
     * 两式的稳态项都是 r_sustain(收支平衡),区别只在余热怎么花。 */
    float r_sustain = cool_per_s / HEAT_GUARD_HEAT_PER_BULLET;
    float budget = (heat_limit - heat_margin_burst()) - heat_q;
    if (budget < 0.0f)
        budget = 0.0f;

#if HEAT_GUARD_RATE_MODE
    /* 恒定摊平:budget 点热量 / (每发点数 × 窗口秒数) + 散热补充 */
    float r = r_sustain + budget / (HEAT_GUARD_HEAT_PER_BULLET * HEAT_GUARD_FLAT_WINDOW_S);
#else
    /* 指数衰减 */
    float r = r_sustain + budget / (HEAT_GUARD_HEAT_PER_BULLET * HEAT_GUARD_SPEND_TIME_S);
#endif

    if (degraded)
        r = r_sustain; // 断流:丢掉花预算的权限,只保留收支平衡

    if (r > demand)
        r = demand;
    if (r < 0.0f)
        r = 0.0f;

    *rate = r;
    return (r > HEAT_RATE_EPS) ? 1 : 0;
}

uint8_t heat_guard_allow_single(void)
{
    float now = dwt_get_timeline_ms();

    if (!HEAT_GUARD_ENABLE)
    {
        last_single_ms = now;
        return 1;
    }

    if (guard_state == HEAT_GUARD_OFF)
        return 1;

    /* 发弹额度硬闸门(2026):额度耗尽连单发也禁,裁判侧会直接锁机构(DEV-20261121-05) */
    if (allowance_blocked)
        return 0;

    if (guard_state == HEAT_GUARD_HOLD)
        return 0;

    /* 单发模式下连点也会涨热:间隔短于 10/c 时净升温,必须强制最小间隔。
     * 断流时同样收紧,因为此时没有任何外部真值可以证明我们还安全。 */
    if (guard_state == HEAT_GUARD_SINGLE || degraded)
    {
        float gap = heat_single_gap_ms();
        if (last_single_ms > 0.0f && (now - last_single_ms) < gap)
            return 0;
    }

    last_single_ms = now;
    heat_guard_commit_shot(); // 放行即记账:这一刻已经必然要付出这一发的热量
    return 1;
}

void heat_guard_commit_shot(void)
{
    heat_q += HEAT_GUARD_HEAT_PER_BULLET;
}

heat_guard_state_e heat_guard_get_state(void)
{
    return guard_state;
}

float heat_guard_get_heat(void)
{
    return heat_q;
}

void heat_guard_get_snapshot(float *q, float *limit, float *cool)
{
    if (q != NULL)
        *q = heat_q;
    if (limit != NULL)
        *limit = heat_limit;
    if (cool != NULL)
        *cool = cool_per_s;
}

uint8_t heat_guard_is_degraded(void)
{
    return degraded;
}
