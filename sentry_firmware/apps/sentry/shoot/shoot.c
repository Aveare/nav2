/**
 * @file    shoot.c
 * @brief   哨兵 17mm 发射:摩擦轮/拨盘、单发三发连发、热量闸门与堵转保护
 * @layer   APP
 * @author  小猫大王
 * @note    连发射速不直接取 cmd,而是向 heat_guard 申请;单发/三发逐发放行。
 * @note    热量记账:单发在放行瞬间(守卫内部),连发在拨盘转过一发角时。
 * @note    摩擦轮互锁 = 双轮在线且在速;掉线时 speed_aps 是旧值,必须先查 daemon。
 */
#include "shoot.h"
#include "heat_guard.h"
#include "sentry_def.h"
#include "robot_def.h"
#include "dji_motor.h"
#include "daemon.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "general_def.h"
#include "dwt_port.h"
#include "log_port.h"
#include "telemetry.h"
#include <math.h>

#define SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF (SHOOT_FRICTION_TARGET_RPM + SHOOT_FRICTION_RPM_OFFSET)

static dji_motor_t *friction_l;
static dji_motor_t *friction_r;
static dji_motor_t *loader;

static msg_pub_t shoot_pub;
static msg_sub_t shoot_sub;
static shoot_ctrl_cmd_t shoot_cmd_recv;
static sentry_shoot_feed_t shoot_fb;
static cmd_watchdog_t shoot_cmd_wd;

static float shoot_last_ms;
static uint8_t loader_jammed;
static float loader_jam_ms;

static int8_t loader_pending;
static uint8_t loader_step_active;
static float loader_step_target;
static float loader_step_ms;
static float loader_wait_ms;
static float friction_off_ms;
static uint8_t last_load_mode;
static uint8_t friction_running;
static float friction_run_ms;

static float burst_prev_angle;
static float burst_acc_angle;
static uint8_t burst_tracking;

static uint8_t shoot_loader_jam_tick(const dji_motor_t *motor, float dt_ms)
{
    if (motor == NULL)
        return 0;
    if (fabsf(motor->measure.speed_aps / FW_RPM_TO_DEG_S) < LOADER_JAM_SPEED_RPM &&
        fabsf((float)motor->measure.real_current) >= LOADER_JAM_CURRENT)
        loader_jam_ms += dt_ms;
    else
        loader_jam_ms = 0.0f;
    if (loader_jam_ms >= LOADER_JAM_HOLD_MS)
    {
        loader_jam_ms = 0.0f;
        return 1;
    }
    return 0;
}

static float shoot_friction_target_error(const dji_motor_t *motor, uint8_t reverse)
{
    float rpm = motor->measure.speed_aps / FW_RPM_TO_DEG_S;
    return reverse ? (rpm + SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF) : (rpm - SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF);
}

static uint8_t shoot_friction_ready(void)
{
    if (friction_l == NULL || friction_r == NULL)
        return 0;
    return friction_running && daemon_is_online(friction_l->daemon) &&
           daemon_is_online(friction_r->daemon) &&
           fabsf(shoot_friction_target_error(friction_l, SHOOT_FRICTION_REVERSE_L)) <=
               SHOOT_FRICTION_READY_TOL_RPM &&
           fabsf(shoot_friction_target_error(friction_r, SHOOT_FRICTION_REVERSE_R)) <=
               SHOOT_FRICTION_READY_TOL_RPM;
}

