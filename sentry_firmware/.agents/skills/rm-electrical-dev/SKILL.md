---
name: rm-electrical-dev
description: 开发、移植和维护 RoboMaster STM32 整车电控（本工程已从跃鹿 basic_framework 升级为自研框架 apps/components/framework/ports）。提供当前树符号级代码地图与调用链速查、上游基线审计与 AI 改动红旗检测、AI 协作约束（AGENTS.md/授权分级/否决台账）、主机测试与按工程升级方案的分批演进管理，覆盖底盘、云台、发射、通信、保护及兵种专用机构。
metadata:
  version: "1.0.0"
  version_date: "2026-11-22"
  updated_at: "2026-11-22T00:00:00+08:00"
  timezone: "Asia/Shanghai"
  author: "金陵科技学院CR战队  Colin"
  upstream_repository: "https://github.com/HNUYueLuRM/basic_framework"
  upstream_commit: "1a136eb1ed2f101110348d5a5cd182272eada38e"
---

# RoboMaster 整车电控开发规范

本 skill 用于建立可追溯的设计基线，并形成可供后续维护者接手的工程记录。**JIT-RM 1029 自 2026-11-22 起已从跃鹿 APP → Module → BSP → HAL 升级为自研框架 `apps → components → framework → ports → HAL`（B0a~B12 收口）**；上游快照与审计工具继续作为历史基线与 AI 越界检测依据。**Colin**（初始作者）搭建 skill 骨架并采集跃鹿快照基线；**小猫大王**（协作者）完成 v0.8 起的全部更新——代码地图、基线审计、AI 协作约束、演进路线与主机测试体系（明细见 [来源声明](references/provenance.md)）。版本与更新时间以 metadata 为准。

## 1. 任务入口

先识别工程根目录、任务类型、受影响模块和验收目标，再按下表执行。已有工程以实际代码、硬件和用户最新反馈为依据；随包示例参数仅作为待核对配置。

| 任务类型 | 首要工作 | 必要产出 |
| --- | --- | --- |
| 新建整车 | 确认兵种、赛事/赛季、机械拓扑和板间职责 | 基于模板的配置/实现、资源分配及已获授权的验证结果 |
| 移植或功能开发 | 接手现状，确认受影响硬件与接口 | 完整调用链、实现及兼容性说明、相关验证 |
| 缺陷修复 | 核对触发条件、源码/固件版本和故障证据 | 最小修复、复现与回归结果、问题状态更新 |
| 只读审查 | 对照文档与实现，定位风险及证据缺口 | 发现、依据与可执行建议；不强制修改工程记录 |
| 渐进演进 | 按工程内架构升级方案（如 `.Doc/plan/自主研发框架升级总方案.md`）的批次执行：单批授权→实施→闸门→审计→回填 DEV；[架构演进路线](references/evolution-roadmap.md) 已改为指针，不再承载批次 | 单批可编译交付物、DEV 条目、批次状态更新 |

**会话最小阅读集**（按需加载，避免全量通读）：
- 新功能/演进会话：本节 + [代码地图](references/code-map.md) + [代码规范](references/coding-contract.md) + 工程 `.Doc/plan/` 架构升级方案（如涉批；[演进路线](references/evolution-roadmap.md) 已改为指针）+ 工程 RM_DEVLOG 摘要与 DEC 台账
- 缺陷修复会话：本节 + 代码地图 + 工程 RM_DEVLOG（问题台账优先）
- 审计/接手会话：本节 + [基线审计](references/baseline-diff.md) + [AI 协作约束](references/ai-constraints.md) + [工程记录规范](references/development-handoff.md)
- 其余 references 按引用需要再读，不默认通读。

**先定位，再动手。** 功能开发与缺陷修复前，按 [代码地图](references/code-map.md) 找到对应功能在当前树中的文件、函数与调用链；升级前的上游对应物在 `assets/yuelu-basic-framework/`，当前工程与上游的差异是排查 AI 过度改动的第一手证据。接手、开工前或故障排查时按 [基线审计](references/baseline-diff.md) 用 `scripts/audit_baseline.py|ps1` 做只读比对（含红旗检测：逻辑层新增硬件句柄引用、新增 `while(1)`），判定哪些改动属于预期参数适配、哪些属于结构性偏离。

**AI 协作纪律。** 以 AI 为主要开发者时，按 [AI 协作约束](references/ai-constraints.md) 执行：工程根维护 `AGENTS.md`（Map/Boundaries/Checks/Evidence 四段式）；会话开始三步（读冷启动摘要→基线审计→diff 预期）、结束三步（DEV 条目+AI 标注→更新决策/否决台账→重跑审计）；授权分级——参数级可直接改，结构级（新增文件/函数/任务/消息字段、改调用链、改 components/framework/ports 接口）必须用户对该次操作明确批准。

接手按 [工程记录规范](references/development-handoff.md) 查找并阅读已有 Markdown，包括其他作者的记录。工程事实与资料冲突时按 [上下文管理](references/project-context.md) 核验，不将历史结论直接用作当前配置。

## 2. 设计基线

按 [设计确认规范](references/design-intake.md) 整理已知项，集中询问影响本次实现的缺项。新建整车必须明确兵种及实际构型；已有工程的局部任务仅补充受影响信息。

- 明确轮型、轮序、轴拓扑、板间职责及传感器处理端。四轮不代表麦克纳姆轮；双 yaw 必须确认轴间机械关系。
- 受影响执行器必须确认型号、电调/模式、所属板/总线、设备编号、控制与反馈 ID、反馈来源、方向及相关零位/限位。
- 分别标记用户确认、源码现状、实测、方案建议和待确认项。未确认项不得写成有效硬件绑定或已确定控制参数；可先完成不依赖该信息的接口、结构和构建工作。

