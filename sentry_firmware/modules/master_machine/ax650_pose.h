/**
 * @file    ax650_pose.h
 * @brief   AX650 姿态上报的字节布局与封包接口
 * @layer   MODULE
 * @note    纯字节协议，不访问硬件；多字节字段使用小端序。
 */
#ifndef AX650_POSE_H
#define AX650_POSE_H

#include <stddef.h>
#include <stdint.h>

#define AX650_POSE_FRAME_SIZE 26u // [协议] 整帧字节数，17 字节数据加 9 字节帧开销

/**
 * @brief  计算 RMCV 帧头 CRC8
 * @param  data  字节数组，无量纲
 * @param  count  参与校验的字节数；帧头取前 4 字节
 * @return 反射多项式 0x8C、初值 FF、无最终异或的校验值。
 */
uint8_t Ax650PoseCrc8(const uint8_t *data, size_t count);

/**
 * @brief  计算 RMCV 整帧 CRC16
 * @param  data  字节数组，无量纲
 * @param  count  参与校验的字节数；26 字节姿态帧取前 24 字节
 * @return 反射多项式 0x8408、初值 FFFF、无最终异或的校验值。
 */
uint16_t Ax650PoseCrc16(const uint8_t *data, size_t count);

/**
 * @brief  将真实四元数封装为 AX650 的 0x1021 姿态帧
 * @param  out  输出缓冲区，无量纲
 * @param  capacity  输出缓冲区容量，字节
 * @param  q_wxyz  机体到导航的主动旋转四元数 [w,x,y,z]，无量纲
 * @param  speed_mps  弹丸初速，m/s；手眼标定填 0
 * @param  mode  AX650 ProgramMode 原始枚举值，无量纲；标定填 0
 * @param  sequence  帧序号，无量纲；255 后允许回绕
 * @return 成功返回 26；输入非法、非有限或接近 ZXY 奇异点时返回 0。
 * @note   输出 yaw/pitch/roll 为 deg，满足 Rz(yaw)Rx(pitch)Ry(roll)。
 * @note   IT/DMA 发送完成前，调用方必须保持输出缓冲区不变。
 */
size_t Ax650PosePack(uint8_t *out, size_t capacity, const float q_wxyz[4],
                     float speed_mps, uint8_t mode, uint8_t sequence);

#endif
