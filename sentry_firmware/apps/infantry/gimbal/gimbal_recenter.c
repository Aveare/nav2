/**
 * @file    gimbal_recenter.c
 * @brief   云台回中:解锁重锚定与零位标定联动
 * @layer   APP
 * @author  小猫大王
* @note    从 gimbal.c 整段搬出(函数体未改);仍是对外 API,cmd 侧调用不变。
 */
// app
#include "robot_def.h"
#include "gimbal.h"
#include "gimbal_internal.h"
// module
#include "dji_motor.h"
#include "ins_task.h"
#include "calib.h"
#include "general_def.h"
// bsp
#include "log_port.h"

/**
 * @brief  当前 pitch 转子单圈编码器角(度,0~360)
 * @note   供标定使用:把枪管摆到机械水平后读本值,即为 PITCH_HORIZON 的候选。
 */
float gimbal_get_pitch_ecd_deg(void)
{
    return (pitch_motor == NULL) ? 0.0f : pitch_motor->measure.angle_single_round;
}

/**
 * @brief  当前 pitch 转子累计角(度,解包后)
 * @note   与单圈角配合可定位唯一机械位置;减速比换算后即输出轴角度。
 */
float gimbal_get_pitch_total_angle(void)
{
    return (pitch_motor == NULL) ? 0.0f : pitch_motor->measure.total_angle;
}

/**
 * @brief  ECD 回正:由标定的机械水平零位算出回正目标(IMU 角度域)
 * @return 目标 IMU Pitch(度);未标定或解算异常时返回 0(即旧的"IMU 水平"回正)
 * @note   为什么目标要换算到 IMU 域:角度环反馈仍是 IMU(见 gimbal.c:220),
 *          若改成电机反馈就要重标重力前馈与限位,风险远大于收益。
 *          这里让 ECD 决定"转到哪",IMU 决定"现在在哪",两者各自取长。
 * @note   安全钳制:目标必须在 PITCH_MIN/MAX_ANGLE 内,且与当前姿态相差不超过 90°;
 *          任一不满足即判定标定/方向系数有问题,回退旧的 IMU 水平回正并报错。
 */
/**
 * @brief  把当前 pitch 姿态保存为机械水平零位(写 Flash)
 * @return 0 成功,-1 失败
 * @note   仅在遥控上挡零力(ROBOT_STATUS_STOP)下调用:擦除期间 CPU 冻结 1~2 s,
 *         控制环与 CAN 全部停摆,有力矩时云台会摔下来(见 components/calib/calib.c)。
 */
int8_t gimbal_save_pitch_horizon_zero(void)
{
    return gimbal_calib_save_pitch_horizon(gimbal_get_pitch_ecd_deg(), gimbal_get_pitch_total_angle());
}

float gimbal_pitch_recenter_target(void)
{
    float target;

    if (pitch_motor == NULL || gimbal_imu_data == NULL)
        return 0.0f;

    if (!calib_is_valid())
        return 0.0f; // 未标定:保持旧行为(IMU 水平)

    target =
        calib_pitch_horizon_target_imu(calib_get_pitch_horizon_ecd(), pitch_motor->measure.total_angle,
                                    PITCH_REDUCTION_RATIO, PITCH_ECD_SIGN, gimbal_imu_data->Pitch);

    if (!isfinite(target) || fabsf(target - gimbal_imu_data->Pitch) > 90.0f)
    {
        char bad[16];
        log_float_to_str(bad, target);
        LOG_ERROR("[gimbal] ECD回正目标异常(%s deg),回退IMU水平", bad);
        return 0.0f;
    }

    MOTOR_LIMIT_MIN_MAX(target, PITCH_MIN_ANGLE, PITCH_MAX_ANGLE);
    return target;
}
