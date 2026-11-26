# 哨兵导航与电控

本仓库保存 ROS 2 Humble 导航、行为决策、上位机 USB 通信及配套 STM32 哨兵电控源码。

## 目录

- `src/cod_navigation`：COD 导航、Small Point-LIO、控制器和工具包。
- `src/cod_behavior_stack`：行为树、裁判系统消息和决策通信。
- `src/cod_nuc_bringup`：仿真与实车启动入口、坐标补偿、USB 桥及测试。
- `sentry_firmware`：以 `hibhvkgvck/shaobin@593d6af` 为电控底座，移植本地 `NV v1` USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`，保留原工程记录与本地变更日志。

上游导航来源为 COD 的 Gitee 仓库；电控来源为 `hibhvkgvck/shaobin`（原基于 `youyucat/RM-JIT-CR` / 跃鹿 `basic_framework`）。本仓库为用户自己的公开同步副本，不向这些上游仓库推送。原有许可证及来源记录保留在对应目录，未给整个项目另行指定许可证。

## 当前验证状态（2026-11-26）

- 小电脑上 `rm_interfaces`、`cod_bringup`、`small_point_lio`、`cod_nuc_bringup` 编译成功。
- `cod_nuc_bringup` 的 24 项 ROS 2 测试通过；包含坐标、硬件 launch、时间戳、RMCV 和 USB 伪终端收发。
- 小电脑实机雷达静态大 yaw 零位建图（`room_scan_20261126_012426`，191×166，`0.05 m/px`，10 Hz 雷达/里程计/scan，200 Hz IMU，0 丢包）与 AMCL（`OmniMotionModel`）实机重定位收敛验证通过（scan-to-map 中位墙距误差 `13.2 cm → 3.7 cm`，`≤10 cm` 命中率 `42.4% → 76.7%`）；期间 `start_serial=false`、`enable_drive=false`。
- 电控通信主机测试在本地 WSL GCC `-O0` 与 `-Ofast -flto`（`feedback-only` 与 `drive-enabled` 共 4 组）全部通过，`NV`、`0x0503`、`0x1021` 的 C/Python 协议字节一致；13 个改动/新增 C 文件语法检查通过；原 `hibhvkgvck/shaobin` 4 套主机回归（底盘仲裁、命令看门狗、哨兵串级控制、大 yaw 纯速度小陀螺）全部通过。
- STM32F407 本地 Windows ARM GCC 14.2.1 清洁全量编译与链接通过（`hibhvkgvck/shaobin@593d6af` 底座 + `NV v1` 导航 + 共享 CDC 自瞄，保留 600×560 mm 底盘、`-1.52914 rad` 大 yaw 机械零位、小陀螺纯速度 PI `Kp=5.2/Ki=1.2`、小 yaw `±90°` 限位及 `SENTRY_AUTONOMY_ENABLE=0` 默认硬清零）：FLASH 11.09%（116,324 B）、RAM 74.27%（97,352 B）、CCMRAM 0.00%，第一方 0 error / 0 warning，ELF SHA256=`A2B36454BC67EF9E59262B8C8877C09233BF08D774F268AE4E5FC7B635B17C8F`（严禁小电脑编译，烧录在本地完成）。
- 独立域 43 的三点 loopback 导航通过，终点误差约 0.147 m，速度输出未超过首测限值。
- 实车 ROS 参数已同步并安装到小电脑；新底座固件已本地编译但尚未烧录，尚未完成整车 USB 联调与闭环行车验证。

## 实车入口与参数

`cod_nuc_bringup/launch/nuc_hardware.launch.py` 使用：

```text
map → odom → base_link → big_yaw_link → livox_frame
```

倒装雷达安装为 XYZ=(-0.053, 0, 0.474) m、roll=π。导航每轴限速 0.35 m/s、角速度上限 0.5 rad/s；斜向合速度可达约 0.50 m/s。导航 USB 协议、采样时间和失效处理见 `sentry_firmware/NAVIGATION_USB.md`、`src/cod_nuc_bringup/HARDWARE_FRAMES.md`；自瞄 USB CDC 的收发字段、ROS 话题和单串口网关约束见 `sentry_firmware/AUTOAIM_USB_PROTOCOL.md`。

默认 `start_navigation=false`、`enable_drive=false`、`enable_autoaim=false`，电控 `SENTRY_AUTONOMY_ENABLE=0`。真实 footprint 与 MID-360S 内部外参尚未核实，入口保留校验门槛。ARM 编译已完成，但尚未烧录，也未完成实车延迟/滑移/制动、平台动态补偿和自瞄云台方向验证。

## 同步规则

本仓库是已整理的源码副本。开发源工作区与原上游仓库分开保存；每轮修改验证后，更新本副本并提交推送到 `Aveare/nav2`，通过远端提交哈希核对。不会后台逐次按键自动提交，也不会自动部署或烧录。

不上传 SSH 密码、访问令牌、运行日志、构建产物和本地备份。后续从 GitHub 拉取的源码可按正常 Git 流程修改与提交。
