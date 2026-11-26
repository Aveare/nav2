/**
 * @file    shoot.c
 * @brief   发射应用:摩擦轮速度环、拨盘单发/三发/连发、堵转保护
 * @layer   APP
 * @note    连发:速度环,射速由 cmd 下发(发/s,config SHOOT_RATE_DEFAULT),双轮互锁 + 堵转锁存。
 * @note    单发/三发:"待发弹数"状态机,逐发角度环推进,到位容差 / 超时 / 堵转三重保护;
 *          一发角 = 输出轴 360 / 槽数(config ONE_BULLET_DELTA_ANGLE),需实车标定。
 * @note    反转(退卡弹)为占位,当前实现为停止。
 * @note    2026-10-29 建立,作者 JIT CR。
 */
#include "shoot.h"
#include "robot_def.h"

#include "dji_motor.h"
#include "message_center.h"
#include "cmd_watchdog.h"
#include "bsp_dwt.h"
#include "bsp_log.h"
#include "general_def.h"
#include "telemetry.h" // P1-B3 需要:摩擦轮转速高频采样(0.5s 日志看不到纹波)

/* 发射标定值统一在 config/robot_infantry_2026.h 维护,本文件只负责换算与互锁判断 */

/* 摩擦轮实际目标 = 名义目标 + 标定微调(P2 标定后填 FRICTION_RPM_OFFSET)。
 * **目标下发、互锁判据、遥测三处必须同源**:若只有 SetRef 加 OFFSET 而判据仍用名义值,
 * 标定后互锁会按错的转速判到速,单发/连发全乱。 */
#define FRICTION_TARGET_RPM_EFF (FRICTION_TARGET_RPM + FRICTION_RPM_OFFSET)

static DJIMotorInstance *friction_l, *friction_r, *loader; // 摩擦轮左右+拨盘
// static servo_instance *lid; // 弹舱盖舵机,接入后在此注册

static Publisher_t *shoot_pub;
static Shoot_Ctrl_Cmd_s shoot_cmd_recv;         // 来自cmd的发射控制信息
static Subscriber_t *shoot_sub;
static CmdWatchdog_t shoot_cmd_wd;              // 命令断流看门狗
static Shoot_Upload_Data_s shoot_feedback_data; // 回传cmd的发射反馈

static uint8_t single_shot_warned = 0; // 单发/三发未标定警告只提示一次(位置环已启用,标定由config参数承担,保留作首次观测提示)

/* 拨盘堵转保护状态(仅本文件内使用):判据与阈值见config的LOADER_JAM_*;
 * 只停拨盘、不反转、不自动重试,拨轮回中(LOAD_STOP且无待发弹)才解除锁存 */
static uint8_t loader_jammed = 0;  // 1=已判堵转并锁存
static float loader_jam_ms = 0.0f; // 堵转累计时间
static float shoot_last_ms = 0.0f; // 上周期时刻
static uint8_t friction_running = 0;  // 1=摩擦轮仍在转(含松开后的延时窗口)
static float friction_run_ms = 0.0f;  // 最近一次收到 FRICTION_ON 的时刻
#if SHOOT_CALIB_LOG
static uint16_t shoot_calib_cnt; // 标定日志分频计数(200Hz / 100 = 0.5s)
#endif

/* 单发/三发"待发弹数"状态机:cmd持续发布load_mode,本任务按模式切换沿登记弹数,
 * 逐发切角度环推进;每发起步前置=摩擦轮双轮在线且在速(与连发共用互锁判据),
 * 到位(容差)或超时(config LOADER_SINGLE_TIMEOUT_MS)结束本发;堵转中止整个队列。
 * 队列有三条终止路径:到位发完、堵转/超时、摩擦轮关闭或等待超时(避免悬挂后误射)。 */
static int8_t loader_pending = 0;        // 待发弹数(0=空闲)
static uint8_t loader_step_active = 0;   // 本发已起步(目标角已设定,未到位)
static float loader_step_target = 0.0f;  // 本发目标总角(电机轴deg)
static float loader_step_ms = 0.0f;      // 本发起步时刻(超时判定)
static float loader_wait_ms = 0.0f;      // 本发开始等待互锁的时刻(等待超时判定)
static float friction_off_ms = 0.0f;     // 摩擦轮连续关闭的起始时刻(停火宽限判定)
static uint8_t last_load_mode = LOAD_STOP; // 上周期load_mode(模式切换沿检测)

