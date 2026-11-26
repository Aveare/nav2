/**
 * @file    calib.c
 * @brief   零位标定记录:Flash 持久化与上电加载
 * @layer   COMPONENT
 * @author  modified by 小猫大王
* @note    接口说明见 calib.h。
*
* @note    **为什么擦写只能在零力状态做**:F407 单 bank,擦除 128 KB 扇区期间
*          Flash 取指被硬件阻塞,CPU(含中断与 FreeRTOS 调度)冻结约 1~2 s。
*          期间控制环不跑、CAN 不发帧 → 电调按丢帧保护把输出归零 → 力矩消失。
*          所以本模块只被 robot_cmd 在 ROBOT_STATUS_STOP(遥控上挡零力)下触发;
*          在出力状态下调用会把云台摔下去,接口不提供"自动为自己找时机"。
*
* @note    **为什么用 ECD 而不是继续用 IMU 零位**:IMU 的 0° 是"上电时刻的芯片姿态",
*          上电姿态一变、模块拆装、温漂都会让"回水平"偏几度;编码器零位由机械结构决定,
*          标定一次即固定。IMU 仍保留两个职责:①单圈 ECD 的多圈消歧(见
*          calib_pitch_horizon_target_imu);②标定值与实测倾斜的一致性检查。
*
* @note    2026-11-16 建立,作者 JIT CR。
 */
#include "calib.h"

#include "config.h" // 参数来自 config,不依赖 application 层 // 本车参数唯一来源(含 PITCH_HORIZON_ECD / PITCH_REDUCTION_RATIO 等)
#include "dji_motor.h"           // DJI_ECD_TO_DEG
#include "flash_port.h"
#include "log_port.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* 上电加载后的缓存,避免每次调用都读 Flash */
static calib_record_t calib_record;
static uint8_t calib_loaded = 0;

/**
 * @brief  简易 CRC32(多项式 0xEDB88320,按位计算)
 * @note   只为一条记录做完整性校验,不需要查表;表会白占 1 KB Flash。
 */
