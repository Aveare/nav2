# NUC—STM32 导航与自瞄 USB 串口协议总表

> 审计日期：2026-11-26（Asia/Shanghai）；导航/电控基线：`Aveare/nav2@0c2ff13`；
> SP25 基线：`Aveare/sp25-nuc@202d16e`；
> 状态：现行协议的源码审计结果；未烧录、未完成实体 USB、云台或底盘闭环验收。

本文是 NUC 与 STM32 之间导航、自瞄通信的统一参考。它同时记录现有线上帧、ROS 2 映射、时间同步、超时门控和已确认缺口。`NAVIGATION_USB.md` 与 `AUTOAIM_USB_PROTOCOL.md` 保留为分域说明；若描述冲突，以本文列出的实际源码调用链和后续提交为准。

## 1. 结论与完整消息清单

当前哨兵 USB CDC 字节流中只有以下 6 类有效消息。检查范围包括 STM32 `usb_init()`、`usb_try_transmit()` 的全部哨兵调用者，以及 NUC 唯一串口网关的全部编码器和解析器。

| # | 方向 | 顶层帧 | 消息 | 总长度 | 计划频率 | 当前用途 |
| ---: | --- | --- | --- | ---: | ---: | --- |
| 1 | NUC → STM32 | `NV v1` | `COMMAND=1` | 32 B | 50 Hz | 底盘 `vx、vy、wz` |
| 2 | STM32 → NUC | `NV v1` | `FEEDBACK=2` | 32 B | ≤50 Hz | 大 yaw 相对底盘角 |
| 3 | NUC → STM32 | `NV v1` | `SYNC_REQUEST=3` | 32 B | 2 Hz | MCU—ROS 软件测时请求 |
| 4 | STM32 → NUC | `NV v1` | `SYNC_RESPONSE=4` | 32 B | 请求触发 | MCU—ROS 软件测时响应 |
| 5 | STM32 → NUC | RMCV `0xA5` | `0x1021` | 31 B | ≤100 Hz | 大 yaw 平台 IMU 姿态、模式、弹速、机器人 ID |
| 6 | NUC → STM32 | RMCV `0xA5` | `0x0503` | 39 B | 100 Hz | 自瞄目标角、角速度、角加速度、距离和发射请求 |

没有其他现行 USB CDC 业务帧。以下内容不属于这根 USB 串口：

- `/autoaim/priority_ids`、`/autoaim/invincible_ids` 和 `/auto_aim_target_pos` 只在 NUC 内部通过 ROS 2 交换。
- `/Odometry` 只作为导航命令新鲜度门控，不会被编码进 STM32 串口帧。
- USART1 的 VOFA JustFloat 是独立调试链路；旧 AX650 USART1 `921600 8E1` 当前关闭。
- 仓库中的旧 `cod_serial`、`ros2_simple_serial` 及 15 字节历史协议不是实车入口，不得与本协议同时打开 `/dev/ttyACM*`。

## 2. 物理链路与唯一所有者

```text
Nav2 /cmd_vel ───────────────┐
LIO /Odometry ───────────────┤
                             ├─ sentry_usb_bridge.py ─ USB CDC ─ autonomy.c
SP25 /autoaim/target ────────┤         唯一所有者           唯一所有者
                             │
/big_yaw/joint_states ◄──────┤
/autoaim/pose ◄──────────────┘
```

NUC 端只有 `sentry_usb_bridge.py` 可以打开设备，使用 `exclusive=True`。默认设备为 `/dev/ttyACM0`，现场应改用稳定的 `/dev/serial/by-id/...`。

脚本以 `115200 8N1` 打开 pyserial。对于 USB CDC ACM，这个波特率通常只是主机侧线路编码参数，不决定 USB 总线实际传输速率；帧字节序、长度和 CRC 才是协议合同。

STM32 USB FS 单次接收块最大 64 B。接收回调只把字节和 `received_ms` 复制到 8 槽队列，`autonomy_task()` 再把同一批字节分别送入 NV 和 RMCV 流式解析器。分包、粘包及两种协议交错均不能依赖一次 USB 回调等于一帧。

