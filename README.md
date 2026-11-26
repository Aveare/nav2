# 哨兵导航与电控

本仓库保存 ROS 2 Humble 导航、行为决策、上位机 USB 通信及配套 STM32 哨兵电控源码。

## 目录

- `src/cod_navigation`：COD 导航、Small Point-LIO、控制器和工具包。
- `src/cod_behavior_stack`：行为树、裁判系统消息和决策通信。
- `src/cod_nuc_bringup`：仿真与实车启动入口、坐标补偿、USB 桥及测试。
- `sentry_firmware`：从 RM-JIT-CR 整理的哨兵电控与公共依赖，保留来源清单和本地变更记录。

上游导航来源为 COD 的 Gitee 仓库；电控来源为 `youyucat/RM-JIT-CR`。本仓库为用户自己的同步副本，不向这些上游仓库推送。原有许可证及来源记录保留在对应目录，未给整个项目另行指定许可证。

## 当前验证状态（2026-11-26）

- 小电脑上 `cod_bringup`、`small_point_lio`、`cod_nuc_bringup` 编译成功。
- 坐标、时间戳和 USB 伪终端测试通过：15 个 pytest 用例；colcon 含汇总项共 20 项，0 失败。
- 电控通信主机测试在 GCC `-O0` 和 `-Ofast -flto` 下通过，C/Python 协议字节一致。
- 独立域 43 的三点 loopback 导航通过，终点误差约 0.147 m，速度输出未超过首测限值。
- 实车参数已同步并安装到小电脑；这不表示已经完成实车运行验证。

## 实车入口与参数

`cod_nuc_bringup/launch/nuc_hardware.launch.py` 使用：

```text
map → odom → base_link → big_yaw_link → livox_frame
```

倒装雷达安装为 XYZ=(-0.053, 0, 0.474) m、roll=π。导航每轴限速 0.35 m/s、角速度上限 0.5 rad/s；斜向合速度可达约 0.50 m/s。USB 协议、采样时间和失效处理见 `sentry_firmware/NAVIGATION_USB.md`、`src/cod_nuc_bringup/HARDWARE_FRAMES.md`。

默认 `start_navigation=false`、`enable_drive=false`，电控 `SENTRY_AUTONOMY_ENABLE=0`。真实 footprint 与 MID-360S 内部外参尚未核实，入口保留校验门槛。尚未完成 ARM 编译、烧录、实车延迟/滑移/制动与平台动态补偿验证。

## 同步规则

本仓库是已整理的源码副本。开发源工作区与原上游仓库分开保存；每轮修改验证后，更新本副本并提交推送到 `Aveare/nav2`，通过远端提交哈希核对。不会后台逐次按键自动提交，也不会自动部署或烧录。

不上传 SSH 密码、访问令牌、运行日志、构建产物和本地备份。后续从 GitHub 拉取的源码可按正常 Git 流程修改与提交。
