/**
 * @file    quaternion_ekf.c
 * @brief   四元数 EKF:姿态解算与陀螺零偏在线估计
 * @layer   COMPONENT
 * @author  Wang Hongxi
 * @author  modified by 小猫大王
* @version V1.2.0
* @date    2022/3/8
 */
#include "quaternion_ekf.h"

ekf_state_t ekf_ins;

const float ekf_f_matrix[36] = {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
                                       0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
float ekf_p_matrix[36] = {100000, 0.1, 0.1, 0.1,    0.1, 0.1,    0.1, 100000, 0.1,
                                 0.1,    0.1, 0.1, 0.1,    0.1, 100000, 0.1, 0.1,    0.1,
                                 0.1,    0.1, 0.1, 100000, 0.1, 0.1,    0.1, 0.1,    0.1,
                                 0.1,    100, 0.1, 0.1,    0.1, 0.1,    0.1, 0.1,    100};
float ekf_k_matrix[18];
float ekf_h_matrix[18];

static float ekf_inv_sqrt(float x);
static void ekf_imu_quaternion_ekf_observe(kalman_filter_t *kf);
static void ekf_fading_linearization(kalman_filter_t *kf);
static void ekf_imu_quaternion_ekf_set_h(kalman_filter_t *kf);
static void ekf_imu_quaternion_ekf_xhat_update(kalman_filter_t *kf);

/**
 * @brief Quaternion EKF initialization and some reference value
 * @param[in] process_noise1 quaternion process noise    10
 * @param[in] process_noise2 gyro bias process noise     0.001
 * @param[in] measure_noise  accel measure noise         1000000
 * @param[in] lambda         fading coefficient          0.9996
 * @param[in] lpf            lowpass filter coefficient  0
 */
void ekf_imu_quaternion_ekf_init(float *init_quaternion, float process_noise1, float process_noise2,
                            float measure_noise, float lambda, float lpf)
{
    ekf_ins.Initialized = 1;
    ekf_ins.Q1 = process_noise1;
    ekf_ins.Q2 = process_noise2;
    ekf_ins.R = measure_noise;
    ekf_ins.ChiSquareTestThreshold = 1e-8;
    ekf_ins.ConvergeFlag = 0;
    ekf_ins.ErrorCount = 0;
    ekf_ins.UpdateCount = 0;
    if (lambda > 1)
    {
        lambda = 1;
    }
    ekf_ins.lambda = lambda;
    ekf_ins.accLPFcoef = lpf;

    // 初始化矩阵维度信息
    kalman_filter_init(&ekf_ins.IMU_QuaternionEKF, 6, 0, 3);
    arm_mat_init_f32(&ekf_ins.ChiSquare, 1, 1, (float *)ekf_ins.ChiSquare_Data);

    // 姿态初始化
    for (int i = 0; i < 4; i++)
    {
        ekf_ins.IMU_QuaternionEKF.xhat_data[i] = init_quaternion[i];
    }

    // 自定义函数初始化,用于扩展或增加kf的基础功能
    ekf_ins.IMU_QuaternionEKF.User_Func0_f = ekf_imu_quaternion_ekf_observe;
    ekf_ins.IMU_QuaternionEKF.User_Func1_f = ekf_fading_linearization;
    ekf_ins.IMU_QuaternionEKF.User_Func2_f = ekf_imu_quaternion_ekf_set_h;
    ekf_ins.IMU_QuaternionEKF.User_Func3_f = ekf_imu_quaternion_ekf_xhat_update;

    // 设定标志位,用自定函数替换kf标准步骤中的SetK(计算增益)以及xhatupdate(后验估计/融合)
    ekf_ins.IMU_QuaternionEKF.SkipEq3 = true;
    ekf_ins.IMU_QuaternionEKF.SkipEq4 = true;

    memcpy(ekf_ins.IMU_QuaternionEKF.F_data, ekf_f_matrix, sizeof(ekf_f_matrix));
    memcpy(ekf_ins.IMU_QuaternionEKF.P_data, ekf_p_matrix, sizeof(ekf_p_matrix));
}

/**
 * @brief Quaternion EKF update
 * @param[in]       gyro x y z in rad/s
 * @param[in]       accel x y z in m/s²
 * @param[in]       update period in s
 */
void ekf_imu_quaternion_ekf_update(float gx, float gy, float gz, float ax, float ay, float az, float dt)
{
    // 0.5(Ohm-Ohm^bias)*deltaT,用于更新工作点处的状态转移F矩阵
    static float halfgxdt, halfgydt, halfgzdt;
    static float accelInvNorm;

    /*   F, number with * represent vals to be set
     0      1*     2*     3*     4     5
     6*     7      8*     9*    10    11
    12*    13*    14     15*    16    17
    18*    19*    20*    21     22    23
    24     25     26     27     28    29
    30     31     32     33     34    35
    */
    ekf_ins.dt = dt;

    ekf_ins.Gyro[0] = gx - ekf_ins.GyroBias[0];
    ekf_ins.Gyro[1] = gy - ekf_ins.GyroBias[1];
    ekf_ins.Gyro[2] = gz - ekf_ins.GyroBias[2];

    // set F
    halfgxdt = 0.5f * ekf_ins.Gyro[0] * dt;
    halfgydt = 0.5f * ekf_ins.Gyro[1] * dt;
    halfgzdt = 0.5f * ekf_ins.Gyro[2] * dt;

    // 此部分设定状态转移矩阵F的左上角部分
    // 4x4子矩阵,即0.5(Ohm-Ohm^bias)*deltaT,右下角有一个2x2单位阵已经初始化好了
    // 注意在predict步F的右上角是4x2的零矩阵,因此每次predict的时候都会调用memcpy用单位阵覆盖前一轮线性化后的矩阵
    memcpy(ekf_ins.IMU_QuaternionEKF.F_data, ekf_f_matrix, sizeof(ekf_f_matrix));

    ekf_ins.IMU_QuaternionEKF.F_data[1] = -halfgxdt;
    ekf_ins.IMU_QuaternionEKF.F_data[2] = -halfgydt;
    ekf_ins.IMU_QuaternionEKF.F_data[3] = -halfgzdt;

    ekf_ins.IMU_QuaternionEKF.F_data[6] = halfgxdt;
    ekf_ins.IMU_QuaternionEKF.F_data[8] = halfgzdt;
    ekf_ins.IMU_QuaternionEKF.F_data[9] = -halfgydt;

    ekf_ins.IMU_QuaternionEKF.F_data[12] = halfgydt;
    ekf_ins.IMU_QuaternionEKF.F_data[13] = -halfgzdt;
    ekf_ins.IMU_QuaternionEKF.F_data[15] = halfgxdt;

    ekf_ins.IMU_QuaternionEKF.F_data[18] = halfgzdt;
    ekf_ins.IMU_QuaternionEKF.F_data[19] = halfgydt;
    ekf_ins.IMU_QuaternionEKF.F_data[20] = -halfgxdt;

    // accel low pass filter,加速度过一下低通滤波平滑数据,降低撞击和异常的影响
    if (ekf_ins.UpdateCount == 0) // 如果是第一次进入,需要初始化低通滤波
    {
        ekf_ins.Accel[0] = ax;
        ekf_ins.Accel[1] = ay;
        ekf_ins.Accel[2] = az;
    }
    ekf_ins.Accel[0] =
        ekf_ins.Accel[0] * ekf_ins.accLPFcoef / (ekf_ins.dt + ekf_ins.accLPFcoef) +
        ax * ekf_ins.dt / (ekf_ins.dt + ekf_ins.accLPFcoef);
    ekf_ins.Accel[1] =
        ekf_ins.Accel[1] * ekf_ins.accLPFcoef / (ekf_ins.dt + ekf_ins.accLPFcoef) +
        ay * ekf_ins.dt / (ekf_ins.dt + ekf_ins.accLPFcoef);
    ekf_ins.Accel[2] =
        ekf_ins.Accel[2] * ekf_ins.accLPFcoef / (ekf_ins.dt + ekf_ins.accLPFcoef) +
        az * ekf_ins.dt / (ekf_ins.dt + ekf_ins.accLPFcoef);

    // set z,单位化重力加速度向量
    accelInvNorm =
        ekf_inv_sqrt(ekf_ins.Accel[0] * ekf_ins.Accel[0] + ekf_ins.Accel[1] * ekf_ins.Accel[1] +
                ekf_ins.Accel[2] * ekf_ins.Accel[2]);
    for (uint8_t i = 0; i < 3; ++i)
    {
        ekf_ins.IMU_QuaternionEKF.MeasuredVector[i] =
            ekf_ins.Accel[i] * accelInvNorm; // 用加速度向量更新量测值
    }

    // get body state
    ekf_ins.gyro_norm =
        1.0f / ekf_inv_sqrt(ekf_ins.Gyro[0] * ekf_ins.Gyro[0] + ekf_ins.Gyro[1] * ekf_ins.Gyro[1] +
                       ekf_ins.Gyro[2] * ekf_ins.Gyro[2]);
    ekf_ins.accl_norm = 1.0f / accelInvNorm;

    // 如果角速度小于阈值且加速度处于设定范围内,认为运动稳定,加速度可以用于修正角速度
    // 稍后在最后的姿态更新部分会利用StableFlag来确定
    if (ekf_ins.gyro_norm < 0.3f && ekf_ins.accl_norm > 9.8f - 0.5f &&
        ekf_ins.accl_norm < 9.8f + 0.5f)
    {
        ekf_ins.StableFlag = 1;
    }
    else
    {
        ekf_ins.StableFlag = 0;
    }

    // set Q R,过程噪声和观测噪声矩阵
    ekf_ins.IMU_QuaternionEKF.Q_data[0] = ekf_ins.Q1 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.Q_data[7] = ekf_ins.Q1 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.Q_data[14] = ekf_ins.Q1 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.Q_data[21] = ekf_ins.Q1 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.Q_data[28] = ekf_ins.Q2 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.Q_data[35] = ekf_ins.Q2 * ekf_ins.dt;
    ekf_ins.IMU_QuaternionEKF.R_data[0] = ekf_ins.R;
    ekf_ins.IMU_QuaternionEKF.R_data[4] = ekf_ins.R;
    ekf_ins.IMU_QuaternionEKF.R_data[8] = ekf_ins.R;

    // 调用kalman_filter.c封装好的函数,注意几个User_Funcx_f的调用
    kalman_filter_update(&ekf_ins.IMU_QuaternionEKF);

    // 获取融合后的数据,包括四元数和xy零飘值
    ekf_ins.q[0] = ekf_ins.IMU_QuaternionEKF.FilteredValue[0];
    ekf_ins.q[1] = ekf_ins.IMU_QuaternionEKF.FilteredValue[1];
    ekf_ins.q[2] = ekf_ins.IMU_QuaternionEKF.FilteredValue[2];
    ekf_ins.q[3] = ekf_ins.IMU_QuaternionEKF.FilteredValue[3];
    ekf_ins.GyroBias[0] = ekf_ins.IMU_QuaternionEKF.FilteredValue[4];
    ekf_ins.GyroBias[1] = ekf_ins.IMU_QuaternionEKF.FilteredValue[5];
    ekf_ins.GyroBias[2] = 0; // 大部分时候z轴通天,无法观测yaw的漂移

    // 利用四元数反解欧拉角
    ekf_ins.Yaw =
        atan2f(2.0f * (ekf_ins.q[0] * ekf_ins.q[3] + ekf_ins.q[1] * ekf_ins.q[2]),
               2.0f * (ekf_ins.q[0] * ekf_ins.q[0] + ekf_ins.q[1] * ekf_ins.q[1]) - 1.0f) *
        57.295779513f;
    ekf_ins.Pitch =
        atan2f(2.0f * (ekf_ins.q[0] * ekf_ins.q[1] + ekf_ins.q[2] * ekf_ins.q[3]),
               2.0f * (ekf_ins.q[0] * ekf_ins.q[0] + ekf_ins.q[3] * ekf_ins.q[3]) - 1.0f) *
        57.295779513f;
    ekf_ins.Roll = asinf(-2.0f * (ekf_ins.q[1] * ekf_ins.q[3] - ekf_ins.q[0] * ekf_ins.q[2])) *
                    57.295779513f;

    // get Yaw total, yaw数据可能会超过360,处理一下方便其他功能使用(如小陀螺)
    if (ekf_ins.Yaw - ekf_ins.YawAngleLast > 180.0f)
    {
        ekf_ins.YawRoundCount--;
    }
    else if (ekf_ins.Yaw - ekf_ins.YawAngleLast < -180.0f)
    {
        ekf_ins.YawRoundCount++;
    }
    ekf_ins.YawTotalAngle = 360.0f * ekf_ins.YawRoundCount + ekf_ins.Yaw;
    ekf_ins.YawAngleLast = ekf_ins.Yaw;
    ekf_ins.UpdateCount++; // 初始化低通滤波用,计数测试用
}

/**
 * @brief 用于更新线性化后的状态转移矩阵F右上角的一个4x2分块矩阵,稍后用于协方差矩阵P的更新;
 *        并对零漂的方差进行限制,防止过度收敛并限幅防止发散
 *
 * @param kf
 */
static void ekf_fading_linearization(kalman_filter_t *kf)
{
    static float q0, q1, q2, q3;
    static float qInvNorm;

    q0 = kf->xhatminus_data[0];
    q1 = kf->xhatminus_data[1];
    q2 = kf->xhatminus_data[2];
    q3 = kf->xhatminus_data[3];

    // quaternion normalize
    qInvNorm = ekf_inv_sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    for (uint8_t i = 0; i < 4; ++i)
    {
        kf->xhatminus_data[i] *= qInvNorm;
    }
    /*  F, number with * represent vals to be set
     0     1     2     3     4*     5*
     6     7     8     9    10*    11*
    12    13    14    15    16*    17*
    18    19    20    21    22*    23*
    24    25    26    27    28     29
    30    31    32    33    34     35
    */
    // set F
    kf->F_data[4] = q1 * ekf_ins.dt / 2;
    kf->F_data[5] = q2 * ekf_ins.dt / 2;

    kf->F_data[10] = -q0 * ekf_ins.dt / 2;
    kf->F_data[11] = q3 * ekf_ins.dt / 2;

    kf->F_data[16] = -q3 * ekf_ins.dt / 2;
    kf->F_data[17] = -q0 * ekf_ins.dt / 2;

    kf->F_data[22] = q2 * ekf_ins.dt / 2;
    kf->F_data[23] = -q1 * ekf_ins.dt / 2;

    // fading filter,防止零飘参数过度收敛
    kf->P_data[28] /= ekf_ins.lambda;
    kf->P_data[35] /= ekf_ins.lambda;

    // 限幅,防止发散
    if (kf->P_data[28] > 10000)
    {
        kf->P_data[28] = 10000;
    }
    if (kf->P_data[35] > 10000)
    {
        kf->P_data[35] = 10000;
    }
}

/**
 * @brief 在工作点处计算观测函数h(x)的Jacobi矩阵H
 *
 * @param kf
 */
static void ekf_imu_quaternion_ekf_set_h(kalman_filter_t *kf)
{
    static float doubleq0, doubleq1, doubleq2, doubleq3;
    /* H
     0     1     2     3     4     5
     6     7     8     9    10    11
    12    13    14    15    16    17
    last two cols are zero
    */
    // set H
    doubleq0 = 2 * kf->xhatminus_data[0];
    doubleq1 = 2 * kf->xhatminus_data[1];
    doubleq2 = 2 * kf->xhatminus_data[2];
    doubleq3 = 2 * kf->xhatminus_data[3];

    memset(kf->H_data, 0, sizeof_float * kf->zSize * kf->xhatSize);

    kf->H_data[0] = -doubleq2;
    kf->H_data[1] = doubleq3;
    kf->H_data[2] = -doubleq0;
    kf->H_data[3] = doubleq1;

    kf->H_data[6] = doubleq1;
    kf->H_data[7] = doubleq0;
    kf->H_data[8] = doubleq3;
    kf->H_data[9] = doubleq2;

    kf->H_data[12] = doubleq0;
    kf->H_data[13] = -doubleq1;
    kf->H_data[14] = -doubleq2;
    kf->H_data[15] = doubleq3;
}

/**
 * @brief 利用观测值和先验估计得到最优的后验估计
 *        加入了卡方检验以判断融合加速度的条件是否满足
 *        同时引入发散保护保证恶劣工况下的必要量测更新
 *
 * @param kf
 */
static void ekf_imu_quaternion_ekf_xhat_update(kalman_filter_t *kf)
{
    static float q0, q1, q2, q3;

    kf->MatStatus = arm_mat_trans_f32(&kf->H, &kf->HT); // z|x => x|z
    kf->temp_matrix.numRows = kf->H.numRows;
    kf->temp_matrix.numCols = kf->Pminus.numCols;
    kf->MatStatus = arm_mat_mult_f32(&kf->H, &kf->Pminus, &kf->temp_matrix); // temp_matrix = H·P'(k)
    kf->temp_matrix1.numRows = kf->temp_matrix.numRows;
    kf->temp_matrix1.numCols = kf->HT.numCols;
    kf->MatStatus =
        arm_mat_mult_f32(&kf->temp_matrix, &kf->HT, &kf->temp_matrix1); // temp_matrix1 = H·P'(k)·HT
    kf->S.numRows = kf->R.numRows;
    kf->S.numCols = kf->R.numCols;
    kf->MatStatus = arm_mat_add_f32(&kf->temp_matrix1, &kf->R, &kf->S); // S = H P'(k) HT + R
    kf->MatStatus = arm_mat_inverse_f32(&kf->S, &kf->temp_matrix1); // temp_matrix1 = inv(H·P'(k)·HT + R)

    q0 = kf->xhatminus_data[0];
    q1 = kf->xhatminus_data[1];
    q2 = kf->xhatminus_data[2];
    q3 = kf->xhatminus_data[3];

    kf->temp_vector.numRows = kf->H.numRows;
    kf->temp_vector.numCols = 1;
    // 计算预测得到的重力加速度方向(通过姿态获取的)
    kf->temp_vector_data[0] = 2 * (q1 * q3 - q0 * q2);
    kf->temp_vector_data[1] = 2 * (q0 * q1 + q2 * q3);
    kf->temp_vector_data[2] = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3; // temp_vector = h(xhat'(k))

    // 计算预测值和各个轴的方向余弦
    for (uint8_t i = 0; i < 3; ++i)
    {
        ekf_ins.OrientationCosine[i] = acosf(fabsf(kf->temp_vector_data[i]));
    }

    // 利用加速度计数据修正
    kf->temp_vector1.numRows = kf->z.numRows;
    kf->temp_vector1.numCols = 1;
    kf->MatStatus = arm_mat_sub_f32(&kf->z, &kf->temp_vector,
                                    &kf->temp_vector1); // temp_vector1 = z(k) - h(xhat'(k))

    // chi-square test,卡方检验
    kf->temp_matrix.numRows = kf->temp_vector1.numRows;
    kf->temp_matrix.numCols = 1;
    kf->MatStatus =
        arm_mat_mult_f32(&kf->temp_matrix1, &kf->temp_vector1,
                        &kf->temp_matrix); // temp_matrix = inv(H·P'(k)·HT + R)·(z(k) - h(xhat'(k)))
    kf->temp_vector.numRows = 1;
    kf->temp_vector.numCols = kf->temp_vector1.numRows;
    kf->MatStatus =
        arm_mat_trans_f32(&kf->temp_vector1, &kf->temp_vector); // temp_vector = z(k) - h(xhat'(k))'
    kf->MatStatus = arm_mat_mult_f32(&kf->temp_vector, &kf->temp_matrix, &ekf_ins.ChiSquare);
    // rk is small,filter converged/converging
    if (ekf_ins.ChiSquare_Data[0] < 0.5f * ekf_ins.ChiSquareTestThreshold)
    {
        ekf_ins.ConvergeFlag = 1;
    }
    // rk is bigger than thre but once converged
    if (ekf_ins.ChiSquare_Data[0] > ekf_ins.ChiSquareTestThreshold && ekf_ins.ConvergeFlag)
    {
        if (ekf_ins.StableFlag)
        {
            ekf_ins.ErrorCount++; // 载体静止时仍无法通过卡方检验
        }
        else
        {
            ekf_ins.ErrorCount = 0;
        }

        if (ekf_ins.ErrorCount > 50)
        {
            // 滤波器发散
            ekf_ins.ConvergeFlag = 0;
            kf->SkipEq5 = false; // step-5 is cov mat P updating
        }
        else
        {
            //  残差未通过卡方检验 仅预测
            //  xhat(k) = xhat'(k)
            //  P(k) = P'(k)
            memcpy(kf->xhat_data, kf->xhatminus_data, sizeof_float * kf->xhatSize);
            memcpy(kf->P_data, kf->Pminus_data, sizeof_float * kf->xhatSize * kf->xhatSize);
            kf->SkipEq5 = true; // part5 is P updating
            return;
        }
    }
    else // if divergent or rk is not that big/acceptable,use adaptive gain
    {
        // scale adaptive,rk越小则增益越大,否则更相信预测值
        if (ekf_ins.ChiSquare_Data[0] > 0.1f * ekf_ins.ChiSquareTestThreshold &&
            ekf_ins.ConvergeFlag)
        {
            ekf_ins.AdaptiveGainScale =
                (ekf_ins.ChiSquareTestThreshold - ekf_ins.ChiSquare_Data[0]) /
                (0.9f * ekf_ins.ChiSquareTestThreshold);
        }
        else
        {
            ekf_ins.AdaptiveGainScale = 1;
        }
        ekf_ins.ErrorCount = 0;
        kf->SkipEq5 = false;
    }

    // cal kf-gain K
    kf->temp_matrix.numRows = kf->Pminus.numRows;
    kf->temp_matrix.numCols = kf->HT.numCols;
    kf->MatStatus =
        arm_mat_mult_f32(&kf->Pminus, &kf->HT, &kf->temp_matrix); // temp_matrix = P'(k)·HT
    kf->MatStatus = arm_mat_mult_f32(&kf->temp_matrix, &kf->temp_matrix1, &kf->K);

    // implement adaptive
    for (uint8_t i = 0; i < kf->K.numRows * kf->K.numCols; ++i)
    {
        kf->K_data[i] *= ekf_ins.AdaptiveGainScale;
    }
    for (uint8_t i = 4; i < 6; ++i)
    {
        for (uint8_t j = 0; j < 3; ++j)
        {
            kf->K_data[i * 3 + j] *= ekf_ins.OrientationCosine[i - 4] / 1.5707963f; // 1 rad
        }
    }

    kf->temp_vector.numRows = kf->K.numRows;
    kf->temp_vector.numCols = 1;
    kf->MatStatus = arm_mat_mult_f32(&kf->K, &kf->temp_vector1,
                                    &kf->temp_vector); // temp_vector = K(k)·(z(k) - H·xhat'(k))

    // 零漂修正限幅,一般不会有过大的漂移
    if (ekf_ins.ConvergeFlag)
    {
        for (uint8_t i = 4; i < 6; ++i)
        {
            if (kf->temp_vector.pData[i] > 1e-2f * ekf_ins.dt)
            {
                kf->temp_vector.pData[i] = 1e-2f * ekf_ins.dt;
            }
            if (kf->temp_vector.pData[i] < -1e-2f * ekf_ins.dt)
            {
                kf->temp_vector.pData[i] = -1e-2f * ekf_ins.dt;
            }
        }
    }

    // 不修正yaw轴数据
    kf->temp_vector.pData[3] = 0;
    kf->MatStatus = arm_mat_add_f32(&kf->xhatminus, &kf->temp_vector, &kf->xhat);
}

/**
 * @brief EKF观测环节,其实就是把数据复制一下
 *
 * @param kf kf类型定义
 */
static void ekf_imu_quaternion_ekf_observe(kalman_filter_t *kf)
{
    memcpy(ekf_p_matrix, kf->P_data, sizeof(ekf_p_matrix));
    memcpy(ekf_k_matrix, kf->K_data, sizeof(ekf_k_matrix));
    memcpy(ekf_h_matrix, kf->H_data, sizeof(ekf_h_matrix));
}

/**
 * @brief 自定义1/sqrt(x),速度更快
 *
 * @param x x
 * @return float
 */
static float ekf_inv_sqrt(float x)
{
    float halfx = 0.5f * x;
    union
    {
        float f;
        uint32_t u;
    } value = {.f = x};
    value.u = 0x5f375a86u - (value.u >> 1);
    float y = value.f;
    y = y * (1.5f - (halfx * y * y));
    return y;
}