兵种差异按 [兵种设计要求](references/robot-types.md) 查证；常用型号按 [硬件配置规范](references/hardware-profiles.md) 识别。比赛限值和协议字段必须核实适用赛事的官方文件、版本及补充公告。

## 3. 架构与实现

**本用户的长期原则：保留原工程的可读性、函数组织和中文注释；不得以“补齐保护”“优化架构”或“方便调试”为由另造一套控制流程。** 详细执行边界见[代码与注释规范](references/coding-contract.md#2-模板内适配优先)。

**2026-11-22 起本工程已执行自研框架升级（B0a~B12 收口）**：新代码按 `apps → components → framework → ports → HAL` 分层；“原模板内适配”只适用于功能轴与升级方案明确留待 M5 的未迁移区，结构轴一律以工程内升级方案为准（分层/消息/总线/监督体系的重构已在其中完成）。

- 新功能优先落在既有层与既有接口上：应用状态/装配进 `apps/<profile>_skeleton`，设备协议与控制器进 `components/`，平台无关能力进 `framework/`，外设落地进 `ports/stm32f4`。默认不新增平行应用或旁路全局状态。
- 复用 `framework/msg` 话题、`framework/kernel` 任务原语与实例注册制；禁止在 apps/components 直接调用 HAL/寄存器或 FreeRTOS。
- 参数适配不扩展为驱动重构；平台细节只允许出现在 `ports/`。

开发前阅读当前工程 `.Doc/spec/架构与开发指南.md`、`.Doc/spec/必须做&禁止做.md`、README 及相关组件说明。架构描述与符号定位的唯一事实源是 [代码地图](references/code-map.md)；上游对应物查 `assets/yuelu-basic-framework/`。新工程的基线使用方式见 [代码基线管理](references/framework-baseline.md)。

- `apps/` 负责应用状态、目标值与装配（`profile.h` 编译期 + robot_task 任务表）；`components/` 放设备协议/控制器/组件算法；`framework/` 放平台无关的 kernel/err/mem/msg/diag（**零 RTOS include**）；`ports/` 落 STM32F4 外设与 FreeRTOS 后端。
- 任务创建/延时/临界区一律走 `framework/kernel`；修改接口必须同步声明、实现、调用者、数据结构、协议两端及构建配置。
- 只有任务本身要求驱动开发时才调整相应层，保留生成代码的 `USER CODE` 区域；禁止在 components/framework 里 include HAL/RTOS 头。
- 核对既有启动、无反馈、失联、限位及恢复行为并记录缺口；新增保护须属于当前任务范围，不能据此自动引入状态机。零速度参考不等同于零输出。

功能集成及故障定位按 [模块实施与验收](references/module-workflows.md) 执行；接口和注释按 [代码与文档规范](references/coding-contract.md) 编写。需要架构级升级（分层/消息/总线/监督体系重构）时，按工程内已批准的升级方案（如 `.Doc/plan/自主研发框架升级总方案.md`）的分批计划推进：每批独立可交付、动工前确认该批授权；[架构演进路线](references/evolution-roadmap.md) 已改为指针文件，不再承载批次。

## 4. 验证与交付

默认只修改源码、文档和必要配置，由用户自行编译和烧录。没有用户针对本次操作的明确授权，不执行构建、编译（含语法编译）、链接或生成测试可执行文件，不新建、复制或归档ELF/HEX/BIN。允许只读检查已有产物，以及不调用编译器的静态/数学检查。以下验收项表示分层证据要求，不构成执行这些操作的授权。


按任务影响范围选择检查，分别记录静态检查、完整 ARM 编译链接、烧录、台架和整车验证的实际状态。每项证据应包含版本、配置、方法与结果；未执行项明确注明。源文件存在、主机测试或编译通过均不能代替机构功能验收。基线审计（`scripts/audit_baseline.ps1`，只读）属于允许的静态检查，可作为"改动范围与本次目标一致"的证据，但不能代替上述任何一层硬件验证。

每次实际开发或修改完成、中断前，按 [工程记录规范](references/development-handoff.md) 更新对应工程的主记录：功能变化、文件/符号、调用接口、配置影响、验证、遗留问题和下一步。没有合适记录时使用 [开发记录模板](assets/RM_DEVLOG.template.md)。已有记录沿用原路径、作者与问题编号。

交付说明应包含完成项、接口/配置影响、验证结论、待确认项、遗留问题及主记录位置。实体故障只有达到其关闭条件后才能标记已解决。

## 5. 参考资源

随包 `assets/yuelu-basic-framework/` 为 metadata 指定 GitHub 提交的原始副本；文件身份由 manifest 核验，既有工程采用差异集成。采集与使用边界见 [代码基线管理](references/framework-baseline.md)。

[跃鹿代码地图](references/code-map.md) 是固定提交的符号级定位索引（症状 → 文件/函数、调用链速查、上游已知陷阱）；[基线审计](references/baseline-diff.md) 与 [audit_baseline.py|ps1](scripts/audit_baseline.py) 用于只读比对工程与基线、识别 AI 过度改动与新增红旗；[架构演进路线](references/evolution-roadmap.md) 已改为指向工程内升级方案的指针（2026-11-22，不再承载批次）。以上游提交为准，提交变更时同步核对更新。

[历史底盘案例](references/jit-case.md) 仅在相关故障接手时查阅，不提供新车默认参数。
