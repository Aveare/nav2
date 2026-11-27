# RM 工程开发记录（公共台账）

> **本文件只放"公共"内容**：工程身份、分支/编号规则、构建、公共决策与公共 DEV 条目。
> **兵种专属状态与条目在各兵种台账**：`.Doc/robots/<robot>/DEVLOG.md`（infantry / sentry / heavy）。
> 所有标题与表格行都带类型标记：`[公共]` / `[INFANTRY]` / `[SENTRY]` / `[HEAVY]`，便于 grep 与并行开发。
> 历史 218 条 DEV 在 `.Doc/history/开发史.md` 附录 I；本文件只保留近期公共条目。
> **并行规则**：做步兵只改 `robots/infantry/DEVLOG.md`，做哨兵只改 `robots/sentry/DEVLOG.md`；
> 只有公共改动才动本文件。守卫会按 `.Doc/robots/<robot>/` 归属拦截跨兵种编辑。

## 0. AI 冷启动摘要

| 项 | 类型 | 内容 |
| --- | --- | --- |
| 工程身份 | [公共] | STM32F407 + FreeRTOS 多兵种电控；共享框架 `apps → components → framework → ports → HAL`；当前交付兵种看对应兵种台账的状态行 |
| 分支模型 | [公共] | `main` 共享主干；`infantry`/`sentry`/`heavy` 长驻兵种分支；多人并行每人一条 `<type>/<robot>/<功能>-<用户>` 短分支，用户确认后合并删除 |
| DEV 编号 | [公共] | `DEV-YYYYMMDD-HHMMSS`（本地时间到秒）；旧 `-NN` 不改写 |
| 基线身份 | [公共] | 已完全脱离上游（2026-11-27）：无 yuelu 快照、无 audit_baseline、无 check_plan；字节事实源 = git，结构基线 = tag `arch-after-U3U4` + g4 manifest |
| 兵种状态 | [INFANTRY] | 见 `.Doc/robots/infantry/DEVLOG.md` §0 |
| 兵种状态 | [SENTRY] | 见 `.Doc/robots/sentry/DEVLOG.md` §0 |
| 兵种状态 | [HEAVY] | 见 `.Doc/robots/heavy/DEVLOG.md` §0 |
| 最近验证 | [公共] | 2026-11-27 文档重构：md 156→82、上游与审计退役、兵种分库、skill 2.0；未跑固件编译（无 C/H 行为改动） |
| 当前问题 | [公共] | 无公共未关闭问题；兵种问题在各兵种台账（步兵 RM-002 挂起） |
| 下一步 | [公共] | 结构轴 M3 整车回归 → M3′；各兵种按自己台账的"下一步"推进；导航 VCP 协议与哨兵功率接入待兵种负责人排期 |
| 文档入口 | [公共] | A 层 5 份见 `.Doc/spec/文档阅读分级.md` §1；规则见 `.Doc/spec/文档存放与管理规则.md` |

## 1. 公共状态

| 项目 | 内容 |
| --- | --- |
| 有效板型 | 单板 `ONE_BOARD`；双板分支未实装（DEC-009） |
| 有效配置 | 构建所选 profile 的 `config/<robot>_2026/config.h`（伞头 + `config_<域>.h`） |
| 共享组件状态 | `components/README.md`（ACTIVE/OPTIONAL/DORMANT） |
| 板上固件身份 | 由各兵种烧录后回填到对应兵种台账 |
| 最近公共条目 | 见 §6（更早见开发史附录 I） |

## 2. 公共设计与配置约定

- 分层与依赖方向：`.Doc/spec/架构与开发指南.md`；纪律：`.Doc/spec/必须做&禁止做.md`。
- 参数只落 `config/<robot>_2026/`；协议合同在 `apps/<robot>/app/<robot>_def.h`（带布局守卫）。
- 兵种设计/参数/操作在 `.Doc/robots/<robot>/`；本文件不写某一兵种的具体数值。
- 文档三层存放：`spec/guide/plan`（共用）、`robots/<robot>/`（兵种）、`history/archive/`（归档）。

