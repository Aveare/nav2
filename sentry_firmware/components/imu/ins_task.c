/**
 * @file    ins_task.c
 * @brief   姿态解算任务:1kHz,零偏估计/温控/欧拉输出
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  annotation and modificaiton by neozng
 * @author  modified by JIT CR (2026-10-27 IMU初始化重试上限)
 * @author  modified by 小猫大王
* @version V2.0.0
* @date    2022/2/23
 */
#include "ins_task.h"
#include "controller.h"
#include "quaternion_ekf.h"
#include "imu_port.h"
#include "user_lib.h"
#include "general_def.h"
#include "log_port.h"
#include "config.h" // 参数来自 config,不依赖 application 层
#include "ax650_uart1.h"

static ins_state_t ins_state;
static ins_imu_param_t ins_imu_param;
static pid_instance_t ins_temp_ctrl = {0};

const float xb[3] = {1, 0, 0};
const float yb[3] = {0, 1, 0};
const float zb[3] = {0, 0, 1};

// 用于获取两次采样之间的时间间隔
static uint32_t ins_dwt_count = 0;
static float dt = 0, t = 0;
static float ins_ref_temp = 40; // 恒温设定温度

static void ins_param_correction(ins_imu_param_t *param, float gyro[3], float accel[3]);
static void ins_bias_track_update(float gyro[3], const float accel[3]);

/* ---------------- 静止零偏在线跟踪(ZUPT) ----------------
 * 上电静态标定(bmi088_calibrate_mpu_offset)只标一次;IMU 暖机与环境温漂使零偏残差
 * 持续存在,无磁力计时 yaw 只能积分它(实测 ~0.09°/s ≈ 6°/分,5 分钟比赛漂 30°)。
 * 本块用一阶低通先滤掉电机振动,再判"静置":低通陀螺全部低于阈值且 ‖a‖−g 够小,
 * 连续 3s 后以 ~20s 时间常数把残差吸收进二级零偏;真实机动/振动立即冻结并清零计数。
 * 二级零偏恒定作用于输出(吸收门控只作用于更新,无台阶)。已知折衷:低于 1.7°/s 且
 * 持续数分钟的真实旋转会被部分吸收——RM 工况无此操作(自转/跟随远高于阈值),接受。 */
#define INS_ZUPT_GYRO_THRESH_RAD_S 0.03f /* 单轴静置判定阈值,≈1.7°/s */
#define INS_ZUPT_ACCEL_DEV_M_S2 1.5f     /* 低通后 ‖a‖−g 容差,0.15g */
#define INS_ZUPT_HOLD_TICKS 3000u        /* 连续静置时长,1kHz 下 3s */
#define INS_ZUPT_VIBE_RC_S 0.3f          /* 振动低通时间常数,s */
#define INS_ZUPT_ALPHA 0.00005f          /* 吸收低通系数,τ≈20s @1kHz */
#define INS_ZUPT_BIAS_LIMIT_RAD_S 0.05f  /* 二级零偏绝对界(≈2.9°/s),防异常吞姿态 */

static void ins_imu_pwm_set(uint16_t pwm)
{
    imu_port_heater_set(pwm);
}

/**
 * @brief 温度控制
 *
 */
static void ins_imu_temperature_ctrl(void)
{
    pid_calculate(&ins_temp_ctrl, bmi088.Temperature, ins_ref_temp);
    ins_imu_pwm_set(math_float_clamp(math_float_round(ins_temp_ctrl.Output), 0, UINT32_MAX));
}

/**
 * @brief  静止零偏在线跟踪:gyro/accel 为本拍测量,函数原地扣除二级零偏
 * @note   吸收只在"低通静置持续达 hold"时进行;恒定输出扣除保证无台阶。
 *         单写者=ins_task;阈值/常数见文件头 ZUPT 块注释。
 */
