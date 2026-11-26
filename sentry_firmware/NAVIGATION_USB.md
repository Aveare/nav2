# 本地哨兵 USB 导航协议 NV v1（2026-11-25）

这是为本次本地接入新增、同时实现于两端的协议，不是声称 GitHub 原电控已经存在的协议。
固件接收端在 `components/navigation` 与 `apps/sentry/autonomy`；ROS 发送端在
`../src/cod_nuc_bringup/scripts/sentry_usb_bridge.py`。该进程同时承载自瞄 RMCV 帧，
是 USB CDC 的唯一上位机所有者。旧 `cod_serial_ul26` 的 15 字节包不能混用，
不要让两个进程打开同一个 USB 口。

## 帧布局

固定 32 字节，小端，无 C 结构体直接透传：

| 偏移 | 字节数 | 内容 |
|---|---:|---|
| 0 | 2 | ASCII `NV`，十六进制 4E 56 |
| 2 | 1 | 版本 1 |
| 3 | 1 | 类型：1 速度、2 角度反馈、3 测时请求、4 测时响应 |
| 4 | 4 | uint32 序号；速度、测时、反馈分别计数 |
| 8 | 8 | uint64 sample_us，语义见下表 |
| 16 | 12 | 三个 IEEE754 binary32 float |
| 28 | 2 | uint16 flags：bit0 有效，其余保留为 0 |
| 30 | 2 | CRC16/MODBUS，覆盖 0..29，初值 FFFF、多项式 A001，低字节先传 |

| 类型 | sample_us | float[0..2] |
|---|---|---|
| 1 速度 | 主机 ROS 发送时间 us，仅追踪，不当成 MCU 时钟 | base_link vx m/s、vy m/s（左正）、wz rad/s（逆时针正） |
| 2 反馈 | 实际 CAN 反馈接收的 MCU tick×1000 | 大 yaw 相对底盘角 rad、0、0 |
| 3 请求 | 主机发送 ROS 时间 us | 0、0、0 |
| 4 响应 | MCU 处理请求时的 tick×1000，序号回显 | 0、0、0 |

CRC16 标准检查向量：ASCII `123456789` -> 4B37。
流式解析允许 USB 分包/粘包；错误帧逐字节滑动重同步。

## 时间与失效

MCU 现有 `kernel_time_us()` 来自未展开的 DWT CYCCNT，168 MHz 下约 25.6 秒回绕。
本协议不用这个时钟，使用 32 位 kernel tick 的毫秒时间，约 49.7 天回绕。
CAN/USB ISR 采样使用 FromISR tick API；大 yaw 角度与其采样时间在同一临界区快照。

主机每 0.5 s 发测时请求，以主机发送/接收 ROS 时间中点估计 MCU 偏移，
取最近 10 秒、最多 20 个样本中的最小 RTT。仅接受 RTT≤20 ms、ROS 与单调时间
差异≤5 ms 的样本；超过 2 秒无有效测时即不再发有效速度，也不发布时间未知的角度。
这属于软件测时，不是硬件同步：链路不对称仍有误差，RTT 20 ms 的样本中点误差可达约
10 ms，另有 1 ms tick 量化与 CAN 测量到接收的延迟，需要实测记录。

主机不使用接收时刻替代 CAN 采样时刻。MCU 时间倒退/重启时清除旧偏移，重新测时；
大 yaw 原始采样时间重复时不重发角度。平台零位、传动比及正方向继承源电控配置，
本次没有重新标定；机械拆装或 MCU 重启后须核对原工程多圈/零位语义。

STM32 在 `SENTRY_AUTONOMY_ENABLE=0` 时仍以 20 ms 周期发送新鲜的大 yaw 角度，
但读取导航命令的出口会把整帧速度清零并返回无效，因此不会把 USB 速度交给底盘 PID。
角度 CAN 采样时间不更新时停止回传，禁止把旧角度包装成新数据。
当前固件设置 `SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE=0`，恢复原大 yaw 角度环、
速度环和 MIT 力矩输出；`SENTRY_AUTONOMY_ENABLE=0` 仍独立禁止 USB 导航速度接管底盘。

ROS 桥接每 20 ms 发送速度，仅当 cmd_vel、底盘 Odometry、角度反馈在 0.2 s 内更新，
Odometry 采样时间也新鲜、测时有效且 enable_drive=true 时标有效。失效则发零速度无效帧。
写阻塞超过 0.1 s 会断开重连并撤销缓存命令；MCU 另以 300 ms 接收超时兜底。
CRC/序号用于错误与重复检测，不是认证或任意网络传输延迟的保证。

## ROS 端操作（NUC 恢复后）

本次仅在本机 WSL/ROS 2 Humble 测试，没有访问 NUC。
在 ROS 工作区安装依赖 `python3-serial` 并重新构建：

```bash
source /opt/ros/humble/setup.bash
colcon build --packages-select rm_interfaces cod_nuc_bringup
source install/setup.bash
ros2 run cod_nuc_bringup sentry_usb_bridge.py --ros-args -p port:=/dev/ttyACM0
```

此命令默认只联调通信：`enable_drive=false`、`enable_autoaim=false`。若设备名不固定，可把 port 换为
实际 `/dev/serial/by-id/...`。当前用户必须已有串口设备访问权限。

物理入口 `nuc_hardware.launch.py` 的 start_serial 现在默认 true，使用此新桥接，
不会启动旧 cod_serial。桥接发布 `/big_yaw/joint_states`，原 `big_yaw_tf.py`
负责动态 TF；完整入口的 MID-360S 内部外参检查依然有效。

```bash
ros2 launch cod_nuc_bringup nuc_hardware.launch.py \
  imu_extrinsics_confirmed:=true serial_port:=/dev/ttyACM0
```

只有实际完成内部外参核对后才设置上面的 confirmed。默认不启动导航动作。
正式低速测试还需实测 footprint、start_navigation:=true、enable_drive:=true，
并按 README 使能电控导航。不要在通信尚未验收时开启这两个驱动开关。

自瞄 `0x1021/0x0503` 字段、ROS 话题和 `enable_autoaim` 开关见
`AUTOAIM_USB_PROTOCOL.md`。导航与自瞄必须共用此网关，不得另起自瞄串口进程。

检查正前速度、左移速度、逆时针旋转，以及平台零位/正转符号；随后分别断开
cmd_vel、Odometry、角度反馈、USB，确认进入原零力安全输出。每项实机结果由用户记录。