void shoot_init(void)
{
    motor_init_config_t friction_config = {
        .can_init_config =
            {
                .can_handle = &hcan2,
            },
        .controller_param_init_config =
            {
                .speed_PID =
                    {
                        .Kp = SHOOT_FRICTION_SPEED_PID_KP,
                        .Ki = SHOOT_FRICTION_SPEED_PID_KI,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = SHOOT_FRICTION_SPEED_PID_IL,
                        .MaxOut = SHOOT_FRICTION_SPEED_PID_MAXOUT,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .outer_loop_type = SPEED_LOOP,
                .close_loop_type = SPEED_LOOP,
                .motor_reverse_flag =
                    SHOOT_FRICTION_REVERSE_L ? MOTOR_DIRECTION_REVERSE : MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = M3508,
    };
    friction_config.can_init_config.tx_id = SHOOT_FRICTION_ID_L;
    friction_l = dji_motor_init(&friction_config);

    friction_config.can_init_config.tx_id = SHOOT_FRICTION_ID_R;
    friction_config.controller_setting_init_config.motor_reverse_flag =
        SHOOT_FRICTION_REVERSE_R ? MOTOR_DIRECTION_REVERSE : MOTOR_DIRECTION_NORMAL;
    friction_r = dji_motor_init(&friction_config);

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
                        .Kp = LOADER_ANGLE_PID_KP,
                        .Ki = LOADER_ANGLE_PID_KI,
                        .Kd = 0,
                        .MaxOut = LOADER_ANGLE_PID_MAXOUT,
                    },
                .speed_PID =
                    {
                        .Kp = LOADER_SPEED_PID_KP,
                        .Ki = LOADER_SPEED_PID_KI,
                        .Kd = 0,
                        .Improve = PID_Integral_Limit,
                        .IntegralLimit = LOADER_SPEED_PID_IL,
                        .MaxOut = LOADER_SPEED_PID_MAXOUT,
                    },
            },
        .controller_setting_init_config =
            {
                .angle_feedback_source = MOTOR_FEED,
                .speed_feedback_source = MOTOR_FEED,
                .outer_loop_type = SPEED_LOOP,
                .close_loop_type = SPEED_LOOP,
                .motor_reverse_flag = MOTOR_DIRECTION_NORMAL,
            },
        .motor_type = M2006,
    };
    loader = dji_motor_init(&loader_config);

    if (friction_l != NULL)
        dji_motor_stop(friction_l);
    if (friction_r != NULL)
        dji_motor_stop(friction_r);
    if (loader != NULL)
        dji_motor_stop(loader);

    shoot_pub = msg_pub("shoot_feed", sizeof(sentry_shoot_feed_t));
    shoot_sub = msg_sub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));
    heat_guard_init();
}

static void shoot_publish_feedback(void)
{
    heat_guard_get_snapshot(&shoot_fb.heat, &shoot_fb.heat_limit, &shoot_fb.cooling);
    shoot_fb.friction_rpm_l =
        (friction_l != NULL) ? friction_l->measure.speed_aps / FW_RPM_TO_DEG_S : 0.0f;
    shoot_fb.friction_rpm_r =
        (friction_r != NULL) ? friction_r->measure.speed_aps / FW_RPM_TO_DEG_S : 0.0f;
    shoot_fb.heat_state = (uint8_t)heat_guard_get_state();
    msg_publish(shoot_pub, (void *)&shoot_fb);
}

