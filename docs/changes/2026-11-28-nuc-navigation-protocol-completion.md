# 完善 NUC 端上位机导航与自瞄共口通信协议

日期：2026-11-28

## 背景与依据

对照电控规范 `rm-jit-sentry/.Doc/robots/sentry/ops/导航协议与上位机.md`，补齐 NUC 端上位机在物理链路稳定性、运行期帧率/拥塞/重连统计、双协议重启一致性、安全门控、一键体检、分步联调与 `cod-sentry-usb-bridge.service` 服务模板上的缺口。

## 变更内容

- `src/cod_nuc_bringup/scripts/nav_wire.py`：
  - 为 `Parser` 增加 `frames_ok`、`crc_errors`、`resync_bytes` 统计计数；为 `ClockSync` 记录最近一次测时 `last_rtt_s`。
- `src/cod_nuc_bringup/scripts/rmcv_wire.py`：
  - 为 `Parser` 增加 `frames_ok`、`crc8_errors`、`crc16_errors`、`length_errors`、`validation_errors`、`unknown_commands`、`resync_bytes` 统计计数。
- `src/cod_nuc_bringup/scripts/sentry_usb_bridge.py`：
  - 新增 `resolve_serial_port()` 与默认 `port='auto'`：优先解析 `/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_*` 稳定设备路径，并在重连时重新解析，找不到时回退 `/dev/ttyACM0`，同时保留显式路径覆盖。
  - 统一 MCU 重启/tick 回绕处理 `_reset_mcu_session()`，在 NV `FEEDBACK` 与 RMCV `0x1022/0x1021` 之间同步清空时钟同步与双链路历史时间戳。
  - 补全 `0x0503` 与 `COMMAND` 安全门控：`target_id==0` 时强制 `shoot=0`，无效输入同步清零缓存；使用无漂移周期推进 `advance_due()` 维持 50 Hz `COMMAND` / 100 Hz `0x0503` 节奏。
  - 新增 `/sentry/usb_bridge_status`（`std_msgs/msg/String` JSON，2 Hz），实时统计收发字节与帧率、TX busy/阻塞计数、重连次数、时钟同步状态与 RTT、驱动门控拒绝原因、`0x1022` `valid_flags`/`mode`/`robot_id`/弹速、以及 NV/RMCV 解析错误与重同步字节数。
- `src/cod_nuc_bringup/launch/nuc_hardware.launch.py`：
  - 将 `serial_port` 默认参数由 `/dev/ttyACM0` 改为 `auto`，自动优先使用 `/dev/serial/by-id/...`。
- `src/cod_nuc_bringup/scripts/sentry_async_workflow.py` 与 `sentry_health_check.py`：
  - 在异步管家状态中同步输出 `0x1022` `full_pose`/`valid_flags`/`mode`/`robot_id`/弹速与串口桥状态；在一键体检工具中识别 `/dev/serial/by-id/usb-YueLuEmbedded*` 并展示时钟同步、拥塞与重连计数。
- `src/cod_nuc_bringup/tools/usb_link_steps/`：
  - 新增用户级 systemd 服务模板 `cod-sentry-usb-bridge.service` 与安装脚本 `install_usb_bridge_service.sh`（默认禁用，避免与 `nuc_hardware.launch.py` 争抢串口）。
  - 更新 `00_按顺序执行.md`、`01_检查设备与环境.sh`、`06_检查自瞄RMCV_可选.sh` 为 `NV v1 + RMCV 0x1022/0x0503` 基线并检查 `/sentry/usb_bridge_status`。
- 更新单元测试：`test_nav_wire.py`、`test_rmcv_wire.py`、`test_usb_bridge.py`、`test_hardware_launch.py`。

## 安全边界

- `sentry_usb_bridge.py` 与 `cod-sentry-usb-bridge.service` 默认保持 `enable_drive=false`、`enable_autoaim=false`。
- 未自动启用或后台拉起常驻串口桥服务，避免与 `nuc_hardware.launch.py`（默认 `start_serial:=true`）产生双进程争占串口。

## 验证记录

- 本地 Python 语法检查、`test_nav_wire.py`、`test_rmcv_wire.py`（6/6 通过）及 `sentry_health_check.py --self-test`（9/9 项通过）。
- NUC `/tmp/cod_nuc_proto_verify` 隔离编译与测试：`colcon build --packages-select cod_nuc_bringup` 与 `colcon test --packages-select cod_nuc_bringup` 8/8 测试集（38 个用例）全部通过。
- NUC 部署前已备份旧文件至 `/home/rm/cod_deploy/backups/nuc_nav_proto_20261128_053440`，部署后 15 个文件 SHA-256 逐一核对一致；已安装 `~/.config/systemd/user/cod-sentry-usb-bridge.service`（默认 `disabled`/`inactive`）。
- NUC 实机只读串口联调（`01`～`07`，`enable_drive=false`、`enable_autoaim=false`）通过：`/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_305E356D3034-if00` 正常连通，`/big_yaw/joint_states` 实测 `49.962 Hz`，`/autoaim/pose` 收到 `0x1022`（`full_pose: true`、`robot_id: 107`、`valid_flags: 671`），`/sentry/usb_bridge_status` 正常输出，步骤 `07` 已停止串口桥并释放设备。
