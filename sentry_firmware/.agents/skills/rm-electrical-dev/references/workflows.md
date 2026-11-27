# 工作流：接入 / 调参 / 验证 / 交付

> 本文件是"怎么做"的操作顺序；规则以 `AGENTS.md` 与 `.Doc/spec/` 为准。
> 所有命令在工程根执行；`python` 指本机可用的 Python 3。

## 1. 接入一个新组件/模块

1. **确认归属层**：设备协议/控制器 → `components/`；平台无关能力 → `framework/`；外设 → `ports/`；
   兵种专用 → `apps/<robot>/`。组件不得 include `apps/` 头。
2. **登记**：在 `components/README.md` 写状态（ACTIVE/OPTIONAL/DORMANT）与接入条件；
   兵种专用模块写进 `.Doc/robots/<robot>/README.md`。
3. **参数**：只 include 当前 profile 的 `config.h` 伞头；新增宏写进对应 `config_<域>.h`，
   并带证据等级注释；跨子头 `#if` 上移伞头。
4. **构建**：CMake 是 GLOB 自动收集；新增 `.c` 若属步兵路径，同步 Makefile 并在
   `check_source_lists.py` 下 PASS。
5. **测试**：纯逻辑补主机测试（`tests/`），接入对应 `run_*.ps1`；不可主机测试的写明理由。
6. **闸门**：`check_naming.py`、`check_macros.py`、`check_comment_style.py`、`check_sections.py`。
7. **交付**：DEV 条目 + 编译/测试输出 + commit/推送；未烧录写明。

## 2. 调参与标定

1. **先看证据表**：`.Doc/robots/<robot>/params/参数证据表.md` + `调参结论.md`；
   确认参数当前等级（`[实测]/[理论]/[待实测]`）。
2. **选预设采数**：`node tools/pitch_trace.js --preset <preset>`（预设见 `tools/trace_presets.json`）；
   预设引用的符号由 `check_symbol_contract.py` 守卫。
3. **分析**：`python tools/analyze_var_trace.py <csv> --all`（振荡/静止/慢漂三态）；
   电机速度环初值用 `python tools/motor_calib.py <csv> ...`。
4. **一次只改一个变量**，写清期望现象与判据；改完保留 CSV 路径。
5. **功率标定整链**：
   `python .agents/skills/rm-electrical-dev/scripts/power_calib.py --cond spin300 --seconds 20`
   （内部：RTT 采点 → `rtt_pwr_to_csv.py` → `fit_power_model.py`）。
6. **回填**：实车结论当天写进 `robots/<robot>/params/` 与 DEV 条目；未上车标 `[待实测]`。

## 3. 验证矩阵（按改动范围选）

| 层级 | 命令 | 判据 |
|---|---|---|
| 静态 | `check_robot_scope.py` / `check_naming.py` / `check_macros.py` / `check_comment_style.py` | 0 违规 |
| 链接 | `python tools/refactor_gate.py check-links` | PASS |
| 主机测试 | `tests/run_fwk_tests.ps1` + 对应域套件 | exit 0 / 全 PASS |
| profile | `tests/run_profile_tests.ps1` | 三兵种可编译 |
| ARM | `.\build.ps1 <robot> -Current` | 0 error / 第一方 0 warning |
| 红旗 | `g4_baseline.py check --baseline .Doc/gate/manifest-arch-after-U3U4` | 无新增红旗 |
| 实体 | 烧录 → 台架 → 整车（用户执行） | 只有实体确认才闭环 |

一条命令跑前三项：`python .agents/skills/rm-electrical-dev/scripts/verify.py`。

## 4. 交付与记录

1. **DEV 编号**：`python .agents/skills/rm-electrical-dev/scripts/dev_id.py --new`；
   兵种改动写 `.Doc/robots/<robot>/DEVLOG.md` §4，公共改动写根 `RM_DEVLOG.md` §6；
   标题 `### DEV-YYYYMMDD-HHMMSS｜日期｜[兵种/公共] 标题`。
2. **内容**：背景、变更位置（文件/符号）、接口与配置影响、验证证据（命令/输出/未执行项）、
   下一步；AI 参与注明模型名。
3. **决策**：被否决方案写根 `RM_DEVLOG.md` §3 DEC 台账（只追加，翻案引用旧号）。
4. **文档**：共用改动进 `spec/guide`；兵种改动进 `robots/<robot>/`；过期文档进
   `history/archive/`（先 `git log --diff-filter=D` 可追溯）。
5. **提交**：`<robot>(<scope>): <说明> (DEV-编号)`；只暂存本次文件；推送后核对
   `git status -sb` 与 `git rev-list --left-right --count HEAD...@{upstream}`。
6. **升级批**（如仍在做结构批）：`refactor_gate.py save-baseline` → 改 → `check`；
   G4 已移除，宏/编译/测试/符号/搬家/R1 仍需 PASS。

## 5. 常见回退

| 情况 | 动作 |
|---|---|
| 参数改坏 | `git revert` 或恢复 config 单文件；重新采数确认 |
| 结构批失败 | 回退到该批开工 tag；按 DEV 条目记录原因 |
| 文档误删 | `git log --diff-filter=D` 找提交 → `git checkout <sha>~1 -- <path>` |
| 短分支未合并要放弃 | 删本地/远端分支；DEV 条目注明"未采纳" |
