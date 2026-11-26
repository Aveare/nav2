# Sentry ROS 2 与 STM32 USB 导航通信

本文说明 NUC 上的 ROS 2 节点如何通过 USB CDC 虚拟串口与 STM32F407 哨兵电控交换导航速度和大 yaw 关节角。协议版本为 `NV v1`。

## 当前安全状态

- STM32 默认 `SENTRY_AUTONOMY_ENABLE=0`，会解析命令、回复测时并回传大 yaw，但不会把 USB 速度送到底盘。
- NUC 的 `sentry_usb_bridge` 默认 `enable_drive=false`，只发送无效零速帧。
- 两端开关必须同时启用，且遥控器左右拨杆均在中位、右摇杆回中，导航才可能接管底盘。
- 本次只完成代码、主机测试和 ARM 编译，未烧录、未做实体串口闭环或底盘运动验收。

## ROS 2 接口

NUC 端唯一串口所有者是 `cod_nuc_bringup/sentry_usb_bridge.py`。不要同时运行旧串口节点或第二个进程打开相同设备。

| 方向 | 话题 | 消息类型 | 约束 |
| --- | --- | --- | --- |
| ROS → bridge | `/cmd_vel` | `geometry_msgs/msg/Twist` | 使用 `base_link` 语义；`linear.x` 前进、`linear.y` 左移、`angular.z` 逆时针 |
| ROS → bridge | `/Odometry` | `nav_msgs/msg/Odometry` | `header.frame_id=odom`、`child_frame_id=base_link`；只用于新鲜度门控 |
| bridge → ROS | `/big_yaw/joint_states` | `sensor_msgs/msg/JointState` | `name=["big_yaw_joint"]`，`position[0]` 为相对底盘角，单位 rad |

桥接节点以 200 Hz 轮询串口、每 20 ms 发送一次速度帧、每 500 ms 发一次测时请求。只有以下条件全部满足才设置速度帧 `VALID`：

1. `enable_drive=true`。
2. `/cmd_vel`、`/Odometry`、大 yaw 反馈的年龄均不超过 0.2 s。
3. Odometry 消息时间戳年龄位于 -0.02~0.2 s。
4. 最近 2 s 内获得有效 MCU 时钟同步。
5. 每轴平移速度不超过 0.35 m/s，角速度不超过 0.5 rad/s，且全部为有限数。

任一条件失效时，NUC 连续发送无效零速帧；STM32 另有 300 ms 接收超时兜底。

## NV v1 帧

所有多字节字段均为小端。帧固定 32 字节，不直接传输 C 结构体。

| 偏移 | 长度 | 字段 |
| ---: | ---: | --- |
| 0 | 2 | ASCII `NV` |
| 2 | 1 | 版本 `1` |
| 3 | 1 | 消息类型 |
| 4 | 4 | `uint32` 序号 |
| 8 | 8 | `uint64 sample_us` |
| 16 | 12 | 3 个 IEEE754 binary32 |
| 28 | 2 | `uint16 flags`，bit0=`VALID`，其余必须为 0 |
| 30 | 2 | CRC16/MODBUS，覆盖字节 0~29 |

CRC 初值为 `0xFFFF`，多项式为 `0xA001`，低字节先传；标准向量 `123456789` 的结果为 `0x4B37`。

| 类型 | 值 | `sample_us` | `float[0..2]` |
| --- | ---: | --- | --- |
| COMMAND | 1 | NUC ROS 发送时间 | `vx` m/s、`vy` m/s、`wz` rad/s |
| FEEDBACK | 2 | 大 yaw CAN 帧进入 MCU 的 tick 时间 | 大 yaw 相对角 rad、0、0 |
| SYNC_REQUEST | 3 | NUC ROS 发送时间 | 0、0、0 |
| SYNC_RESPONSE | 4 | MCU 处理时的 tick 时间 | 0、0、0 |

STM32 流式解析 USB 分包和粘包；CRC 或帧头错误时逐字节滑动重同步。有效会话只接受前进序号，重复或倒退序号不刷新超时。

## 坐标与单位转换

ROS 使用 REP-103 的 `base_link`：

- `+x` 向前。
- `+y` 向左。
- `+z` 逆时针。

电控底盘内部使用 `x` 前、`y` 右、`wz` 逆时针，因此接收命令时执行：

```text
vx_mm_s  =  vx_m_s × 1000
vy_mm_s  = -vy_m_s × 1000
wz_deg_s =  wz_rad_s × 180 / π
```

`CHASSIS_NAVIGATION` 直接表示底盘物理坐标系，不再按大 yaw 偏角旋转速度。导航模式使用独立的 350 mm/s² 平移斜坡。

## 启动与检查

NUC 当前服务名为 `cod-sentry-usb-bridge.service`，串口应使用稳定的 `/dev/serial/by-id/...` 路径。

```bash
systemctl --user status cod-sentry-usb-bridge.service
ros2 node info /sentry_usb_bridge
ros2 topic hz /big_yaw/joint_states
ros2 topic echo /big_yaw/joint_states --once
```

如果烧录新固件后仍没有 `/big_yaw/joint_states`：

1. 确认 `/dev/serial/by-id/...` 存在且服务用户可访问。
2. 确认没有其他进程占用同一 VCP。
3. 检查 STM32 是否收到大 yaw DM CAN 反馈；没有新 CAN 时间戳时固件不会伪造新角度。
4. 查看服务日志中的 USB 重连、CRC、测时或串口写阻塞信息。

## 实车放行顺序

1. 保持两端驱动开关关闭，先确认测时响应和 `/big_yaw/joint_states` 约 50 Hz。
2. 架空底盘，分别核对正前、左移、逆时针三个符号。
3. 断开 `/cmd_vel`、`/Odometry`、大 yaw CAN 和 USB，逐项确认零速/零力降级。
4. 只有完成上述验收后，才将 STM32 `SENTRY_AUTONOMY_ENABLE` 改为 1，并以 `enable_drive=true` 启动桥接。
5. 首次落地限低速、留急停人员，不同时开展导航参数整定和云台/发射调试。

ROS 端实现位于 Aveare/nav2 的 `src/cod_nuc_bringup/scripts/sentry_usb_bridge.py`；STM32 实现位于本仓库 `components/navigation/` 与 `apps/sentry/autonomy/`。
