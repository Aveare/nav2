# .Doc 文档索引

> 本文件是 `.Doc/` 的唯一入口。**不知道该看哪份时,先看这张表。**
> 文档按"共用 / 兵种专属 / 历史归档"三层存放,规则见 `.Doc/spec/文档存放与管理规则.md`。
> 历史 DEV 条目已并入 `.Doc/history/开发史.md` 附录 I；主台账只留当前状态。

## 目录

| 目录 | 装什么 | 什么时候看 |
|---|---|---|
| `spec/` | **共用规范**：文档阅读分级、文档存放与管理规则、兵种分支协作规范、架构与开发指南、必须做&禁止做、命名规范、注释规范、热量管理设计、多兵种切换架构、版权与署名 | 写代码 / 改名 / 加宏 / 改注释 / 开分支 / 查判据 |
| `guide/` | **共用操作**：调试与测试流水线、调参诊断手册、常见故障排查、RTT日志与调试通道、VSCode+Ozone使用方法、如何定位bug、嵌入式教学大纲 | 搭环境、跑测试、调参、排障 |
| `robots/` | **兵种专属**：`infantry/`、`sentry/`、`heavy/` 各自的 README、**DEVLOG**、design/params/plans/ops | 做某一个兵种的事时 |
| `plan/` | **当前任务与路线** | 想知道现在做什么、什么算过 |
| `history/` | `开发史.md`（含 218 条 DEV 原档）与 `archive/`（旧计划/审查/设计原文） | 追溯决策与历史 |
| `gate/` | 机器基线：manifest、宏账本、符号契约、红旗清单 | 审计工具读取；**不手改** |
| `data/` | 实测数据 json | 核对历史实测结论 |

## 兵种库

| 兵种 | 文档库 | 台账 | 代码入口 | 说明 |
|---|---|---|---|---|
| 步兵 | `robots/infantry/` | [DEVLOG](robots/infantry/DEVLOG.md) | `apps/infantry/` + `config/infantry_2026/` | 共享回归 + 功率控制实现 |
| 哨兵 | `robots/sentry/` | [DEVLOG](robots/sentry/DEVLOG.md) | `apps/sentry/` + `config/sentry_2026/` | 手动整机实装，`[待实测]` |
| 重装 | `robots/heavy/` | [DEVLOG](robots/heavy/DEVLOG.md) | `apps/heavy/` + `config/heavy_2026/` | 服务骨架，不得烧录 |

## 按问题查

| 我想知道 | 看 |
|---|---|
| 新会话先读哪几份、哪些不用读 | `spec/文档阅读分级.md` |
| 某个文档该放哪、归谁维护 | `spec/文档存放与管理规则.md` |
| 我在哪个兵种/分支上开发、这个文件能不能改 | `spec/兵种分支协作规范.md` |
| 分层、数据流、任务表、加模块 | `spec/架构与开发指南.md` |
| 多兵种怎么切、加兵种步骤 | `spec/多兵种切换架构.md` |
| 命名 / 注释 / 证据标记 | `spec/命名规范.md`、`spec/注释规范.md` |
| 框架纪律、禁改清单 | `spec/必须做&禁止做.md` + 根 `AGENTS.md` |
| 热量守卫的规则与参数 | `spec/热量管理设计.md` |
| 电机/CAN/裁判/DBUS 协议速查 | `.agents/skills/rm-electrical-dev/references/hardware.md` |
| 改完代码怎么验、收工清单 | `guide/调试与测试流水线.md` |
| 某症状该调什么参数 | `guide/调参诊断手册.md` |
| 车不正常了怎么办 | `guide/常见故障排查.md` |
| 抓日志 / 加日志 | `guide/RTT日志与调试通道.md` |
| 工具脚本干什么 | 根 `tools/README.md` |
| 某次改动改了什么 | `history/开发史.md`（附录 I 按编号/日期）；近期看对应兵种 `DEVLOG.md` |
| 被否决的方案 | 根 `RM_DEVLOG.md` 的 DEC 台账（带类型标记） |
| 现在做什么、下一步 | 根 `RM_DEVLOG.md` §0 + 当前兵种 `DEVLOG.md` §0 |

## 维护规则

- 新增文档先按 `spec/文档存放与管理规则.md` 判定"共用 / 兵种 / 归档",再在本表或兵种库 README 登记。
- 引用写带目录的路径（如 `.Doc/spec/架构与开发指南.md`），不要只写文件名。
- `history/` 只追加；过期计划移入 `history/archive/`，不留在活跃目录。
- `python tools/refactor_gate.py check-links` 自检引用；`python tools/check_robot_scope.py --gate` 自检兵种归属。
