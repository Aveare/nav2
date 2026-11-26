/**
 * @file    shoot.c
 * @brief   发射应用:摩擦轮/拨盘、单发三发状态机与互锁
 * @layer   APP
 * @author  小猫大王
* @note    连发:速度环,射速**不直接取 cmd**,而是向 heat_guard 申请 —— 守卫按余热预算
*          给出允许射速,逼近闸门时收敛到散热速率,单发区/禁发区返回 0。
* @note    单发/三发:"待发弹数"状态机,逐发角度环推进,到位容差 / 超时 / 堵转三重保护;
*          每发起步前还要过热量闸门,三发在单发区会被降级为一发。
*          一发角 = 输出轴 360 / 槽数(config SHOOT_ONE_BULLET_DELTA_ANGLE),需实车标定。
* @note    2026-11-24(DEV-20261124-04):loader 的 close_loop_type 补上 ANGLE_LOOP 位,
*          角度环 PID 由 config 提供(原为全零 = 角度环整段短路);每发到位立即切回速度环
*          并清 PID,堵住"单发 2~3 连 / 三发 4~5 连"的过冲。两个回退宏见 config_shoot.h。
* @note    热量记账点:单发在"放行"瞬间(heat_guard 内部记账),连发在拨盘转过一发角时
*          (本文件槽沿计数)。两者都不等裁判确认 —— 裁判 ΔQ 迟到 100~200ms。
* @note    反转(退卡弹)为占位,当前实现为停止。
* @note    2026-10-29 建立,作者 JIT CR。
 */
#include "shoot.h"
#include "robot_def.h"

#include "dji_motor.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "dwt_port.h"
#include "log_port.h"
#include "general_def.h"
#include "telemetry.h"  // P1-B3 需要:摩擦轮转速高频采样(0.5s 日志看不到纹波)
#include "heat_guard.h" // 枪口热量守卫:放行/射速的唯一决策点(见 heat_guard.c 文件头)
#include <math.h>       // isfinite:射速/角度目标的非法值护栏(DEV-20261121-07)

/* 发射标定值统一在 config/infantry_2026/config.h 维护,本文件只负责换算与互锁判断 */

/* 摩擦轮实际目标 = 名义目标 + 标定微调(P2 标定后填 SHOOT_FRICTION_RPM_OFFSET)。
 * **目标下发、互锁判据、遥测三处必须同源**:若只有 SetRef 加 OFFSET 而判据仍用名义值,
 * 标定后互锁会按错的转速判到速,单发/连发全乱。 */
#define SHOOT_FRICTION_TARGET_RPM_EFF (SHOOT_FRICTION_TARGET_RPM + SHOOT_FRICTION_RPM_OFFSET)

static dji_motor_t *friction_l, *friction_r, *loader; // 摩擦轮左右+拨盘
// static servo_instance *lid; // 弹舱盖舵机,接入后在此注册

static msg_pub_t shoot_pub;
static shoot_ctrl_cmd_t shoot_cmd_recv; // 来自cmd的发射控制信息
static msg_sub_t shoot_sub;
static cmd_watchdog_t shoot_cmd_wd;              // 命令断流看门狗
static shoot_upload_t shoot_fb; // 回传cmd的发射反馈

static uint8_t single_shot_warned =
    0; // 单发/三发未标定警告只提示一次(位置环已启用,标定由config参数承担,保留作首次观测提示)

/* 拨盘堵转保护状态(仅本文件内使用):判据与阈值见config的LOADER_JAM_*;
 * 只停拨盘、不反转、不自动重试,拨轮回中(LOADER_STOP且无待发弹)才解除锁存 */
static uint8_t loader_jammed = 0;  // 1=已判堵转并锁存
static float loader_jam_ms = 0.0f; // 堵转累计时间

