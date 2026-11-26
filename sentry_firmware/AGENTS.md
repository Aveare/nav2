# AGENTS.md — AI 协作契约(工程根)

> 本目录现为 `Aveare/nav2` 受管副本的一部分。用户已要求每轮源码改动同步更新 Markdown、
> 提交并推送到该仓库；不得向来源仓库推送。原始下载目录只读保留。
> 仅保留哨兵 CMake 构建，不使用上游步兵 Makefile。允许主机测试与 ARM 编译；
> 未经用户另行明确授权，不烧录、不接管实车运动或发射机构。

> 按 agents.md 标准四段式维护,保持简短、可执行、随赛季更新(过期的规则比没有更糟)。
> 每条规则要么改变一个动作、要么产生可检查的证据,否则删掉。
> 规范依据:skill rm-electrical-dev(随仓库 `.agents/skills/rm-electrical-dev/`,其 references/ai-constraints.md 为唯一依据)。

## Map(工程地图)

- 工程:JIT-RM 1029,2026 步兵,单板 STM32F407IG + FreeRTOS。**自研框架分层 `apps → components → framework → ports → HAL`**(2026-11-22 升级 B0a~B12 收口,DEV-20261122-21/22/23);`application/`、`modules/`、`bsp/` 已删除,历史条目里的这些路径按当时语义理解。
- **执行入口(两轴)**:**结构轴** = `.Doc/plan/自主研发框架升级总方案.md`(v2.6.1,状态 C 升级期;B0a~B12 已收口,剩 M3 实车回归 + M3′ 结项;**每批开工前需用户对该批明确批准**);**功能轴** = `.Doc/plan/总计划书.md`(**冻结**,解冻 = 升级结项,或用户书面插队并记"升级暂停/恢复")。
- 重构总方案已结项(2026-11-20),只作历史;各轴计划书(yaw/pitch/底盘/发射)只作技术细节库。
- 代码地图:`.agents/skills/rm-electrical-dev/references/code-map.md`(当前树症状→文件/函数定位,唯一事实源)。
- 参数唯一改动点:`config/infantry_2026/config.h`(伞头 + `config_<域>.h` 子头);协议合同:`apps/infantry/app/robot_def.h`(有布局守卫)。
- 装配层:三骨架 `apps/{infantry,heavy,sentry}/app/`;heavy/sentry 为参数锚点,ARM 构建仅 infantry(CMake `ROBOT_PROFILE`,默认 infantry);编译期开关只允许出现在 `profile.h`/config。
- 现行基线:最新收工 tag(`arch-after-U3U4`)+ `.Doc/gate/manifest-arch-after-U3U4/`;`arch-before-M3p` 及其 manifest 作为 M3 前历史基线保留;上游快照 manifest(`assets/yuelu-basic-framework.manifest.json`,@1a136eb1,460 条)只作历史审计对照。
- 历史调参结论与踩坑:`.Doc/spec/本车调参结论.md`(改参数前必读)。
- 注释规范:`.Doc/spec/注释规范.md`(写/改注释前必读);命名规范:`.Doc/spec/命名规范.md`(新增/改名/加宏前必读);
- 参数证据等级:`.Doc/spec/参数证据表.md`(标 `[实测]/[理论]/[待实测]`,未实测不得写调参现象)。

## Before editing(会话开始三步)

1. 读 `RM_DEVLOG.md` §0 AI 冷启动摘要(只读这一节即可接手);
2. 跑基线审计:`python .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .`(只读;`--skill` 默认取脚本上级目录,无需显式传;Python 无 `python` 时用本机绝对路径,见下方 Checks);
3. 对受影响文件核对审计结果与预期,发现未记录改动先问人,不要在未知状态上继续堆叠。

## Boundaries(禁区)

- 禁改:`Drivers/`、`Middlewares/`(第三方);`Src/Inc/` 仅 USER CODE 区;`.ioc` 需用户明确批准。
- 结构级变更需用户对该次操作明确批准:新增/删除 C/H、新函数/任务/消息字段、改调用链、改 components/framework/ports 接口、改构建文件。参数级(config 数值/PID/注释)可直接改。
- 逻辑层(apps/components/framework)禁触硬件句柄 `hcan*/husart*/hspi*/hi2c*/htim*`,一律经实例注入。
- 逻辑层禁 `HAL_GetTick`(时基走 framework 时间原语 `kernel_tick_ms`/`kernel_time_us`;ports 启动期/桩路径豁免,RM-009)。
- **句柄口径**:Init 时把 `&hcan*/&huart*` 注入模块/端口配置 = 允许(依赖注入,实例注册制);在 Task/控制函数里直接访问句柄或寄存器 = 禁止。
- **依赖方向**:`components/`、`framework/`、`ports/` 不得 include `apps/` 头;组件需要参数只 include `config/infantry_2026/config.h`;**framework 零 RTOS include**(FreeRTOS 后端唯一落点 `ports/freertos`);ports 只经 HAL/寄存器落地平台。
- 任务创建/延时/临界区一律走 `framework/kernel` 接口;禁止在 apps/components 里直接调 FreeRTOS/CMSIS-RTOS。
- 运行期错误禁止 `while(1)` 挂死:走 `err_report` + 安全态(启动期 fail-fast 例外 = `.Doc/gate/redflags` 已裁决清单)。
- 单写者:每个消息话题/共享结构体只有一个生产任务。
- 任务启停只用 `robot_task.h` 的 `ROBOT_TASK_*_ENABLE` 宏或 profile 装配,禁止注释启停。
- 未授权不烧录、不动实车;主机测试与 ARM 编译已获授权可直接跑。
- 新增 .c 后 Makefile 必须同步登记(CMake 是 GLOB 自动收集)。
- 每次修改源码/config/构建文件，必须同步更新相关 Markdown（至少包含 `RM_DEVLOG.md`），并提交、推送当前跟踪分支；推送失败须明确报告，不得称已同步。

