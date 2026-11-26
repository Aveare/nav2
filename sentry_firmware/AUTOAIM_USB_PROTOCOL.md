# 哨兵自瞄 USB CDC 通信方案

> 状态：2026-11-26 已完成 STM32 与 ROS 2 双端实现、主机协议测试和 ARM 编译；尚未烧录与实车联调，不能据此声称云台或发射机构已经可用。

## 1. 结论

自瞄不要再设计一套新的目标字段。STM32 工程已经实现并测试了 AX650/RMCV 的两类二进制消息：

- STM32 → 自瞄：`0x1021` 姿态与弹速；
- 自瞄 → STM32：`0x0503` 目标角、角速度、角加速度和目标状态。

迁移到 USB 虚拟串口时，保留这两类帧的字节布局、单位、CRC 和超时语义，只替换底层传输端口。导航仍使用 `NAVIGATION_USB.md` 中的 `NV` 帧。

USB CDC 设备只能由一个上位机进程独占打开。当前实现为：

```text
ROS 2 导航节点 ─┐
                 ├─ sentry_usb_bridge.py（唯一串口所有者）─ /dev/serial/by-id/... ─ STM32
自瞄程序 ───────┘

NV 帧：导航速度、平台角度、测时
0xA5 帧：自瞄姿态、目标指令
```

不得同时运行两个分别打开 `/dev/ttyACM0` 的进程。USB CDC 的“波特率”通常不是实际链路速率，但二进制布局和小端序必须保持一致。

实现文件：

- STM32 传输状态：`components/ax650/ax650_usb.c`；
- STM32 唯一 USB 所有者：`apps/sentry/autonomy/autonomy.c`；
- ROS 2 网关：`src/cod_nuc_bringup/scripts/sentry_usb_bridge.py`；
- Python 协议：`src/cod_nuc_bringup/scripts/rmcv_wire.py`；
- ROS 消息：`rm_interfaces/msg/AutoAimTarget.msg` 与 `AutoAimPose.msg`。

## 2. 通用 RMCV 帧格式

| 偏移 | 长度 | 字段 |
|---:|---:|---|
| 0 | 1 | 帧头 `0xA5` |
| 1 | 2 | 数据区长度，小端 `uint16` |
| 3 | 1 | 帧序号，`uint8` 回绕 |
| 4 | 1 | CRC8，覆盖字节 0..3 |
| 5 | 2 | 命令号，小端 `uint16` |
| 7 | N | 数据区 |
| 7+N | 2 | CRC16，覆盖此前全部字节，小端存放 |

- CRC8：初值 `0xFF`，反射多项式 `0x8C`，无最终异或；
- CRC16：初值 `0xFFFF`，反射多项式 `0x8408`，无最终异或；
- 浮点数：IEEE754 binary32，小端；
- 接收端必须支持 USB 分包、粘包、坏帧后逐字节重同步。

## 3. STM32 发送给自瞄程序

### 3.1 `0x1021` 姿态帧

现有代码：`components/ax650/ax650_pose.c::ax650_pose_pack()`。

数据区长度 22 字节，整帧 31 字节，建议 100 Hz 发送。

| 数据区偏移 | 类型 | 字段 | 单位/语义 |
|---:|---|---|---|
| 0 | float | yaw | deg，世界系绝对角，ZXY 欧拉角 |
| 4 | float | pitch | deg，IMU 反馈域绝对角 |
| 8 | float | roll | deg |
| 12 | float | projectile_speed | m/s，裁判系统实测弹速；未知填 0 |
| 16 | uint8 | program_mode | 自瞄模式；非自瞄状态由双方约定为 0 |
| 17 | uint32 | sample_time_ms | MCU 姿态采样时间，毫秒，小端，允许回绕 |
| 21 | uint8 | robot_id | 裁判系统本车 ID；0 表示未知，自瞄据此判断敌方颜色 |