## 3. 公共问题与决策

### 3.1 问题索引（[公共]）

| ID | 归属 | 状态 |
| --- | --- | --- |
| RM-002 | [INFANTRY] | 挂起（发射机构硬件）；详情与关闭条件见 `.Doc/robots/infantry/DEVLOG.md` |
| RM-003~012 | [INFANTRY] | 全部关闭；证据见开发史附录 I |

> 兵种新问题开在该兵种 DEVLOG；只有"影响 ≥2 兵种或共享层"的问题才登记在本表。

### 3.2 决策与否决台账(DEC-NN)

<!-- 只追加不删除;翻案新增条目并引用旧编号。每条必须标类型：公共/兵种。 -->

| 编号 | 类型 | 决策/被否方案 | 理由 | 日期 |
| --- | --- | --- | --- | --- |
| DEC-001 | [公共] | 架构与配置决策：APP→Module→BSP→HAL；参数收敛 config；协议合同独立；记录体系用冷启动摘要+DEV/DEC/RM | 换车/换赛季不改业务代码（分层已被 DEC-011/升级取代，仅存史） | 2026-10-29 |
| DEC-002 | [INFANTRY] | 小陀螺采用恒定自转速度，否决"瞄准时自转变速" | 用户体感否决（"太狠"） | 2026-10-29 |
| DEC-003 | [公共] | 基线审计以本工程 manifest 为基线 | 字节历史由 git 承担（已废止，见 DEC-017） | 2026-10-30 |
| DEC-004 | [INFANTRY] | pitch 手感三轮实验全回滚至 Gyro[0] 反馈 | 无遥测支撑的盲改无效；后续先取波形 | 2026-10-30 |
| DEC-005 | [公共] | 热量管理采用"估计器+前馈射速"，否决"裁判热量直连射速" | 对象死区 + 膛内余弹导致反馈必过冲 | 2026-11-19 |
| DEC-006 | [公共] | 底盘 VOFA 临时占用 USART1，AX650 链路停用 | 无空闲串口；标定需要 `[chcal]` 功率/缓冲能量 | 2026-11-20 |
| DEC-007 | [公共] | AX650 的 USART1 所有权与下行解析拆两级开关 | 允许只保留姿态上报；非法组合 `#error` | 2026-11-22 |
| DEC-008 | [公共] | 每次代码/config/构建改动必须同步 Markdown 并提交推送 | 文档与 Git 同步是交付动作；推送失败须如实报告 | 2026-11-22 |
| DEC-009 | [公共] | 板型运行期化不做 | 无双板硬件/零消费者；保持编译期互斥守卫 | 2026-11-22 |
| DEC-010 | [公共] | daemon 落 `components/daemon/` | 依赖 log/dwt/buzzer，进 framework 会破坏"零平台 include" | 2026-11-22 |
| DEC-011 | [公共] | 命名统一 snake_case + 模块前缀 + `module_verb_object` | 避免混写导致 grep 失效 | 2026-11-23 |
| DEC-012 | [公共] | config 子头统一 `config_<域>.h` 前缀，跨域 `#if` 上移伞头 | 扁平化后同名头由 include 顺序决定语义 | 2026-11-23 |
| DEC-013 | [公共] | 旧视觉栈 `master_process.*` + `seasky_protocol.*` 整组删除 | 零调用者；只保留被引用类型到 robot_def.h | 2026-11-23 |
| DEC-014 | [公共] | DM/HT/servo、super_cap、can_comm 保留 OPTIONAL/DORMANT | 下赛季可能换硬件；在 components/README 登记 | 2026-11-23 |
| DEC-015 | [INFANTRY] | `YAW_SPEED_KP_HIGH` 维持 40，电流模式回落 20 搁置 | 域换算推演未经实测，先完成自检再议 | 2026-11-23 |
| DEC-016 | [INFANTRY] | 放弃"平移免疫 + ss 只压自转"，改总 power scale 作用于四轮电流，floor 0.20 | 20 Hz 实测：只有总电流 scale 能全工况 ≤85 W | 2026-11-26 |
| DEC-017 | [公共] | 彻底脱离上游：删 yuelu 快照、`audit_baseline`、`check_plan`；基线改 git + g4 manifest | 上游对照已无消费者；历史证据在开发史 | 2026-11-27 |
| DEC-018 | [公共] | 文档单入口 + 阅读分级 + 历史唯一归档地（开发史） | 活跃文档压缩；A 层 5 份每对话必读 | 2026-11-27 |
| DEC-019 | [公共] | **DEVLOG 按兵种分库**：根台账只留公共，兵种台账在 `robots/<robot>/DEVLOG.md`；所有标题/行带类型标记 | 并行开发不再改同一文件，避免互相冲突；grep 可筛选 | 2026-11-27 |