顶层分流特征：

| 首字节/前缀 | 解析器 |
| --- | --- |
| `4E 56 01`，即 `NV\x01` | NV v1 固定 32 B 解析器 |
| `A5` | RMCV 变长帧解析器 |
| 其他字节 | 逐字节丢弃并重新寻找帧头 |

## 3. 公共数据约定

- 多字节整数和 IEEE754 binary32 `float` 均为小端。
- 禁止直接传输 C/C++ 结构体内存；所有字段按偏移逐项编解码。
- ROS 车体坐标采用 `base_link`：`+x` 前、`+y` 左、`+z` 上，`angular.z` 逆时针为正。
- 四元数内部约定为 `[w,x,y,z]`，表示机体到导航/世界坐标的主动旋转。
- 世界角度使用度 `deg`，角速度和角加速度使用 `rad/s`、`rad/s²`；底盘速度使用 `m/s`。
- 所有浮点输入必须为有限数。NaN、Inf、CRC 错误和长度错误均不得刷新命令有效期。

## 4. NV v1 导航帧

### 4.1 固定帧布局

| 帧偏移 | 长度 | 类型 | 字段 |
| ---: | ---: | --- | --- |
| 0 | 2 | `uint8[2]` | ASCII `NV`，十六进制 `4E 56` |
| 2 | 1 | `uint8` | 版本，固定为 `1` |
| 3 | 1 | `uint8` | 消息类型 `1～4` |
| 4 | 4 | `uint32` | `sequence` |
| 8 | 8 | `uint64` | `sample_us` |
| 16 | 4 | `float` | `value[0]` |
| 20 | 4 | `float` | `value[1]` |
| 24 | 4 | `float` | `value[2]` |
| 28 | 2 | `uint16` | `flags` |
| 30 | 2 | `uint16` | CRC16/MODBUS |

CRC16/MODBUS 参数：

| 参数 | 值 |
| --- | --- |
| 初值 | `0xFFFF` |
| 反射多项式 | `0xA001` |
| 覆盖范围 | 帧字节 `0..29` |
| 存放顺序 | 低字节在前 |
| 标准向量 | ASCII `123456789` → `0x4B37` |

`flags` 当前只定义 bit0：

| 位 | 名称 | 含义 |
| ---: | --- | --- |
| 0 | `VALID` | 数据有效 |
| 1～15 | 保留 | 必须为 0 |

### 4.2 `COMMAND=1`：底盘速度

| 字段 | 含义 |
| --- | --- |
| `sequence` | NUC 底盘命令独立的 `uint32` 递增序号，允许自然回绕 |
| `sample_us` | NUC 生成该帧时的 ROS 时钟微秒值；只用于追踪，不当作 MCU 时间 |
| `value[0]` | `base_link vx`，m/s，前进为正 |
| `value[1]` | `base_link vy`，m/s，左移为正 |
| `value[2]` | `base_link wz`，rad/s，逆时针为正 |
| `flags.bit0` | 1=有效速度；0=停止/失效帧 |

NUC 只取 `/cmd_vel` 的 `linear.x`、`linear.y`、`angular.z`，Twist 其余 3 个分量不发送。当前边界为每轴平移 `0.35 m/s`、角速度 `0.5 rad/s`。

STM32 接收后执行：

```text
vx_mm_s  =  vx_m_s × 1000
vy_mm_s  = -vy_m_s × 1000
wz_deg_s =  wz_rad_s × 180 / π
```

`VALID=0` 时 STM32 显式把三轴速度清零。有效会话内只接受向前序号；重复序号或相对当前序号倒退至少 `2^31` 的帧不会续命。命令已超过 300 ms 后允许新主机从任意序号重新建立会话。

### 4.3 `FEEDBACK=2`：大 yaw 角度反馈

