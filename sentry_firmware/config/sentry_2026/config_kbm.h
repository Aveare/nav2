/**
 * @file    config_kbm.h
 * @brief   哨兵键鼠参数:灵敏度/响应/切换
 * @layer   CONFIG
 * @author  小猫大王
 * @note    键鼠字段随 DBUS 帧下发;无键鼠数据时恒 0,永不激活。
 */
#pragma once

#define KBM_YAW_SENS 0.7f          /* [待实测] 鼠标 x → 世界 yaw 增量,deg/帧计数 */
#define KBM_PITCH_SENS 0.43f       /* [待实测] 鼠标 y → pitch 增量,deg/帧计数 */
#define KBM_DBUS_FRAME_S 0.014f    /* [规格] DBUS 帧周期,s */
#define KBM_CMD_DT_S 0.005f        /* [规格] cmd 控制周期,s(200Hz) */
#define KBM_SLOW_RATIO 0.4f        /* [待实测] Shift 慢速比例 */
#define KBM_TIMEOUT_MS 3000.0f     /* [待实测] 键鼠无输入回落遥控超时,ms */
#define KBM_MOUSE_DEADZONE 2       /* [待实测] 鼠标静止死区,原始计数 */
#define KBM_INPUT_LP 0.6f          /* [待实测] 输入一阶低通系数 */
#define KBM_YAW_MAX_RATE 450.0f    /* [待实测] 甩枪角速度上限,deg/s */
#define KBM_PITCH_MAX_RATE 150.0f  /* [待实测] 甩枪俯仰角速度上限,deg/s */
#define KBM_SENS_SCALE_MIN 0.2f    /* [待实测] 灵敏度倍率下限 */
#define KBM_SENS_SCALE_STEP 0.85f  /* [待实测] 每档步进 */
#define KBM_SENS_SCALE_MAX 2.5f    /* [待实测] 灵敏度倍率上限 */
#define KBM_STRAFE_DIR (+1.0f)     /* [待实测] 平移方向符号 */
#define KBM_PITCH_DIR (-1.0f)      /* [实测-沿用] 鼠标上滑=枪口上抬 */