static uint32_t calib_crc32(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint32_t)p[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

/**
 * @brief  存储区是否安全(代码没长到扇区头上)
 * @note   _etext 是代码段末尾的链接符号;预留 4 KB 余量。
 * @return 1 安全,0 不安全
 */
static uint8_t calib_flash_safe(void)
{
    extern uint32_t _etext;
    return ((uint32_t)&_etext + 4096u < CALIB_FLASH_ADDR) ? 1u : 0u;
}

/**
 * @brief  校验记录魔数/版本/长度/CRC
 * @return 1 有效,0 无效
 */
static uint8_t calib_record_valid(const calib_record_t *rec)
{
    uint32_t crc;

    if (rec->magic != CALIB_MAGIC || rec->version != CALIB_VERSION ||
        rec->size != sizeof(calib_record_t))
        return 0;

    crc = calib_crc32(rec, (uint32_t)offsetof(calib_record_t, crc32));
    return (crc == rec->crc32) ? 1 : 0;
}

/**
 * @brief  把缓存记录整条写入 Flash 并回读校验
 * @return 0 成功,-1 失败
 */
static int8_t calib_write_record(void)
{
    static calib_record_t verify; // 20 字节级结构,放静态区避免占栈

    if (!calib_flash_safe())
    {
        LOG_ERROR("[calib] 存储扇区 0x%08X 已被代码占用,拒绝写入", (unsigned)CALIB_FLASH_ADDR);
        return -1;
    }

    calib_record.magic = CALIB_MAGIC;
    calib_record.version = (uint16_t)CALIB_VERSION;
    calib_record.size = (uint16_t)sizeof(calib_record_t);
    calib_record.crc32 = calib_crc32(&calib_record, (uint32_t)offsetof(calib_record_t, crc32));

    flash_erase_address(CALIB_FLASH_ADDR, CALIB_FLASH_SECTOR_COUNT); // 这里会冻结 1~2 s
    if (flash_write_single_address(CALIB_FLASH_ADDR, (uint32_t *)&calib_record,
                                   sizeof(calib_record_t) / 4u) != 0)
    {
        LOG_ERROR("[calib] 写入失败");
        return -1;
    }

    flash_read(CALIB_FLASH_ADDR, (uint32_t *)&verify, sizeof(calib_record_t) / 4u);
    if (!calib_record_valid(&verify))
    {
        LOG_ERROR("[calib] 回读校验失败,记录不可用");
        return -1;
    }

    calib_record = verify;
    return 0;
}

void calib_init(void)
{
    flash_read(CALIB_FLASH_ADDR, (uint32_t *)&calib_record, sizeof(calib_record_t) / 4u);

    if (calib_record_valid(&calib_record))
    {
        char ecd[10];
        char yaw[10];

        calib_loaded = 1;
        log_float_to_str(ecd, calib_record.pitch_horizon_ecd);
        log_float_to_str(yaw, calib_record.yaw_align_ecd);
        LOG_INFO("[calib] 已加载标定: pitch水平ECD=%s deg yaw对齐ECD=%s (v%u)", ecd, yaw,
                (unsigned)CALIB_VERSION);
    }
    else
    {
        /* 未标定(出厂/首次烧录/扇区被擦):控制回退到 config 宏,不影响行驶 */
        calib_loaded = 0;
        memset(&calib_record, 0, sizeof(calib_record));
        LOG_WARN(
            "[calib] 无有效标定记录,回正回退 config 宏(PITCH_HORIZON_ECD/YAW_CHASSIS_ALIGN_ECD)");
    }
}

uint8_t calib_is_valid(void)
{
    return calib_loaded;
}

float calib_get_pitch_horizon_ecd(void)
{
    return calib_loaded ? calib_record.pitch_horizon_ecd
                        : (float)PITCH_HORIZON_ECD * DJI_ECD_TO_DEG;
}

float calib_get_yaw_align_ecd(void)
{
    return calib_loaded ? calib_record.yaw_align_ecd : (float)YAW_CHASSIS_ALIGN_ECD;
}

int8_t gimbal_calib_save_pitch_horizon(float horizon_ecd, float horizon_total)
{
    char ecd[10];
    char total[10];

    calib_record.pitch_horizon_ecd = horizon_ecd;
    calib_record.pitch_horizon_total = horizon_total;
    calib_record.pitch_reduction = PITCH_REDUCTION_RATIO;
    calib_record.pitch_ecd_sign = PITCH_ECD_SIGN;

    if (calib_write_record() != 0)
        return -1;

    calib_loaded = 1;
    log_float_to_str(ecd, calib_record.pitch_horizon_ecd);
    log_float_to_str(total, calib_record.pitch_horizon_total);
    LOG_INFO("[calib] 已保存 pitch 水平位: ECD=%s deg 转子累计=%s deg", ecd, total);
    return 0;
}

int8_t calib_save_yaw_align(float align_ecd)
{
    char yaw[10];

    calib_record.yaw_align_ecd = align_ecd;
    if (calib_write_record() != 0)
        return -1;

    calib_loaded = 1;
    log_float_to_str(yaw, calib_record.yaw_align_ecd);
    LOG_INFO("[calib] 已保存 yaw 对齐 ECD=%s", yaw);
    return 0;
}

float calib_pitch_axis_deg(float total_now_deg, float ratio, float sign)
{
    if (ratio <= 0.01f)
        return 0.0f;
    return sign * total_now_deg / ratio;
}

float calib_pitch_horizon_target_imu(float ecd_horizon_deg, float total_now_deg, float ratio,
                                  float sign, float imu_pitch_now)
{
    float best_total = 0.0f;
    float best_cost = 1.0e9f;
    uint8_t found = 0;

    if (ratio <= 0.01f)
        return 0.0f;

    /* 单圈 ECD 在行程内会绕多圈(45° 行程 / 19:1 ≈ 2.4 圈),逐个候选圈比较:
     * 目标处 IMU 应读 0,故用 |Δθ + imu_pitch| 作代价取最小。
     * Δθ = sign*(total_h - total)/ratio 即"从这里转到该候选位需要转多少输出角"。 */
    for (int k = -4; k <= 4; k++)
    {
        float total_h = ecd_horizon_deg + 360.0f * (float)k;
        float dtheta = sign * (total_h - total_now_deg) / ratio;
        float cost = fabsf(dtheta + imu_pitch_now);

        if (cost < best_cost)
        {
            best_cost = cost;
            best_total = total_h;
            found = 1;
        }
    }

    if (!found)
        return 0.0f;

    /* 目标 IMU 读数 = 当前读数 + 需要转动的角度(与 IMU 同向约定) */
    return imu_pitch_now + sign * (best_total - total_now_deg) / ratio;
}
