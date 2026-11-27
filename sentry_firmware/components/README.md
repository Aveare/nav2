# components/ 模块登记表

> 目的：把"编进固件但本车未接入"的模块从灰色状态变成**显式登记**，避免新人误以为它们在跑，也避免为"整洁"误删下赛季要用的能力。
> 规则：新增组件必须在本表登记状态；`OPTIONAL` 组件不得被 `apps/infantry/` 的现行调用链引用（编译通过不等于接入）。

## 状态定义

| 状态 | 含义 | 构建 | 调用链 |
|---|---|---|---|
| `ACTIVE` | 本车现行使用 | 编入 | 有 Init/Task 接线 |
| `OPTIONAL` | 预留能力，本车未接线 | 编入（保持可编译） | 禁止被现行 apps 引用 |
| `DORMANT` | 休眠协议/双板预留 | 编入 | 仅条件编译分支 |
| `VENDOR` | 上游算法/传感器驱动 | 编入 | 由 ACTIVE 组件调用 |

## 登记

| 模块 | 状态 | 职责 | 接入条件 / 备注 |
|---|---|---|---|
| `motor/dji/dji_motor` | ACTIVE | DJI 3508/2006/6020 驱动与组帧 | 底盘/云台/发射主链路 |
| `motor/core/motor_task` | ACTIVE | 1kHz 集中电机调度 | 由 `robot_task` 创建 |
| `motor/dm/dm_motor` | OPTIONAL | 达妙 MIT 模式电机 | 换 DM 电机时在 profile 接线；当前无实例 |
| `motor/ht/ht_motor` | OPTIONAL | 海泰 HT04 电机 | 换 HT 电机时接线；任务创建/延时走 kernel |
| `motor/servo/servo_motor` | OPTIONAL | 舵机（弹舱盖等） | `lid_mode` 当前无效；接舵机后启用 |
| `control/controller` | ACTIVE | PID（位置式/改进项） | 电机环、云台、热量守卫 |
| `control/kalman_filter` | VENDOR | 卡尔曼滤波基类 | 仅被 `quaternion_ekf` 内部使用;自瞄 yaw 整形是 `cmd_autoaim` 自带实现,不经本模块(2026-11-23 修正原"ACTIVE/自瞄 yaw 整形"的失实登记) |
| `control/quaternion_ekf` | ACTIVE | 四元数 EKF 姿态 | `ins_task` 调用 |
| `crc/crc8`、`crc/crc16` | ACTIVE | 通用 CRC | 裁判/协议校验 |
| `math/user_lib` | ACTIVE | 矩阵/向量/滤波工具 | 控制与 EKF |
| `imu/bmi088_*` | ACTIVE | BMI088 驱动与中间件 | SPI/片选/加热器经 `ports/stm32f4/imu_port` |
| `imu/ins_task` | ACTIVE | 姿态解算任务 | 1kHz;不直接访问 HAL 句柄 |
| `remote/remote_control` | ACTIVE | DBUS/SBUS 解析与失联 | USART3 |
| `referee/protocol`(`referee.c` 解析/`referee_crc`/`referee_protocol` 表) | ACTIVE | 裁判系统协议/UI/车间通信 | USART6 固定；热量/冷却/弹速/功率/UI；UI 受 `ROBOT_TASK_UI_ENABLE` 控制 |
| `referee/ui`(`referee_task/referee_ui/referee_app_status`) | ACTIVE | UI 绘制与任务调度、应用状态提供者 | 同上;状态经提供者注册,组件不依赖 apps |
| `referee/link`(`referee_custom/referee_vt*`) | ACTIVE | 0x0302 自定义交互与图传链路发送 | 多机交互按 2027 规则受限于终端 |
| `ax650/ax650_pose`、`ax650_uart1` | ACTIVE | 自瞄姿态上报与 0x0503 下行 | USART1 宏二选一；受 `AX650_UART1_ENABLE` 控制，调试时与 `CHASSIS_VOFA_UART1_ENABLE` 互斥 |
| `calib` | ACTIVE | 零位 Flash 标定记录 | 与 `gimbal_recenter` 配合 |
| `daemon` | ACTIVE | 模块在线检测 | 心跳监督 |
| `navigation/nav_protocol` | ACTIVE(哨兵) | NV v1 USB 导航帧编解码、CRC16/MODBUS 与滑动重同步 | 哨兵 `autonomy` 调用；`SENTRY_AUTONOMY_ENABLE 0` 时只通信不驱动底盘 |
| `alarm/buzzer` | ACTIVE | 蜂鸣器/开机播报 | PWM |
| `can_comm` | DORMANT | 双板可靠帧 | 仅双板；当前 `ONE_BOARD`，profile 守卫 `#error` |
| `super_cap` | OPTIONAL | 超级电容 | 硬件未装；接入时在 profile 注册并加功率策略 |

## 功能缺口（预留，不在本轮实现）

| 缺口 | 现状 | 架构落点 | 完成判据 |
|---|---|---|---|
| 功率控制 | `chassis_estimate_speed()` 仅声明/空实现 | `apps/infantry/chassis/` + `config_protect.h` | 逆解算 + 功率闭环 + 台架标定 |
| 超电 | 组件在、未注册 | `components/super_cap/` | 硬件到位 + CAN 0x301/0x302 联调 |
| IST8310 磁力计 | 未接，yaw 无绝对参考会慢漂 | `components/imu/` | 接入后 EKF 观测更新 + 实车漂移对比 |
| 发射机构 | 硬件问题挂起（RM-002） | `apps/infantry/shoot/` | 单发/三发/射速台架验证 |
| 视觉自瞄 | AX650 链路已通，待实车 | `components/ax650/` | 4 条判据见 DEV-20261122-19 |

## 新增组件的最小动作

1. 在 `components/<name>/` 建 `<name>.c/.h`，文件头按《注释规范》。
2. 在本表登记状态；`OPTIONAL` 必须写"接入条件"。
3. 只 include `config.h` 取参数，不 include `apps/` 头。
4. 若新增 `.c`：同步 `Makefile`（CMake 是 GLOB 自动收集）。
5. 加主机测试或说明"为何不可主机测试"；跑 `check_naming.py` / `check_source_lists.py`。
