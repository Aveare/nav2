/**
 * @file    chassis.c
 * @brief   底盘应用:速度指令 → 全向轮运动学解算 → 电机参考
 * @layer   APP
 * @note    右手系:平面视图中 x 前进、y 向右、z 逆时针为正。
 * @note    本文件源自跃鹿 basic_framework(作者 NeoZeng,上游版本 0.1 / 2022-12-04),
 *          由 JIT CR 于 2026-10-26 完成整车适配:45° 全向轮运动学与速度环换算。
 */

#include "chassis.h"
#include "robot_def.h"
#include "power_control.h"
#include "super_cap.h"
#include "message_center.h"
#include "cmd_watchdog.h"
#include "ins_task.h" // 跟随/前馈取云台世界角速度 INS.Gyro[2]
#include "referee_task.h"

#include "general_def.h"
#include "bsp_dwt.h"
#include "referee_UI.h"
#include "arm_math.h"
#include "telemetry.h" // P1 速度估计验证:底盘反馈遥测环 + [chcal] 标定日志
#if CHASSIS_CALIB_LOG
#include "bsp_log.h" // Float2Str/LOGINFO(RTT 不支持 %f)
#endif

/* 根据robot_def.h中的宏自动计算的参数 */
#define HALF_WHEEL_BASE (WHEEL_BASE / 2.0f)   // 半轴距
#define HALF_TRACK_WIDTH (TRACK_WIDTH / 2.0f) // 半轮距
#define PERIMETER_WHEEL (RADIUS_WHEEL * 2 * PI) // 轮子周长

/* mm/s ↔ 转子 deg/s 的唯一换算系数:正解与逆解共用,禁止各自写一份(几何一改就会静默走偏) */
#define WHEEL_RATIO (360.0f * REDUCTION_RATIO_WHEEL / PERIMETER_WHEEL)

/* EstimateSpeed() 的对称消元要求四个力臂相等(即云台居中)。设了偏心就必须重推逆解,
 * 这里用静态断言把"改了偏心但忘了改逆解"挡在编译期。 */
_Static_assert(CENTER_GIMBAL_OFFSET_X == 0.0f && CENTER_GIMBAL_OFFSET_Y == 0.0f,
               "EstimateSpeed 逆解假设云台居中;设置 CENTER_GIMBAL_OFFSET 后须重推逆解公式");

/* 底盘是单例,不需要单独的结构体 */
#ifdef CHASSIS_BOARD // 底盘板用板载IMU获取底盘转动角速度
#include "can_comm.h"
#include "ins_task.h"
static CANCommInstance *chasiss_can_comm; // 双板通信
attitude_t *Chassis_IMU_data;
#endif // CHASSIS_BOARD
#ifdef ONE_BOARD
static Publisher_t *chassis_pub;            // 发布底盘反馈
static Subscriber_t *chassis_sub;           // 订阅底盘控制命令
static CmdWatchdog_t chassis_cmd_wd;        // 命令断流看门狗(仅单板)
static referee_info_t *referee_data;        // 裁判系统反馈数据(热量/血量/弹速,二期接入cmd)
static Referee_Interactive_info_t ui_data;  // UI绘制所需的机器人状态(二期填充真实模式)
static float chassis_vx_ramp, chassis_vy_ramp; // 平移斜坡状态
static float chassis_last_ms;                  // 上周期时刻,算斜坡步长用
/* 跟随外环:offset变化率低通态,作为速度阻尼项 */
static float offset_last = 0.0f, offset_rate_f = 0.0f;
static attitude_t *chassis_IMU = 0; // 云台IMU指针(与gimbal.c共用同一INS实例)
#endif // ONE_BOARD
static Chassis_Ctrl_Cmd_s chassis_cmd_recv;         // 收到的控制命令
static Chassis_Upload_Data_s chassis_feedback_data; // 回传的反馈数据
/* 轮速逆解出的底盘真实速度(单写者=ChassisTask)。
 * 刻意**不塞进 Chassis_Upload_Data_s**:那是对板协议结构体,有布局守卫、双板必须同步;
 * 当前消费者只有遥测与后续功率控制(同任务),先留文件内,需要跨任务时再单独加字段。 */
static float est_vx, est_vy, est_wz; // mm/s, mm/s, deg/s
#if CHASSIS_CALIB_LOG
static uint16_t chassis_calib_cnt; // 标定日志分频计数(200Hz / 100 = 0.5s)
#endif

static DJIMotorInstance *motor_lf, *motor_rf, *motor_lb, *motor_rb; // left right forward back