| 字段 | 含义 |
| --- | --- |
| `sequence` | STM32 大 yaw 反馈独立的 `uint32` 递增序号 |
| `sample_us` | 大 yaw DM CAN 反馈进入 MCU 时的 `kernel_tick_ms() × 1000` |
| `value[0]` | 大 yaw 相对底盘机械对齐零位的角度，rad，折回 `[-π,π]` |
| `value[1]` | 0，保留 |
| `value[2]` | 0，保留 |
| `flags.bit0` | 大 yaw DM 反馈新鲜/在线 |

只有 CAN 样本时间变化时才尝试发送，禁止把同一个旧编码器样本反复包装成新反馈。NUC 完成时钟映射后发布：

```text
/big_yaw/joint_states
sensor_msgs/msg/JointState
name     = ["big_yaw_joint"]
position = [value[0]]
```

### 4.4 `SYNC_REQUEST=3`：时间同步请求

| 字段 | 含义 |
| --- | --- |
| `sequence` | NUC 测时请求独立的 `uint32` 序号 |
| `sample_us` | NUC 发起请求时的 ROS 时钟微秒值 |
| `value[0..2]` | 0 |
| `flags` | 0 |

NUC 每 500 ms 生成一次请求。当前 `ClockSync` 只保留一个 pending 请求，新的请求会替换尚未响应的旧请求。

### 4.5 `SYNC_RESPONSE=4`：时间同步响应

| 字段 | 含义 |
| --- | --- |
| `sequence` | 原样回显请求序号 |
| `sample_us` | STM32 处理请求时的 `kernel_tick_ms() × 1000` |
| `value[0..2]` | 0 |
| `flags` | 0 |

STM32 不是回显请求中的主机时间，而是写入本机处理时刻。该响应只建立 MCU tick 与 ROS 时钟的偏移关系。

## 5. RMCV 自瞄帧

### 5.1 通用变长布局

| 帧偏移 | 长度 | 类型 | 字段 |
| ---: | ---: | --- | --- |
| 0 | 1 | `uint8` | 帧头 `0xA5` |
| 1 | 2 | `uint16` | payload 长度 `N`，不含命令号与 CRC |
| 3 | 1 | `uint8` | `sequence`，255 后回绕 |
| 4 | 1 | `uint8` | CRC8，覆盖字节 `0..3` |
| 5 | 2 | `uint16` | 命令号 |
| 7 | N | bytes | payload |
| 7+N | 2 | `uint16` | CRC16，覆盖此前所有字节 |

| 校验 | 初值 | 反射多项式 | 最终异或 | 存放 |
| --- | --- | --- | --- | --- |
| CRC8 | `0xFF` | `0x8C` | 无 | 单字节 |
| CRC16 | `0xFFFF` | `0x8408` | 无 | 小端 |

NUC Python 接收端把 payload 上限限制为 64 B；STM32 缓冲更大，但双端新增消息仍必须遵守 64 B 共同上限。未知命令号在 CRC 和长度正确后整帧跳过，不会被解释成已知命令。

### 5.2 `0x1021`：STM32 姿态上行

payload 22 B，整帧 31 B：

| payload 偏移 | 长度 | 类型 | 字段 | 单位与语义 |
| ---: | ---: | --- | --- | --- |
| 0 | 4 | `float` | `yaw` | deg，ZXY 欧拉角中的 Z |
| 4 | 4 | `float` | `pitch` | deg，ZXY 欧拉角中的 X |
| 8 | 4 | `float` | `roll` | deg，ZXY 欧拉角中的 Y |
| 12 | 4 | `float` | `projectile_speed` | m/s；当前固定为 0 |
| 16 | 1 | `uint8` | `program_mode` | 当前左拨杆中位上报 1，否则 0 |
| 17 | 4 | `uint32` | `sample_time_ms` | INS 更新后读取的 MCU `kernel_tick_ms()` |
| 21 | 1 | `uint8` | `robot_id` | 裁判系统本车 ID；当前固定为 0 |

当前姿态来源：

```text
BMI088 → EKF INS q[w,x,y,z]
       → ax650_usb_update_quaternion(q, kernel_tick_ms())
       → ZXY 欧拉角
       → 0x1021
```

欧拉角满足：

```text
R = Rz(yaw) × Rx(pitch) × Ry(roll)
```

