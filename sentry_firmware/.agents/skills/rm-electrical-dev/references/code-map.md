# 工程代码地图 v3（当前树符号级定位索引）

## 0. 版本与用途

- **v3.1（2026-11-26，Sentry RM_shaobin2 移植）**：补齐 `apps/sentry/` 的遥控、三轴串级、
  发射和 USART1/VCP 定位；当前交付兵种为 Sentry，infantry 表只作共享框架与历史回归参考。
- **v3（2026-11-23，工程统一化收官，DEV-20261123-01）**：路径与符号全部对齐当前树 `apps → components → framework → ports → HAL`；函数/类型/宏/变量已统一为 `snake_case + 模块前缀`（规范见 `.Doc/spec/命名规范.md`）。
- v1 的"上游快照定位表"仍可查 `.agents/skills/rm-electrical-dev/assets/yuelu-basic-framework/`（@1a136eb1），用于比对派生差异；本文件与它不重复维护同一事实。
- 行号不写（会漂移），一律用路径 + 函数/类型/宏名定位。发现地图与实现不符时，以核验过的实现为准并更新本图。
- 历史 DEV 条目里的 `application/`、`modules/`、`bsp/` 路径是当时语义；现已分别迁到 `apps/infantry/`、`components/`、`ports/stm32f4/`。

## 1. 症状/功能 → 当前代码定位

