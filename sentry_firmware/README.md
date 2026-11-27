# JIT-RM 1029 · 哨兵 Sentry STM32 整车电控

> **项目身份**:本仓库当前交付目标是 **哨兵（Sentry）**。哨兵应用入口为 `apps/sentry/`,参数入口为 `config/sentry_2026/config.h`;构建和生成固定烧录副本时必须显式选择 `sentry`。仓库保留 infantry/heavy profile 作为共享框架、历史回归和多兵种兼容代码,不得据此把当前项目误标为步兵工程。

基于跃鹿 [basic_framework](https://github.com/HNUYueLuRM/basic_framework) 起步、2026 年完成自研框架升级的 **STM32F407IGHx + FreeRTOS 单板哨兵工程**。
现行分层为 `apps → components → framework → ports → HAL`(方案 `.Doc/plan/自主研发框架升级总方案.md`;B0a~B12 已收口,见 `RM_DEVLOG.md` DEV-20261122-21/22/23);
哨兵整车含底盘、三轴云台、17 mm 发射和遥控/键鼠仲裁;旧 AX650 USART1 链路关闭,上位机通过独立 USB CDC 虚拟串口共用 `NV v1` 导航帧与 RMCV `0x1022/0x0503` 自瞄帧，NUC 仍兼容旧 `0x1021`。当前 `0x0503` 仅解析保存、尚未接入哨兵云台/发射控制,完整协议状态见 `.Doc/spec/NUC-STM32导航与自瞄USB串口协议.md`;三轴零位、方向和 PID 仍以 `[待实测]` 证据等级管理。

> **接手顺序**:本文件 → `RM_DEVLOG.md` §0(AI 冷启动摘要) → `.Doc/README.md`(文档总索引)。
> **协作契约**:`AGENTS.md`(工程地图/禁区/检查与证据);症状→代码定位见 `.agents/skills/rm-electrical-dev/references/code-map.md`。
>
> 当前哨兵快照:单板 ONE_BOARD;`apps/sentry/{cmd,chassis,gimbal,shoot,autonomy}` 与 `config/sentry_2026/` 已完成手动整机实装,`NV v1` 已完成主机测试和 ARM 编译但尚未烧录与实体联调(DEV-20261126-30)。
> 功能状态与下一步以 `RM_DEVLOG.md` §0 为准。

## 目录结构

```
apps/          装配层(按兵种)：sentry(当前交付目标) / infantry(共享回归) / heavy(兼容锚点)
                 └─ <robot>/app/(profile.h + robot.c + robot_def.h + robot_task.c)
                    sentry/{chassis,cmd,gimbal,shoot,autonomy}/
components/    组件(按域)：motor(core+dji/dm/ht/servo 分组) control crc math navigation imu
                 remote referee ax650 calib super_cap can_comm daemon alarm
framework/     平台无关框架：kernel/err/mem/msg/diag + types.h/general_def.h(零 RTOS include)
ports/         平台落地：stm32f4 全总线 *_port + freertos kernel 后端 + telemetry arena
config/        哨兵参数唯一改动点：sentry_2026/config.h 伞头 + config_<域>.h 子头
tools/         开发工具：命名/宏/源清单/符号契约/重构闸门/基线审计/RTT 抓取/数据分析
tests/         主机单元测试(纯逻辑,无需硬件;含四套件 + 三骨架 sim 编译矩阵)
.Doc/          文档体系(plan/spec/detail/guide/history/review/gate,入口 .Doc/README.md)
Src/ Inc/      CubeMX 生成代码(USER CODE 区外不要手改)
Drivers/       ST HAL + CMSIS
Middlewares/   FreeRTOS / USB / DSP库 / SEGGER RTT
```

> `application/`、`modules/`、`bsp/` 目录已按升级方案删除(B8/B12);这些路径只出现在历史 DEV 条目与非顺序设备上下文里。

## 保留的步兵参考接线（非哨兵烧录依据）

> 下表用于共享代码回归与历史追溯。哨兵接线、设备 ID 和方向必须以 `config/sentry_2026/`、对应 DEV 实测条目及当前实车为准,不得照抄本表烧录哨兵。

| 总线 | 设备 | 电调拨码/ID |
|---|---|---|
| CAN1 | 轮电机 M3508+C620 | 左前=4 右前=1 左后=2 右后=3 |
| CAN1 | 云台 YAW GM6020 | 5 |
| CAN2 | 摩擦轮 M3508+C620 | 左=1 右=2(REVERSE) |
| CAN2 | 拨盘 M2006+C610 | 3 |
| CAN2 | 云台 PITCH M3508+C620 | 4 |
| USART3 | DBUS 遥控器 | 100000 8E1 反相 |
| USART6 | 裁判系统 C 板 / 自瞄 yaw VOFA(宏二选一) | 裁判 115200 8N1;VOFA 115200 8N1 |
| USART1 | AX650 自瞄 / 底盘 VOFA(宏二选一) | 视觉 921600 8E1;VOFA 921600 8N1 |
| SPI1 | BMI088 IMU | — |

## 编译与烧录

```powershell
# ARM 全量编译(需 arm-none-eabi-gcc;本机实测 15.3.1,不同版本 text 有差异,不要混用)
.\build.ps1 sentry -Current                 # 哨兵推荐入口:生成 firmware/sentry.elf
powershell -ExecutionPolicy Bypass -File tools\flash_manual.ps1   # 按 profile 烧对应 bin
cmake --preset sentry
cmake --build --preset sentry
mingw32-make -j24                           # Legacy:仅步兵,需 MinGW make,产物 build_make/

# 运行日志
JlinkRTTClient                              # SEGGER RTT,支持彩色 LOGERROR
```

### 切换兵种(编译期)

兵种在编译期固定,烧录后不会自动切换;换兵种 = 重新编译 + 重新烧录。推荐用 CMake Presets + 一键脚本:

> 本仓库交付哨兵时只认 `sentry` 构建。CMake 无参数默认值仍为 infantry,仅为历史兼容;无参数构建产物不得作为哨兵固件烧录。

```powershell
.\build.ps1 sentry      # 明确产物 build_sentry/sentry.elf
.\build.ps1 infantry    # 产物 build/basic_framework.elf(默认)
.\build.ps1 heavy       # 产物 build_heavy/basic_framework.elf

# 同时复制到固定烧录副本(EmberProbe 只需首次把 ELF 指向它)
.\build.ps1 sentry -Current
# -> firmware/sentry.elf(哨兵明确命名)
# 兼容保留 firmware/current.elf + firmware/current.txt(兵种/来源/时间/SHA256)

# 等价的标准 CMake 命令
cmake --preset sentry
cmake --build --preset sentry
```

- 预设见 `CMakePresets.json`:每个 preset 同时固定 `ROBOT_PROFILE` 与 `CONFIG_PROFILE`,避免 `CONFIG_PROFILE` 缓存残留导致“兵种代码 + 错误参数”。
- 每个兵种独立 build 目录(`build/`、`build_sentry/`、`build_heavy/`),不共享缓存。
- `make`/`Makefile` 只支持步兵;哨兵/重装必须走 CMake。
- 脚本只编译（可选复制固定烧录副本）并打印 ELF 路径,不自动烧录。哨兵始终额外生成
  `build_sentry/sentry.elf`;带 `-Current` 时再生成 `firmware/sentry.elf`,避免与步兵 ELF 混淆。
- **哨兵烧录优先选 `firmware/sentry.elf`**,文件名直接表明兵种。`firmware/current.elf` 仅为
  EmberProbe 和既有 VSCode 调试配置兼容保留;其身份必须同时核对 `firmware/current.txt` 的
  `PROFILE=sentry`。不要把 `build/basic_framework.elf` 当成哨兵固件。
- VSCode 任务见 `.vscode/tasks.json`。`Ctrl+Shift+B` 默认任务会弹出兵种选择(sentry/infantry/heavy),构建并更新 `firmware/current.elf`;也可直接运行 `Build: Sentry (current.elf)` 等任务。
- 原 Makefile 构建任务保留为 `Legacy:` 前缀任务(仅步兵,产物 `build_make/`,需 `mingw32-make` 在 PATH;本机未装,日常不要用)。
- `download dap` 任务先按所选民种编译,再调 `tools/flash_manual.ps1`;烧哪个兵种只看 `firmware/current.txt`(可用 `-Profile` 覆盖),避免“编译 sentry 烧了步兵 bin”。

> `build.ps1` 自动定位 cmake/ninja/gcc(PATH → ST bundle → `C:\Toolchain`),`cmake` 不在 PATH 也能用;
> 新增 build 目录时脚本会显式传入 `CMAKE_C_COMPILER/ASM_COMPILER/MAKE_PROGRAM`,防止落到 PATH 上旧版 gcc(14.2)。
> CI 同时验证 Debug 与 RelWithDebInfo;硬件 Release 产物需使用同一 arm-none-eabi-gcc 版本。
> 新增 `.c` 必须同步登记 Makefile(CMake 是 GLOB 自动收集),否则 Makefile 产物缺文件。

## 主机测试与提交前检查

```powershell
# 主机单元测试(无需硬件,需 gcc)
powershell -File tests\run_chassis_tests.ps1
powershell -File tests\run_fwk_tests.ps1
powershell -File tests\run_ax650_tests.ps1
powershell -File tests\run_profile_tests.ps1

# 命名/宏/源清单/符号契约(提交前必过)
python tools/check_naming.py
python tools/check_macros.py
python tools/check_macro_dups.py
python tools/check_symbol_contract.py

# 基线审计(只读):相对上游模板改了什么
python .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .

# 日志格式:禁止 %f/%e/%g/%a;打印浮点先 log_float_to_str 再用 %s
powershell -ExecutionPolicy Bypass -File tools\check_log_format.ps1

# 源文件清单/Makefile 登记一致性 + 文档链接
python tools/check_source_lists.py
python tools/refactor_gate.py check-links
```

> 重构期每批还要过 `python tools/refactor_gate.py check`(G1~G5 零行为变化闸门,
> 判据见 `.Doc/plan/重构总方案.md` §2)。

## 枪口热量守卫(发射链路)

`apps/infantry/shoot/heat_guard.c` 在发射前裁决"能否发 / 发多快",数据来自裁判系统
0x0201/0x0202/0x0204(热量上限/实时热量/冷却)。三条不变量:反馈只校正热量估计 Q̂、
记账点前移到放行瞬间、误差只允许往安全方向。

- **两级闸门**:连发保留余量 30 点 → 降到 `SINGLE` 只放单发;单发保留余量 10 点 → `HOLD` 禁发。
  裁判未上线时 `OFF` 直通(台架);曾上线但断流超过 300ms 时降级限速。
- **射速律**:`HEAT_GUARD_RATE_MODE=1` 恒定摊平(窗口 4s),稳态回到散热平衡。
  230/14 档下连发约 7 发/s,约 4s 花完余热后落到 1.4 发/s。
- **战术档位**:键鼠 **X** 循环 `ECO(1.4) → NORMAL(6) → BURST(12)` 发/s;
  档位只决定请求射速,实际放行仍由守卫裁决,爆发档不会越过热量。
- **本车 UI**:裁判 UI 左下新增热量条、当前热量、档位与守卫状态;热量条 10Hz 刷新,
  数字/档位/状态仅在变化时发送。
- **允许发弹量**:`HEAT_GUARD_ALLOWANCE_ENABLE` 默认 0(台架额度恒为 0,开着会永久锁枪);
  **正式比赛上电前必须置 1**。

参数在 `config/infantry_2026/heat.h`;规则依据、架构与标定方法见 `.Doc/spec/热量管理设计.md`。

## 哨兵 USART1 VOFA 与上位机虚拟串口

当前哨兵配置严格分开两条链路:

- **USART1 = 云台编码器 VOFA**:`GIMBAL_VOFA_UART1_ENABLE=1`,PA9 TX/PB7 RX,
  921600 8N1。USB-TTL 的 RX 接 PA9、GND 与主控共地,不要接 5V。
- **USB CDC VCP = ROS 导航物理链路**:`VISION_USE_VCP` 已启用,`NV v1` 负责底盘速度、
  测时和大 yaw 反馈。默认 `SENTRY_AUTONOMY_ENABLE=0`,只通信不驱动;完整帧格式、ROS
  话题和放行步骤见 [NAVIGATION_USB.md](NAVIGATION_USB.md)。
- **AX650 已停用**:`AX650_UART1_ENABLE=0`、`AX650_UART1_RX_ENABLE=0`。
  `CHASSIS_VOFA_UART1_ENABLE=0`,因此 USART1 当前只有云台 VOFA 一个所有者。
- **USART6 保持裁判系统**。本次改动不改变裁判接收、UI 或热量/功率数据链路。

VOFA+ 选择 **JustFloat**、921600 baud、8 data bits、None parity、1 stop bit。云台任务
200 Hz 发送 3 个 `float` 和定界符 `00 00 80 7F`,共 16 字节/帧:

| 通道 | 数据 | 单位/范围 |
|---|---|---|
| 0 | 大 yaw DM 电机多圈编码器位置 `total_position` | rad |
| 1 | 小 yaw GM6020 单圈编码器位置 `ecd` | 原始码 0~8191 |
| 2 | pitch DM 电机多圈编码器位置 `total_position` | rad |

发送分频由 `GIMBAL_VOFA_SEND_DIVIDER` 控制:1=200 Hz,2=100 Hz。USART1 云台 VOFA、
底盘 VOFA 和历史 AX650 链路有编译期互斥检查,不能同时开启。

## 遥控器操作

| 操作 | 效果 |
|---|---|
| 左/右上挡 | 急停+锁存 |
| 右上+左中+全杆回中 | 解锁；只重锚当前云台目标，不触发底盘自动回中 |
| 右中挡 | 人工自由移动；右摇杆平移，左摇杆横向控制底盘转向 |
| 右下挡 | 小陀螺；右摇杆仍可同时平移 |
| 左中挡 | 左摇杆横向控制底盘转向；AX650 保持关闭 |
| 左下挡 | 开启摩擦轮并持续拨弹；左摇杆横向仍控制小 yaw GM6020 |
| 左摇杆纵向 | 控制 pitch 相对解锁位置 ±20° |
| 右摇杆 | 控制四个底盘 M3508 前后、左右平移 |
| 拨轮下压 | 启动摩擦轮；短拨松手=单发×1，按住≥250ms=连发；松手后1s自动关摩擦轮 |
| 拨轮>300 | 急停锁存 |

当前 `SENTRY_SMALL_YAW_ENABLE=1`、`SENTRY_BIG_YAW_OUTPUT_ENABLE=1`。普通挡下大 yaw DM
按最短路径锁定机械正前方 `SENTRY_BIG_YAW_FRONT_ALIGN_RAD=-1.52914 rad`，左摇杆不直接给大 yaw
目标；小陀螺挡下大 yaw 使用 IMU Z 轴角速度做世界系零角速度阻尼。IMU 已确认安装在大 yaw
平台。左下挡左摇杆横向给小 yaw 角速度目标，其余左挡位给底盘转向。
左下挡同时持续请求 `FRICTION_ON + LOADER_BURST`；拨回中挡后停止连发请求，摩擦轮按 1 s
延时停转。实际拨弹仍需摩擦轮在线并到速，且通过热量守卫。

> 大 yaw 的 10 N·m 输出上限和 3 N·m 积分额度来自本地 RM_shaobin2 工作树，当前仅通过
> 编译与主机测试，尚未在本工程固件上完成台架回归。此前本车出现过 10 N·m 饱和、机构带不动
> 和 DM 线圈过温；首次烧录必须架空、低时长测试并监测驱动状态和温度。

AX650 关闭后左中挡不会进入旧自瞄目标整形或禁用回中逻辑。VCP 导航只解析 CRC 正确、
序号前进且未越限的 `NV v1` 帧；默认配置不会把已解析速度送入底盘。

> 实际放行还受热量守卫约束:热量进入 `SINGLE` 只放单发,进入 `HOLD` 禁发;
> 裁判系统未接线时守卫为 `OFF`,全放行(台架行为)。

## 键鼠操作(图传链路,任意键/鼠标动作即激活,超时自动回落遥控)

| 操作 | 效果 |
|---|---|
| W/A/S/D(Shift) | 底盘平移(Shift慢速 ×0.4) |
| 鼠标横移 | 普通挡控制底盘转向；小陀螺挡忽略横向转向量 |
| 鼠标纵移 | 控制 pitch 相对目标；键鼠不控制小 yaw |
| 右键按住 | 摩擦轮 |
| 左键按住 | 连发(需摩擦轮已开) |
| R | 单发×1 |
| E | 三发×3 |
| Q | 小陀螺切换 |
| X | 战术射速档循环 ECO→NORMAL→BURST |
| F | 当前无动作(`SENTRY_GIMBAL_RECENTER_ENABLE=0`) |
| V | 鼠标灵敏度降档(×0.85,下限 0.2) |
| B | 鼠标灵敏度升档(上限 2.5) |
| G | 灵敏度重置为 1.0 |

> V/B/G 调档后通过 RTT 打印当前倍率。灵敏度基准见 `config` 的 `KBM_YAW_SENS` / `KBM_PITCH_SENS`。
> X 切档后 RTT 打印当前档位,裁判 UI 同步显示。
> 单发/三发按 R/E 前需先开摩擦轮,否则请求会挂在待发队列,开轮后才补发。

## 文档索引

- `.Doc/README.md` —— 文档总索引(按问题查:计划/规范/细节/操作/归档)
- `.Doc/plan/自主研发框架升级总方案.md` —— 结构轴执行入口(状态 C 升级期;B0a~B12 已收口,待 M3 实车回归)
- `.Doc/plan/总计划书.md` —— 功能执行队列(冻结;解冻 = 升级结项)
- `.Doc/plan/重构总方案.md` —— 2026-11-20 已结项,只作历史
- `.Doc/spec/架构与开发指南.md` —— 分层/数据流/任务表/如何加新模块
- `.Doc/spec/命名规范.md` —— 文件/类型/函数/变量/宏命名与例外清单(改名前必读)
- `.Doc/spec/多兵种切换架构.md` —— ROBOT_PROFILE/装配壳/兵种接入步骤(加兵种前必读)
- `components/README.md` —— 组件状态登记(ACTIVE/OPTIONAL/DORMANT)与功能缺口落点
- `.Doc/spec/必须做&禁止做.md` —— 框架纪律
- `.Doc/spec/注释规范.md` —— 注释风格、参数证据标记、批量重写闸门(改代码前必读)
- `.Doc/spec/参数证据表.md` —— 每个参数的证据等级与待实测清单(调参前先看这张表)
- `.Doc/spec/本车调参结论.md` —— 实车标定值与调参经验(改参数前必读)
- `.Doc/detail/SENTRY_RM_shaobin2移植说明.md` —— 本次 Sentry 专用移植范围、遥控/PID/串口与验证
- `.Doc/spec/PID参数整定.md` —— Ozone 示波整定方法
- `.Doc/spec/热量管理设计.md` —— 热量守卫的规则依据、架构与标定方法
- `.Doc/guide/调试与测试流水线.md` —— 改码→闸门→测试→上车→smoke→采集 全流程
- `.Doc/guide/调参诊断手册.md` —— 症状→变量/参数→工具→判据(调参前必读)
- `.Doc/guide/常见故障排查.md` —— 现象→证据→处置(排障先读)
- `tools/README.md` —— 工具索引(每个脚本一行职责)
- `.Doc/guide/团队协作指南.md` —— git/GitHub 日常协作操作(新成员从这里接入)
- `RM_DEVLOG.md` —— DEV/DEC/RM 台账(接手先读 §0)
- `AGENTS.md` —— AI/协作契约(工程地图、禁区、检查与证据)
- `.agents/skills/rm-electrical-dev/references/code-map.md` —— 症状→文件/函数定位