接近 ZXY 的 `pitch=±90°` 奇异点时，打包函数拒绝发送该姿态。`sequence` 只有成功生成帧后才递增；USB 忙时姿态帧会顺延，不保证严格 100 Hz。

重要限制：`SENTRY_IMU_AFTER_SMALL_YAW=0`，BMI088 安装在大 yaw 平台。因此现行 `0x1021` 只表示大 yaw 平台姿态，不含小 yaw 编码器和 pitch 编码器，不是完整枪管或相机姿态。

NUC 发布：

```text
/autoaim/pose
rm_interfaces/msg/AutoAimPose
```

ROS 消息字段映射：

| ROS 字段 | 来源 |
| --- | --- |
| `header.stamp` | `sample_time_ms` 经 NV 测时映射得到的 ROS 时间 |
| `header.frame_id` | 固定 `gimbal_imu` |
| `sequence` | RMCV 8 位序号 |
| `mcu_sample_time_ms` | 原样保留 MCU 时间 |
| `yaw/pitch/roll` | `0x1021` 欧拉角，deg |
| `projectile_speed` | `0x1021`，当前 0 |
| `mode` | `program_mode` |
| `robot_id` | 当前 0 |

### 5.3 `0x0503`：NUC 自瞄目标下行

payload 30 B，整帧 39 B：

| payload 偏移 | 长度 | 类型 | 字段 | 单位与语义 |
| ---: | ---: | --- | --- | --- |
| 0 | 4 | `float` | `yaw` | deg，世界系绝对目标角 |
| 4 | 4 | `float` | `pitch` | deg，当前 IMU 反馈域的绝对目标角 |
| 8 | 4 | `float` | `yaw_speed` | rad/s |
| 12 | 4 | `float` | `pitch_speed` | rad/s |
| 16 | 4 | `float` | `yaw_acc` | rad/s² |
| 20 | 4 | `float` | `pitch_acc` | rad/s² |
| 24 | 4 | `float` | `distance` | m，必须非负 |
| 28 | 1 | `uint8` | `shoot` | 发射请求，合法值 0～3 |
| 29 | 1 | `uint8` | `target_id` | 0=无目标，1～9=目标编号 |

ROS 输入为 `/autoaim/target`，类型 `rm_interfaces/msg/AutoAimTarget`。其 `header.stamp` 和 `header.frame_id` **不会写入串口**。网关只记录该 ROS 消息到达时的 NUC 单调时间，并重新分配 RMCV `sequence`。

SP25 当前把 `header.stamp` 设置为发布时的 ROS `now()`，并非图像时间。相同 `target_id` 的连续有效目标只有在本机发布间隔位于 2～200 ms 时才用角度差分估算速度和加速度；角速度限制为 `±8 rad/s`，角加速度限制为 `±100 rad/s²`，不满足条件时四项导数为 0。`shoot` 由 SP25 布尔结果映射为 0 或 1。

网关每 10 ms 发送一帧：

- `enable_autoaim=true` 且最后一次合法 ROS 目标到达未超过 `autoaim_timeout` 时，发送目标内容。
- 其他情况发送全零目标，明确设置 `shoot=0、target_id=0`。
- 网关关闭时尽力发送一次无效 NV 停车帧和一次无目标 `0x0503`；STM32 超时仍是最终兜底。

目标生成时间、图像采集时间和预测命中时间均未进入 `0x0503`。

STM32 解析器拒绝 NaN/Inf、负距离、`shoot>3`、`target_id>9`。通过 CRC 和字段检查后写入 `ax650_usb_state_t`，记录 USB 数据块的 `received_ms` 并增加 `command_count`。

当前 STM32 不使用 `0x0503` 的 8 位 `sequence` 做去重、乱序判断或时间计算；每一帧通过校验的目标都会刷新接收时间并增加计数。该序号目前只能用于链路诊断，不能替代目标时间戳。

当前实际调用链止于解析保存：哨兵应用没有调用 `ax650_usb_get_state()`，所以 `0x0503` 尚未进入 `gimbal_cmd` 或 `shoot_cmd`。`shoot` 也不会驱动发射机构。协议可收，不等于云台闭环已接通。

