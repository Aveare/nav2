# RM-JIT-CR · RoboMaster 整车电控

> 基于 STM32F407IGHx + FreeRTOS 的多兵种电控工程，自研分层 `apps → components → framework → ports → HAL`
>（升级方案已收口，见 `.Doc/history/开发史.md` 第十六章）。同一个仓库用编译期 `ROBOT_PROFILE` 产出
> **步兵 / 哨兵 / 重装** 三个 profile；共享代码不分兵种，兵种差异只落在 `apps/<robot>/` 与 `config/<robot>_2026/`。
>
> **当前交付兵种以各兵种 `DEVLOG.md` 的状态行为准**；兵种专属文档在各兵种库
> `.Doc/robots/<robot>/`，共用规范在 `.Doc/spec|guide`。

## 接手顺序

1. `AGENTS.md`（协作契约）→ 2. `.Doc/spec/文档阅读分级.md` §1 的 A 层 5 份 → 3. 本文 → 4. 当前兵种库。

## 目录结构

```
apps/          装配层：common(通用壳) + infantry / sentry / heavy
components/    组件：motor(core+dji/dm/ht/servo) control crc math imu remote referee
                 ax650 calib super_cap can_comm daemon alarm navigation
framework/     平台无关框架：kernel/err/mem/msg/diag + types/general_def（零 RTOS include）
ports/         平台落地：stm32f4 全总线 *_port + freertos 内核后端 + telemetry arena
config/        参数：<robot>_2026/config.h 伞头 + config_<域>.h 子头
tools/         工具：闸门 / 构建 / 采样分析 / 功率标定 / 烧录
tests/         主机单元测试（四套件 + 三兵种 profile 编译矩阵）
.Doc/          文档：spec / guide / robots / plan / history / gate / data（入口 .Doc/README.md）
Src/ Inc/      CubeMX 生成代码（USER CODE 区外不要手改）
Drivers/       ST HAL + CMSIS（第三方，禁改）
Middlewares/   FreeRTOS / USB / DSP / SEGGER RTT（第三方，禁改）
```

## 兵种入口

| 兵种 | 应用 | 参数 | 构建 | 文档 / 台账 | 状态 |
|---|---|---|---|---|---|
| 步兵 | `apps/infantry/` | `config/infantry_2026/` | `.\build.ps1 infantry` | `.Doc/robots/infantry/`（[DEVLOG](.Doc/robots/infantry/DEVLOG.md)） | 共享回归 + 功率控制实现 |
| 哨兵 | `apps/sentry/` | `config/sentry_2026/` | `.\build.ps1 sentry -Current` | `.Doc/robots/sentry/`（[DEVLOG](.Doc/robots/sentry/DEVLOG.md)） | 手动整机实装，`[待实测]` |
| 重装 | `apps/heavy/` | `config/heavy_2026/` | `.\build.ps1 heavy` | `.Doc/robots/heavy/`（[DEVLOG](.Doc/robots/heavy/DEVLOG.md)） | 服务骨架，**不得烧录** |

> **台账分工**：公共状态/决策在根 `RM_DEVLOG.md`；兵种状态、问题、近期 DEV 在各兵种 `DEVLOG.md`。
> 标题与行带 `[公共]/[INFANTRY]/[SENTRY]/[HEAVY]` 标记，并行开发互不改同一文件。

## 编译与烧录

```powershell
# 全量编译（需 arm-none-eabi-gcc；本机实测 15.3.1，不同版本 text 有差异，不要混用）
.\build.ps1 sentry -Current        # 生成 build_sentry/sentry.elf + firmware/sentry.elf
.\build.ps1 infantry               # build/basic_framework.elf
.\build.ps1 heavy                  # build_heavy/basic_framework.elf

# 等价 CMake（预设同时固定 ROBOT_PROFILE 与 CONFIG_PROFILE）
cmake --preset sentry && cmake --build --preset sentry

# 烧录（按 firmware/current.txt 选兵种；脚本只烧，不编译）
powershell -ExecutionPolicy Bypass -File tools\flash_manual.ps1
```

- 每个兵种独立 build 目录（`build/`、`build_sentry/`、`build_heavy/`），不共享缓存。
- `-Current` 额外写 `firmware/<profile>.elf` 与 `firmware/current.txt`（兵种/来源/时间/SHA256）；
  **烧录前必须核对 `PROFILE=` 与文件名**，不要把 `build/basic_framework.elf` 当哨兵固件。
- `Makefile` 只支持步兵；哨兵/重装必须走 CMake。新增 `.c` 要同步登记 Makefile（CMake 是 GLOB）。
- `build.ps1` 自动定位 cmake/ninja/gcc（PATH → ST bundle → `C:\Toolchain`），不会自动烧录。

## 主机测试与提交前检查

```powershell
powershell -File tests\run_fwk_tests.ps1       # 框架全家桶 + 金标准对拍 + 三骨架
powershell -File tests\run_chassis_tests.ps1   # cmd 仲裁/看门狗
powershell -File tests\run_ax650_tests.ps1     # 视觉编解码/串口链路
powershell -File tests\run_profile_tests.ps1   # 三兵种 profile/assembly 编译

python tools\check_robot_scope.py      # R1 兵种范围（跨兵种改动 FAIL）
python tools\refactor_gate.py check-links       # .Doc 引用不悬空
python tools\check_naming.py           # 命名
python tools\check_macros.py           # 宏账本
python tools\check_source_lists.py     # Makefile 与磁盘一致
```

> 2026-11-27 起已脱离上游：不再有 `audit_baseline` / `check_plan`；字节事实源是 git，
> 结构基线是 tag `arch-after-U3U4` + `.Doc/gate/manifest-arch-after-U3U4/`。

## 文档体系

- `.Doc/README.md`：唯一入口索引。
- `.Doc/spec/`：共用规范（架构、命名、注释、纪律、热量、版权、文档存放规则）。
- `.Doc/guide/`：共用操作（流水线、调参、排障、RTT、Ozone）。
- `.agents/skills/rm-electrical-dev/references/`：AI 速查（代码地图、历史踩坑、工作流、硬件协议、协作约束）。
- `.Doc/robots/<robot>/`：兵种专属设计/参数/计划/操作。
- `.Doc/plan/`：当前任务与路线；`.Doc/history/`：开发史与归档原文；`.Doc/gate/`：机器基线（不手改）。
- 分支与提交：`.Doc/spec/兵种分支协作规范.md`；文档分库：`.Doc/spec/文档存放与管理规则.md`。