void ShootInit()
{
    // 左摩擦轮(M3508+C620,速度环)
    Motor_Init_Config_s friction_config = {
        .can_init_config = {
            .can_handle = &hcan2,
        },
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = FRICTION_SPEED_PID_KP, // deg/s速度反馈
                .Ki = FRICTION_SPEED_PID_KI,
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = FRICTION_SPEED_PID_IL,
                .MaxOut = FRICTION_SPEED_PID_MAXOUT,
            },
            .current_PID = {
                .Kp = 0, // C620内部电流环已启用,外环电流环保留占位
                .Ki = 0,
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 10000,
                .MaxOut = 15000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP,
            /* 电机安装镜像:两轮接触面需同朝枪口。
             * 2026-11-22 用户实测"摩擦轮反向",故把 REVERSE 从**左轮换到右轮**——
             * reverse 只作用于输出,反馈是原始方向,所以互锁的到速符号必须同步互换
             * (FrictionReady: 左 +目标 / 右 −目标)。回退 = 还原这两处 + 互锁符号。 */
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
        },
        .motor_type = M3508};
    friction_config.can_init_config.tx_id = FRICTION_ID_L;
    friction_l = DJIMotorInit(&friction_config);

    friction_config.can_init_config.tx_id = FRICTION_ID_R; // 右摩擦轮:改id
    friction_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_REVERSE;
    friction_r = DJIMotorInit(&friction_config);

    // 拨盘电机(M2006+C610,速度环)
    Motor_Init_Config_s loader_config = {
        .can_init_config = {
            .can_handle = &hcan2,
            .tx_id = LOADER_ID,
        },
        .controller_param_init_config = {
            .angle_PID = {
                /* 单发/三发靠角度环推一发角。参考工程此处 Kp=Ki=0(等于禁用,请求会一直等到超时);
                 * 本车按"低阻尼对象"取小 P + 小 I 起标,见 config 的 LOADER_ANGLE_PID_* 注释。 */
                .Kp = LOADER_ANGLE_PID_KP,
                .Ki = LOADER_ANGLE_PID_KI,
                .Kd = 0,
                .MaxOut = LOADER_ANGLE_PID_MAXOUT,
            },
            .speed_PID = {
                .Kp = LOADER_SPEED_PID_KP, // deg/s速度反馈
                .Ki = LOADER_SPEED_PID_KI,
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = LOADER_SPEED_PID_IL,
                .MaxOut = LOADER_SPEED_PID_MAXOUT,
            },
            .current_PID = {
                .Kp = 0,
                .Ki = 0,
                .Kd = 0,
                .Improve = PID_Integral_Limit,
                .IntegralLimit = 5000,
                .MaxOut = 5000,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type = SPEED_LOOP, // 初始化为速度环让拨盘停在原地
            .close_loop_type = SPEED_LOOP, // C610内部完成电流控制
            .motor_reverse_flag = MOTOR_DIRECTION_NORMAL, // 正转=拨出
        },
        .motor_type = M2006};
    loader = DJIMotorInit(&loader_config);
    DJIMotorStop(friction_l);
    DJIMotorStop(friction_r);
    DJIMotorStop(loader);

    shoot_pub = PubRegister("shoot_feed", sizeof(Shoot_Upload_Data_s));
    shoot_sub = SubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
}

/**
 * @brief 摩擦轮互锁:双轮在线且都到速
 * @return 1=可以拨弹
 * @note 连发速度环与单发/三发的起步前置**必须共用本函数**——历史上这两处各写了一份判据,
 *       方向反转(SHOT-20261114-1)时只改了一处,导致单发前置用旧符号、永远等不到"到速"。
 *       反馈是原始方向(reverse 只作用于输出):左轮 NORMAL → 反馈 +目标;右轮 REVERSE → −目标。
 *       目标量用 FRICTION_TARGET_RPM_EFF(含标定微调),与 SetRef 同源。
 *       掉线时 speed_aps 保持旧值,故必须先查 daemon 在线,否则陈旧速度会误判已到速。
 */
static uint8_t FrictionReady()
{
    return friction_running &&
           DaemonIsOnline(friction_l->daemon) && DaemonIsOnline(friction_r->daemon) &&
           fabsf(friction_l->measure.speed_aps / RPM_2_ANGLE_PER_SEC - FRICTION_TARGET_RPM_EFF) <= FRICTION_READY_TOL_RPM &&
           fabsf(friction_r->measure.speed_aps / RPM_2_ANGLE_PER_SEC + FRICTION_TARGET_RPM_EFF) <= FRICTION_READY_TOL_RPM;
}