## 6. ROS 2 消息与串口字段对照

| ROS 话题 | 类型 | 是否进串口 | 处理方式 |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 是 | 只取 `linear.x/y`、`angular.z` → NV COMMAND |
| `/Odometry` | `nav_msgs/msg/Odometry` | 否 | 只检查 frame 和时间戳新鲜度，作为 COMMAND 的 `VALID` 门控 |
| `/big_yaw/joint_states` | `sensor_msgs/msg/JointState` | 串口输出 | NV FEEDBACK 映射后发布 |
| `/autoaim/pose` | `rm_interfaces/msg/AutoAimPose` | 串口输出 | `0x1021` 映射后发布 |
| `/autoaim/target` | `rm_interfaces/msg/AutoAimTarget` | 是 | header 丢弃，数据区 → `0x0503` |
| `/autoaim/priority_ids` | `std_msgs/msg/UInt8MultiArray` | 否 | NUC 内部筛选目标 |
| `/autoaim/invincible_ids` | `std_msgs/msg/UInt8MultiArray` | 否 | NUC 内部过滤无敌目标 |
| `/auto_aim_target_pos` | `std_msgs/msg/String` | 否 | NUC 内部决策/诊断输出 |

## 7. 三种时钟与完整时间链

### 7.1 时钟域

| 时钟 | 所在端 | 用途 | 特性 |
| --- | --- | --- | --- |
| ROS 时钟 `node.get_clock()` | NUC | ROS 消息时间戳、测时基准 | 当前使用系统 ROS 时间，无 `use_sim_time` |
| `time.monotonic()` / `steady_clock` | NUC | 超时、新鲜度、SP25 图像/姿态插值 | 不受系统校时跳变影响 |
| `kernel_tick_ms()` | STM32 | CAN、INS、USB 接收和命令超时 | 32 位毫秒 tick，约 49.7 天回绕，1 ms 量化 |

协议没有使用约 25.6 秒回绕的未展开 DWT 微秒计数；名称为 `sample_us` 的 MCU 值实际来自毫秒 tick 乘 1000。

### 7.2 MCU—ROS 偏移估计

一次有效交换：

```text
t0_ros ── SYNC_REQUEST ──► MCU
                           tmcu
t1_ros ◄─ SYNC_RESPONSE ── MCU
```

NUC 计算：

```text
RTT       = t1_monotonic - t0_monotonic
offset_ns = (t0_ros_ns + t1_ros_ns) / 2 - tmcu_us × 1000
stamp_ns  = sample_us × 1000 + offset_ns
```

接受条件：

- `0 < RTT ≤ 20 ms`。
- 同一次交换中 ROS 时间增量与单调时间增量之差不超过 5 ms。
- 保存最近 10 秒、最多 20 个样本，采用 RTT 最小样本的偏移。
- 偏移超过 2 秒未更新即失效。
- 映射出的样本相对当前 ROS 时间必须位于未来不超过 20 ms、过去不超过 200 ms。

该方法不是硬件时间同步。链路不对称时，20 ms RTT 样本的中点误差理论上可接近 10 ms，另有 1 ms tick 量化及传感器采样到软件打时间戳的延迟。

### 7.3 大 yaw 时间链

```text
DM CAN 接收回调
  → rx_sample_us = kernel_tick_ms() × 1000
  → 与 big_yaw_rel_deg 一起进入 gimbal_feed
  → NV FEEDBACK.sample_us
  → ClockSync 映射
  → /big_yaw/joint_states.header.stamp
```

MCU tick 倒退或回绕时，NUC 清空旧偏移并重新同步。重复 `sample_us` 不重复发布。

### 7.4 自瞄姿态时间链

```text
INS EKF 更新 q
  → kernel_tick_ms()
  → 0x1021.sample_time_ms
  → ClockSync 映射
  → /autoaim/pose.header.stamp
  → SP25 换算为本机 steady_clock 时间
  → 与 image_time - 1 ms 做四元数 SLERP
```