## Checks + Evidence(检查与证据)

```powershell
# 基线审计(只读;上游快照对照,计数漂移属预期,红旗以 g4 执行器为准)
python .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .
# 本机无 `python` 别名时用绝对路径(本会话实测可用):
# "C:/Users/ZhuanZ/.workbuddy/binaries/python/versions/3.13.12/python.exe" .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .
# PowerShell 孪生版(等价输出)
powershell -ExecutionPolicy Bypass -File .agents/skills/rm-electrical-dev/scripts/audit_baseline.ps1 -ProjectRoot .
# 红旗权威机制 = tools/g4_baseline.py check(--baseline .Doc/gate/manifest-*) + .Doc/gate/redflags

# 主机单元测试(chassis/fwk/ax650/profile 矩阵;fwk 聚合整链 700 拍金标准)
powershell -ExecutionPolicy Bypass -File tests\run_chassis_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_fwk_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_ax650_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_profile_tests.ps1

# ARM 全量编译(CMake+Ninja)
# 本机实测工具链:arm-none-eabi-gcc 15.3.1 @ C:/Toolchain/bin(build/CMakeCache 记录的就是它);
# PATH 上另有一版 14.2.1,两者编出的 text 差约 600 B,不要混用。
# cmake 不在 PATH,用 STM32Cube bundle:
# C:/Users/ZhuanZ/AppData/Local/stm32cube/bundles/cmake/4.3.1+st.1/bin/cmake.exe
cmake -G Ninja -B build && ninja -C build    # 期望 0 error,第一方 0 warning
powershell -ExecutionPolicy Bypass -File build.ps1 sentry -Current   # 兵种构建 + 固定烧录副本 firmware/current.elf

# 零行为闸门(升级期每批必过;判据见升级方案 §5)
python tools/refactor_gate.py save-baseline   # 只在每批开工前重锚一次
python tools/refactor_gate.py check           # G1 宏/G1a 版本/G2 编译/G3 测试/G4 审计/G5 符号
python tools/refactor_gate.py check-links     # .Doc/ 引用路径存在性
python tools/check_source_lists.py            # Makefile 与磁盘 .c 一致(G10)
python tools/check_sections.py                # G5c 段占用(CCM≤90%/RAM≤70%)
python tools/check_symbol_contract.py         # T13 工具符号契约
python tools/check_naming.py                  # 命名规范(文件/函数/类型/宏/变量)
python tools/check_macros.py                  # 宏账本/跨子头 #if
python tools/check_macro_golden.py            # G1b 宏守恒探针(-E -dM 快照)
python tools/check_abi_golden.py              # T11 协议 ABI 快照
python tools/check_comment_style.py           # T16 注释风格
python tools/check_build_parity.py            # 双构建 -D parity
python tools/check_macro_dups.py              # 宏重复/冲突/单字母宏(T14/附D)
python tools/check_tags.py --batch <Bx>       # G7 DEV 条目 + 标签链
# 注意:本机 `pwsh` 实为 PowerShell 5.1,Get-Content 默认按 ANSI 读 UTF-8 会乱码且少算行数;
# 统计中文文件一律加 -Encoding utf8,或改用 read 工具。**新增 .ps1 注释一律 ASCII**
# (无 BOM UTF-8 中文注释的字节可能被 GBK 解析成续行反引号,导致脚本语法错)。
```

- 交付证据:DEV 条目(RM_DEVLOG,注明 AI 模型名)+ 审计计数 + 各层验证结果(未执行项注明)+ Markdown 路径 + commit SHA/分支/推送结果。
- 编译通过/主机测试通过不构成实体故障关闭;烧录/台架/整车始终由用户执行。
- 被否决的方案写入 RM_DEVLOG 的 DEC 台账;经验结论沉淀 `.Doc/spec/本车调参结论.md`。
