# 完整枪管姿态与 NUC 标定链路

日期：2026-11-27

## 目标

- 将文档草案中的 RMCV `0x1022` 落地为 STM32→NUC 的完整云台姿态帧。
- NUC 同时接收大 yaw 平台 IMU、大 yaw 编码器、小 yaw 编码器和 pitch 编码器。
- 在 NUC 按机械关节链计算枪管世界姿态，供相机旋转外参标定使用。
- 保持旧 `0x1021` 解析兼容，不改变底盘导航和 `0x0503` 的安全开关。

## 坐标链

大 yaw 绝对方向已经包含在平台 IMU 四元数中，大 yaw 编码器只用于有效性检查，不能重复叠加：

```text
R_world_gun =
  R_world_big_imu
  R_big_imu_to_small_zero
  R_small_yaw(theta_small)
  R_small_to_pitch_zero
  R_pitch(theta_pitch)
  R_pitch_to_gun
```

小 yaw、pitch 的轴、符号、零偏和三段静态安装旋转由 NUC 参数配置。默认小 yaw 为局部 `+Z`，pitch 为局部 `+Y`；这些默认值属于待实车核对项。

## 安全边界

- 本次只生成、解析和使用反馈姿态，不启用底盘驱动、自瞄控制或发射。
- 允许主机测试与 ARM/NUC 编译；不自动烧录 STM32，不重启实车进程。
- 标定程序只采图和读取 ROS 反馈，不拥有 USB 串口。

## 验证计划

- 固件 `0x1022` 黄金字节、CRC、非法输入和 8 位序号回绕测试。
- NUC Python 分片/粘包/CRC/字段映射测试。
- 枪管旋转链单位测试，验证大 yaw 不重复叠加、小 yaw/pitch 轴向和插值。
- NUC 编译标定程序，并用已有标定图片执行内参离线回归。

## 当前实现边界

- 大 yaw 与 pitch 使用 DM CAN 样本时间；小 yaw 电机实例暂未保存 CAN 回调硬时间，因此其时间字段使用在线时的 gimbal 任务快照时间。
- Sentry 当前 `pitch_deg` 是相对云台启用时机械参考位，不是绝对枪管水平角；所以 `valid_flags.bit5` 保持 0，NUC 通过 `pitch_zero_deg` 配置补偿。
- 首版 NUC 在每帧内先合成枪管四元数，再把枪管四元数插值到图像时刻；四路原始时间仍保留，便于后续按动态误差升级为各分量独立插值。
- 新固件尚未由本次任务烧录。烧录前实体 STM32 继续发送旧 `0x1021`，标定程序会显示 `pose=WAIT` 并拒绝保存外参样本。

## 验证结果

- 固件 AX650 测试通过；导航反馈/驱动开关与 O0/Ofast 四种组合全部通过。
- C 与 Python 的 `0x1022` 61 B 帧逐字节一致。
- Sentry ARM 全量编译通过，未烧录。
- NUC 的 `rm_interfaces`、`cod_nuc_bringup` 编译通过，隔离 PTY 测试 5/5 通过。
- SP25 枪管旋转链单元测试和 `calibrate_gun_camera` 目标在 NUC 编译通过。
- NUC 验证期间发生系统重启，重启后 USB CDC 尚未枚举；按安全约束未自动重启串口桥。