自瞄程序应使用 `sample_time_ms` 做时间对齐，不要把 USB 接收时刻当成姿态采样时刻。当前网关复用 `NV` 类型 3/4 的测时结果估计 MCU 与主机时钟偏移，并发布原采样时间对应的 ROS 时间戳。

当前 `projectile_speed` 和 `robot_id` 的默认值均为 0；在裁判系统弹速和机器人 ID 尚未接入 `ax650_usb_set_*()` 前，自瞄端不得把它们当成有效实测值。

### 3.2 建议后续增加的状态反馈

首轮联调可只用 `0x1021`。正式比赛前建议再增加一类状态帧，但命令号尚未分配，不能直接写死：

- 最近有效 `0x0503` 的序号和接收计数；
- 自瞄链路在线/超时；
- 当前是否允许自瞄、摩擦轮是否就绪；
- 裁判热量是否允许发射；
- CRC 错误、丢帧和 USB 重连计数；
- 当前大 yaw、小 yaw、pitch 实际角度或控制误差。

该状态帧用于调试和安全确认，不替代 STM32 本地保护逻辑。

## 4. 自瞄程序发送给 STM32

### 4.1 `0x0503` 目标控制帧

现有代码：`components/ax650/ax650_pose.c::ax650_parser_feed()`。

数据区长度 30 字节，整帧 39 字节。自瞄有目标时建议 100 Hz 发送；无目标时也应主动发送 `target_id=0、shoot=0`，随后由 STM32 的 100 ms 超时继续兜底。

| 数据区偏移 | 类型 | 字段 | 单位/语义 |
|---:|---|---|---|
| 0 | float | yaw | deg，世界系绝对目标角 |
| 4 | float | pitch | deg，IMU 反馈域绝对目标角 |
| 8 | float | yaw_speed | rad/s，目标 yaw 角速度 |
| 12 | float | pitch_speed | rad/s，目标 pitch 角速度 |
| 16 | float | yaw_acc | rad/s²，目标 yaw 角加速度 |
| 20 | float | pitch_acc | rad/s²，目标 pitch 角加速度 |
| 24 | float | distance | m，目标距离 |
| 28 | uint8 | shoot | 自瞄发射请求，当前固件尚未接入发射控制 |
| 29 | uint8 | target_id | 0=无目标，1..9=目标编号 |

必须拒绝 NaN/Inf；当前 STM32 还会拒绝 `shoot>3` 或 `target_id>9`。

### 4.2 发射请求的安全边界

当前 `shoot` 字段只被解析，不会驱动拨盘。后续接入时必须把它定义为“发射请求”，不能作为直接电机命令。最终发射必须同时满足：

1. `0x0503` 帧 CRC 正确且未超过超时；
2. `target_id != 0`，自瞄解算有效；
3. 人工/裁判允许进入自瞄发射状态；
4. 摩擦轮达到稳定转速；
5. STM32 本地热量守卫允许发弹；
6. 拨盘无堵转、离线或其他故障；
7. 连续发射必须由 STM32 限频，USB 命令不能绕过保护。

断开 USB、CRC 连续错误、目标丢失或命令超过 100 ms 时，必须立即撤销发射请求。云台角度的超时行为可先保持上一安全目标，但不得继续发弹。

## 5. 与导航消息共用一个 USB 口

当前 `NV v1` 已使用以下类型：

| 方向 | 类型 | 内容 |
|---|---:|---|
| NUC → STM32 | 1 | 底盘 `vx、vy、wz` |
| STM32 → NUC | 2 | 大 yaw 相对底盘角 |
| NUC → STM32 | 3 | 测时请求 |
| STM32 → NUC | 4 | 测时响应 |

同一 USB 字节流的顶层分流规则：

- 首字节为 `0xA5`：进入 RMCV 自瞄解析器；
- 首两字节为 ASCII `NV`：进入导航解析器；
- 其他字节：丢弃一个字节后继续寻找帧头；
- 必须先按长度收齐整帧再校验 CRC，不能假设一次 USB 回调就是一帧。