| 症状或目标 | 首查位置 | 关键符号 |
| --- | --- | --- |
| 某电机不转 / 报文错乱 | `components/motor/dji/dji_motor.c` | `dji_motor_control()`、分组表、`can_port_bus_of()` |
| 电机反馈异常 / 多圈角跳变 | `components/motor/dji/dji_motor.c` | `dji_motor_decode()`、`total_round` 跨 4096 判断 |
| 电机停不下来 / 停止后仍发热 | `dji_motor.c` | `stop_flag`、`dji_motor_stop()` |
| 反馈源要换 IMU/外部 | `dji_motor.c/.h` | `dji_motor_change_feed()`、`other_angle_feedback_ptr` |
| 底盘不动 / 横移异常 / 旋转异常 | `apps/infantry/chassis/chassis.c` | `chassis_mecanum_calculate()`、轮号映射、`chassis_kinematics.c` |
| 底盘不跟随云台 / 自瞄解耦 | `chassis.c` + `apps/infantry/cmd/robot_cmd.c` | `CHASSIS_FOLLOW_GIMBAL_YAW` / `CHASSIS_NO_FOLLOW`、`cmd_calc_offset_angle()` |
| 云台方向/零位不对 | `apps/infantry/gimbal/gimbal.c` + `robot_def.h` | `GIMBAL_GYRO_DIR_*`、`PITCH_HORIZON_ECD`、`gimbal_imu_data` |
| 云台模式切换行为 | `gimbal.c` | `GIMBAL_GYRO_MODE / FREE / ZERO_FORCE` 分支 |
| 发射机构逻辑 / 热量降级 | `apps/infantry/shoot/shoot.c` + `heat_guard.c` | `heat_guard_update/allow_burst/allow_single/commit_shot` |
| 姿态角异常 / yaw 慢漂 | `components/imu/ins_task.c` | `ins_task()`、`ekf_imu_quaternion_ekf_update()`；**无磁力计 → yaw 无绝对参考** |
| IMU 温控 | `ins_task.c` | `ins_imu_temperature_ctrl()` |
| 遥控器失联 / 通道错乱 | `components/remote/remote_control.c` | `remote_sbus_to_rc()`、`remote_rectify_joystick()`、失联回调 |
| 模块离线误报/漏报 | `components/daemon/daemon.c` | `daemon_reload()`、`reload_count`（单位=daemon_task 拍数） |
| 心跳/看门狗/复位 | `framework/diag/diag.c` + `apps/infantry/app/robot.c` + `robot_task.c` | `diag_watch_register/kick/tick`、`iwdg_feed` 门控、`RCC->CSR` 复位原因 |
| 板间通信不通 / 数据错位 | `components/can_comm/can_comm.c/.h` | 帧格式 `'s'+len+data+crc8+'e'`、两侧 `sizeof` 一致、`pack(1)`(休眠双板支) |
| 视觉数据不对 | `components/ax650/ax650_uart1.c` / `ax650_pose.c` | USART1 宏二选一的自瞄支路；`AX650_UART1_ENABLE` / `AX650_UART1_RX_ENABLE`；旧 `master_process` 已删除 |
| 裁判系统/UI/多机交互 | `components/referee/` | USART6 裁判模式；热量/冷却/弹速/功率/UI 数据；`referee_ui_task_init` / `referee_my_ui_init` / `referee_ui_task()`、0xA5 帧、`referee_send()` 限频 |
| CAN 发送拥堵 / 丢帧 | `ports/stm32f4/can_port.c` | 每总线发送 FIFO、`can_send_fifo_put/transmit()`、TX 邮箱续传、丢帧计数 |
| CAN 收不到某 ID | `can_port.c` | `can_add_filter()`（bank 分配、rx_id<<5、奇偶 FIFO） |
| 日志无输出 / 格式崩溃 | `ports/stm32f4/log_port.h/.c` | RTT sink、`LOG_DISABLE_SYSTEM`、`%f` 禁令、`log_printf` 运行期拦截 |
| PID 手感/积分饱和 | `components/control/controller.c` | `pid_calculate()`、Improve 位掩码 |
| 消息话题/丢帧 | `framework/msg/msg.c` | `msg_pub/msg_sub/msg_publish/msg_recv`、深度 1 只保最新、ISR 丢弃计数 |
| 任务创建/延时/临界区 | `framework/kernel/kernel.h` + `ports/freertos/kernel_freertos.c` | `kernel_task_create/delay/enter_critical`、优先级 3/4/5 |
| 错误分级/安全态 | `framework/err/err.c` | `err_report` 四档、关联表计数、`err_log_drain` |
| 编译错误：任务函数重复定义 | `apps/infantry/app/robot_task.h` | 任务实现在 .c，头只留声明与 `ROBOT_TASK_*_ENABLE` |
| 装配/板型/骨架选择 | `apps/*/app/profile.h` + `CMakeLists.txt` | `ROBOT_PROFILE`、`ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD`、`PROFILE_ROBOT_*` |
| 参数疑问 | `config/infantry_2026/config.h`(伞头) | 子头 `config_<域>.h`；不要改业务代码 |
| Sentry 大 yaw 不动/过温/正前不准 | `apps/sentry/gimbal/gimbal.c` + `gimbal_control.c` + `config/sentry_2026/config_gimbal.h` | `gimbal_big_yaw_step()`、`SENTRY_BIG_YAW_*`、DM 状态/温度 |
| Sentry 小 yaw/pitch 方向或限位不对 | `apps/sentry/gimbal/gimbal.c` | `gimbal_small_yaw_step()`、`gimbal_pitch_step()`、ECD 27、相对 ±20° |
| Sentry 遥控映射不对 | `apps/sentry/cmd/cmd_remote.c` / `cmd_kbm.c` | `CHASSIS_RC_CONTROL`、`small_yaw_rate_deg_s` |
| Sentry USART1 VOFA / VCP 无数据 | `apps/sentry/gimbal/gimbal.c` + `apps/sentry/autonomy/autonomy.c` + `config/sentry_2026/config_board.h` | `GIMBAL_VOFA_UART1_ENABLE`、`VISION_USE_VCP`、`autonomy_usb_receive()`；两条物理链路分离 |

## 2. 调用链速查

**启动链**：`Src/main.c: main()` → `MX_*_Init` → 构建所选 `apps/<profile>/app/assembly.c`
的 `robot_init()`【关中断】→ `ports/stm32f4/board_port.h: board_init()`(DWT+LOG) →
`cmd_init/chassis_init/gimbal_init/shoot_init` → `robot_task_init()` →【开中断】→
`Src/freertos.c` 的 `MX_FREERTOS_Init()` → `osKernelStart()`。

**任务与频率**(`robot_task.c`；启停由 `robot_task.h` 的 `ROBOT_TASK_*_ENABLE` 与 profile 决定)：

| 任务 | 周期 | 循环体 / 监督 |
| --- | --- | --- |
| `robot_task_start_ins` | 1kHz | `ins_task()`；`diag_watch_kick(INS)` |
| `robot_task_start_motor` | 1kHz | `motor_control_task()`；`diag_watch_kick(MOTOR)` |
| `robot_task_start_daemon` | 100Hz | `daemon_task()` + `buzzer_task()` + `diag_watch_tick()`；`kick(DAEMON)` |
| `robot_task_start_robot` | 200Hz | `robot_task()` → `cmd_task()` + `chassis_task()`；`kick(ROBOT)` |
| `robot_task_start_ui` | 可选 | `referee_ui_task_init()` 后 `referee_ui_task()`；未注册必活 |

