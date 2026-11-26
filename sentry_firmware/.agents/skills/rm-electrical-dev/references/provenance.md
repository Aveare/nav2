# 作者、版本与引用来源声明

## 1. 文档身份

- **作者：金陵科技学院CR战队  Colin**——搭建 skill 骨架：任务入口/设计确认/架构规范/验证交付的工作流，采集跃鹿固定提交快照与 manifest，编写 vendor 采集脚本。
- **协作者：金陵科技学院CR战队  小猫大王**——更新功能：代码地图、基线审计与红旗检测、AI 协作约束、架构演进路线、主机测试体系与署名版本维护。

skill 名称、版本、版本日期与更新时间以 `SKILL.md` metadata 为唯一主记录。本版为工程规范整理稿，版本号不表示已经完成固件或整车验收。

本署名适用于新增工作流、设计确认规范、工程记录模板及采集脚本，不替代引用代码和架构的原作者、版权或许可声明。新增内容尚未单独指定开源许可证；上游许可不自动为新增内容或全部依赖授权。

## 2. 主代码基线

| 项目 | 记录 |
| --- | --- |
| 来源 | 湖南大学 RoboMaster 跃鹿战队 [HNUYueLuRM/basic_framework](https://github.com/HNUYueLuRM/basic_framework) |
| 仓库 URL | `https://github.com/HNUYueLuRM/basic_framework.git` |
| 固定提交 | [1a136eb1ed2f101110348d5a5cd182272eada38e](https://github.com/HNUYueLuRM/basic_framework/commit/1a136eb1ed2f101110348d5a5cd182272eada38e) |
| 提交时间 | 2026-09-07T13:32:05+08:00 |
| 采集信息 | 2026-10-10，采集时分支 master，原始 Git blob 字节经核验 |
| 随包目录 | `assets/yuelu-basic-framework/` |
| 完整性依据 | `assets/yuelu-basic-framework.manifest.json` 的文件集合、长度与 SHA-256 |
| 根目录许可 | MIT；Copyright (c) 2022 NeoZng，原文件随包保留 |

README 标注框架起源为 2022–2023 赛季。固定提交不代表当前赛季完整适配，也不随 master 自动更新。副本不包含作者个人工程的底盘改动；使用方式见 [代码基线管理](framework-baseline.md)。

## 3. 架构和接口引用

以下路径均属于上述固定提交：

| 引用资料 | 使用范围 |
| --- | --- |
| `.Doc/架构介绍与开发指南.md` | APP/Module/BSP 职责、实例/回调、命名、接口和模块文档 |
| `.Doc/必须做&禁止做.md` | 临界区、延时、竞争、接口检查、供电和警告约束 |
| `.Doc/VSCode+Ozone使用方法.md` | 构建/调试环境参考；按任务核对当前工具 |
| `.Doc/如何定位bug.md`、`.Doc/合理地进行PID参数整定.md` | 故障定位和调参方法；历史技术描述需复核 |
| `application/robot.c`、`robot_task.h`、`robot_def.h`、`Src/main.c` | 初始化、任务、配置及有效功能入口 |
| `modules/motor/DJImotor/`、`modules/message_center/`、`bsp/can/` | 设备控制、消息语义和 CAN 接口 |
| `CMakeLists.txt`、`Makefile`、`.ioc`、启动/链接文件 | MCU、ABI、宏、文件集合及依赖 |

归纳说明与上游原文分别维护。原文与当前实现冲突时核验声明、实现和调用者，并记录差异；保留原文不代表无条件采纳历史技术建议。

## 4. 第三方和外部资料

副本含 ST HAL/CMSIS、FreeRTOS、SEGGER RTT、DSP 等依赖。保留各自 LICENSE、版权头和原 README 致谢；未完成逐文件许可审计，不将整个副本视为单一 MIT 授权。

规则索引和战队一手案例的 URL、年份、读取范围及局限集中在 [兵种设计要求](robot-types.md)。除跃鹿基线外，未包含这些案例的代码。[历史底盘案例](jit-case.md) 是有限开发证据摘要，不构成已验收实现。

## 5. 新增组件登记（v0.8–v0.9）

v0.7 采集快照之后新增的组件均由协作者小猫大王主导，适用 §1 的署名与许可边界（未单独设开源许可证）：

| 组件 | 版本 | 性质 |
| --- | --- | --- |
| `references/code-map.md`、`baseline-diff.md`、`ai-constraints.md`、`evolution-roadmap.md` | v0.8–v0.9 | 规范文档；code-map 与 evolution-roadmap 绑定 upstream_commit，换提交须重核 |
| `scripts/audit_baseline.py`、`audit_baseline.ps1` | v0.8–v0.9 | 孪生脚本（逻辑必须一致，修改须双端同步）；含快照自检与红旗检测 |
| 工程侧 `AGENTS.md` 规范 | v0.9 | 内容规范在 ai-constraints §3，实际文件属各工程 |

这些组件引用的外部开源资料及核查日期见各文件内的来源小节（ai-constraints、evolution-roadmap、robot-types）。

## 6. 版本维护

skill 更新时修改 metadata 的版本、日期和实际时间。manifest 的 `skill_metadata` 保留采集当时版本；本版仅调整维护文档，沿用 0.6.1-draft 采集的同一 GitHub 快照，不重写采集历史。

skill 版本、接手 schema、上游提交、工程源码与板上固件分别记录。改变上游提交时重新采集、核验接口/依赖差异并更新来源；仅整理说明文档时不改动原始快照。
