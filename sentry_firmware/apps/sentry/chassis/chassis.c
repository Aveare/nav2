/**
 * @file    chassis.c
 * @brief   哨兵底盘应用:模式仲裁、大 yaw 跟随律与麦轮解算
 * @layer   APP
 * @author  小猫大王
 * @note    右手系:平面视图 x 前进、y 向右、z 逆时针为正。
 * @note    跟随目标 offset 来自大 yaw 相对底盘机械零位;大 yaw 世界环保持瞄准方向,
 *          底盘转动只改变关节角,不会带走枪口世界指向。
 */
#include "chassis.h"
#include "chassis_internal.h"
#include "chassis_power.h"
#include "robot_def.h"
#include "sentry_def.h"
#include "msg.h"
#include "cmd_watchdog.h"
#include "ins_task.h"
#include "referee_task.h"
#include "general_def.h"
#include "dwt_port.h"
#include "telemetry.h"
#include "log_port.h"
#include <math.h>

/* 逆解消元要求四个力臂相等(大 yaw 居中);设了偏心必须重推逆解。 */
_Static_assert(CHASSIS_GIMBAL_OFFSET_X == 0.0f && CHASSIS_GIMBAL_OFFSET_Y == 0.0f,
               "逆解假设大 yaw 居中;设置 CHASSIS_GIMBAL_OFFSET 后须重推逆解公式");

static msg_pub_t chassis_pub;
static msg_sub_t chassis_sub;
static cmd_watchdog_t chassis_cmd_wd;
static referee_interactive_info_t ui_data;
static referee_info_t *referee_data;
static attitude_t *chassis_imu;

static float chassis_vx_ramp, chassis_vy_ramp;
static float chassis_last_ms;

chassis_ctrl_cmd_t chassis_cmd_recv;
dji_motor_t *motor_lf, *motor_rf, *motor_lb, *motor_rb;
float chassis_vx, chassis_vy;
float vt_lf, vt_rf, vt_lb, vt_rb;
float est_vx, est_vy, est_wz;

static sentry_chassis_feed_t chassis_fb;

/* 运行期可写(ELF/DWARF):导航平移斜坡 mm/s^2;默认=config。 */
volatile float sentry_rt_nav_accel_mm_s2 = SENTRY_NAVIGATION_ACCEL_MM_S2;