static void ins_bias_track_update(float gyro[3], const float accel[3])
{
    static float bias[3];
    static float gyro_lp[3], accel_dev_lp;
    static uint32_t still_ticks;
    static uint8_t lp_init;

    if (!lp_init)
    {
        for (uint8_t i = 0; i < 3; ++i)
            gyro_lp[i] = gyro[i];
        lp_init = 1u;
    }
    const float alpha = dt / (INS_ZUPT_VIBE_RC_S + dt);
    const float a_norm = sqrtf(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
    const float accel_dev = fabsf(a_norm - 9.81f);

    uint8_t still = 1u;
    for (uint8_t i = 0; i < 3; ++i)
    {
        gyro_lp[i] += alpha * (gyro[i] - gyro_lp[i]);
        if (fabsf(gyro_lp[i]) > INS_ZUPT_GYRO_THRESH_RAD_S)
            still = 0u;
    }
    accel_dev_lp += alpha * (accel_dev - accel_dev_lp);
    if (accel_dev_lp > INS_ZUPT_ACCEL_DEV_M_S2)
        still = 0u;

    if (!still)
        still_ticks = 0u;
    else if (++still_ticks >= INS_ZUPT_HOLD_TICKS)
    {
        for (uint8_t i = 0; i < 3; ++i)
        {
            bias[i] += INS_ZUPT_ALPHA * (gyro[i] - bias[i]);
            if (bias[i] > INS_ZUPT_BIAS_LIMIT_RAD_S)
                bias[i] = INS_ZUPT_BIAS_LIMIT_RAD_S;
            else if (bias[i] < -INS_ZUPT_BIAS_LIMIT_RAD_S)
                bias[i] = -INS_ZUPT_BIAS_LIMIT_RAD_S;
        }
    }

    for (uint8_t i = 0; i < 3; ++i)
        gyro[i] -= bias[i];
}

// 使用加速度计的数据初始化Roll和Pitch,而Yaw置0,这样可以避免在初始时候的姿态估计误差
static void ins_init_quaternion(float *init_q4)
{
    float acc_init[3] = {0};
    float gravity_norm[3] = {0, 0, 1}; // 导航系重力加速度矢量,归一化后为(0,0,1)
    float axis_rot[3] = {0};           // 旋转轴
    // 读取100次加速度计数据,取平均值作为初始值
    for (uint8_t i = 0; i < 100; ++i)
    {
        bmi088_read(&bmi088);
        acc_init[IMU_AXIS_X] += bmi088.Accel[IMU_AXIS_X];
        acc_init[IMU_AXIS_Y] += bmi088.Accel[IMU_AXIS_Y];
        acc_init[IMU_AXIS_Z] += bmi088.Accel[IMU_AXIS_Z];
        dwt_delay(0.001);
    }
    for (uint8_t i = 0; i < 3; ++i)
        acc_init[i] /= 100;
    math_norm3d(acc_init);
    // 计算原始加速度矢量和导航系重力加速度矢量的夹角
    float angle = acosf(fmaxf(-1.0f, fminf(1.0f, math_dot3d(acc_init, gravity_norm))));
    math_cross3d(acc_init, gravity_norm, axis_rot);
    // 平放时旋转轴为零；只修正原初始化函数，不新增姿态接口或状态机。
    if (math_dot3d(axis_rot, axis_rot) < 1e-8f)
    {
        init_q4[0] = acc_init[IMU_AXIS_Z] >= 0 ? 1.0f : 0.0f;
        init_q4[1] = acc_init[IMU_AXIS_Z] >= 0 ? 0.0f : 1.0f;
        init_q4[2] = init_q4[3] = 0.0f;
        return;
    }
    math_norm3d(axis_rot);
    init_q4[0] = cosf(angle / 2.0f);
    for (uint8_t i = 0; i < 2; ++i)
        init_q4[i + 1] = axis_rot[i] * sinf(angle / 2.0f); // 轴角公式,第三轴为0(没有z轴分量)
}

attitude_t *ins_init(void)
{
    if (!ins_state.init)
        ins_state.init = 1;
    else
        return (attitude_t *)&ins_state.Gyro;

    (void)imu_port_heater_start();

    // IMU初始化重试上限:连续失败50次说明硬件故障,LOG_ERROR后挂起(初始化期fail-fast)。
    // 注意本函数可能处于robot.c的__disable_irq区间内(经gimbal_init调用),此时HAL超时依赖的
    // SysTick被屏蔽,while无上限会永久无提示挂死。fail-fast为此处唯一安全出路:
    // 无姿态数据时云台/底盘闭环不可用,继续运行必然失控(启动期fail-fast例外,见coding-contract)。
    uint16_t imu_init_retry = 0;
    while (bmi088_init(1) != BMI088_NO_ERROR)
    {
        if (++imu_init_retry > 50)
        {
            LOG_ERROR("[ins] BMI088 init failed after %u retries, entering safe stop",
                     imu_init_retry);
            /* T02-WHITELIST: 启动期 fail-fast(白名单 4 类 10 点;B0 台账键控,B3 锚注) */ while (1)
                ;
        }
    }
    ins_imu_param.scale[IMU_AXIS_X] = 1;
    ins_imu_param.scale[IMU_AXIS_Y] = 1;
    ins_imu_param.scale[IMU_AXIS_Z] = 1;
    ins_imu_param.Yaw = 0;
    ins_imu_param.Pitch = 0;
    ins_imu_param.Roll = 0;
    ins_imu_param.flag = 1;

    float init_quaternion[4] = {0};
    ins_init_quaternion(init_quaternion);
    ekf_imu_quaternion_ekf_init(init_quaternion, 10, 0.001, 1000000, 1, 0);
    // imu heat init
    pid_init_config_t config = {.MaxOut = 2000,
                                .IntegralLimit = 300,
                                .DeadBand = 0,
                                .Kp = 1000,
                                .Ki = 20,
                                .Kd = 0,
                                .Improve = 0x01}; // enable integratiaon limit
    pid_init(&ins_temp_ctrl, &config);

    // noise of accel is relatively big and of high freq,thus lpf is used
    ins_state.AccelLPF = 0.0085;
    dwt_get_delta_t(&ins_dwt_count);
    return (attitude_t *)&ins_state
        .Gyro; // @todo: 这里偷懒了,不要这样做! 修改INT_t结构体可能会导致异常,待修复.
}

/* 注意以1kHz的频率运行此任务 */
void ins_task(void)
{
    static uint32_t count = 0;
    const float gravity[3] = {0, 0, 9.81f};

    dt = dwt_get_delta_t(&ins_dwt_count);
    t += dt;

    // ins update
    if ((count % 1) == 0)
    {
        bmi088_read(&bmi088);

        ins_state.Accel[IMU_AXIS_X] = bmi088.Accel[IMU_AXIS_X];
        ins_state.Accel[IMU_AXIS_Y] = bmi088.Accel[IMU_AXIS_Y];
        ins_state.Accel[IMU_AXIS_Z] = bmi088.Accel[IMU_AXIS_Z];
        ins_state.Gyro[IMU_AXIS_X] = bmi088.Gyro[IMU_AXIS_X];
        ins_state.Gyro[IMU_AXIS_Y] = bmi088.Gyro[IMU_AXIS_Y];
        ins_state.Gyro[IMU_AXIS_Z] = bmi088.Gyro[IMU_AXIS_Z];

        // 静止零偏在线跟踪:原地扣除温漂残差,EKF 与全部消费者读到的是修正后角速度
        ins_bias_track_update(ins_state.Gyro, ins_state.Accel);

        // demo function,用于修正安装误差,可以不管,本demo暂时没用
        ins_param_correction(&ins_imu_param, ins_state.Gyro, ins_state.Accel);

        // 计算重力加速度矢量和b系的XY两轴的夹角,可用作功能扩展,本demo暂时没用

        // 核心函数,EKF更新四元数
        ekf_imu_quaternion_ekf_update(ins_state.Gyro[IMU_AXIS_X], ins_state.Gyro[IMU_AXIS_Y], ins_state.Gyro[IMU_AXIS_Z], ins_state.Accel[IMU_AXIS_X], ins_state.Accel[IMU_AXIS_Y],
                                 ins_state.Accel[IMU_AXIS_Z], dt);

        memcpy(ins_state.q, ekf_ins.q, sizeof(ekf_ins.q));
#if AX650_UART1_ENABLE
        ax650_uart1_send_quaternion(ins_state.q);
#endif

        // 机体系基向量转换到导航坐标系，本例选取惯性系为导航系
        ins_body_to_earth(xb, ins_state.xn, ins_state.q);
        ins_body_to_earth(yb, ins_state.yn, ins_state.q);
        ins_body_to_earth(zb, ins_state.zn, ins_state.q);

        // 将重力从导航坐标系n转换到机体系b,随后根据加速度计数据计算运动加速度
        float gravity_b[3];
        ins_earth_to_body(gravity, gravity_b, ins_state.q);
        for (uint8_t i = 0; i < 3; ++i) // 同样过一个低通滤波
        {
            ins_state.MotionAccel_b[i] = (ins_state.Accel[i] - gravity_b[i]) * dt / (ins_state.AccelLPF + dt) +
                                   ins_state.MotionAccel_b[i] * ins_state.AccelLPF / (ins_state.AccelLPF + dt);
        }
        ins_body_to_earth(ins_state.MotionAccel_b, ins_state.MotionAccel_n, ins_state.q); // 转换回导航系n

        ins_state.Yaw = ekf_ins.Yaw;
        ins_state.Pitch = ekf_ins.Pitch;
        ins_state.Roll = ekf_ins.Roll;
        ins_state.YawTotalAngle = ekf_ins.YawTotalAngle;
    }

    // temperature control
    if ((count % 2) == 0)
    {
        // 500hz
        ins_imu_temperature_ctrl();
    }

    if ((count++ % 1000) == 0)
    {
        // 1Hz 可以加入monitor函数,检查IMU是否正常运行/离线
    }
}

/**
 * @brief          Transform 3dvector from BodyFrame to EarthFrame
 * @param[1]       vector in BodyFrame
 * @param[2]       vector in EarthFrame
 * @param[3]       quaternion
 */
void ins_body_to_earth(const float *vecBF, float *vecEF, float *q)
{
    vecEF[0] =
        2.0f * ((0.5f - q[2] * q[2] - q[3] * q[3]) * vecBF[0] +
                (q[1] * q[2] - q[0] * q[3]) * vecBF[1] + (q[1] * q[3] + q[0] * q[2]) * vecBF[2]);

    vecEF[1] = 2.0f * ((q[1] * q[2] + q[0] * q[3]) * vecBF[0] +
                       (0.5f - q[1] * q[1] - q[3] * q[3]) * vecBF[1] +
                       (q[2] * q[3] - q[0] * q[1]) * vecBF[2]);

    vecEF[2] =
        2.0f * ((q[1] * q[3] - q[0] * q[2]) * vecBF[0] + (q[2] * q[3] + q[0] * q[1]) * vecBF[1] +
                (0.5f - q[1] * q[1] - q[2] * q[2]) * vecBF[2]);
}

/**
 * @brief          Transform 3dvector from EarthFrame to BodyFrame
 * @param[1]       vector in EarthFrame
 * @param[2]       vector in BodyFrame
 * @param[3]       quaternion
 */
void ins_earth_to_body(const float *vecEF, float *vecBF, float *q)
{
    vecBF[0] =
        2.0f * ((0.5f - q[2] * q[2] - q[3] * q[3]) * vecEF[0] +
                (q[1] * q[2] + q[0] * q[3]) * vecEF[1] + (q[1] * q[3] - q[0] * q[2]) * vecEF[2]);

    vecBF[1] = 2.0f * ((q[1] * q[2] - q[0] * q[3]) * vecEF[0] +
                       (0.5f - q[1] * q[1] - q[3] * q[3]) * vecEF[1] +
                       (q[2] * q[3] + q[0] * q[1]) * vecEF[2]);

    vecBF[2] =
        2.0f * ((q[1] * q[3] + q[0] * q[2]) * vecEF[0] + (q[2] * q[3] - q[0] * q[1]) * vecEF[1] +
                (0.5f - q[1] * q[1] - q[2] * q[2]) * vecEF[2]);
}

/**
 * @brief reserved.用于修正IMU安装误差与标度因数误差,即陀螺仪轴和云台轴的安装偏移
 *
 *
 * @param param IMU参数
 * @param gyro  角速度
 * @param accel 加速度
 */
static void ins_param_correction(ins_imu_param_t *param, float gyro[3], float accel[3])
{
    static float lastYawOffset, lastPitchOffset, lastRollOffset;
    static float c_11, c_12, c_13, c_21, c_22, c_23, c_31, c_32, c_33;
    float cosPitch, cosYaw, cosRoll, sinPitch, sinYaw, sinRoll;

    if (fabsf(param->Yaw - lastYawOffset) > 0.001f ||
        fabsf(param->Pitch - lastPitchOffset) > 0.001f ||
        fabsf(param->Roll - lastRollOffset) > 0.001f || param->flag)
    {
        cosYaw = arm_cos_f32(param->Yaw / 57.295779513f);
        cosPitch = arm_cos_f32(param->Pitch / 57.295779513f);
        cosRoll = arm_cos_f32(param->Roll / 57.295779513f);
        sinYaw = arm_sin_f32(param->Yaw / 57.295779513f);
        sinPitch = arm_sin_f32(param->Pitch / 57.295779513f);
        sinRoll = arm_sin_f32(param->Roll / 57.295779513f);

        // 1.yaw(alpha) 2.pitch(beta) 3.roll(gamma)
        c_11 = cosYaw * cosRoll + sinYaw * sinPitch * sinRoll;
        c_12 = cosPitch * sinYaw;
        c_13 = cosYaw * sinRoll - cosRoll * sinYaw * sinPitch;
        c_21 = cosYaw * sinPitch * sinRoll - cosRoll * sinYaw;
        c_22 = cosYaw * cosPitch;
        c_23 = -sinYaw * sinRoll - cosYaw * cosRoll * sinPitch;
        c_31 = -cosPitch * sinRoll;
        c_32 = sinPitch;
        c_33 = cosPitch * cosRoll;
        param->flag = 0;
    }
    float gyro_temp[3];
    for (uint8_t i = 0; i < 3; ++i)
        gyro_temp[i] = gyro[i] * param->scale[i];

    gyro[IMU_AXIS_X] = c_11 * gyro_temp[IMU_AXIS_X] + c_12 * gyro_temp[IMU_AXIS_Y] + c_13 * gyro_temp[IMU_AXIS_Z];
    gyro[IMU_AXIS_Y] = c_21 * gyro_temp[IMU_AXIS_X] + c_22 * gyro_temp[IMU_AXIS_Y] + c_23 * gyro_temp[IMU_AXIS_Z];
    gyro[IMU_AXIS_Z] = c_31 * gyro_temp[IMU_AXIS_X] + c_32 * gyro_temp[IMU_AXIS_Y] + c_33 * gyro_temp[IMU_AXIS_Z];

    float accel_temp[3];
    for (uint8_t i = 0; i < 3; ++i)
        accel_temp[i] = accel[i];

    accel[IMU_AXIS_X] = c_11 * accel_temp[IMU_AXIS_X] + c_12 * accel_temp[IMU_AXIS_Y] + c_13 * accel_temp[IMU_AXIS_Z];
    accel[IMU_AXIS_Y] = c_21 * accel_temp[IMU_AXIS_X] + c_22 * accel_temp[IMU_AXIS_Y] + c_23 * accel_temp[IMU_AXIS_Z];
    accel[IMU_AXIS_Z] = c_31 * accel_temp[IMU_AXIS_X] + c_32 * accel_temp[IMU_AXIS_Y] + c_33 * accel_temp[IMU_AXIS_Z];

    lastYawOffset = param->Yaw;
    lastPitchOffset = param->Pitch;
    lastRollOffset = param->Roll;
}

//------------------------------------functions below are not used in this
// demo-------------------------------------------------
//----------------------------------you can read them for learning or
// programming-----------------------------------------------
//----------------------------------they could also be helpful for further
// design-----------------------------------------------

/**
 * @brief        Update quaternion
 */
void ins_quaternion_update(float *q, float gx, float gy, float gz, float dt)
{
    float qa, qb, qc;

    gx *= 0.5f * dt;
    gy *= 0.5f * dt;
    gz *= 0.5f * dt;
    qa = q[0];
    qb = q[1];
    qc = q[2];
    q[0] += (-qb * gx - qc * gy - q[3] * gz);
    q[1] += (qa * gx + qc * gz - q[3] * gy);
    q[2] += (qa * gy - qb * gz + q[3] * gx);
    q[3] += (qa * gz + qb * gy - qc * gx);
}

/**
 * @brief        Convert quaternion to eular angle
 */
void ins_quaternion_to_euler(float *q, float *Yaw, float *Pitch, float *Roll)
{
    *Yaw = atan2f(2.0f * (q[0] * q[3] + q[1] * q[2]), 2.0f * (q[0] * q[0] + q[1] * q[1]) - 1.0f) *
           57.295779513f;
    *Pitch = atan2f(2.0f * (q[0] * q[1] + q[2] * q[3]), 2.0f * (q[0] * q[0] + q[3] * q[3]) - 1.0f) *
             57.295779513f;
    *Roll = asinf(2.0f * (q[0] * q[2] - q[1] * q[3])) * 57.295779513f;
}

/**
 * @brief        Convert eular angle to quaternion
 */
void ins_euler_to_quaternion(float Yaw, float Pitch, float Roll, float *q)
{
    float cosPitch, cosYaw, cosRoll, sinPitch, sinYaw, sinRoll;
    Yaw /= 57.295779513f;
    Pitch /= 57.295779513f;
    Roll /= 57.295779513f;
    cosPitch = arm_cos_f32(Pitch / 2);
    cosYaw = arm_cos_f32(Yaw / 2);
    cosRoll = arm_cos_f32(Roll / 2);
    sinPitch = arm_sin_f32(Pitch / 2);
    sinYaw = arm_sin_f32(Yaw / 2);
    sinRoll = arm_sin_f32(Roll / 2);
    q[0] = cosPitch * cosRoll * cosYaw + sinPitch * sinRoll * sinYaw;
    q[1] = sinPitch * cosRoll * cosYaw - cosPitch * sinRoll * sinYaw;
    q[2] = sinPitch * cosRoll * sinYaw + cosPitch * sinRoll * cosYaw;
    q[3] = cosPitch * cosRoll * sinYaw - sinPitch * sinRoll * cosYaw;
}