/**
 * @brief  拨盘堵转判据单点实现(连发/单发共用,RM-008)
 * @param  loader 拨盘电机实例(只读)
 * @param  dt_ms 本周期时长,ms(调用方已保证非负)
 * @return 1=本次触发锁存沿(内部累计已清零);0=未触发
 * @note   判据:被要求转动但转速低于 LOADER_JAM_SPEED_RPM 且电流达到
 *         LOADER_JAM_CURRENT,持续超过 LOADER_JAM_HOLD_MS 则沿触发。
 *         "转起来就清零"内聚于此 —— 两调用点各自维护一份曾是 RM-008 的
 *         分叉隐患,抽取后判据再也不会单点漂移。
 * @note   锁存后的反应由调用点决定(连发保持锁存等回中;单发丢队列),
 *         语义差异是有意的,不内聚。
 */
static uint8_t shoot_loader_jam_tick(const dji_motor_t *loader, float dt_ms)
{
    if (fabsf(loader->measure.speed_aps / FW_RPM_TO_DEG_S) < LOADER_JAM_SPEED_RPM &&
        fabsf((float)loader->measure.real_current) >= LOADER_JAM_CURRENT)
        loader_jam_ms += dt_ms;
    else
        loader_jam_ms = 0.0f; // 转起来就清零
    if (loader_jam_ms >= LOADER_JAM_HOLD_MS)
    {
        loader_jam_ms = 0.0f;
        return 1;
    }
    return 0;
}
static float shoot_last_ms = 0.0f; // 上周期时刻
#if SHOOT_CALIB_LOG
static uint16_t shoot_calib_cnt; // 标定日志分频计数(200Hz / 100 = 0.5s)
#endif

/* 单发/三发"待发弹数"状态机:cmd持续发布load_mode,本任务按模式切换沿登记弹数,
 * 逐发切角度环推进;每发起步前置=摩擦轮双轮在线且在速(与连发共用互锁判据),
 * 到位(容差)或超时(config LOADER_SINGLE_TIMEOUT_MS)结束本发;堵转中止整个队列。
 * 队列有三条终止路径:到位发完、堵转/超时、摩擦轮关闭或等待超时(避免悬挂后误射)。 */
static int8_t loader_pending = 0;          // 待发弹数(0=空闲)
static uint8_t loader_step_active = 0;     // 本发已起步(目标角已设定,未到位)
static float loader_step_target = 0.0f;    // 本发目标总角(电机轴deg)
static float loader_step_ms = 0.0f;        // 本发起步时刻(超时判定)
static float loader_wait_ms = 0.0f;        // 本发开始等待互锁的时刻(等待超时判定)
static float friction_off_ms = 0.0f;       // 摩擦轮连续关闭的起始时刻(停火宽限判定)
static uint8_t last_load_mode = LOADER_STOP; // 上周期load_mode(模式切换沿检测)

/* 连发槽沿计数(热量记账用):拨盘每转过一发角 = 又有一发被推入摩擦轮。
 * 用增量累加而不是绝对角差:模式切换、角度回绕时不会把整段跳变算成几十发。
 * 取 fabsf 是刻意偏保守 —— 堵转抖动会多记,多记只损失射速,漏记会锁枪。 */
static float burst_prev_angle = 0.0f; // 上周期拨盘总角,电机轴 deg
static float burst_acc_angle = 0.0f;  // 尚未记账的累计转角,电机轴 deg
static uint8_t burst_tracking = 0;    // 1 = 正在累计(进入连发时初始化基准)