void chassis_init(void)
{
    chassis_imu = ins_init(); // 幂等:与 gimbal 共用同一 ins_state 实例

    motor_init_config_t chassis_motor_config = {
        .can_init_config.can_handle = &hcan1,
        .controller_param_init_config =
            {
                .speed_PID =
                    {
                        .Kp = CHASSIS_SPEED_PID_KP,
                        .Ki = CHASSIS_SPEED_PID_KI,
                        .Kd = 0,
                        .IntegralLimit = CHASSIS_SPEED_PID_IL,
                        .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit |
                                   PID_Derivative_On_Measurement,
                        .MaxOut = CHASSIS_SPEED_PID_MAXOUT,
                        .Output_LPF_RC = CHASSIS_SPEED_PID_RC,
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
        .motor_type = M3508,
    };

    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_LF;
    motor_lf = dji_motor_init(&chassis_motor_config);
    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_RF;
    motor_rf = dji_motor_init(&chassis_motor_config);
    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_LB;
    motor_lb = dji_motor_init(&chassis_motor_config);
    chassis_motor_config.can_init_config.tx_id = CHASSIS_MOTOR_ID_RB;
    motor_rb = dji_motor_init(&chassis_motor_config);
    dji_motor_stop(motor_lf);
    dji_motor_stop(motor_rf);
    dji_motor_stop(motor_lb);
    dji_motor_stop(motor_rb);

    /* 裁判系统接收注册(USART6);ui_enable=false 时不创建 UI 绘制任务,
     * 但数据表仍由串口中断持续填充,供热量守卫/功率监控读取。 */
#if !YAW_AUTO_VOFA_USART6_ENABLE
    referee_data = referee_ui_task_init(&huart6, &ui_data);
#endif
#if CHASSIS_POWER_CTRL_EN
    chassis_power_reset();
#endif

    chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    chassis_pub = msg_pub("chassis_feed", sizeof(sentry_chassis_feed_t));

    chassis_cmd_recv.chassis_mode = CHASSIS_ZERO_FORCE;
}

void chassis_task(void)
{
    const float now_ms = dwt_get_timeline_ms();
    if (msg_recv(chassis_sub, &chassis_cmd_recv))
        cmd_watchdog_feed(&chassis_cmd_wd, now_ms);

    /* 1. 模式解算 */
    switch (chassis_cmd_recv.chassis_mode)
    {
    case CHASSIS_NAVIGATION:
        break; /* 已是底盘速度,保留导航 wz。 */

    case CHASSIS_NO_FOLLOW:
        MOTOR_LIMIT_MIN_MAX(chassis_cmd_recv.wz, -CHASSIS_ROTATE_SPEED, CHASSIS_ROTATE_SPEED);
        break;

    case CHASSIS_RC_CONTROL:
        MOTOR_LIMIT_MIN_MAX(chassis_cmd_recv.wz, -CHASSIS_ROTATE_SPEED,
                            CHASSIS_ROTATE_SPEED);
        break;

    case CHASSIS_ALIGN:
    {
        float off = chassis_cmd_recv.offset_angle;
        float sgn = (off > 0.0f) ? 1.0f : -1.0f;
        float cmd = sgn * CHASSIS_ALIGN_SPEED;
        if (fabsf(off) < CHASSIS_ALIGN_SOFT)
            cmd *= fabsf(off) / CHASSIS_ALIGN_SOFT;
        chassis_cmd_recv.wz = cmd;
        break;
    }

    case CHASSIS_FOLLOW_GIMBAL_YAW:
    {
        const float lim = (CHASSIS_FOLLOW_WZ_LIMIT > 0.0f) ? CHASSIS_FOLLOW_WZ_LIMIT : 1.0e6f;
        const float ff_cap = lim * (1.0f - CHASSIS_FOLLOW_P_RESERVE);

        float err = chassis_cmd_recv.offset_angle - CHASSIS_FOLLOW_TARGET_OFFSET;
        if (err > 180.0f)
            err -= 360.0f;
        else if (err < -180.0f)
            err += 360.0f;

        if (fabsf(err) < CHASSIS_FOLLOW_DEADBAND)
            err = 0.0f;
        else
            err -= (err > 0.0f) ? CHASSIS_FOLLOW_DEADBAND : -CHASSIS_FOLLOW_DEADBAND;

        float gz_dps = (chassis_imu != 0) ? chassis_imu->Gyro[IMU_AXIS_Z] * FW_RAD_TO_DEG : 0.0f;
        float ff = CHASSIS_FOLLOW_FF * gz_dps;
        float wz_pos = CHASSIS_FOLLOW_KP * err;

        float wz_cmd = ff + wz_pos;
        if (fabsf(wz_cmd) > lim)
        {
            MOTOR_LIMIT_MIN_MAX(ff, -ff_cap, ff_cap);
            MOTOR_LIMIT_MIN_MAX(wz_pos, -(lim - fabsf(ff)), lim - fabsf(ff));
            wz_cmd = ff + wz_pos;
        }
        MOTOR_LIMIT_MIN_MAX(wz_cmd, -lim, lim);
        chassis_cmd_recv.wz = wz_cmd;
        break;
    }

    case CHASSIS_ROTATE:
        chassis_cmd_recv.wz = CHASSIS_ROTATE_SPEED;
        break;

    default: /* CHASSIS_ZERO_FORCE:不改 wz,下方停电机 */
        break;
    }

    /* 2. 命令断流 → 零力 */
    if (cmd_watchdog_expired(&chassis_cmd_wd, now_ms, CMD_WATCHDOG_TIMEOUT_MS))
    {
        chassis_cmd_recv.chassis_mode = CHASSIS_ZERO_FORCE;
        chassis_cmd_recv.vx = 0.0f;
        chassis_cmd_recv.vy = 0.0f;
        chassis_cmd_recv.wz = 0.0f;
    }

    /* 3. 小陀螺平移偏置(默认 0,按需标定) */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE && CHASSIS_TRANSLATE_BIAS_ANGLE != 0.0f)
    {
        const float bias_rad = CHASSIS_TRANSLATE_BIAS_ANGLE * FW_DEG_TO_RAD;
        const float cb = cosf(bias_rad);
        const float sb = sinf(bias_rad);
        const float vx_bias = chassis_cmd_recv.vx * cb - chassis_cmd_recv.vy * sb;
        const float vy_bias = chassis_cmd_recv.vx * sb + chassis_cmd_recv.vy * cb;
        chassis_cmd_recv.vx = vx_bias;
        chassis_cmd_recv.vy = vy_bias;
    }

    /* 4. 平移斜坡 */
    {
        float dt_ms = now_ms - chassis_last_ms;
        chassis_last_ms = now_ms;
        if (dt_ms > 0.0f && dt_ms <= 100.0f)
        {
            float dt_sec = dt_ms * 0.001f;
            float nav_accel = sentry_rt_nav_accel_mm_s2;
            if (!(nav_accel >= 50.0f && nav_accel <= 5000.0f))
                nav_accel = SENTRY_NAVIGATION_ACCEL_MM_S2;
            const float accel = chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION
                                    ? nav_accel
                                    : CHASSIS_VX_RAMP;
            const float lateral_accel = chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION
                                            ? nav_accel
                                            : CHASSIS_VY_RAMP;
            chassis_vx_ramp =
                chassis_ramp_to(chassis_vx_ramp, chassis_cmd_recv.vx, accel, dt_sec);
            chassis_vy_ramp =
                chassis_ramp_to(chassis_vy_ramp, chassis_cmd_recv.vy, lateral_accel, dt_sec);
        }
    }

    /* 5. 电机使能/停止 */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE)
    {
        dji_motor_stop(motor_lf);
        dji_motor_stop(motor_rf);
        dji_motor_stop(motor_lb);
        dji_motor_stop(motor_rb);
    }
    else
    {
        dji_motor_enable(motor_lf);
        dji_motor_enable(motor_rf);
        dji_motor_enable(motor_lb);
        dji_motor_enable(motor_rb);
    }

    /* 6. 云台系速度旋转到底盘系 */
    {
        const float theta = chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION
                                ? 0.0f
                                : chassis_cmd_recv.offset_angle * FW_DEG_TO_RAD;
        const float ct = cosf(theta);
        const float st = sinf(theta);
        chassis_vx = chassis_vx_ramp * ct + chassis_vy_ramp * st;
        chassis_vy = -chassis_vx_ramp * st + chassis_vy_ramp * ct;
    }

    /* 7. 运动学 */
    chassis_mecanum_calculate();

    /* 8. 功率限幅:用未限幅电流指令 + 转子转速预测,scale 直接压四轮电流。
     * 裁判未接时回退 config 兜底值;缓冲能量只在收到过 0x0202 后参与衰减。 */
    float power_scale = 1.0f;
    float integral_scale = 1.0f;
#if CHASSIS_POWER_CTRL_EN
    {
        const float i_cmd[4] = {
            motor_lf->current_cmd,
            motor_rf->current_cmd,
            motor_lb->current_cmd,
            motor_rb->current_cmd,
        };
        const float w_dps[4] = {
            motor_lf->measure.speed_aps,
            motor_rf->measure.speed_aps,
            motor_lb->measure.speed_aps,
            motor_rb->measure.speed_aps,
        };
        float limit_w = CHASSIS_POWER_LIMIT_DEFAULT;
        if (referee_data != NULL &&
            (float)referee_data->GameRobotState.chassis_power_limit > 1.0f)
        {
            limit_w = (float)referee_data->GameRobotState.chassis_power_limit;
            if (limit_w > CHASSIS_POWER_LIMIT_MAX)
                limit_w = CHASSIS_POWER_LIMIT_MAX;
        }
        limit_w -= CHASSIS_POWER_MARGIN_W;
        if (limit_w < 20.0f)
            limit_w = 20.0f;
        if (referee_data != NULL && referee_data->PowerHeatTick > 0u)
        {
            float be = (float)referee_data->PowerHeatData.buffer_energy;
            if (be > 0.0f)
            {
                if (be > CHASSIS_POWER_BUFFER_MAX_J)
                    be = CHASSIS_POWER_BUFFER_MAX_J;
                const float atten = CHASSIS_POWER_BUFFER_MIN_SCALE +
                                    (1.0f - CHASSIS_POWER_BUFFER_MIN_SCALE) *
                                        (be / CHASSIS_POWER_BUFFER_MAX_J);
                limit_w *= atten;
            }
        }
        power_scale = chassis_power_scale(i_cmd, w_dps, limit_w, CHASSIS_POWER_MIN_SCALE, NULL,
                                          NULL);
    }
#endif

    chassis_limit_output(power_scale, integral_scale);
    chassis_estimate_speed();

    /* 8. 遥测与反馈 */
    telemetry_push(&telemetry_chassis_fb, chassis_cmd_recv.vx, est_vx, est_vy, est_wz,
                   (uint8_t)chassis_cmd_recv.chassis_mode);
    chassis_fb.est_vx = est_vx;
    chassis_fb.est_vy = est_vy;
    chassis_fb.est_wz = est_wz;
    chassis_fb.mode = (uint8_t)chassis_cmd_recv.chassis_mode;
    msg_publish(chassis_pub, (void *)&chassis_fb);
}