## 4. 构建与恢复（[公共]）

- 工具链：arm-none-eabi-gcc 15.3.1（`C:/Toolchain/bin`）+ Ninja + CMake；主机测试 gcc（mingw64）。不要混用 PATH 上的 14.2。
- 构建：`.\build.ps1 <robot> -Current`（`sentry` 生成 `firmware/sentry.elf`）；等价 `cmake --preset <robot> && cmake --build --preset <robot>`。
- 烧录：`tools/flash_manual.ps1`（按 `firmware/current.txt` 选兵种）或 EmberProbe `mcu-flash`；日志 `capture_rtt.bat` / `JlinkRTTClient`。
- 恢复顺序：本文件 §0 → 对应兵种 DEVLOG §0 → 开发史附录 I → 源码/config → 守卫与测试 → 板上固件核验。

## 5. 资料与设计依据（[公共]）

- 架构：`.Doc/spec/架构与开发指南.md`；纪律：`.Doc/spec/必须做&禁止做.md`。
- 调参方法：`.Doc/guide/调参诊断手册.md`；验证：`.Doc/guide/调试与测试流水线.md`。
- 兵种参数：`.Doc/robots/<robot>/params/`；兵种设计/操作：`.Doc/robots/<robot>/{design,ops}/`。
- 历史：`.Doc/history/开发史.md`（218 条 DEV 原档 + 归档索引）；旧文档原文：`.Doc/history/archive/`。

## 6. 公共开发条目

<!-- 只放 [公共] 条目；兵种条目写各自 DEVLOG。格式：### DEV-…｜日期｜[公共] 标题 -->

### DEV-20261128-042039｜2026-11-28｜[公共] RMCV `0x1022` 完整姿态字节打包

**触发**。哨兵 NUC 端已经按统一 USB 协议解析 `0xA5/0x1022`，但电控共享 RMCV 编解码层仍只支持旧 `0x1021`，无法上送 IMU 四元数、三轴关节角、独立采样时间与有效位。

**实现**。`components/ax650/ax650_pose.h/.c` 新增 61 B `ax650_full_pose_pack()`：payload 固定 52 B，命令号 `0x1022`；逐字段小端编码，四元数归一化，拒绝 NaN/Inf、负弹速和保留有效位；旧 `0x1021` 与 `0x0503` 接口不变。新增 10 位有效标志定义，覆盖 IMU、三轴、标定、弹速、robot_id、自瞄允许与执行器在线状态。

**验证**。`tests/ax650_pose_test.c` 增加 NUC Python `FullPoseFrame` 生成的 61 B 跨语言黄金向量、CRC、字段偏移、非法标志、NaN 和短缓冲测试；`run_ax650_tests.ps1` 的 `-O0/-Ofast` 全部 PASS。兵种装配与 ARM 构建在对应哨兵 DEV 条目记录。

**未执行**。共享层不直接烧录；实体 USB、姿态方向和时序由哨兵分支台架确认。

**AI**：OpenAI Codex（GPT-5），2026-11-28。

### DEV-20261127-203159｜2026-11-27｜[公共] 采集闸门 capture_guard + EmberProbe 烧录技能更新