/* 私有中介变量,设为静态避免参数传递开销 */
static float chassis_vx, chassis_vy; // 云台系速度投影到底盘系
static float vt_lf, vt_rf, vt_lb, vt_rb; // 解算后的轮速,待限幅

void ChassisInit()
{
#if defined(ONE_BOARD) && !CHASSIS_TUNING_ONLY
    /* 云台角速度前馈需要IMU姿态;INS_Init幂等,返回与gimbal.c相同的实例 */
    chassis_IMU = INS_Init();
#endif
    // 四个轮子参数一样:CAN1 左上1、右上2、左下3、右下4
    Motor_Init_Config_s chassis_motor_config = {
        .can_init_config.can_handle = &hcan1,
        .controller_param_init_config = {
            .speed_PID = {
                .Kp = 0.75f, // 速度反馈为deg/s域(M3508实测)
                .Ki = 1.0f,  // 消除纯P速度跟踪滞后
                .Kd = 0,
                .IntegralLimit = 3000,
                .Improve = PID_Trapezoid_Intergral | PID_Integral_Limit | PID_Derivative_On_Measurement,
                .MaxOut = 10000,
                .Output_LPF_RC = 0.3,
            },
        },
        .controller_setting_init_config = {
            .angle_feedback_source = MOTOR_FEED,
            .speed_feedback_source = MOTOR_FEED,
            .outer_loop_type = SPEED_LOOP,
            .close_loop_type = SPEED_LOOP,
        },
        .motor_type = M3508,
    };
    chassis_motor_config.can_init_config.tx_id = 1;
    chassis_motor_config.controller_setting_init_config.motor_reverse_flag = MOTOR_DIRECTION_NORMAL;
    motor_lf = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 2;
    motor_rf = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 3;
    motor_lb = DJIMotorInit(&chassis_motor_config);

    chassis_motor_config.can_init_config.tx_id = 4;
    motor_rb = DJIMotorInit(&chassis_motor_config);
    DJIMotorStop(motor_lf);
    DJIMotorStop(motor_rf);
    DJIMotorStop(motor_lb);
    DJIMotorStop(motor_rb);

    /* 裁判系统UI任务注册(DEV-20261030-12一期:仅通链路,UI内为假数据循环RobotModeTest,
     * 硬件联调通过后接真实模式数据再关测试函数)。硬件未接时MyUIInit会等robot_id,不阻塞其他任务 */
    if (!CHASSIS_TUNING_ONLY)
        referee_data = UITaskInit(&huart6, &ui_data);

#if 0 // 超电未接;功率控制用power_control模块,接入时在此注册
    SuperCap_Init_Config_s cap_conf = {
        .can_config = {
            .can_handle = &hcan2,
            .tx_id = 0x302,
            .rx_id = 0x301,
        }};
    cap = SuperCapInit(&cap_conf);
#endif

#ifdef CHASSIS_BOARD
    Chassis_IMU_data = INS_Init();

    CANComm_Init_Config_s comm_conf = {
        .can_config = {
            .can_handle = &hcan2,
            .tx_id = 0x311,
            .rx_id = 0x312,
        },
        .recv_data_len = sizeof(Chassis_Ctrl_Cmd_s),
        .send_data_len = sizeof(Chassis_Upload_Data_s),
    };
    chasiss_can_comm = CANCommInit(&comm_conf);
#endif // CHASSIS_BOARD

#ifdef ONE_BOARD
    chassis_sub = SubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
    chassis_pub = PubRegister("chassis_feed", sizeof(Chassis_Upload_Data_s));
#endif // ONE_BOARD
}

/* 四轮旋转力臂(mm->rad) */
#define LF_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RF_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define LB_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RB_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)

#ifdef ONE_BOARD
/**
 * @brief 斜坡限速:单周期最大变化量=限加速度x dt
 * @note  消除阶跃指令在纯P速度环下的电流阶跃(起步冲击/急停)
 */
static float RampTo(float cur, float target, float accel, float dt)
{
    float step = accel * dt;
    float d = target - cur;
    if (d > step)
        d = step;
    if (d < -step)
        d = -step;
    return cur + d;
}
#endif

/**
 * @brief 45°全向轮正运动学解算:输入mm/s与deg/s,输出速度环所需转子deg/s
 * @note  vx/vy系数整体取反是实车标定结果(本车轮布局/电调拨码下的正方向);
 *        改动轮布局或拨码后须重新走方向标定流程
 */