void shoot_init()
{
    // 左摩擦轮(M3508+C620,速度环)
    motor_init_config_t friction_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
            },
        .controller_param_init_config =
            {
                .speed_PID =
                    {
                        .Kp = SHOOT_FRICTION_SPEED_PID_KP, // deg/s速度反馈
                        .Ki = SHOOT_FRICTION_SPEED_PID_KI,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = SHOOT_FRICTION_SPEED_PID_IL,
                        .MaxOut = SHOOT_FRICTION_SPEED_PID_MAXOUT,
                    },
                .current_PID =
                    {
                        .Kp = 0, // C620内部电流环已启用,外环电流环保留占位
                        .Ki = 0,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = 10000,
                        .MaxOut = 15000,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .outer_loop_type = SPEED_LOOP,
                .close_loop_type = SPEED_LOOP,
                /* 新结构电机安装镜像,两轮轮面原都朝供弹方向推弹(SHOT-20261114-1):REVERSE 换到左轮,
                 * 使两轮接触面同朝枪口。reverse 只作用于输出,反馈是原始方向——连发互锁的到速判定
                 * 符号已随之互换,回退 = 还原两处 REVERSE 位置 + 互锁符号。 */
                .motor_reverse_flag = MOTOR_DIRECTION_REVERSE,
            },
        .motor_type = M3508};
    friction_config.can_init_config.tx_id = SHOOT_FRICTION_ID_L;
    friction_l = dji_motor_init(&friction_config);

    friction_config.can_init_config.tx_id = SHOOT_FRICTION_ID_R; // 右摩擦轮:改id
    friction_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    friction_r = dji_motor_init(&friction_config);

    // 拨盘电机(M2006+C610,速度环)
    motor_init_config_t loader_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
                .tx_id = LOADER_ID,
            },
        .controller_param_init_config =
            {
                .angle_PID =
                    {
#if LOADER_ANGLE_LOOP_EN
                        .Kp = LOADER_ANGLE_PID_KP,
                        .Ki = LOADER_ANGLE_PID_KI,
                        .Kd = 0,
                        .MaxOut = LOADER_ANGLE_PID_MAXOUT,
#else
                        // 回退档:零参数等价于角度环禁用,配合下面 SPEED_LOOP 的单发行为
                        .Kp = 0,
                        .Ki = 0,
                        .Kd = 0,
                        .MaxOut = 200,
#endif
                    },
                .speed_PID =
                    {
                        .Kp = LOADER_SPEED_PID_KP, // deg/s速度反馈
                        .Ki = LOADER_SPEED_PID_KI,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = LOADER_SPEED_PID_IL,
                        .MaxOut = LOADER_SPEED_PID_MAXOUT, // 40%满量程力矩不足,提到80%
                    },
                .current_PID =
                    {
                        .Kp = 0,
                        .Ki = 0,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = 5000,
                        .MaxOut = 5000,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .outer_loop_type = SPEED_LOOP, // 初始化为速度环让拨盘停在原地
#if LOADER_ANGLE_LOOP_EN
                /* 单发/三发要角度闭环:close_loop_type 必须含 ANGLE_LOOP 位,
                 * 否则 dji_motor_control 的角度环分支整段短路(DEV-20261124-04)。 */
                .close_loop_type = ANGLE_LOOP | SPEED_LOOP,
#else
                .close_loop_type = SPEED_LOOP, // 回退档:原满速冲行为
#endif
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL, // 正转=拨出
            },
        .motor_type = M2006};
    loader = dji_motor_init(&loader_config);
    dji_motor_stop(friction_l);
    dji_motor_stop(friction_r);
    dji_motor_stop(loader);

    shoot_pub = msg_pub("shoot_feed", sizeof(shoot_upload_t));
    shoot_sub = msg_sub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));

    heat_guard_init(); // 热量守卫只清状态,不读裁判系统(此时裁判任务可能还没初始化)
}

/**
 * @brief 摩擦轮互锁:双轮在线且都到速
 * @return 1=可以拨弹
 * @note 连发速度环与单发/三发的起步前置**必须共用本函数**——历史上这两处各写了一份判据,
 *       方向反转(SHOT-20261114-1)时只改了一处,导致单发前置用旧符号、永远等不到"到速"。
 *       反馈是原始方向(reverse 只作用于输出):左轮 REVERSE → 反馈 −目标;右轮 NORMAL → +目标。
 *       目标量用 SHOOT_FRICTION_TARGET_RPM_EFF(含标定微调),与 SetRef 同源。
 *       掉线时 speed_aps 保持旧值,故必须先查 daemon 在线,否则陈旧速度会误判已到速。
 */
static uint8_t shoot_friction_ready()
{
    if (friction_l == NULL || friction_r == NULL) // 初始化失败(堆耗尽)时不得解引用
        return 0;
    return shoot_cmd_recv.friction_mode == FRICTION_ON && daemon_is_online(friction_l->daemon) &&
           daemon_is_online(friction_r->daemon) &&
           fabsf(friction_l->measure.speed_aps / FW_RPM_TO_DEG_S + SHOOT_FRICTION_TARGET_RPM_EFF) <=
               SHOOT_FRICTION_READY_TOL_RPM &&
           fabsf(friction_r->measure.speed_aps / FW_RPM_TO_DEG_S - SHOOT_FRICTION_TARGET_RPM_EFF) <=
               SHOOT_FRICTION_READY_TOL_RPM;
}