**触发**。2026-11-27 实测定性：EmberProbe 的 ELF 固定指向 `firmware/current.elf`，会被任意兵种 `build.ps1 -Current` 覆盖；采集按错地址解析变量、探针失败还重试 24 遍。用户指示随本次一并推送，本轮补记。

**改动**。
- 新增 `tools/lib/capture_guard.js`：采集前把 ELF 钉到 `firmware/<robot>.elf`、校验 `current.txt` 记录的 SHA、用一次真实读变量核对 `flashedSha256 == elfSha256`，并加 `build/.capture.lock` 单进程互斥。
- `tools/chassis_trace.js`：接入 guard。
- `.agents/skills/_emberprobe/*` 与 `mcu-*/.emberprobe-skill.json`：EmberProbe 技能更新（OpenOCD 路径解析、目标检测、烧录流程）。

**验证**。JS 工具不在工程编译链内，本轮未执行 RTT/烧录；`build.ps1 sentry` 编译通过。采集闸门的实车验证待下次台架。

**AI**。工作区既有改动；本轮由 DeepSeek-V4.1-Flash 补记。

### DEV-20261127-000119｜2026-11-27｜[公共] 全仓文档重构：脱离上游、归档历史、兵种分库、Skill 收编

**背景**。用户要求单开临时分支，重写必要的 md；开发记录/归档并入开发史；失效上游 md 删除；
skill 参考开发史收编。执行中按用户新指示收敛为"共用不动、兵种分库、哨兵只隔离"。

**交付**。
1. 脱离上游：删 `assets/yuelu-basic-framework/`（460 文件）、manifest、`audit_baseline`、`vendor_framework`、`tools/check_plan.py`；`refactor_gate.py` 移除 G4 与 `--expect-audit`。
2. 历史：218 条 DEV 全文并入 `.Doc/history/开发史.md` 附录 I（按编号排序、重复编号加注）；新增第十六/十七章；主台账瘦身。
3. 分库：新增 `.Doc/spec/文档存放与管理规则.md`；兵种专属文档移入 `.Doc/robots/{infantry,sentry,heavy}/`；守卫识别 `.Doc/robots/<robot>/`。
4. 归档：旧 plan/review/detail/guide/msg-semantic-map → `.Doc/history/archive/`；删除 `.workbuddy`、`.zcode` 工件。
5. Skill：`rm-electrical-dev` references 15→5，新增 `dev_id/verify/power_calib`；`mcu-*` 不动。
6. 规模：md 156→79（后续随 DEVLOG 分库增至 82）。

**验证**。`check-links` PASS；`check_robot_scope --gate` PASS；`check_tags` 条目可检索（标签链缺失为既有问题）；新脚本 `py_compile` 通过；无固件改动，未跑 ARM/主机测试与烧录。

**AI**：DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-003347｜2026-11-27｜[公共] DEVLOG 分兵种：根台账只留公共 + 三个兵种台账

**背景**。用户指出：步兵与哨兵并行开发时，共用一份 `RM_DEVLOG.md` 会互相干扰（同文件追加/表格行冲突，
也难分辨某条状态属于哪个兵种）。要求"要么分类存放，要么加类型标识"。

**方案（两者同时做）**。
- **物理分库**：根 `RM_DEVLOG.md` 只留公共内容；新增
  `.Doc/robots/infantry/DEVLOG.md`、`.Doc/robots/sentry/DEVLOG.md`、`.Doc/robots/heavy/DEVLOG.md`。
- **类型标记**：所有 DEV 标题与台账行统一 `[公共]/[INFANTRY]/[SENTRY]/[HEAVY]` 前缀；
  DEC 台账新增"类型"列；问题表只有"影响 ≥2 兵种"才进公共区。
- **归属与守卫**：`.Doc/robots/<robot>/DEVLOG.md` 归该兵种分支；`check_robot_scope.py` 已按目录判定，
  在哨兵分支改步兵台账会 FAIL。
- **工具**：`check_tags.py` 与 skill `dev_id.py` 的检索范围扩到三个兵种台账，避免编号/批次漏检。