**串口归属**：Sentry 当前为 USART1 = 三轴编码器 VOFA、USB CDC VCP = NV v1 ROS 导航链路、
USART6 = 裁判系统、USART3 = DBUS，AX650 关闭。infantry 仍保留 USART1 AX650/VOFA 编译期二选一；
USART6 裁判/自瞄 yaw VOFA 二选一。比赛/需要裁判数据时 USART6 必须回裁判模式。

**控制流**：`cmd.c: cmd_task()` 读遥控/自瞄/键鼠 → `cmd_authority_arbitrate()` → `cmd_calc_offset_angle()` → `cmd_apply_safe_outputs()` → `msg_publish(chassis_cmd/gimbal_cmd/shoot_cmd)`；各订阅任务取最新帧，深度 1。

**底盘流**：`chassis.c: chassis_task()` 取命令 → ZERO_FORCE 停机 / 使能 → 模式定 wz → 云台系→底盘系投影 → `chassis_mecanum_calculate()` → `chassis_limit_output()` → 反馈发布 `chassis_feed`。

**电机控制流**：`components/motor/core/motor_task.c: motor_control_task()` → `dji_motor_control()`(串级 angle→speed→current) → `dm_motor_task()` 分频；HT 电机独立任务 `ht_motor_control_init()`。

**DJI 组帧规则**（`dji_motor.c`，电调拨码号≠组帧 ID）：2006/3508 id1-4→`0x200`、id5-8→`0x1FF`，rx=`0x200+id`；GM6020 id1-4→`0x1FF`、id5-8→`0x2FF`，rx=`0x204+id`；CAN1/CAN2 分组经 `can_port_bus_of()` 映射。

**反馈链**：`ports/stm32f4/can_port.c` 的 HAL RX FIFO 回调 → 遍历实例匹配 hcan+rx_id → `memcpy` 进实例 → 组件回调(如 `dji_motor_decode()`，内含 `daemon_reload` 与 DWT 计 dt)。

**发送链**：`can_transmit()` → 每总线 FIFO(BASEPRI 临界区，满丢最老+计数) → 入邮箱 → TX 空中断续传，全程非阻塞。

**姿态流**：`ins_task.c: ins_task()` → `bmi088` 全局驱动 → `ekf_imu_quaternion_ekf_update()` → `ins_state.Yaw/Pitch/Roll/q`；旧 `VisionSetAltitude()` 调用已随旧视觉栈删除(RM-011)。

## 3. 分层索引

### apps/infantry/
| 文件 | 职责与关键符号 |
| --- | --- |
| `app/robot.c/.h` | `robot_init()/robot_task()`；板型守卫、`RCC->CSR` 复位原因、IWDG 门控、err 消费 |
| `app/robot_def.h` | 协议合同：cmd/feed 结构体(`pack(1)` + 布局守卫)、`ROBOT_DEF_PARAM_WARNING`、`Enemy_Color_e`/`Bullet_Speed_e` |
| `app/robot_task.c/.h` | `robot_task_init()` + 五个任务体(`ins_task/motor_control_task/daemon_task/robot_task/referee_ui_task`)、`ROBOT_TASK_*_ENABLE`、diag 注册 |
| `app/profile.h` | 编译期装配锚点：板型断言、`PROFILE_ROBOT_*` |
| `cmd/robot_cmd.c` | `cmd_init/cmd_task`、遥控/键鼠/自瞄仲裁、`cmd_calc_offset_angle` |
| `cmd/cmd_authority.c` | `cmd_authority_arbitrate()`、`cmd_apply_safe_outputs()`(失联>急停>拨轮>运行) |
| `cmd/cmd_remote.c` / `cmd_kbm.c` / `cmd_autoaim.c` | 三路输入解析与目标整形 |
| `chassis/chassis.c` | `chassis_init/chassis_task`、`chassis_mecanum_calculate`/`chassis_limit_output`/`chassis_estimate_speed`(空实现)；`chassis_kinematics.c` 纯运动学 |
| `gimbal/gimbal.c` + `gimbal_recenter.c` | yaw/pitch 配置(IMU 反馈)、三模式、回中 |
| `shoot/shoot.c` + `heat_guard.c` | 摩擦轮/拨盘/发射状态机、热量守卫 |

