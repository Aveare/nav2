# AI 协作约束

> 与根 `AGENTS.md` 同源；冲突时以 `AGENTS.md` 与 `.Doc/spec/` 为准。本文件只列 AI 会话必须记住的约束。

## 1. 会话仪式

1. 读 `.Doc/spec/文档阅读分级.md` §1 的 A 层 5 份（每会话一次，不重复读）。
2. `python tools/check_robot_scope.py` + `git status -sb`，确认分支/兵种/profile 与工作区状态。
3. 首条回复声明：**兵种 / 分支 / profile / 验证方式**。
4. 收工时：DEV 条目 + 文档同步 + 提交/推送 + 核对上游同步。

## 2. 授权分级

| 级别 | 内容 | 授权 |
|---|---|---|
| 参数级 | `config/**` 数值、PID、注释、文档 | 可直接改 |
| 结构级 | 新增/删除 C/H、新函数/任务/消息字段、改调用链、改 components/framework/ports 接口、改构建文件 | 必须用户**对该次操作**明确批准 |
| 硬件动作 | 烧录、上电、转电机、动实车 | 始终由用户执行 |
| 删除/归档 | 删文档、删模块、改写历史记录 | 先确认可追溯，再执行 |

## 3. 边界（AI 最容易越界的三处）

- **跨兵种**：兵种私有路径 = `apps/<robot>/`、`config/<robot>_2026/`、`tests/*<robot>*`、
  `.Doc/robots/<robot>/`。跨兵种改动会被守卫判 FAIL；需要时拆分支/拆 DEV。
- **共享层**：`components/framework/ports/tools` 与共用文档只走 `fix/shared-*` → `main`，
  不在兵种分支顺手改默认值。
- **分层**：组件不得 include `apps/` 头；framework 零 RTOS include；逻辑层不碰 HAL 句柄/寄存器。

## 4. 记录

- 新 DEV：`DEV-YYYYMMDD-HHMMSS`（`scripts/dev_id.py --new`），兵种改动写 `.Doc/robots/<robot>/DEVLOG.md` §4，公共改动写根 `RM_DEVLOG.md` §6；
  历史条目在 `.Doc/history/开发史.md` 附录 I，不要重复搬回主台账。
- 被否决方案：根 `RM_DEVLOG.md` §3 DEC 台账，只追加；翻案新增条目并引用旧号。
- 未关闭问题：RM 编号；实体故障只有达到关闭条件才能关闭。
- 文档归属：共用 `spec/guide/plan`；兵种 `robots/<robot>/`；过期 `history/archive/`。
  规则见 `.Doc/spec/文档存放与管理规则.md`。

## 5. 证据

- 编译通过/主机测试通过**不构成**实体闭环；烧录/台架/整车结论由用户回填。
- 每条改动写清：命令、配置、结果、未执行项；调参必须带 CSV/日志路径。
- 不得把理论推演写成实测现象；未实测标 `[待实测]`。

## 6. 提醒义务（主动、简短、不重复）

| 场景 | 必须提醒 |
|---|---|
| 改动落在其他兵种目录 | 立即停手，确认是否切分支 |
| 要改 `components/framework/ports` | 走 `fix/shared-*` → `main` 回流 |
| 用户说"功能正常/验证通过" | 合并回兵种分支、删短分支、DEV 回填结论 |
| 提交前 | profile 编译 + 主机测试 + DEV 条目 + 推送 |
| 烧录前 | 核对 `firmware/<robot>.elf` 与 `current.txt` |
| 分支落后 `main` | 先 `git merge main` 下行同步 |
| 用户要求跨兵种 | 拆两个分支/两条 DEV；授权后执行并记录 |

同一事项本对话已提醒且用户已确认的，不再重复；提醒放回复末尾一行，不打断操作。