**迁移**。哨兵状态/路由/参数入口 → `robots/sentry/DEVLOG.md`；RM-002 与步兵闭项 → `robots/infantry/DEVLOG.md`；
重装骨架 → `robots/heavy/DEVLOG.md`；公共构建/分支/决策留在根台账（DEC-019 记录本决策）。

**验证**。`check-links` PASS；`check_robot_scope --gate` PASS；`check_tags --batch B1` 仍能检索条目；
md 总数 82（工程 59 + skill 6 + 插件 17），A/B/C/D/E = 5/21/39/17/0。

**未执行**：无固件改动，未编译/烧录。

**AI**：DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-004648｜2026-11-27｜[公共] 短分支合并后即删 + 分支清理与找回规则

**背景**。用户要求：短时分支在用户确认完成、并入 `main` 后立即删除，避免分支数量膨胀；
并确认删除后的可恢复性。

**决策/规则**。
- 长驻分支只有 4 条：`main` + `infantry`/`sentry`/`heavy`；其余一律短命。
- 短分支流程：用户确认 → `--no-ff` 合入兵种分支 → 兵种分支验证后合入 `main` → **删除本地+远端短分支**；
  共享/文档短分支直接合入 `main` 后即删。
- 分支类型补 `docs`/`chore`；共享/文档短分支从 `main` 切，兵种功能从兵种分支切。
- 新增 §3.5 清理与找回：收工跑 `git branch -vv` / `git branch --merged main` / `git fetch --prune`；
  已合并的找回味道路径为合并点第二父（`<merge>^2`）、reflog 90 天、GitHub Recently deleted；
  未合并分支不直接删，必要时先打 `rescue/*` 标签。

**交付**。`.Doc/spec/兵种分支协作规范.md` §3.4/新增 §3.5；`AGENTS.md` 分支纪律同步。
本次已按新规删除 `docs/rewrite-md-youyucat`（本地+远端，合并点 `8529ce4^2` 可找回）。

**验证**。`check-links` PASS；`check_robot_scope --gate` PASS；远端仅剩 `main`/`infantry`/`sentry`/`heavy`。

**未执行**：无固件改动，未编译/烧录。

**AI**：DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-005636｜2026-11-27｜[公共] 工程更名：JIT-RM 1029 → RM-JIT-CR

**背景**。用户要求删除 "1029" 这个工程名，统一改为仓库名 **RM-JIT-CR**。

**范围（用户确认）**。① 活跃文档与固件版本串改新名；② 历史文档保留旧名，只在开发史加更名说明
（历史文件名如 `jit-rm-1029.manifest.json`、`JIT-RM 1029.rar` 是当时事实，不改）。

**改动**。
1. `README.md` 标题、`AGENTS.md` 工程行、skill `SKILL.md` 服务对象 → `RM-JIT-CR`。
2. `framework/kernel/kernel_version.c`：`"fw 0.1.0-b1 (jit-rm-1029)"` → `"fw 0.1.0-b1 (rm-jit-cr)"`。
3. `.Doc/history/开发史.md` 序前加"更名说明"，正文旧名与历史文件名保留。
4. 机器基线 `files.sha256` 里的 "1029" 是哈希子串，未动。

**验证**。
- 活跃区 `git grep -i "jit-rm 1029|jit-rm-1029"`（排除 history/gate）→ 0 命中。
- ARM 构建：`build.ps1 sentry -Current` 成功；text **117,284 B** / data 684 / bss 128,240；
  RAM 56.44% / CCM 83.84% / FLASH 11.25%。0 error。
- `check-links` / `check_robot_scope --gate` PASS。

**影响与注意**。版本串缩短 2 字符，`.rodata` 与 ELF 大小有微小变化（行为不变）；
下一批若跑 `refactor_gate.py check`，G5 需用 `--allow-linkage` 声明该尺寸变化，或先 `save-baseline` 重锚。

**未执行**：未烧录；未跑主机测试（仅字符串改动）；未台架。

**AI**：DeepSeek-V4.1-Flash,2026-11-27。