static void MecanumCalculate()
{
    vt_lf = (-chassis_vx - chassis_vy - chassis_cmd_recv.wz * LF_CENTER) * 0.70710678f * WHEEL_RATIO;
    vt_rf = ( chassis_vx - chassis_vy - chassis_cmd_recv.wz * RF_CENTER) * 0.70710678f * WHEEL_RATIO;
    vt_lb = (-chassis_vx + chassis_vy - chassis_cmd_recv.wz * LB_CENTER) * 0.70710678f * WHEEL_RATIO;
    vt_rb = ( chassis_vx + chassis_vy - chassis_cmd_recv.wz * RB_CENTER) * 0.70710678f * WHEEL_RATIO;
}

/**
 * @brief 四轮等比例限幅(保持平移与旋转比例)并设置电机参考值
 * @note  功率限制待接入裁判系统后添加,接口见power_control模块
 */
static void LimitChassisOutput()
{
    float max_speed = fmaxf(fmaxf(fabsf(vt_lf), fabsf(vt_rf)), fmaxf(fabsf(vt_lb), fabsf(vt_rb)));
    float limit = CHASSIS_MAX_MOTOR_RPM * RPM_2_ANGLE_PER_SEC;
    if (max_speed > limit)
    {
        float scale = limit / max_speed;
        vt_lf *= scale;
        vt_rf *= scale;
        vt_lb *= scale;
        vt_rb *= scale;
    }

    DJIMotorSetRef(motor_lf, vt_lf);
    DJIMotorSetRef(motor_rf, vt_rf);
    DJIMotorSetRef(motor_lb, vt_lb);
    DJIMotorSetRef(motor_rb, vt_rb);
}

/**
 * @brief 45°全向轮**逆**运动学:由四轮实测转速解算底盘真实速度(mm/s 与 deg/s)
 * @note  与 MecanumCalculate() 共用同一组几何宏(LF/RF/LB/RB_CENTER)与 WHEEL_RATIO,
 *        两处若各写一份,几何一改就会静默走偏。
 *        推导:正解里 `vt = (…- wz*L) * 0.7071 * WHEEL_RATIO`,反解时先除掉 WHEEL_RATIO*0.7071 得到
 *        四轮等效线速度 u,再用对称组合消元。**该消元假设云台居中**(四个 L 相等),
 *        设了 CENTER_GIMBAL_OFFSET 就必须重推,故下方有静态断言兜住。
 */
static void EstimateSpeed()
{
    /* 实测轮速(转子 deg/s)→ 四轮等效线速度 mm/s */
    float u_lf = motor_lf->measure.speed_aps / (WHEEL_RATIO * 0.70710678f);
    float u_rf = motor_rf->measure.speed_aps / (WHEEL_RATIO * 0.70710678f);
    float u_lb = motor_lb->measure.speed_aps / (WHEEL_RATIO * 0.70710678f);
    float u_rb = motor_rb->measure.speed_aps / (WHEEL_RATIO * 0.70710678f);

    est_vx = ((u_rf + u_rb) - (u_lf + u_lb)) * 0.25f;
    est_vy = ((u_lb + u_rb) - (u_lf + u_rf)) * 0.25f;
    /* wz 单位 deg/s:LF_CENTER 已含 DEGREE_2_RAD,与正解量纲一致 */
    est_wz = -(u_lf + u_rf + u_lb + u_rb) / (4.0f * LF_CENTER);
}