### apps/sentry/

| 文件 | 职责与关键符号 |
| --- | --- |
| `app/sentry_def.h` | Sentry cmd/feed 消息合同；`small_yaw_rate_deg_s` 为小 yaw 目标角速度 |
| `cmd/cmd_remote.c` / `cmd_kbm.c` | RM_shaobin2 映射：右杆平移、左纵 pitch、左下持续连发且左横小 yaw、其余左横底盘转向 |
| `cmd/cmd_authority.c` / `robot_cmd.c` | 失联/急停/解锁仲裁、Sentry 三话题发布；旧 AX650 与回中流程关闭 |
| `chassis/chassis.c` | `CHASSIS_RC_CONTROL` 直接使用 `wz`，右中自由移动、右下小陀螺 |
| `gimbal/gimbal.c` | 大 yaw 机械正前/世界角速度阻尼、小 yaw 相对中位、pitch 相对解锁点、USART1 VOFA |
| `gimbal/gimbal_control.c/.h` | SI 单位角度→速度→力矩串级、积分/输出限幅、目标速度前馈 |
| `shoot/shoot.c` + `heat_guard.c` | 单/连发、摩擦轮 1 s 延时停转、热量守卫与堵转保护 |
| `autonomy/autonomy.c` | NV v1 USB RX 队列、命令超时/序号仲裁、测时响应与大 yaw 反馈；默认只通信不驱动 |

### components/
| 文件 | 职责与关键符号 |
| --- | --- |
| `motor/dji/dji_motor.c/.h` | `dji_motor_init/set_ref/change_feed/outer_loop/stop/enable/control`、`dji_motor_t` |
| `motor/core/motor_task.c/.h` + `motor_def.h` | `motor_control_task()` 集中调度 |
| `motor/ht/ht_motor.c` | 三环 PID + P/V/T 映射；每电机独立任务 |
| `motor/dm/dm_motor.c` | NORMAL/MIT 双模式、多圈计数、`dm_auto_recover_comm()` |
| `motor/servo/servo_motor.c` | 舵机驱动(OPTIONAL) |
| `control/controller.c` | **PID 实现**：位置式 + Improve 位(梯形积分/限幅/微分先行/滤波/变速/堵转检测) |
| `control/quaternion_ekf.c`、`control/kalman_filter.c` | 四元数 EKF、通用 Kalman |
| `crc/crc8.c`、`crc/crc16.c`、`math/user_lib.c` | 通用 CRC、矩阵/向量/滤波工具 |
| `navigation/nav_protocol.c/.h` | NV v1 固定帧、小端编解码、CRC16/MODBUS、滑动重同步与速度边界校验 |
| `imu/bmi088_driver.c`、`bmi088_middleware.c`、`ins_task.c` | `bmi088_init`、`ins_init/ins_task`、`ins_state_t` |
| `remote/remote_control.c` | SBUS/FS 遥控解析、`remote_rc_ctrl_t` |
| `referee/protocol/`(referee.c/referee_crc/referee_protocol.h)、`referee/ui/`(task/ui/app_status)、`referee/link/`(custom/vt*) | 裁判协议、UI、多机交互 |
| `ax650/ax650_pose.c`、`ax650_uart1.c` | 视觉姿态编解码与 0x0503 下行 |
| `calib/calib.c` | Flash 零位持久化 |
| `daemon/daemon.c` | `daemon_register/daemon_reload/daemon_is_online/daemon_task`，上限 64、静态池 |
| `can_comm/can_comm.c` | 双板可靠帧(休眠支，当前单板) |
| `super_cap/super_cap.c` | 超级电容(0x302/0x301，OPTIONAL) |
| `alarm/buzzer.c` | 蜂鸣码 |

### framework/
| 文件 | 职责与关键符号 |
| --- | --- |
| `kernel/kernel.h`、`kernel_version.c` | 任务/延时/临界区/ISR 判定/时间；优先级 3/4/5 静态断言；`fw_abi_anchor` |
| `err/err.c` | 四档错误分投递、关联表计数、`err_log_drain` |
| `mem/mem.c` | bump 池 |
| `msg/msg.c` | 话题上限 8、深度 1、多订阅链表、ISR 丢弃 |
| `diag/diag.c`、`telemetry.c` | 心跳监督(必活/可选)、遥测逻辑(arena 注入) |
| `types.h`、`general_def.h` | 共享枚举、单位换算/数学宏(`FW_PI`/`MATH_ABS` 等) |

