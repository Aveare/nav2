# 重装 Heavy 开发台账

> 重装专属台账；分支 `heavy`。公共内容见根 `RM_DEVLOG.md`；文档索引见 `.Doc/robots/heavy/README.md`。
> 条目标题统一 `[HEAVY]` 前缀。

## 0. 当前状态

| 项目 | 内容 |
| --- | --- |
| 定位 | 服务骨架：板级初始化/IMU/守护任务可跑，`init/run` 显式留空，`calib_enable=false` |
| 应用/参数 | `apps/heavy/` + `config/heavy_2026/`（带 `pragma message "do NOT flash"`） |
| 构建 | `.\build.ps1 heavy`（产物 `build_heavy/basic_framework.elf`） |
| 实车状态 | **不得烧录**；没有实车参数，三轴/发射/底盘均未接 |
| 下一步 | 指定重装负责人后：先定构型与执行器，再按 `config/heavy_2026/` 重标并移除 SKELETON 标记 |

## 1. 问题台账

| ID | 现象 | 状态 | 下一步 |
| --- | --- | --- | --- |
| — | 暂无 | — | — |

## 2. 近期开发条目

<!-- 格式:### DEV-YYYYMMDD-HHMMSS｜日期｜[HEAVY] 标题 -->

暂无（2026-11-27 分库后新增条目从这里开始）。

## 3. 历史

- `.Doc/history/开发史.md` 附录 I：按 `[HEAVY]` 检索（B11/B12 骨架与编译矩阵）。
