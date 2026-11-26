# 哨兵导航与电控

本仓库保存 ROS 2 Humble 导航、行为决策、上位机 USB 通信及配套 STM32 哨兵电控源码。

## 目录

- `src/cod_navigation`：COD 导航、Small Point-LIO、控制器和工具包。
- `src/cod_behavior_stack`：行为树、裁判系统消息和决策通信。
- `src/cod_nuc_bringup`：仿真与实车启动入口、坐标补偿、USB 桥及测试。
- `sentry_firmware`：同步自 `youyucat/RM-JIT-CR@sentry`（`a24eeb6`，已完整移植 `RM_shaobin2@593d6af` 实车参数与控制），叠加本地 `NV v1` USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`，保留原工程记录与本地变更日志。

上游导航来源为 COD 的 Gitee 仓库；电控来源为 `youyucat/RM-JIT-CR`（`sentry` 分支，已合入 `hibhvkgvck/shaobin` 实车迁移）。本仓库为用户自己的公开同步副本，不向这些上游仓库推送。原有许可证及来源记录保留在对应目录，未给整个项目另行指定许可证。

## 当前验证状态（2026-11-26）

- 小电脑上 `rm_interfaces`、`cod_bringup`、`small_point_lio`、`cod_nuc_bringup` 编译成功。
- `cod_nuc_bringup` 的 24 项 ROS 2 测试通过；包含坐标、硬件 launch、时间戳、RMCV 和 USB 伪终端收发。
- 小电脑实机雷达静态大 yaw 零位建图（`room_scan_20261126_012426`，191×166，`0.05 m/px`，10 Hz 雷达/里程计/scan，200 Hz IMU，0 丢包；另已保存手持图 `handheld_map_20261126`，201×193）与 AMCL（`OmniMotionModel`）实机重定位收敛验证通过（scan-to-map 中位墙距误差 `13.2 cm → 3.7 cm`，`≤10 cm` 命中率 `42.4% → 76.7%`）；期间 `start_serial=false`、`enable_drive=false`。
- 小电脑实机雷达障碍物检测、Nav2 避障重规划与自瞄+导航异步工作流验证通过（`enable_drive=false`、`enable_autoaim=false` 不驱动底盘，600×600 mm 底盘 `footprint` 与 `local/global_costmap`、`ObstaclesCritic` 一致）：实测 `/livox/lidar`→`/scan`（680/720 有效射线，最近障碍 `0.787 m @ -159.0°`）→`/local_costmap/costmap`（20,053 致命栅格、3,639 膨胀栅格）实时标障；`SmacPlanner2D` 输出 33 点避障 `/plan`，`MPPIController`+`velocity_smoother` 输出 45/47 帧非零避障 `/cmd_vel`（单轴最大 `0.165 m/s ≤ 0.35 m/s`，最大 `0.500 rad/s ≤ 0.5 rad/s`）；`sentry_async_workflow.py` 多线程独立回调组并发运行 Nav2 目标导航与 `/autoaim/target` 自瞄状态流无阻塞。
- 电控通信主机测试在本地 WSL GCC `-O0` 与 `-Ofast -flto`（`feedback-only` 与 `drive-enabled` 共 4 组）全部通过，`NV`、`0x0503`、`0x1021` 的 C/Python 协议字节一致；`youyucat/RM-JIT-CR@sentry` 5 套主机回归（`profile_compile_{heavy,sentry}`、`sentry_gimbal_alloc_test`、`sentry_gimbal_control_test`、`sentry_remote_mapping_test`）全部通过。
- STM32F407 本地 Windows ARM GCC 14.2.1 清洁全量编译与链接通过（`youyucat/RM-JIT-CR@sentry` `a24eeb6` 底座 + `NV v1` 导航 + 共享 CDC 自瞄，保留 `RM_shaobin2` 600×560 mm 底盘、150°/s 小陀螺、`-1.52914 rad` 大 yaw 机械零位与 `Kp=5.2/Ki=1.2/3 N·m` 积分、小 yaw `27/±90°`、USART1 三轴 VOFA 及 `SENTRY_AUTONOMY_ENABLE=0` 默认硬清零）：FLASH 11.37%（119,184 B）、RAM 56.55%（74,120 B）、CCMRAM 83.84%（54,944 B），第一方 0 error / 0 warning，ELF SHA256=`152DEC03A4308FF92533CCC1612C69F1B0FE9A0D65874DF8EBE3259DD4138B1D`（严禁小电脑编译，烧录在本地完成）。
- 独立域 43 的三点 loopback 导航通过，终点误差约 0.147 m，速度输出未超过首测限值。
- 本地与小电脑源码已双端同步（小电脑未编译）；新底座固件已本地编译但尚未烧录，尚未完成整车 USB 联调与闭环行车验证。

## 实车入口与参数

`cod_nuc_bringup/launch/nuc_hardware.launch.py` 使用：

```text
map → odom → base_link → big_yaw_link → livox_frame
```

倒装雷达安装为 XYZ=(-0.053, 0, 0.474) m、roll=π。导航每轴限速 0.35 m/s、角速度上限 0.5 rad/s；斜向合速度可达约 0.50 m/s。导航 USB 协议、采样时间和失效处理见 `sentry_firmware/NAVIGATION_USB.md`、`src/cod_nuc_bringup/HARDWARE_FRAMES.md`；自瞄 USB CDC 的收发字段、ROS 话题和单串口网关约束见 `sentry_firmware/AUTOAIM_USB_PROTOCOL.md`。

默认 `start_navigation=false`、`enable_drive=false`、`enable_autoaim=false`，电控 `SENTRY_AUTONOMY_ENABLE=0`。真实 footprint 与 MID-360S 内部外参尚未核实，入口保留校验门槛。ARM 编译已完成，但尚未烧录，也未完成实车延迟/滑移/制动、平台动态补偿和自瞄云台方向验证。

## 同步规则

本仓库是已整理的源码副本。开发源工作区与原上游仓库分开保存。每轮导航修改前先获取 `origin/main`、检查新增提交与差异，再以仅快进方式拉取最新代码并阅读相关变更；修改时同步新增或更新 Markdown 说明，验证后提交推送到 `Aveare/nav2`，通过远端提交哈希核对。

NUC 不再直接连接 GitHub。NUC 运行工作区中的现场改动通过 SSH 下载到本地仓库，本地先检查并拉取云端最新提交，再审查、合并、补充 Markdown 说明并推送到 `Aveare/nav2`。需要反向部署到 NUC 时同样只通过 SSH 逐项传输，写入前必须备份并核对差异；源码同步不会自动编译、重启建图、启动底盘、烧录固件或覆盖现场改动。

本轮双端同步的目录关系、差异处理和后续固定流程见 [NUC导航代码双端同步说明.md](NUC导航代码双端同步说明.md)。

不上传 SSH 密码、访问令牌、运行日志、构建产物和本地备份。后续从 GitHub 拉取的源码可按正常 Git 流程修改与提交。