/* 机器人底盘控制核心任务(200Hz) */
void ChassisTask()
{
    // 获取控制命令(收到才算"命令在流",未收到由看门狗兜底)
    const float cmd_now_ms = DWT_GetTimeline_ms();
#ifdef ONE_BOARD
    /* 取消息与喂狗用同一时刻,避免两次调用间时间推进造成的边界抖动 */
    if (SubGetMessage(chassis_sub, &chassis_cmd_recv))
        CmdWatchdogFeed(&chassis_cmd_wd, cmd_now_ms);
#endif
#ifdef CHASSIS_BOARD
    // 双板:数据来自CAN,断流由can_comm自带daemon判定
    chassis_cmd_recv = *(Chassis_Ctrl_Cmd_s *)CANCommGet(chasiss_can_comm);
#endif

    // 按模式设定旋转速度
    switch (chassis_cmd_recv.chassis_mode)
    {
    case CHASSIS_NAVIGATION: // USB 导航底盘速度,保留 wz
        break;
    case CHASSIS_NO_FOLLOW: // 不旋转,保持全向机动
        chassis_cmd_recv.wz = 0;
        break;
    case CHASSIS_RC_CONTROL: // 手柄直接给定底盘转向速度
        LIMIT_MIN_MAX(chassis_cmd_recv.wz, -CHASSIS_ROTATE_SPEED, CHASSIS_ROTATE_SPEED);
        break;
    case CHASSIS_FOLLOW_GIMBAL_YAW: // 跟随云台:wz=位置项+速度阻尼项+角速度前馈
    {
        /* 位置项:软死区+小角度线性/大角度平方律。
         * offset取自yaw电机单圈ecd且电机被IMU闭环推着走,静止残差±1.4°永不归零;
         * |offset|<死区时位置项清零,超出后先减死区再进律——边界处输出连续,无跳变。 */
        float off = chassis_cmd_recv.offset_angle;
        float mag = fabsf(off);
        float wz_pos = 0.0f;
        if (mag >= CHASSIS_FOLLOW_DEADBAND)
        {
            float eff = mag - CHASSIS_FOLLOW_DEADBAND; // 死区外有效角,边界连续
            float sgn = (off > 0.0f) ? 1.0f : -1.0f;
            if (eff < CHASSIS_FOLLOW_LIN_RANGE)
                wz_pos = CHASSIS_FOLLOW_KP * CHASSIS_FOLLOW_LIN_RANGE * eff * sgn;
            else
                wz_pos = CHASSIS_FOLLOW_KP * eff * eff * sgn;
        }
        /* 速度阻尼项:不受死区门控(否则云台起步时底盘被禁用,"慢一拍");
         * 仅当量小于噪声门限时忽略,静止不产生wz。 */
        float spd = CHASSIS_FOLLOW_DAMP * offset_rate_f;
        if (fabsf(spd) < CHASSIS_FOLLOW_SPD_MIN)
            spd = 0.0f;

        /* 角速度前馈:INS.Gyro[2]是云台绕世界yaw的真实角速度,无微分延迟;
         * 云台一转底盘即刻同速转,offset几乎不建立。
         * 符号:若推杆后偏得更厉害(发散),把CHASSIS_FOLLOW_FF改成负值即可。 */
        float ff = 0.0f;
        if (chassis_IMU != 0)
            ff = CHASSIS_FOLLOW_FF * chassis_IMU->Gyro[Z] * RAD_2_DEGREE;

        chassis_cmd_recv.wz = wz_pos + spd + ff;
        LIMIT_MIN_MAX(chassis_cmd_recv.wz, -CHASSIS_ROTATE_SPEED, CHASSIS_ROTATE_SPEED);
        break;
    }
    case CHASSIS_ROTATE: // 小陀螺:恒速自旋,同时保持全向机动
        chassis_cmd_recv.wz = CHASSIS_ROTATE_SPEED;
        break;
    default:
        break;
    }

#ifdef ONE_BOARD
    /* 命令断流保护:CMD任务卡死/断流时底盘不得按最后一帧速度行驶 */
    if (CmdWatchdogExpired(&chassis_cmd_wd, cmd_now_ms, CMD_WATCHDOG_TIMEOUT_MS))
    {
        chassis_cmd_recv.chassis_mode = CHASSIS_ZERO_FORCE;
        chassis_cmd_recv.vx = 0.0f;
        chassis_cmd_recv.vy = 0.0f;
        chassis_cmd_recv.wz = 0.0f;
    }

    /* 小陀螺平移方向偏置(DEV-20261103-03):自转存在固定方向的漂移(实测正转向右微漂),
     * 平移指令预先旋转固定补偿角使轨迹回正;仅ROTATE模式生效,斜坡之前施加(斜坡作用于补偿后指令) */
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ROTATE && CHASSIS_TRANSLATE_BIAS_ANGLE != 0.0f)
    {
        const float bias_rad = CHASSIS_TRANSLATE_BIAS_ANGLE * DEGREE_2_RAD;
        const float cb = arm_cos_f32(bias_rad);
        const float sb = arm_sin_f32(bias_rad);
        const float vx_bias = chassis_cmd_recv.vx * cb - chassis_cmd_recv.vy * sb;
        const float vy_bias = chassis_cmd_recv.vx * sb + chassis_cmd_recv.vy * cb;
        chassis_cmd_recv.vx = vx_bias;
        chassis_cmd_recv.vy = vy_bias;
    }

    /* 平移斜坡与offset变化率累计。
     * 单位注意:DWT毫秒 vs 加速度/s^2,必须先换算成秒(否则斜坡永冻)。
     * dt异常(>100ms)时本周期保持原值不推进。 */
    {
        float dt_ms = cmd_now_ms - chassis_last_ms;
        chassis_last_ms = cmd_now_ms;
        if (dt_ms > 0.0f && dt_ms <= 100.0f)
        {
            float dt_sec = dt_ms * 0.001f;
            const float nav_accel = SENTRY_NAVIGATION_ACCEL_MM_S2;
            chassis_vx_ramp = RampTo(chassis_vx_ramp, chassis_cmd_recv.vx,
                                     chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION ? nav_accel : CHASSIS_VX_RAMP, dt_sec);
            chassis_vy_ramp = RampTo(chassis_vy_ramp, chassis_cmd_recv.vy,
                                     chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION ? nav_accel : CHASSIS_VY_RAMP, dt_sec);

            /* offset变化率低通(约3Hz),死区内也照常累计(速度项依赖) */
            float rate = (chassis_cmd_recv.offset_angle - offset_last) / dt_sec;
            offset_rate_f += 0.1f * (rate - offset_rate_f);
        }
        offset_last = chassis_cmd_recv.offset_angle;
    }