SP25 最多等待 100 ms 获取覆盖图像时刻的较新姿态样本；姿态接收超过 200 ms 即抑制目标。姿态 ROS 时间若比当前未来超过 50 ms或过去超过 1 s，SP25 会退化为使用接收时刻。

当前相机 `image_time` 是 NUC 相机线程取到图像时的 `steady_clock::now()`，不是相机硬件曝光时间戳。固定减 1 ms 也是现有经验补偿，不是已标定的链路延迟。

### 7.5 自瞄下行缺失的时间信息

现行 `/autoaim/target.header.stamp` 在网关中被丢弃，`0x0503` 没有以下字段：

- 图像曝光/采集时间。
- 目标解算时间。
- 目标适用时间或预测命中时间。
- NUC 发送时间。
- STM32 应用目标的期望时间。

因此当前只实现“姿态对齐到图像时间”，没有实现“目标命令按指定时间执行”。如果运动目标延迟补偿需要这些信息，必须扩展协议，不能把 RMCV 帧序号当时间戳。

## 8. 频率、超时与使能门控

| 项目 | 当前值 | 失效行为 |
| --- | ---: | --- |
| 串口轮询 | 200 Hz | 影响接收延迟和测时 RTT |
| NV COMMAND 周期 | 20 ms / 50 Hz | 条件不满足仍发送无效零速帧 |
| NV FEEDBACK 周期上限 | 20 ms / 50 Hz | 无新 CAN 样本不发送 |
| SYNC 周期 | 500 ms / 2 Hz | 2 s 无有效同步则时间映射失效 |
| `0x1021` 周期上限 | 10 ms / 100 Hz | USB 忙时顺延 |
| `0x0503` 周期 | 10 ms / 100 Hz | 无目标也发送 `target_id=0、shoot=0` |
| NUC 导航输入新鲜度 | 200 ms | COMMAND 变为无效零速 |
| NUC 自瞄目标新鲜度 | 默认 100 ms | 下发无目标帧 |
| STM32 导航命令超时 | 300 ms | `auto_cmd.valid=0` |
| STM32 自瞄命令超时 | 100 ms | `ax650_usb_state.command_valid=0`，但当前没有消费者 |
| NUC 串口写阻塞 | 100 ms | 断开、清缓存，1 s 后重连 |
| SP25 姿态新鲜度 | 200 ms | 发布无目标并停止解算控制 |

导航 COMMAND 只有以下条件全部满足时才设置 `VALID=1`：

1. NUC `enable_drive=true`。
2. `/cmd_vel` 接收年龄 ≤200 ms，且速度有限并在边界内。
3. `/Odometry` 接收年龄 ≤200 ms，`header.frame_id=odom`、`child_frame_id=base_link`。
4. Odometry 消息时间戳年龄位于 `[-20 ms,200 ms]`。
5. 大 yaw 有效反馈接收年龄 ≤200 ms。
6. 最近 2 秒内有有效 NV 测时结果。

底盘真正接管还必须满足：

1. STM32 编译时 `SENTRY_AUTONOMY_ENABLE=1`；当前为 0。
2. 遥控器左右拨杆均在中位。
3. 右摇杆两个通道均在死区内。
4. 原有遥控失联、急停锁存和 `SENTRY_CHASSIS_FORCE_DISABLE` 仲裁放行。

自瞄网关下发真实目标需要 `enable_autoaim=true`。但当前 STM32 哨兵调用链没有把已解析目标接入云台，所以即使打开该参数也不能据此宣称云台会跟踪。

## 9. 当前源码状态与已确认缺口