void shoot_task(void)
{
    if (loader == NULL || friction_l == NULL || friction_r == NULL)
        return;

    const float now_ms = dwt_get_timeline_ms();
    float dt_ms = now_ms - shoot_last_ms;
    shoot_last_ms = now_ms;
    if (dt_ms < 0.0f || dt_ms > 100.0f)
        dt_ms = 0.0f;

    if (msg_recv(shoot_sub, &shoot_cmd_recv))
        cmd_watchdog_feed(&shoot_cmd_wd, now_ms);

    /* 热量守卫先更新,停火时冷却也在继续 */
    heat_guard_update();

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
        loader_pending = 0;
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        friction_running = 0u;
        last_load_mode = shoot_cmd_recv.load_mode;
        shoot_publish_feedback();
        return;
    }

    dji_motor_enable(friction_l);
    dji_motor_enable(friction_r);
    dji_motor_enable(loader);

    /* 本地哨兵扳机语义:松开后延时 1 s 停轮；待发队列未清时持续保速。 */
    if (shoot_cmd_recv.friction_mode == FRICTION_ON || loader_pending > 0)
        friction_run_ms = now_ms;
    friction_running =
        (shoot_cmd_recv.friction_mode == FRICTION_ON || loader_pending > 0 ||
         (friction_running && now_ms - friction_run_ms < SHOOT_FRICTION_STOP_DELAY_MS))
            ? 1u
            : 0u;

    /* 1. 拨盘模式 */
    switch (shoot_cmd_recv.load_mode)
    {
    case LOADER_STOP:
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0.0f);
        burst_tracking = 0;
        if (loader_pending == 0)
        {
            loader_jammed = 0;
            loader_jam_ms = 0.0f;
        }
        break;

    case LOADER_SINGLE:
    case LOADER_BURST3:
        if (last_load_mode != shoot_cmd_recv.load_mode && loader_pending == 0 && !loader_jammed)
        {
            int8_t bullets = (shoot_cmd_recv.load_mode == LOADER_SINGLE) ? 1 : 3;
            heat_guard_state_e state = heat_guard_get_state();
            if (state == HEAT_GUARD_HOLD)
            {
                LOG_WARN("[shoot] heat HOLD, %d-bullet request rejected", (int)bullets);
            }
            else
            {
                if (bullets > 1 && state == HEAT_GUARD_SINGLE)
                {
                    bullets = 1;
                    LOG_WARN("[shoot] heat SINGLE, 3-bullet degraded to 1");
                }
                loader_pending = bullets;
                loader_wait_ms = now_ms;
            }
        }
        break;

    case LOADER_BURST:
    {
        loader_pending = 0;
        loader_step_active = 0;
        loader_wait_ms = 0.0f;
        dji_motor_outer_loop(loader, SPEED_LOOP);
        if (loader_jammed)
        {
            dji_motor_set_ref(loader, 0.0f);
            burst_tracking = 0;
            break;
        }

        float fire_rate = 0.0f;
        if (shoot_friction_ready() && heat_guard_allow_burst(shoot_cmd_recv.shoot_rate, &fire_rate))
        {
            if (!isfinite(fire_rate) || fire_rate > SHOOT_RATE_MAX)
            {
                LOG_ERROR("[shoot] illegal burst rate, clamped to 0");
                fire_rate = 0.0f;
            }
            if (fire_rate < 0.0f)
                fire_rate = 0.0f;
            dji_motor_set_ref(loader,
                              fire_rate * SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION);

            const float slot_motor =
                SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION;
            if (!burst_tracking)
            {
                burst_prev_angle = loader->measure.total_angle;
                burst_acc_angle = 0.0f;
                burst_tracking = 1;
            }
            burst_acc_angle += fabsf(loader->measure.total_angle - burst_prev_angle);
            burst_prev_angle = loader->measure.total_angle;
            if (!isfinite(burst_acc_angle) || burst_acc_angle > slot_motor * 20.0f)
                burst_acc_angle = 0.0f;
            uint8_t commits = 0;
            while (burst_acc_angle >= slot_motor && commits < 20u)
            {
                burst_acc_angle -= slot_motor;
                heat_guard_commit_shot();
                commits++;
            }
            if (burst_acc_angle >= slot_motor)
                burst_acc_angle = 0.0f;

            if (shoot_loader_jam_tick(loader, dt_ms))
            {
                loader_jammed = 1;
                burst_tracking = 0;
                dji_motor_set_ref(loader, 0.0f);
                LOG_ERROR("[shoot] loader jammed, output latched off; release dial to reset");
            }
        }
        else
        {
            dji_motor_set_ref(loader, 0.0f);
            loader_jam_ms = 0.0f;
            burst_tracking = 0;
        }
        break;
    }

    case LOADER_REVERSE:
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0.0f);
        burst_tracking = 0;
        break;

    default:
        LOG_ERROR("[shoot] unknown load mode %d, forced stop", (int)shoot_cmd_recv.load_mode);
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0.0f);
        break;
    }
    last_load_mode = shoot_cmd_recv.load_mode;

    /* 2. 摩擦轮关闭宽限后丢弃待发队列 */
    if (shoot_cmd_recv.friction_mode == FRICTION_OFF && !friction_running)
    {
        if (friction_off_ms <= 0.0f)
            friction_off_ms = now_ms;
        if (loader_pending > 0 && now_ms - friction_off_ms > LOADER_FRICTION_OFF_GRACE_MS)
        {
            LOG_INFO("[shoot] friction off, %d pending bullet(s) dropped", (int)loader_pending);
            loader_pending = 0;
            loader_step_active = 0;
            loader_wait_ms = 0.0f;
        }
    }
    else
        friction_off_ms = 0.0f;

    /* 3. 禁发区丢掉未起步的待发弹 */
    if (loader_pending > 0 && !loader_step_active && !loader_jammed &&
        heat_guard_get_state() == HEAT_GUARD_HOLD)
    {
        LOG_WARN("[shoot] heat HOLD, %d pending bullet(s) dropped", (int)loader_pending);
        loader_pending = 0;
        loader_wait_ms = 0.0f;
        dji_motor_outer_loop(loader, SPEED_LOOP);
        dji_motor_set_ref(loader, 0.0f);
    }

    /* 4. 单发/三发状态机 */
    if (loader_pending > 0)
    {
        const float step_motor = SHOOT_ONE_BULLET_DELTA_ANGLE * SHOOT_LOADER_REDUCTION;
        dji_motor_outer_loop(loader, ANGLE_LOOP);
        if (!loader_step_active)
        {
            uint8_t friction_ok = shoot_friction_ready();
            if (friction_ok && heat_guard_allow_single())
            {
                loader_step_target = loader->measure.total_angle + step_motor;
                if (!isfinite(loader_step_target))
                {
                    LOG_ERROR("[shoot] illegal loader step target, request dropped");
                    loader_pending = 0;
                    loader_wait_ms = now_ms;
                }
                else
                {
                    loader_step_active = 1;
                    loader_step_ms = now_ms;
                    loader_jam_ms = 0.0f;
                }
            }
            else if (!friction_ok && now_ms - loader_wait_ms > LOADER_WAIT_TIMEOUT_MS)
            {
                LOG_ERROR("[shoot] feed wait timeout (friction not ready), %d dropped",
                         (int)loader_pending);
                loader_pending = 0;
                loader_wait_ms = 0.0f;
            }
            else if (friction_ok)
            {
                loader_wait_ms = now_ms;
            }
        }
        else
        {
            dji_motor_set_ref(loader, loader_step_target);
            if (fabsf(loader->measure.total_angle - loader_step_target) <=
                LOADER_ANGLE_TOL_DEG * SHOOT_LOADER_REDUCTION)
            {
                loader_pending--;
                loader_step_active = 0;
                loader_wait_ms = now_ms;
            }
            else if (shoot_loader_jam_tick(loader, dt_ms))
            {
                loader_jammed = 1;
                loader_pending = 0;
                loader_step_active = 0;
                loader_wait_ms = 0.0f;
                dji_motor_outer_loop(loader, SPEED_LOOP);
                dji_motor_set_ref(loader, 0.0f);
                LOG_ERROR("[shoot] loader jammed during feed, queue aborted");
            }

            if (loader_step_active && now_ms - loader_step_ms > LOADER_SINGLE_TIMEOUT_MS)
            {
                LOG_ERROR("[shoot] single feed timeout, %d aborted", (int)loader_pending);
                loader_pending = 0;
                loader_step_active = 0;
                loader_wait_ms = 0.0f;
            }
        }
        if (loader_pending == 0 && loader_step_active == 0 && !loader_jammed)
        {
            dji_motor_outer_loop(loader, SPEED_LOOP);
            dji_motor_set_ref(loader, 0.0f);
        }
    }

    /* 5. 摩擦轮 */
    if (shoot_cmd_recv.friction_mode == FRICTION_ON || loader_pending > 0)
    {
        friction_running = 1u;
        friction_run_ms = now_ms;
    }
    else if (friction_running && now_ms - friction_run_ms >= SHOOT_FRICTION_STOP_DELAY_MS)
    {
        friction_running = 0u;
    }

    if (friction_running)
    {
        dji_motor_set_ref(friction_l, SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF * FW_RPM_TO_DEG_S);
        dji_motor_set_ref(friction_r, SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF * FW_RPM_TO_DEG_S);
    }
    else
    {
        dji_motor_set_ref(friction_l, 0.0f);
        dji_motor_set_ref(friction_r, 0.0f);
    }

    telemetry_push(&telemetry_shoot, SENTRY_SHOOT_FRICTION_TARGET_RPM_EFF,
                   friction_l->measure.speed_aps / FW_RPM_TO_DEG_S,
                   friction_r->measure.speed_aps / FW_RPM_TO_DEG_S,
                   loader->measure.speed_aps / FW_RPM_TO_DEG_S,
                   (uint8_t)shoot_cmd_recv.friction_mode);
    shoot_publish_feedback();
}
