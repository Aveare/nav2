/**
 * @file    calib.h
 * @brief   标定记录接口:读取/写入/有效性
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    解决"零位每次编译都要改宏重烧"的问题:标定值上电从 Flash 读,标定一次长期有效。
* @note    记录格式自带 magic/version/size/crc32,任一不符即视为无效并回退到编译期宏。
* @note    **写 Flash 会短暂冻结 CPU**(整扇区擦除,F407 单 bank 期间无法取指),
*          因此标定只能在遥控上挡零力(ROBOT_STATUS_STOP)状态下触发,详见 calib.c 顶部说明。
* @note    2026-11-16 建立,作者 JIT CR。
 */
#ifndef _CALIB_H
#define _CALIB_H

#include "main.h"

/* ---- Flash 存储位置 ---------------------------------------------------- */
/**
 * 标定记录存放扇区。当前固件约 128 KB(占 sector 0~4),0x080E0000 是 sector 11,
 * 距代码尾 >900 KB,正常迭代不会撞上;若某天代码超过 0x080E0000,本模块会在 Init 时
 * 检测到并只告警不写入(见 calib_flash_safe())。
 */
#define CALIB_FLASH_ADDR         0x080E0000u
#define CALIB_FLASH_SECTOR_COUNT 1u

#define CALIB_MAGIC   0x31424C43u /* "CLB1" */
#define CALIB_VERSION 1u

/* ---- 标定记录 ---------------------------------------------------------- */
typedef struct
{
    uint32_t magic;            /**< CALIB_MAGIC,不符即未标定 */
    uint16_t version;          /**< 记录格式版本 */
    uint16_t size;             /**< 本结构体字节数,防止结构变更后误读 */
    float yaw_align_ecd;       /**< yaw 与车体纵轴对齐时的编码器值(已存储,尚未接管控制) */
    float pitch_horizon_ecd;   /**< pitch 机械水平位:转子单圈角(度,0~360) */
    float pitch_horizon_total; /**< 标定当时的转子累计角(度),仅用于反查轴向,不参与控制 */
    float pitch_reduction;     /**< 标定时的减速比(转子度/输出轴度),仅用于追溯 */
    float pitch_ecd_sign;      /**< 标定时的方向系数(+1/-1),仅用于追溯 */
    uint32_t crc32;            /**< 覆盖本结构体 crc32 之前的全部字节 */
    uint32_t reserved[3];      /**< 预留,保持 4 字节对齐与整字写入 */
} calib_record_t;

/**
 * @brief  上电加载标定记录(robot_init 中、应用初始化之前调用)
 * @note   记录无效时只打印告警并置"未标定",所有读取接口回退到编译期宏。
 */
void calib_init(void);

/**
 * @brief  标定记录是否有效(有效才启用 ECD 回正)
 */
uint8_t calib_is_valid(void);

/**
 * @brief  取 pitch 机械水平位的编码器值(度)
 * @return 未标定时返回 config 的 PITCH_HORIZON_ECD 折算值
 */
float calib_get_pitch_horizon_ecd(void);

/**
 * @brief  取 yaw 与车体纵轴对齐的编码器值
 * @return 未标定时返回 config 的 YAW_CHASSIS_ALIGN_ECD
 */
float calib_get_yaw_align_ecd(void);

/**
 * @brief  把当前 pitch 姿态保存为机械水平零位
 * @param  horizon_ecd   当前转子单圈角(度)
 * @param  horizon_total 当前转子累计角(度)
 * @return 0 成功,-1 失败(未擦写/校验不符/存储区不安全)
 * @note   会先擦整扇区再写入并回读校验;调用前必须确保电机处于零力,
 *         因为擦除期间 CPU 冻结约 1~2 s,控制环与 CAN 发送全部停摆。
 */
int8_t gimbal_calib_save_pitch_horizon(float horizon_ecd, float horizon_total);

/**
 * @brief  保存 yaw 对齐编码器值(与 pitch 记录同一条,内部合并后整条重写)
 * @param  align_ecd yaw 与车体纵轴对齐时的编码器值
 * @return 0 成功,-1 失败
 */
int8_t calib_save_yaw_align(float align_ecd);

/**
 * @brief  纯函数:由 ECD 零位解算"回正目标"(IMU 角度域),供回正与主机单元测试共用
 * @param  ecd_horizon_deg 机械水平位对应的转子单圈角(度)
 * @param  total_now_deg   当前转子累计角(度)
 * @param  ratio           减速比(转子度/输出轴度),如 19.0
 * @param  sign            方向系数:使 sign*total/ratio 与 IMU Pitch 同向(+1/-1)
 * @param  imu_pitch_now   当前 IMU Pitch(度)
 * @return 目标 IMU Pitch(度);参数非法时返回 0
 * @note   单圈 ECD 在行程内会绕多圈,故先用 ±180° 折叠拿到最近的一圈,
 *         再用 IMU 粗测"当前偏离水平多少"来消歧(IMU 只需优于 ±半个绕圈周期
 *         =±(360/ratio)/2 ≈ ±9.5°,远低于其实际精度);
 *         这就是"ECD 定绝对零位、IMU 辅助判圈"。
 */
float calib_pitch_horizon_target_imu(float ecd_horizon_deg, float total_now_deg, float ratio,
                                  float sign, float imu_pitch_now);

/**
 * @brief  纯函数:转子累计角换算输出轴角度(与 IMU Pitch 同向,供日志/校验用)
 */
float calib_pitch_axis_deg(float total_now_deg, float ratio, float sign);

#endif // _CALIB_H