#endif

    // 零力模式停止电机,其余使能
    if (chassis_cmd_recv.chassis_mode == CHASSIS_ZERO_FORCE)
    {
        DJIMotorStop(motor_lf);
        DJIMotorStop(motor_rf);
        DJIMotorStop(motor_lb);
        DJIMotorStop(motor_rb);
    }
    else
    {
        DJIMotorEnable(motor_lf);
        DJIMotorEnable(motor_rf);
        DJIMotorEnable(motor_lb);
        DJIMotorEnable(motor_rb);
    }

    // 云台系速度按offset角旋转投影到底盘系(底盘逆时针旋转为正)
    static float sin_theta, cos_theta;
    const float nav_theta = (chassis_cmd_recv.chassis_mode == CHASSIS_NAVIGATION) ? 0.0f : (chassis_cmd_recv.offset_angle * DEGREE_2_RAD);
    cos_theta = arm_cos_f32(nav_theta);
    sin_theta = arm_sin_f32(nav_theta);
    chassis_vx = chassis_vx_ramp * cos_theta + chassis_vy_ramp * sin_theta;
    chassis_vy = -chassis_vx_ramp * sin_theta + chassis_vy_ramp * cos_theta;

    MecanumCalculate();

    LimitChassisOutput();

    EstimateSpeed();

    /* 底盘反馈遥测(单写者=本任务):ref=vx指令,fb=逆解vx,out=逆解vy,extra=逆解wz */
    TelemetryPush(&telemetry_chassis_fb, chassis_cmd_recv.vx, est_vx,
                  est_vy, est_wz,
                  (uint8_t)chassis_cmd_recv.chassis_mode);
#if CHASSIS_CALIB_LOG
    /* 标定日志(P1 速度估计验证用;P2 再加 p/scale 两列):
     * gz = IMU 世界系 yaw 角速度。**注意它不能当底盘 wz 用**——小陀螺瞄准保持时云台世界系静止,
     * 装在云台上的 IMU 读到的就是 0;只有"不跟随 + 不自转"时 gz 才等于底盘转速。
     * 自转工况的真实转速靠**计时法**(转 10 圈取平均)独立测量,见 .Doc/底盘优化计划.md V3。 */
    if (++chassis_calib_cnt >= 100) // 200Hz / 100 = 0.5s
    {
        char c_vx[16], c_vy[16], c_wz[16], c_gz[16], c_be[16];
        chassis_calib_cnt = 0;
        Float2Str(c_vx, est_vx);
        Float2Str(c_vy, est_vy);
        Float2Str(c_wz, est_wz);
        Float2Str(c_gz, chassis_IMU ? chassis_IMU->Gyro[Z] * RAD_2_DEGREE : 0.0f);
        Float2Str(c_be, referee_data ? (float)referee_data->PowerHeatData.buffer_energy : -1.0f);
        LOGINFO("[chcal] vx=%s vy=%s wz=%s gz=%s be=%s", c_vx, c_vy, c_wz, c_gz, c_be);
    }
#endif

#ifdef ONE_BOARD
    PubPushMessage(chassis_pub, (void *)&chassis_feedback_data);
#endif
#ifdef CHASSIS_BOARD
    CANCommSend(chasiss_can_comm, (void *)&chassis_feedback_data);
#endif
}