/* 机器人发射机构控制核心任务(200Hz) */
void ShootTask()
{
    float now_ms = DWT_GetTimeline_ms();

    // 从cmd获取控制数据(收到才刷新看门狗)
    if (SubGetMessage(shoot_sub, &shoot_cmd_recv))
        CmdWatchdogFeed(&shoot_cmd_wd, now_ms);

    // 周期时间:堵转累计用;异常值(负/超长)丢弃为0不参与累计
    float dt_ms = now_ms - shoot_last_ms;
    shoot_last_ms = now_ms;
    if (dt_ms < 0.0f || dt_ms > 100.0f)
        dt_ms = 0.0f;

    // 命令断流:超时全停,否则摩擦轮/拨盘会按最后一帧一直动作
    if (CmdWatchdogExpired(&shoot_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS))
    {
        shoot_cmd_recv.shoot_mode = SHOOT_OFF;
        shoot_cmd_recv.load_mode = LOAD_STOP;
    }

    if (shoot_cmd_recv.shoot_mode == SHOOT_OFF)
    {
        DJIMotorStop(friction_l);
        DJIMotorStop(friction_r);
        DJIMotorStop(loader);
        loader_pending = 0; // 急停清空待发队列
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        last_load_mode = shoot_cmd_recv.load_mode;
        PubPushMessage(shoot_pub, (void *)&shoot_feedback_data);
        return;
    }
    else
    {
        DJIMotorEnable(friction_l);
        DJIMotorEnable(friction_r);
        DJIMotorEnable(loader);
    }

    /* 摩擦轮运行态(必须在本拍所有"是否可拨弹"的判断之前算好):
     *   扳机按下 → 立刻记时并置 1;
     *   松开后靠 FRICTION_STOP_DELAY_MS 尾窗继续转;
     *   只要还有待发弹(loader_pending>0)也保持转,否则三发打到一半轮子就掉了。
     * 这一路是 FrictionReady() 的唯一依据,也是 cmd 侧"摩擦轮由扳机驱动"的落地。 */
    if (shoot_cmd_recv.friction_mode == FRICTION_ON || loader_pending > 0)
        friction_run_ms = now_ms; // 扳机按下或队列未清都刷新计时,尾窗从"最后一次需要转"起算
    {
        const uint8_t keep = (shoot_cmd_recv.friction_mode == FRICTION_ON) ||
                             (loader_pending > 0) ||
                             (friction_running && (now_ms - friction_run_ms < FRICTION_STOP_DELAY_MS));
        friction_running = keep ? 1u : 0u;
    }

    // 拨盘模式处理(单发/三发的实际出弹由下方状态机执行)
    switch (shoot_cmd_recv.load_mode)
    {
    case LOAD_STOP: // 停止拨盘;拨轮回中且无待发弹时解除堵转锁存
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        DJIMotorSetRef(loader, 0);
        if (loader_pending == 0)
        {
            loader_jammed = 0;
            loader_jam_ms = 0.0f;
        }
        break;
    case LOAD_1_BULLET: // 单发请求(cmd拨轮浅段持续发布,沿检测只登记一次)
    case LOAD_3_BULLET: // 三发(机制就绪,输入待图传/键鼠接入后定义)
        if (last_load_mode != shoot_cmd_recv.load_mode &&
            loader_pending == 0 && !loader_jammed)
        {
            loader_pending = (shoot_cmd_recv.load_mode == LOAD_1_BULLET) ? 1 : 3;
            loader_wait_ms = now_ms; // 开始等待互锁,超时兜底从这里计时
            if (single_shot_warned == 0)
            {
                LOGINFO("[shoot] first single feed: check ONE_BULLET_DELTA_ANGLE calibration");
                single_shot_warned = 1;
            }
        }
        break;
    case LOAD_BURSTFIRE: // 连发:射速(发/s)换算拨盘角速度,ω=R×每发角×减速比(电机轴deg/s)
        loader_pending = 0; // 连发优先,清除未完成的单发队列
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        if (loader_jammed)
        {
            DJIMotorSetRef(loader, 0); // 已堵转:保持零参考等拨轮回中
            break;
        }
        /* 互锁:双摩擦轮"在线且在速"才拨弹,判据统一见 FrictionReady() */
        if (FrictionReady())
        {
            DJIMotorSetRef(loader, shoot_cmd_recv.shoot_rate * ONE_BULLET_DELTA_ANGLE * REDUCTION_RATIO_LOADER);
            // 堵转判据:在给目标但转速≈0且电流超阈,累计超时则停盘锁存
            if (fabsf(loader->measure.speed_aps / RPM_2_ANGLE_PER_SEC) < LOADER_JAM_SPEED_RPM &&
                fabsf((float)loader->measure.real_current) >= LOADER_JAM_CURRENT)
                loader_jam_ms += dt_ms;
            else
                loader_jam_ms = 0.0f; // 转起来就清零
            if (loader_jam_ms >= LOADER_JAM_HOLD_MS)
            {
                loader_jammed = 1;
                loader_jam_ms = 0.0f;
                DJIMotorSetRef(loader, 0);
                LOGERROR("[shoot] loader jammed, output latched off; release dial to reset");
            }
        }
        else
        {
            DJIMotorSetRef(loader, 0);
            loader_jam_ms = 0.0f; // 互锁未通过不计堵转(电机本就没被要求转)
        }
        break;
    case LOAD_REVERSE: // 拨盘反转(退卡弹),待实现
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        DJIMotorSetRef(loader, 0);
        break;
    default: // 未知模式:切安全态并告警(不挂死,保持可控)
        LOGERROR("[shoot] unknown load mode %d, forced stop", (int)shoot_cmd_recv.load_mode);
        DJIMotorOuterLoop(loader, SPEED_LOOP);
        DJIMotorSetRef(loader, 0);
        break;
    }
    last_load_mode = shoot_cmd_recv.load_mode;

    /* 摩擦轮关闭 = 停火:持续关闭超过宽限期后丢弃待发队列。
     * 陷阱1:不能改放到 LOAD_STOP 分支 —— 单发/三发是沿触发,cmd 只发一帧,
     *        下一帧即回落 LOAD_STOP,那样会在登记后立刻被自己清掉,一发都打不出去。
     * 陷阱2:不能"一关就清" —— 按 R 与按右键常在同一帧,右键的摩擦轮标志可能
     *        比 R 的按键沿晚一个周期(5ms),立即清会把刚登记的请求误删。
     *        故留 LOADER_FRICTION_OFF_GRACE_MS 宽限。
     * 不加这段的后果:pending 挂在下方"等待互锁"分支无限等待,下次开摩擦轮时
     * 残余弹数自动补发,造成未瞄准的意外出膛。 */
    if (!friction_running)
    {
        if (friction_off_ms <= 0.0f)
            friction_off_ms = now_ms; // 记录本次关闭的起始时刻
        if (loader_pending > 0 && now_ms - friction_off_ms > LOADER_FRICTION_OFF_GRACE_MS)
        {
            LOGINFO("[shoot] friction off, %d pending bullet(s) dropped", loader_pending);
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
    if (loader_pending > 0)
    {
        const float step_motor = ONE_BULLET_DELTA_ANGLE * REDUCTION_RATIO_LOADER; // 一发角(电机轴deg)
        DJIMotorOuterLoop(loader, ANGLE_LOOP);
        if (!loader_step_active)
        {
            /* 起步前置与连发共用同一互锁判据(见 FrictionReady) */
            if (FrictionReady())
            {
                loader_step_target = loader->measure.total_angle + step_motor;
                loader_step_active = 1;
                loader_step_ms = now_ms;
                loader_jam_ms = 0.0f;
            }
            else if (now_ms - loader_wait_ms > LOADER_WAIT_TIMEOUT_MS)
            {
                /* 兜底:摩擦轮迟迟不到速(或掉线)时放弃队列,
                 * 否则 pending 会永久悬挂,并在之后某个时刻突然补发。 */
                LOGERROR("[shoot] feed wait timeout (friction not ready), %d bullet(s) dropped",
                         loader_pending);
                loader_pending = 0;
                loader_wait_ms = 0.0f;
            }
        }
        else
        {
            DJIMotorSetRef(loader, loader_step_target);
            if (fabsf(loader->measure.total_angle - loader_step_target) <=
                LOADER_ANGLE_TOL_DEG * REDUCTION_RATIO_LOADER)
            {
                loader_pending--;
                loader_step_active = 0; // 本发完成,下一发重新走起步前置(互锁+间隔)
                loader_wait_ms = now_ms; // 下一发的等待超时从这里重新计时
            }
            else if (fabsf(loader->measure.speed_aps / RPM_2_ANGLE_PER_SEC) < LOADER_JAM_SPEED_RPM &&
                     fabsf((float)loader->measure.real_current) >= LOADER_JAM_CURRENT)
            {
                loader_jam_ms += dt_ms;
                if (loader_jam_ms >= LOADER_JAM_HOLD_MS)
                {
                    loader_jammed = 1;
                    loader_pending = 0;
                    loader_step_active = 0;
                    loader_wait_ms = 0.0f;
                    DJIMotorOuterLoop(loader, SPEED_LOOP);
                    DJIMotorSetRef(loader, 0);
                    LOGERROR("[shoot] loader jammed during feed, queue aborted; release dial to reset");
                }
            }
            else
                loader_jam_ms = 0.0f;

            if (loader_step_active && now_ms - loader_step_ms > LOADER_SINGLE_TIMEOUT_MS)
            {
                LOGERROR("[shoot] single feed timeout, %d bullet(s) aborted", loader_pending);
                loader_pending = 0;
                loader_step_active = 0;
                loader_wait_ms = 0.0f;
            }
        }
        if (loader_pending == 0 && loader_step_active == 0 && !loader_jammed)
        {
            DJIMotorOuterLoop(loader, SPEED_LOOP); // 队列清空:回速度环停住
            DJIMotorSetRef(loader, 0);
        }
    }

    // 摩擦轮:实际弹速未标定,各弹速挡暂用同一目标转速(标定后按挡位分速)
    /* 延时停(用户要求"主动晚停 1 秒"):松开后仍按目标转速再跑 FRICTION_STOP_DELAY_MS,
     * 让最后拨出的那发还吃得到轮速;到点再归零。
     * 急停/SHOOT_OFF 走函数前面的 return,直接 Stop,**不享受这个延时**——安全优先。 */
    if (shoot_cmd_recv.friction_mode == FRICTION_ON)
    {
        friction_running = 1;
        friction_run_ms = now_ms;
    }
    else if (friction_running && now_ms - friction_run_ms >= FRICTION_STOP_DELAY_MS)
    {
        friction_running = 0;
    }

    if (friction_running)
    {
        DJIMotorSetRef(friction_l, FRICTION_TARGET_RPM_EFF * RPM_2_ANGLE_PER_SEC);
        DJIMotorSetRef(friction_r, FRICTION_TARGET_RPM_EFF * RPM_2_ANGLE_PER_SEC);
    }
    else
    {
        DJIMotorSetRef(friction_l, 0);
        DJIMotorSetRef(friction_r, 0);
    }

    // 弹舱盖:接入舵机后按lid_mode控制(见shoot.h的lid_mode_e)

    /* 发射遥测(第5环,单写者=本任务):ref=目标RPM fb=左轮RPM out=右轮RPM extra=拨盘RPM,
     * 带符号、与互锁判定同源换算(speed_aps/RPM_2_ANGLE_PER_SEC)。B3 峰峰值/B5 单发角用 Ozone 高频观察此环。 */
    TelemetryPush(&telemetry_shoot, (float)FRICTION_TARGET_RPM_EFF,
                  friction_l->measure.speed_aps / RPM_2_ANGLE_PER_SEC,
                  friction_r->measure.speed_aps / RPM_2_ANGLE_PER_SEC,
                  loader->measure.speed_aps / RPM_2_ANGLE_PER_SEC,
                  (uint8_t)shoot_cmd_recv.friction_mode);
#if SHOOT_CALIB_LOG
    /* 标定日志:200Hz/100 = 0.5s 一行,四列覆盖 P2/P4 采集需求
     * (fl/fr=双轮RPM带符号,ldr=拨盘RPM,cur=拨盘电流——堵转判据的量)。 */
    if (++shoot_calib_cnt >= 100)
    {
        char s_fl[16], s_fr[16], s_ldr[16], s_cur[16];
        shoot_calib_cnt = 0;
        Float2Str(s_fl, friction_l->measure.speed_aps / RPM_2_ANGLE_PER_SEC);
        Float2Str(s_fr, friction_r->measure.speed_aps / RPM_2_ANGLE_PER_SEC);
        Float2Str(s_ldr, loader->measure.speed_aps / RPM_2_ANGLE_PER_SEC);
        Float2Str(s_cur, loader->measure.real_current);
        LOGINFO("[shootcal] fl=%s fr=%s ldr=%s cur=%s", s_fl, s_fr, s_ldr, s_cur);
    }
#endif

    // 反馈回传:后续可增加卡弹/离线监测
    PubPushMessage(shoot_pub, (void *)&shoot_feedback_data);
}