| 功能 | 当前状态 | 证据/影响 |
| --- | --- | --- |
| NV 四类帧 | 已双端实现 | C/Python 黄金字节测试存在 |
| `0x1021` | 已双端实现 | 发布 `/autoaim/pose` |
| `0x0503` | 已双端编解码 | STM32 仅保存，无哨兵控制消费者 |
| 底盘导航接管 | 代码存在、默认关闭 | `SENTRY_AUTONOMY_ENABLE=0`、`enable_drive=false` |
| 自瞄真实目标下发 | 网关默认关闭 | `enable_autoaim=false` |
| 枪管完整姿态 | 未实现 | IMU 在大 yaw 平台，缺小 yaw/pitch 合成 |
| pitch 水平绝对零位 | 未实现 | 当前 `pitch_deg` 相对云台启用时位置，不是枪管水平零位 |
| 弹速上行 | 接口存在、无生产者 | `projectile_speed=0` |
| 机器人 ID 上行 | 接口存在、无生产者 | `robot_id=0` |
| 目标时间戳下行 | 未定义 | `0x0503` 无时间字段，ROS header 不序列化 |
| 命令确认/状态回传 | 未定义 | 无最近接收序号、使能状态、发射允许或错误计数 |
| 实车 USB/运动验证 | 未执行 | 主机测试和编译不等于实体闭环 |

此外，`config_ax650.h` 文件头仍写有“VCP 帧协议尚未定义”的历史注释，但当前代码已实现共享 USB CDC `0x1021/0x0503`。实际宏值 `AX650_USB_ENABLE=1、AX650_UART1_ENABLE=0` 和源码调用链优先于该过期注释。

## 10. 完整枪管姿态所需扩展（草案，尚未实现）

为了在 NUC 按“大 yaw 平台 IMU + 小 yaw 编码器 + pitch 编码器”合成枪管/相机姿态，现行 `0x1021` 信息不足。建议保留 `0x1021` 兼容旧接口，新增暂定命令 `0x1022`。在代码正式实现并通过双端黄金测试前，`0x1022` 不是线上协议，不得发送。

建议 payload 52 B，整帧 61 B，能够装入一次 USB FS 64 B 数据包：

| payload 偏移 | 长度 | 类型 | 字段 | 单位 |
| ---: | ---: | --- | --- | --- |
| 0 | 4 | `float` | `q_w` | 无量纲 |
| 4 | 4 | `float` | `q_x` | 无量纲 |
| 8 | 4 | `float` | `q_y` | 无量纲 |
| 12 | 4 | `float` | `q_z` | 无量纲 |
| 16 | 4 | `float` | `big_yaw_rel` | rad |
| 20 | 4 | `float` | `small_yaw` | rad，相对机械中位 |
| 24 | 4 | `float` | `pitch` | rad，相对枪管水平零位 |
| 28 | 4 | `float` | `projectile_speed` | m/s |
| 32 | 4 | `uint32` | `imu_sample_time_ms` | MCU tick |
| 36 | 4 | `uint32` | `big_yaw_sample_time_ms` | MCU tick |
| 40 | 4 | `uint32` | `small_yaw_sample_time_ms` | MCU tick |
| 44 | 4 | `uint32` | `pitch_sample_time_ms` | MCU tick |
| 48 | 2 | `uint16` | `valid_flags` | 位标志 |
| 50 | 1 | `uint8` | `program_mode` | 模式 |
| 51 | 1 | `uint8` | `robot_id` | 本车 ID |

建议 `valid_flags`：

| 位 | 含义 |
| ---: | --- |
| 0 | 大 yaw 平台 IMU 有效 |
| 1 | 大 yaw 编码器有效 |
| 2 | 小 yaw 编码器有效 |
| 3 | pitch 编码器有效 |
| 4 | 小 yaw 机械中位已标定 |
| 5 | pitch 枪管水平零位已标定 |
| 6 | 弹速有效 |
| 7 | robot_id 有效 |
| 8 | 电控允许自瞄 |
| 9 | 云台执行器均在线 |
| 10～15 | 保留为 0 |

使用每个传感器自己的采样时刻，NUC 可以先对 IMU 和关节角分别插值到图像时刻，再组合：

```text
q_world_gun =
    q_world_big_imu
  ⊗ q_big_imu_to_small_zero
  ⊗ q_small_yaw(theta_small)
  ⊗ q_small_to_pitch_zero
  ⊗ q_pitch(theta_pitch)
  ⊗ q_pitch_to_gun

q_world_camera = q_world_gun ⊗ q_gun_to_camera
```