/* 机器人发射机构控制核心任务(200Hz) */
void shoot_task()
{
    if (loader == NULL || friction_l == NULL || friction_r == NULL)
        return; // 初始化失败(堆耗尽):保持静默,不做任何发射动作

    float now_ms = dwt_get_timeline_ms();

    // 从cmd获取控制数据(收到才刷新看门狗)
    if (msg_recv(shoot_sub, &shoot_cmd_recv))
        cmd_watchdog_feed(&shoot_cmd_wd, now_ms);

    // 周期时间:堵转累计用;异常值(负/超长)丢弃为0不参与累计
    float dt_ms = now_ms - shoot_last_ms;
    shoot_last_ms = now_ms;
    if (dt_ms < 0.0f || dt_ms > 100.0f)
        dt_ms = 0.0f;

    /* 热量守卫:必须先于任何发射决策更新,否则本周期用的是上一拍的闸门。
     * 放在总开关判断之前 —— 停火时冷却仍在继续,不能因为 SHOOT_OFF 就停止推算。 */
    heat_guard_update();

    // 命令断流:超时全停,否则摩擦轮/拨盘会按最后一帧一直动作
    if (cmd_watchdog_expired(&shoot_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS))
    {
        shoot_cmd_recv.shoot_mode = SHOOT_OFF;
        shoot_cmd_recv.load_mode = LOADER_STOP;
    }

    if (shoot_cmd_recv.shoot_mode == SHOOT_OFF)
    {
        dji_motor_stop(friction_l);
        dji_motor_stop(friction_r);
        dji_motor_stop(loader);
        loader_pending = 0; // 急停清空待发队列
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        last_load_mode = shoot_cmd_recv.load_mode;
        msg_publish(shoot_pub, (void *)&shoot_fb);
        return;
    }
    else
    {
        dji_motor_enable(friction_l);
        dji_motor_enable(friction_r);
        dji_motor_enable(loader);
    }

    // 拨盘模式处理(单发/三发的实际出弹由下方状态机执行)
    switch (shoot_cmd_recv.load_mode)
    {
    case LOADER_STOP: // 停止拨盘;拨轮回中且无待发弹时解除堵转锁存
        /* 单发/三发指令是单拍脉冲(cmd_kbm 的 R/E 沿、自瞄拨轮的 armed 沿):
         * 起步后的下一拍 load_mode 就回到 STOP。若无条件切速度环,在途那一发的
         * **绝对角** ref 会被速度环当成 deg/s 目标 → 拨盘满速冲,角度环整段失效。
         * 只有在途发弹时把电机交给下面的状态机(DEV-20261124-05)。 */
        if (!loader_step_active)
        {
            dji_motor_outer_loop(loader, SPEED_LOOP);
            dji_motor_set_ref(loader, 0);
        }
        burst_tracking = 0; // 下一段连发重新取基准角
        if (loader_pending == 0)
        {
            loader_jammed = 0;
            loader_jam_ms = 0.0f;
        }
        break;
    case LOADER_SINGLE: // 单发请求(键鼠 R 沿为单拍脉冲,遥控拨轮可持续;沿检测只登记一次)
    case LOADER_BURST3: // 三发(键鼠 E 沿单拍脉冲,逐发推进)
        if (last_load_mode != shoot_cmd_recv.load_mode && loader_pending == 0 && !loader_jammed)
        {
            int8_t bullets = (shoot_cmd_recv.load_mode == LOADER_SINGLE) ? 1 : 3;
            heat_guard_state_e heat_state = heat_guard_get_state();

            /* 闸门在"登记"处先挡一道:三发若已进队列,热量就按三发付账,
             * 那时再逐发拦截只能少打不能少记,所以降级必须在登记时完成。 */
            if (heat_state == HEAT_GUARD_HOLD)
            {
                LOG_WARN("[shoot] heat HOLD, %d-bullet request rejected", (int)bullets);
            }
            else
            {
                if (bullets > 1 && heat_state == HEAT_GUARD_SINGLE)
                {
                    bullets = 1; // 单发区:三发降级一发,逐发放行仍由下方状态机把关
                    LOG_WARN("[shoot] heat SINGLE, 3-bullet degraded to 1");
                }
                loader_pending = bullets;
                loader_wait_ms = now_ms; // 开始等待互锁,超时兜底从这里计时
                if (single_shot_warned == 0)
                {
                    LOG_INFO("[shoot] first single feed: check ONE_BULLET_DELTA_ANGLE calibration");
                    single_shot_warned = 1;
                }
            }
        }
        break;
    case LOADER_BURST:    // 连发:射速(发/s)换算拨盘角速度,ω=R×每发角×减速比(电机轴deg/s)
        loader_pending = 0; // 连发优先,清除未完成的单发队列
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        dji_motor_outer_loop(loader, SPEED_LOOP);
        if (loader_jammed)
        {
            dji_motor_set_ref(loader, 0); // 已堵转:保持零参考等拨轮回中
            burst_tracking = 0;        // 拨盘停转期间总角可能被手动转动,基准必须作废
            break;
        }
        /* 互锁:双摩擦轮"在线且在速"才拨弹,判据统一见 shoot_friction_ready()。
         * 射速不再直接用 shoot_rate,而是向热量守卫申请 —— 守卫按余热预算给出
         * 允许射速,逼近闸门时自动收敛到散热速率,单发/禁发区直接返回 0。 */
        float fire_rate = 0.0f;
        if (shoot_friction_ready() && heat_guard_allow_burst(shoot_cmd_recv.shoot_rate, &fire_rate))
        {
            /* 护栏(DEV-20261121-07):实车出现"拨盘突然飞转"。合法路径上 fire_rate 被
             * 守卫钳到 demand(6 发/s),任何 NaN/越界都说明上游数据异常 —— 绝不能喂给
             * 速度环,否则 PID 输出发散会让拨盘全速乱转。异常时按 0 处理并告警。 */
            if (!isfinite(fire_rate) || fire_rate > SHOOT_RATE_MAX)
            {
                LOG_ERROR("[shoot] illegal burst rate, clamped to 0 (DEV-20261121-07)");
                fire_rate = 0.0f;
            }
            if (fire_rate < 0.0f)
                fire_rate = 0.0f;
            dji_motor_set_ref(loader, fire_rate * SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION);

            /* 槽沿记账:拨盘转过一发角 = 一发被推入摩擦轮,此刻(0~5ms)记账。
             * 不等裁判确认 —— 裁判 ΔQ 迟到 100~200ms,按确认记账会连续漏记 1~2 发。 */
            if (!burst_tracking)
            {
                burst_prev_angle = loader->measure.total_angle;
                burst_acc_angle = 0.0f;
                burst_tracking = 1;
            }
            const float slot_motor =
                SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION; // 一发角,电机轴 deg
            burst_acc_angle += fabsf(loader->measure.total_angle - burst_prev_angle);
            burst_prev_angle = loader->measure.total_angle;
            if (!isfinite(burst_acc_angle) || burst_acc_angle > slot_motor * 20.0f)
                burst_acc_angle = 0.0f; // 反馈跳变:丢弃异常累计(DEV-20261121-08)
            uint8_t commits = 0;
            while (burst_acc_angle >= slot_motor && commits < 20u)
            {
                burst_acc_angle -= slot_motor;
                heat_guard_commit_shot();
                commits++;
            }
            if (burst_acc_angle >= slot_motor)
                burst_acc_angle = 0.0f; // 兜底:单拍记账有上限,绝不进入超长循环

            /* 堵转判据单点实现(RM-008);锁存后果为连发语义:
             * 保持 jammed 等拨轮回中,同时清连发记账基准 */
            if (shoot_loader_jam_tick(loader, dt_ms))
            {
                loader_jammed = 1;
                burst_tracking = 0;
                dji_motor_set_ref(loader, 0);
                LOG_ERROR("[shoot] loader jammed, output latched off; release dial to reset");
            }
        }
        else
        {
            dji_motor_set_ref(loader, 0);
            loader_jam_ms = 0.0f; // 互锁未通过不计堵转(电机本就没被要求转)
            burst_tracking = 0;   // 被闸门挡下/摩擦轮不在速:下段重新取基准角
        }
        break;
    case LOADER_REVERSE: // 拨盘反转(退卡弹),待实现
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0);
        burst_tracking = 0; // 反转不是射弹,不得计入热量
        break;
    default: // 未知模式:切安全态并告警(不挂死,保持可控)
        LOG_ERROR("[shoot] unknown load mode %d, forced stop", (int)shoot_cmd_recv.load_mode);
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0);
        break;
    }
    last_load_mode = shoot_cmd_recv.load_mode;

    /* 摩擦轮关闭 = 停火:持续关闭超过宽限期后丢弃待发队列。
     * 陷阱1:不能改放到 LOADER_STOP 分支 —— 单发/三发是沿触发,cmd 只发一帧,
     *        下一帧即回落 LOADER_STOP,那样会在登记后立刻被自己清掉,一发都打不出去。
     * 陷阱2:不能"一关就清" —— 按 R 与按右键常在同一帧,右键的摩擦轮标志可能
     *        比 R 的按键沿晚一个周期(5ms),立即清会把刚登记的请求误删。
     *        故留 LOADER_FRICTION_OFF_GRACE_MS 宽限。
     * 不加这段的后果:pending 挂在下方"等待互锁"分支无限等待,下次开摩擦轮时
     * 残余弹数自动补发,造成未瞄准的意外出膛。 */
    if (shoot_cmd_recv.friction_mode == FRICTION_OFF)
    {
        if (friction_off_ms <= 0.0f)
            friction_off_ms = now_ms; // 记录本次关闭的起始时刻
        if (loader_pending > 0 && now_ms - friction_off_ms > LOADER_FRICTION_OFF_GRACE_MS)
        {
            LOG_INFO("[shoot] friction off, %d pending bullet(s) dropped", loader_pending);
            loader_pending = 0;
            loader_step_active = 0;
            loader_wait_ms = 0.0f;
        }
    }
    else
        friction_off_ms = 0.0f; // 摩擦轮开启,清除宽限计时

    /* 单发/三发状态机:pending>0时逐发角度环推进(优先级高于上面的模式输出)。
     * 每发:起步前置=摩擦轮互锁通过 -> ref=当前总角+一发角(输出轴定义,乘减速比到电机轴)
     * -> 到位(容差)pending-1;起步后转速≈0且电流超阈=堵转,锁存并清队列;
     * 超时(卡弹/打滑)中止队列并告警。 */
    /* 禁发区:丢掉还没起步的待发弹。已起步的那一发不能中途撤 ——
     * 角度目标已下发,停在槽中间会压着弹丸,反而更容易卡,不如让它走完这一发。
     * 未起步就丢,是"三发进队列后热量越线"时唯一能少付账的地方。 */
    if (loader_pending > 0 && !loader_step_active && !loader_jammed &&
        heat_guard_get_state() == HEAT_GUARD_HOLD)
    {
        LOG_WARN("[shoot] heat HOLD, %d pending bullet(s) dropped", (int)loader_pending);
        loader_pending = 0;
        loader_wait_ms = 0.0f;
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0);
    }

    if (loader_pending > 0)
    {
        const float step_motor =
            SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION; // 一发角(电机轴deg)
        if (!loader_step_active)
        {
            /* 等待起步:拨盘必须停在速度环 ref=0。
             * 切角度环的动作只在"本发真正起步"那一刻做 —— 若在每拍无条件切,
             * 到位后已归零的 ref 会被角度环当成 0deg 目标,与累计几万度的当前角
             * 构成巨大反向误差,拨盘会反转(DEV-20261124-04)。 */
#if LOADER_STOP_AFTER_STEP_EN
            dji_motor_outer_loop(loader, SPEED_LOOP);
            dji_motor_set_ref(loader, 0);
#endif
            /* 起步前置与连发共用同一互锁判据(见 shoot_friction_ready);
             * 外加热量闸门 —— 单发区/断流降级时这里会等够最小间隔才放行。 */
            uint8_t friction_ok = shoot_friction_ready();
            if (friction_ok && heat_guard_allow_single())
            {
                float step_target = loader->measure.total_angle + step_motor;
                /* 护栏(DEV-20261121-07):反馈异常(total_angle 非有限值)时目标会发散,
                 * 角度环会驱动拨盘全速追一个不可能到达的位置 —— 直接丢弃本次放行。 */
                if (!isfinite(step_target))
                {
                    LOG_ERROR(
                        "[shoot] illegal loader step target, request dropped (DEV-20261121-07)");
                    loader_pending = 0;
                    loader_wait_ms = now_ms;
                }
                else
                {
                    loader_step_target = step_target;
                    /* 先写 ref 再切外环:反过来的话两条语句之间若被 1kHz 电机任务抢占,
                     * 角度环会拿旧 ref=0 追几万度的当前角 → 拨盘反向满速。先写 ref 时
                     * 最坏只是速度环多跑一拍(1ms)。 */
                    dji_motor_set_ref(loader, loader_step_target);
                    dji_motor_clear_pid(loader); // 清掉上一发/连发的积分,避免起步瞬间的残余力矩
                    dji_motor_outer_loop(loader, ANGLE_LOOP);
                    loader_step_active = 1;
                    loader_step_ms = now_ms;
                    loader_jam_ms = 0.0f;
                }
            }
            else if (!friction_ok && now_ms - loader_wait_ms > LOADER_WAIT_TIMEOUT_MS)
            {
                /* 兜底:摩擦轮迟迟不到速(或掉线)时放弃队列,
                 * 否则 pending 会永久悬挂,并在之后某个时刻突然补发。 */
                LOG_ERROR("[shoot] feed wait timeout (friction not ready), %d bullet(s) dropped",
                         loader_pending);
                loader_pending = 0;
                loader_wait_ms = 0.0f;
            }
            else if (friction_ok)
            {
                /* 摩擦轮已就绪、只是被热量闸门挡着(等最小间隔):
                 * 刷新等待起点,否则冷却慢的车会被摩擦轮超时误伤而丢掉整队。 */
                loader_wait_ms = now_ms;
            }
        }
        else
        {
            /* 幂等重申角度环:LOADER_STOP/LOADER_BURST 等路径可能在在途期间改 outer_loop,
             * 每拍兜一次,保证这一发始终由角度环推进(DEV-20261124-05)。 */
            dji_motor_outer_loop(loader, ANGLE_LOOP);
            dji_motor_set_ref(loader, loader_step_target);
            if (fabsf(loader->measure.total_angle - loader_step_target) <=
                LOADER_ANGLE_TOL_DEG * SHOOT_LOADER_REDUCTION)
            {
                loader_pending--;
                loader_step_active = 0;  // 本发完成,下一发重新走起步前置(互锁+间隔)
                loader_wait_ms = now_ms; // 下一发的等待超时从这里重新计时
#if LOADER_STOP_AFTER_STEP_EN
                /* 到位立即停车:切回速度环 + 清 PID + ref=0。
                 * 少了这一步,三发队列的中间两发到位后仍保持角度环满速追旧目标,
                 * 拨盘继续推弹,叠加每发过冲 —— 单发 2~3 连 / 三发 4~5 连(DEV-20261124-04)。 */
                dji_motor_outer_loop(loader, SPEED_LOOP);
                dji_motor_clear_pid(loader);
                dji_motor_set_ref(loader, 0);
#endif
            }
            else if (shoot_loader_jam_tick(loader, dt_ms))
            {
                /* 锁存后果为单发语义:丢队列并回速度环,等拨轮回中重登记 */
                loader_jammed = 1;
                loader_pending = 0;
                loader_step_active = 0;
                loader_wait_ms = 0.0f;
                dji_motor_outer_loop(loader, SPEED_LOOP);
                dji_motor_clear_pid(loader); // 堵转时也要清:否则残余积分会继续压着弹
                dji_motor_set_ref(loader, 0);
                LOG_ERROR(
                    "[shoot] loader jammed during feed, queue aborted; release dial to reset");
            }

            if (loader_step_active && now_ms - loader_step_ms > LOADER_SINGLE_TIMEOUT_MS)
            {
                LOG_ERROR("[shoot] single feed timeout, %d bullet(s) aborted", loader_pending);
                loader_pending = 0;
                loader_step_active = 0;
                loader_wait_ms = 0.0f;
            }
        }
        if (loader_pending == 0 && loader_step_active == 0 && !loader_jammed)
        {
            dji_motor_outer_loop(loader, SPEED_LOOP); // 队列清空:回速度环停住
            dji_motor_clear_pid(loader);              // 清角度环积分,下次起步从零状态开始
            dji_motor_set_ref(loader, 0);
        }
    }

    // 摩擦轮:实际弹速未标定,各弹速挡暂用同一目标转速(标定后按挡位分速)
    if (shoot_cmd_recv.friction_mode == FRICTION_ON)
    {
        dji_motor_set_ref(friction_l, SHOOT_FRICTION_TARGET_RPM_EFF * FW_RPM_TO_DEG_S);
        dji_motor_set_ref(friction_r, SHOOT_FRICTION_TARGET_RPM_EFF * FW_RPM_TO_DEG_S);
    }
    else
    {
        dji_motor_set_ref(friction_l, 0);
        dji_motor_set_ref(friction_r, 0);
    }

    // 弹舱盖:接入舵机后按lid_mode控制(见shoot.h的lid_mode_e)

    /* 发射遥测(第5环,单写者=本任务):ref=目标RPM fb=左轮RPM out=右轮RPM extra=拨盘RPM,
     * 带符号、与互锁判定同源换算(speed_aps/FW_RPM_TO_DEG_S)。B3 峰峰值/B5 单发角用 Ozone
     * 高频观察此环。 */
    telemetry_push(&telemetry_shoot, (float)SHOOT_FRICTION_TARGET_RPM_EFF,
                  friction_l->measure.speed_aps / FW_RPM_TO_DEG_S,
                  friction_r->measure.speed_aps / FW_RPM_TO_DEG_S,
                  loader->measure.speed_aps / FW_RPM_TO_DEG_S,
                  (uint8_t)shoot_cmd_recv.friction_mode);
