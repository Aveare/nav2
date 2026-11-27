# 重装 Heavy 文档库

> 本目录只放**重装专属**文档。存放规则见 `.Doc/spec/文档存放与管理规则.md`。

## 当前定位（2026-11-27）

> **台账**：[DEVLOG.md](DEVLOG.md)（状态、问题、近期 DEV；标题带 `[HEAVY]` 标记）。

- `apps/heavy/` 与 `config/heavy_2026/` 是**服务骨架**：板级初始化/IMU/守护任务可跑，
  `init/run` 显式留空，`calib_enable=false`；config 带 `pragma message "do NOT flash"`。
- 用途：三兵种编译矩阵、共享框架回归；**没有实车参数，不得烧录**。
- 构建：`.\build.ps1 heavy`（产物 `build_heavy/basic_framework.elf`）。

## 目录

| 路径 | 内容 |
|---|---|
| `design/` | （待建）重装机构与构型设计 |
| `params/` | （待建；实车前必须重标并移除 SKELETON 标记） |
| `plans/` | （待建） |
| `ops/` | （待建） |

## 入口

- 应用：`apps/heavy/app/`；参数：`config/heavy_2026/config.h`。
- 多兵种结构：`.Doc/spec/多兵种切换架构.md`；分支规则：`.Doc/spec/兵种分支协作规范.md`。