上位机由一个 `sentry_usb_bridge.py` 统一收发，并通过 ROS 话题与导航、自瞄模块交换数据。STM32 端把同一批 USB 字节同时送入两套带 CRC 的流解析器；只有完整且校验正确的对应帧才会生效。两个程序分别打开同一设备会导致其中一个启动失败或运行中抢占断线。

## 6. ROS 2 接口与启动

| 方向 | 话题 | 类型 | 频率/说明 |
|---|---|---|---|
| 自瞄 → 网关 | `/autoaim/target` | `rm_interfaces/msg/AutoAimTarget` | 收到后转成 `0x0503`；网关固定 100 Hz 下发 |
| 网关 → 自瞄 | `/autoaim/pose` | `rm_interfaces/msg/AutoAimPose` | STM32 以 100 Hz 准备 `0x1021`；USB 忙时顺延 |

`AutoAimTarget` 字段与 4.1 的数据区一一对应；`header` 当前仅供自瞄节点自行记录，网关的 100 ms 新鲜度判断使用本机收到话题的单调时间。`AutoAimPose.header.stamp` 是 MCU 采样时间经 `NV` 测时映射后的 ROS 时间，`header.frame_id` 为 `gimbal_imu`。

在 NUC 上重新编译：

```bash
cd ~/cod_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select rm_interfaces cod_nuc_bringup
source install/setup.bash
```

只启用通信和姿态接收，不允许自瞄目标生效：

```bash
ros2 run cod_nuc_bringup sentry_usb_bridge.py --ros-args \
  -p port:=/dev/serial/by-id/你的设备名 \
  -p enable_drive:=false \
  -p enable_autoaim:=false
```

台架确认方向、单位、超时和遥控急停后，才把 `enable_autoaim` 改为 `true`。完整硬件入口也可传：

```bash
ros2 launch cod_nuc_bringup nuc_hardware.launch.py \
  serial_port:=/dev/serial/by-id/你的设备名 \
  enable_autoaim:=true
```

默认 `enable_autoaim=false`。关闭或目标超过 `autoaim_timeout=0.1` 秒时，网关仍以 100 Hz 发送 `target_id=0、shoot=0`；STM32 另有 100 ms 命令超时。固件同时要求机器人处于 READY、遥控在线并进入自瞄状态，才会使用收到的角度目标。

## 7. 最小联调顺序

1. 只接收 `0x1021`，核对 yaw/pitch/roll 零位、正方向、单位和 100 Hz 周期；
2. 回环发送 `target_id=0、shoot=0` 的 `0x0503`，确认计数增长且云台不动作；
3. 先只给小角度 yaw，再给 pitch，检查双 yaw 分配和限位；
4. 断开 USB，确认 100 ms 后自瞄命令失效且绝不发弹；
5. 与导航 `NV` 帧混合发送，验证分包、粘包、错误帧和两类 CRC；
6. 最后才接入 `shoot`，并逐项验证热量、摩擦轮、堵转和人工使能门槛。

所有实车结果必须另行记录；主机协议测试通过不等于云台和发射机构已经可安全运行。

## 8. 已完成的软件验证

- Python 对 CRC8、CRC16、分包、坏帧重同步和双向字段进行单元测试；
- C 与 Python 对 `NV`、`0x0503`、`0x1021` 三类帧逐字节黄金向量一致；
- 固件测试覆盖共享 USB 忙状态、分片接收和 100 ms 命令超时；
- ROS 2 伪终端测试覆盖目标使能、目标超时、无目标帧和分片姿态上报；
- STM32F407 ARM Debug 全量编译、链接通过。

未完成项：烧录、USB 枚举实测、云台方向与零位核对、100 Hz 抖动/丢包测量、遥控失联、发射机构保护和整车比赛环境验证。