实现前还必须确认两个关节轴、正方向、传动比、机械零位和固定安装旋转。不能仅凭代码名称假定小 yaw 绕世界 Z、pitch 绕世界 Y。

### 10.1 建议但非首轮必需的状态确认帧

正式比赛诊断建议再分配一类 STM32 → NUC 状态帧，至少包含：

- 最近接收的 NV COMMAND 序号、年龄和有效状态。
- 最近接收的 `0x0503` 序号、年龄、`target_id` 和有效状态。
- 导航、自瞄、遥控、机器人 READY 的实际使能状态。
- 大 yaw、小 yaw、pitch 在线状态及控制误差。
- 摩擦轮就绪、热量允许、堵转状态和最终发射允许状态。
- USB RX 队列溢出、CRC 错误、未知帧和 TX busy 计数。

该帧命令号和字节布局尚未分配。它只用于确认和诊断，不能替代 STM32 本地超时、限位、热量和急停保护。

## 11. 联调检查顺序

1. 保持 `enable_drive=false、enable_autoaim=false、SENTRY_AUTONOMY_ENABLE=0`。
2. 只检查 SYNC_REQUEST/RESPONSE，确认持续获得有效偏移。
3. 检查 NV FEEDBACK 和 `/big_yaw/joint_states` 的频率、零位、方向和时间戳。
4. 检查 `0x1021` 和 `/autoaim/pose`，确认 8 位序号、MCU 时间、模式和姿态方向。
5. 发送 `target_id=0、shoot=0` 的 `0x0503`，只确认解析计数和 100 ms 超时，不让云台运动。
6. 完成 `0x1022` 或等价扩展后，分别只转大 yaw、小 yaw、pitch，核对各轴方向和图像时刻插值。
7. 接通 `0x0503 → gimbal_cmd` 后，关闭发射机构，从小角度静态目标开始验证。
8. 分别断开 `/cmd_vel`、`/Odometry`、大 yaw CAN、姿态、SP25 和 USB，确认每一层超时降级。
9. 最后才考虑把 `shoot` 请求接入发射仲裁，并逐项验证摩擦轮、热量、堵转、裁判和遥控门槛。

## 12. 源码对应关系

| 内容 | 路径 |
| --- | --- |
| NUC 唯一串口网关 | `src/cod_nuc_bringup/scripts/sentry_usb_bridge.py` |
| NV Python 编解码与测时 | `src/cod_nuc_bringup/scripts/nav_wire.py` |
| RMCV Python 编解码 | `src/cod_nuc_bringup/scripts/rmcv_wire.py` |
| ROS 自瞄消息 | `src/cod_behavior_stack/rm_interfaces/msg/AutoAimPose.msg`、`AutoAimTarget.msg` |
| STM32 NV 编解码 | `components/navigation/nav_protocol.c/.h` |
| STM32 USB 唯一所有者 | `apps/sentry/autonomy/autonomy.c` |
| STM32 RMCV 编解码 | `components/ax650/ax650_pose.c/.h` |
| STM32 RMCV USB 状态 | `components/ax650/ax650_usb.c/.h` |
| IMU 姿态生产者 | `components/imu/ins_task.c` |
| 三轴反馈生产者 | `apps/sentry/gimbal/gimbal.c` |
| SP25 ROS 适配 | `io/ros2/cod_bridge.cpp/.hpp` |
| SP25 主循环与图像时刻 | `src/cod_sentry.cpp` |

## 13. 维护规则

- 新增、删除或修改任何线上字段时，必须同步更新 STM32、NUC、ROS 消息、双端黄金字节测试和本文。
- 已实现消息写入第 1 节清单；只建议但未实现的消息必须明确标为草案。
- 时间字段必须说明“传感器采样时刻、软件处理时刻、发送时刻、接收时刻”中的哪一种。
- 不得用帧序号代替时间戳，也不得用 USB 接收时刻冒充传感器采样时刻。
- 主机测试、ARM 编译、烧录、台架和整车验证必须分层记录，任何一层不能替代后一层。