#if SHOOT_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行
     * (fl/fr=双轮RPM带符号,ldr=拨盘RPM,cur=拨盘实际电流——堵转判据的量;
     *  icl/icr=双轮速度环输出,即发往 C620 的电流指令码(上限 6000 码≈7.3A);
     *  ifl/ifr=双轮实际电流反馈码,电调回传,C620 满量程 16384。)
     * 指令与反馈分列的意义:两者长期不一致说明电调没吃满指令(限流/供电跌压/编码器坏)。 */
    if (++shoot_calib_cnt >= 100)
    {
        char s_fl[16], s_fr[16], s_ldr[16], s_cur[16];
        char s_icl[16], s_icr[16], s_ifl[16], s_ifr[16];
        shoot_calib_cnt = 0;
        log_float_to_str(s_fl, friction_l->measure.speed_aps / FW_RPM_TO_DEG_S);
        log_float_to_str(s_fr, friction_r->measure.speed_aps / FW_RPM_TO_DEG_S);
        log_float_to_str(s_ldr, loader->measure.speed_aps / FW_RPM_TO_DEG_S);
        log_float_to_str(s_cur, loader->measure.real_current);
        log_float_to_str(s_icl, friction_l->motor_controller.speed_PID.Output);
        log_float_to_str(s_icr, friction_r->motor_controller.speed_PID.Output);
        log_float_to_str(s_ifl, (float)friction_l->measure.real_current);
        log_float_to_str(s_ifr, (float)friction_r->measure.real_current);
        LOG_INFO("[shootcal] fl=%s fr=%s ldr=%s cur=%s icl=%s icr=%s ifl=%s ifr=%s", s_fl, s_fr,
                s_ldr, s_cur, s_icl, s_icr, s_ifl, s_ifr);
    }
#endif

    // 反馈回传:后续可增加卡弹/离线监测
    msg_publish(shoot_pub, (void *)&shoot_fb);
}