### ports/
| 文件 | 职责与关键符号 |
| --- | --- |
| `stm32f4/can_port.c/.h` | `can_register/can_transmit`、双 FIFO、发送队列、`can_port_bus_of` |
| `stm32f4/usart_port.c/.h` | `usart_register/usart_send`、循环 DMA、RING≥1024、`overrun_cnt` |
| `stm32f4/imu_port.c/.h` | BMI088 片选/SPI/加热器 HAL 落地;组件不再直接持有 `hspi1/htim10` |
| `stm32f4/pwm_port/flash_port/usb_port/dwt_port/fault_port/iwdg_port/log_port` | 对应外设落地；`fault_port` 含复位后防御环；`log_port` 为 RTT sink |
| `stm32f4/board_port.h` | `board_init()` = DWT(SystemCoreClock 派生)+日志初始化 |
| `stm32f4/telemetry_arena.c` | 五环对象(.ccmbss)+tag 赋值 |
| `stm32f4/dg_test_hang.c` | G9 看门狗注入钩子(`-DDG_TEST_HANG`) |
| `freertos/kernel_freertos.c` | FreeRTOS 后端(唯一 RTOS 落点) |

## 4. 与上游快照对照

```bash
# 单文件逐行对照(快照 vs 当前工程)
git diff --no-index ".agents/skills/rm-electrical-dev/assets/yuelu-basic-framework/application/chassis/chassis.c" "apps/infantry/chassis/chassis.c"

# 文件级审计(改动/新增/缺失 + 符号差集)，只读
python .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .
```

审计计数漂移属预期(批量迁出/删除)；**红旗权威机制** = `tools/g4_baseline.py check --baseline .Doc/gate/manifest-*` + `.Doc/gate/redflags`。解读规则见 [基线审计](baseline-diff.md)。

## 5. 当前陷阱与已修项（勿重新引入）

- **yaw 无绝对参考**：无磁力计接入，yaw 仅陀螺积分，长期漂移属硬件现状。
- **`while(1)` 只允许启动期 fail-fast**：白名单见 `.Doc/gate/redflags`(4 条豁免)；新增即为红旗。
- **msg 深度 1**：只保最新，事件型消息不得依赖排队；`msg_publish` 禁止 ISR 调用。
- **framework 零 RTOS include**：需要 RTOS 的能力一律经 `ports/freertos` 后端。
- **profile.h 是唯一 `#if` 允许点**(装配开关注册 B 类账本)；config 子头不得跨文件 `#if`(已全部上移伞头)。
- **MATH_MALLOC 不要"去重"**：`kalman_filter.h` 不能 include `cmsis_os.h`，否则分配器从 `malloc` 静默切到 `pvPortMalloc`，text +68B(DEV-20261122-25 实证)。
- **`rc`/`mouse` 是协议字段名**：它们不是 typedef，改名会破坏 DBUS 帧语义；命名检查已白名单。
- **旧视觉栈已删**：`master_process.*`/`seasky_protocol.*` 不存在；`Enemy_Color_e`/`Bullet_Speed_e` 在 `robot_def.h`。
- **已修**(不要按旧地图找)：`message_center`→`framework/msg`；`bsp_init.h`→`board_port.h`；`USARTIsReady()`/`PWMSelectTclk()` 上游 bug 已随 ports 重写；daemon `init_count` 覆盖已修、注册改静态池。
- **双板支休眠**：`can_comm` 与 `CHASSIS_BOARD/GIMBAL_BOARD` 分支未实装，profile 守卫会 `#error`；恢复需硬件窗口(DEC-009)。

## 6. 命名与配置速查

- 命名：`.Doc/spec/命名规范.md`；机器检查 `python tools/check_naming.py`；clang-tidy 规则 `.clang-tidy`。
- 参数：`config/infantry_2026/config.h` 伞头 + `config_<域>.h`；单头内不跨域 `#if`。
- 模块状态(ACTIVE/OPTIONAL/DORMANT)：`components/README.md`。
