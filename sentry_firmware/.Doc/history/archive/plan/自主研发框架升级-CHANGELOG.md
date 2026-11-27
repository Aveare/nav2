# 自主研发框架升级总方案 · 变更日志

> 正文:`.Doc/plan/自主研发框架升级总方案.md`。本文件只增不改;每条含版本、日期、要点。

## v2.6.1(2026-11-22)——批准签署(A→B)+ 配套同步

- §0.1 批准记录签署:范围 = **先批 B0a**(§9 清单式批准);档位 = 档二·硬日期暂无 → 按 §7.3 只授档一授权(B5/B6a-b/B8/B11/B12 锁定);B0a 四项结构级操作(RM-012 测试桩修复 / Makefile 补 2 条登记 / G4 自基线执行器 / redflags 文件)随批授予;状态 A→**B 已批准待启动**。
- AGENTS.md Map 改两轴(结构轴=本文件;功能轴=总计划书,冻结);skill `evolution-roadmap.md` 整体替换为指针、SKILL.md 三处同步、`coding-contract.md` §2 加架构升级批次豁免(§11-A1/A15)。
- `.Doc/review/` 七份原始审查文档随批准文档入库(用户曾判过期删除,后找回恢复;check-links 仍按方案 A 跳过该目录)。

## v2.6(2026-11-22)——外部代码级审查收口

- **P0 修正**:B5 的 `can_handle→bus_id` 进行为白名单并附等价表;B4b 的 `USART_RX_RING_SIZE 256→≥1024` 从“零行为”改为行为白名单(921600 下 256B≈2.8ms < 5ms 消费周期);T02/T07 改为 B0 台账,区分 ist8310 注释、bsp_fault 复位后不可达、CubeMX/任务主循环与随删除站点。
- **数量口径**:`while/for` raw 48 行(原 47 站点);B0 删除量 2213 行 = 741 + bsp_tools89 + 1383,`motor_task.c:9` 只删 LK9025.h 一处;T13/T15 不再写死 9 文件/42 路径,统一由 `check_symbol_contract` 清单产出(`gimba_IMU_data` 代码 22 行 + 8 工具文件/33 命中)。
- **算术/边界**:T07 47 = user_malloc27 + malloc20(不是 kalman27 + live7);§0.4 档二去掉重复 B3;G2 第一方编译单元边界明确(当前 90 个 .c,tests 5 个排除);G9 阈值从 4.2s 收紧到 3.0s(LSI 26kHz 最坏 2.46s + 余量);M3′ 的“T05 后端②”改为 G-C②。
- **结构风险**:B3 从并行改为串行先于 B4a,独占 dji_motor/bsp_can/ins_task 的 err/diag 改动;新增 §7.3 比赛硬日期断路器(T_season、arch-pause、main 最后全绿 tag);B1b 全仓格式化加“批间隙+队友停写+单日完成,否则后移 M3 后”。
- **提交顺序**:批准文档(方案/CHANGELOG/.Doc/review/gate 文档)先提交并打 `arch-before-upgrade`,seed 从该 tag 采(代码树=84d51d7),避免 B0a 首个 G4 diff 把文档算成 added/changed。

## v2.5.2(2026-11-22)——定量断言抽验(8 项,发现 1 硬错 + 3 口径漂移)

- **抽验成立 4 项**:Makefile 缺 bsp_fault/bsp_iwdg 2 条(grep 0 命中)✅;RM_DEVLOG 确有 DEC-001~006 ✅;PI 双定义值差(…535f vs …5354f)✅;TRUE×3/SMOOTH_COEF×3 文件 ✅。
- **硬错误 1 处**:T13"根目录 8 个 .js"——实测 8 个 .js **全在 tools/ 下**,根目录为 0;已改"T13 + 冻结清单"归属。
- **口径漂移 3 处**(不改历史数,加复测注/授权脚本):`while(1)/for(;;)` 复测 48 行 vs "47 站点"(行≠站点);`gimba_IMU_data` 复测 22 行 vs 21;"待实车/待验证"复测 md 67 处 vs 43(43 为 11-21 时点)。T13 的"42 条 -> 路径"复现不出(原始 71/去重 27)→ 条数改"以契约脚本清点为准"。
- **处置原则落盘(§0.2 复测注)**:全部定量断言以 B0 交付的脚本产出为唯一权威,正文数字是 84d51d7 时点的审查快照,允许 ±口径漂移,**不得据手数验收**。

## v2.5.1(2026-11-22)——v2.5 采纳审查(4 处残留修正,核验全部通过)

- **核验通过项**:工期三档(29~42/54~79/62~92)逐批求和验证一致(B1 上修 7~10 已同步);日历 14~20 周=54~79÷4 进位;G.1 P1-11/G.2 工期行同步;G4 排除清单里的 `.zcode/` 确实存在(非噪音);B0a ⓪ 改 `git ls-tree -r` 确定性生成并排除 gate 产物自身——同时消除了 v2.4 ⓪"扫工作树"的隐患;G7"批次号含 B1b"与附B 标签链(arch-before-B0→B1→B1b→B2)完整;B1 scope ① 九项工具与 §13/附B fixture 一一对应;《参考文献.md》入链后 check-links PASS。
- **修正 4 处**:①"全部批继承"清单漏 G1a(§5/附B 两处补齐);②继承注"批次表只列本批新增"与实际全量速查列不符→改"速查";③§10 计数法措辞(rg -c 前提=每行恰一条,rg -o|wc -l 等效);④§6 B0 行补"收工打 arch-before-B1+入库 manifest"(原只在附B);另 §13 --trees 分隔符标注"以交付脚本为准"。

## v2.5(2026-11-22)——codex 五轮最终收口

- **B1 工具先行并入批准范围**:G1a gcc 版本门禁、G2 三断言、run_fwk_tests、--budget、check_sections、--mode/--paths、check_tags、check_plan、clang-format 版本记录全部写进 B1 scope;工具必须先于扩目录交付并自检。B1 估算 4~6 → **7~10 会话**。
- **补 G1a/G2/G9/G10**:§13 新增 gcc 版本门禁、G2 三断言、`check_source_lists --trees`、G9 注入 hook 与 B1 fixture 自检,直接关闭"凡闸门必有交付物"的遗漏。
- **全部批继承**:§5 明确 G2/G3/G4/G5c/G7/G10 默认叠加到每批;§6 批次表只列本批新增/强化项。
- **B1 验收加工具自检**:版本不一致、单元数少 1、CCM 越界、预算超限、move/rewrite、缺标签、G.2 断言各一条 fixture,全部贴输出。
- **G4/G1a/G11 基线可复现**:seed 由 `git ls-tree -r 84d51d7` 生成并增加 `env.txt`;G4 manifest 从收工 tag 确定性生成并入库,排除 gate 产物自身;`build/refactor_baseline` 只作本地缓存,不得作为唯一证据;G1a/G11 的版本基线改为入库 env。
- **B0a 验收纠偏**:G2 不再被 `--no-build` 跳过;G3 同时跑 chassis/ax650;B0/B1/B1b 收工补 `arch-before-B1/B1b/B2` 标签与 G4 manifest。
- **工期重算**:档一 29~42 / 档二 54~79 / 档三 62~92;日历约 14~20 周到 B12。

## v2.4.1(2026-11-22)——codex 四轮复核:闸门判据与仓库工具对齐(9 条全部实测成立)

- **"判据引用了不存在的工具"5 条**(argparse/ls 实测):G3 引用 `run_fwk_tests`(tests/ 仅 chassis/ax650 两套,CI 同);G5b `--budget`、G6 `--mode/--paths` 均不在 refactor_gate.py 参数表(现只有 --no-build/--no-tests/--since/--max-unmatched/--expect-audit/--allow-linkage/--allow-new-macros/--include-history);G5c `check-sections`、G7 `check-tags` 无对应脚本。→ §5 各行改为"工具于 B1 交付,此前该闸门项 SKIP+DEV(不得静默通过、也不得因缺工具 FAIL)";§5 增"工具可用性口径"总注。
- **§13 落 7 行交付物**(消除"凡闸门必有交付物"自家条款的违反):run_fwk_tests.ps1、refactor_gate --budget、check_sections.py、refactor_gate --mode/--paths、check_tags.py、check_plan.py(方案自检,按章节解析+附G 排除界)、env.txt 增记 clang-format --version——全部 owner=B1。
- **G.2 验证口径可复现**(codex:按文档原样执行 grep 仍命中本表引文):表头改判据为「命中数=声明数且全部位于本表」;§10 六条 ARCH-DEC **拆为每条一行**,`rg -c '^\*\*ARCH-DEC-00'` 直接 = 6(引用处均在行中不匹配行首锚);`check_plan.py` 作为机器可复核版落 §13。
- **G11 补 clang-format 版本钉死**:save-baseline 的 env.txt 现只记 gcc(refactor_gate.py:183 实测),不同 LLVM 版本格式化结果可能不同 → B1 起增记 `clang-format --version`,版本 ≠ env.txt → FAIL。
- **两处小残留**:§5 标题 G1a~G10 → **G1a~G11**;附E 三处("先于 B1 收尾"/N12"B1 单次格式化提交"/N13"(B1)")→ 统一为配置 B1、执行 B1b。

## v2.4(2026-11-22)——codex 三轮逐行复核修正(9 处口径冲突清零)

- **G4 执行器归属唯一化**:G.1 P0-5 行"B0 交付"→ **B0a**(与 §6 B0a ⑤、G.2、附B 四处同源;B0 只产出基线数据)。归属分裂会让 B0a 的入口权限与退出判据不清。
- **ARCH-DEC 落到 §10 正文**:六条全部加 `ARCH-DEC-` 前缀(v2.3 只改了引用处)+ 新增命名空间纪律段(本文件 ARCH-DEC 与 RM_DEVLOG DEC 两套编号不互通、新增从 ARCH-DEC-007 起)。
- **G11 接线补全**:B1b 闸门列挂 G11;§13 增"G11 等价脚本(owner B1b)"。§5 白名单废除"16 处 while(1)"手数,改"按 B0 台账三类处置,仅「转 err_report」子集属行为变化"。
- **终态里程碑统一**:档二结项 = **M3′**,档三全量 = M5。修 4 处混用:§0.1-D(B12+M5→B12+M3′)、§1 表头、M5 行(原来把 M3′ 的判定内容写在 M5 行里)。
- **计数统一**:B0a = **9 项未验证改动(8 行为 + 1 零行为 RAM)**(§0.2 原写"8 条"但列了 9 个 DEV;附B 原写 8 条);实体验证 = **8 次(+返修 2)**(§0.4 表/§0.4 正文原写 6~8)。
- **删除时点唯一化**:`bsp/` = **B8**(§3.1 原写 B4c,与 B4c/B8/附H 打架);`modules/` = **B12**(§3.1/§6.0/B6b 原写 B6b,但 can_comm 留置 + DM/HT 活桩还在目录里,B6b 删目录会连留置件一起删)。附H 口径本就正确,本轮把正文对齐到附H。
- **附B 重写为与批次表同源**:B0a 验收 = ⓪~⑤ 逐条贴输出(9 项/执行器自检/redflags/check_source_lists/标签);B0 验收 = **2 个删除提交**(原写 3 个)+ 台账先于删除。
- **补"改动前快照"**(P0):B0a 新增 **⓪ 种子快照**——从 `84d51d7` 采全树文件清单+sha256 入 `.Doc/gate/seed-84d51d7/`,先于任何文件改动,作为 G4 首个 before 状态(原流程收工才 save-baseline,第一个批次的 G4 无 before 可比);B0 的 47 站点台账改为**先于删除生成**(口径固定 `84d51d7`,删除站点预标「随删除」,删后不重算);G4 行同步写明基线来源。
- **§13 重写为逐项 owner 表**:补齐 6 个此前"有闸门无交付物"的项(G4 执行器/redflags/RM-012/check_source_lists/47 站点台账/G11 脚本/种子快照)。
- **G.2 改三列**(发现/落点/验证方式+状态):声明与验证分离,✅=可 grep 复核、🟡=待 owner 批;同时修正 G.2 内残留的旧工期数字(21~31/49~72/57~85 → 26~38/51~75/59~88),并把 v2.3"12 项全部落地"改判为"3 项只改了声明"。

## v2.3(2026-11-22)——codex 二轮复核采纳

- **逐站点台账脚本生成**:T02/B3 废除手数(33+6 口径与 E2 表无法勾稽),B0 交付 while(1)/for(;;) 逐站点台账(47 站点 grep 实测,含任务主循环/CubeMX 合法豁免分类);B3 按台账执行,验收=逐行勾稽。
- **红旗白名单归属统一为 B0a**:G4 自基线执行器(文件清单+sha256)+ 红旗白名单文件(.Doc/gate/redflags)为 B0a 交付物(T02/G4/附B 同步;原 T02"B1 产出"/G.1"B0 交付"/G.2"B0a 交付"三口径收口)。
- **ARCH-DEC 前缀全量落地**(§10 六条全部改前缀,不再只是引用处);RM_DEVLOG DEC-001~006 已占用,冲突消除。
- **目录树/§3.2 旧口径清理**:§3.1 bsp/ 行改"B4c 迁移+shim、B8 删目录(逐文件归属见附H)";§3.2/T01 扫描规则改"framework/ 一律 0 个 RTOS include,后端唯一落点 ports/freertos"。
- **新增附H**:bsp/ 与 modules/ 逐文件归属表(迁/删/留置;含 bsp_tools 死代码补删、can_comm 留置至 B12、bsp_adc 占位保留);**B0b→B0 删除清单改 2 提交**(0 字节占位保留后)。
- **新增 G11**(B1b 格式化等价:重放 0 diff+编译+nm/size 等价+#include 集合 diff——G6 剥 # 行会漏 include 变化);M1/M2 补 B1b/B3。
- 工期校正(含 B1b/B3):档一 26~38/**档二 51~75**/档三 59~88;实体 8 次(+返修预留 2)。

## v2.2(2026-11-22)——codex 审查采纳

- **P0 修复**:①B4c 删 bsp/ 时点不成立(application/modules 尚有 39 文件引用 bsp_*)→ B4c 只做实现迁移+forwarding shim,**bsp/ 目录删除后移到 B8**;②幽灵引用清零(G8a/G8b/依赖行的 B0b→B0;B1 的 §3.7/3.8/3.9→§4;恢复 §6.0 M↔B 映射表);③新增 **M3'(档二结项验收)** 定义(G-A~G-F 中 T05 后端②/T08 双构建按档二口径、G-B 三骨架、G-C' 缺口登记;证据/执行人明确)——修"档二无结项闸门"矛盾;④红旗白名单机制改为 **G4 自基线执行器承载**(audit_baseline 是文本正则,锚注不生效);⑤DEC 编号冲突(RM_DEVLOG 已有 DEC-001~006)→ 改 **ARCH-DEC-001~006** 命名空间。
- **P1 修复**:①**B3 批次行恢复**(v2.0 重写时遗失:16 处 while(1) 转换+白名单 10 点锚注+G9 钩子,3~4 会话)+ 逐站点处置表;②kernel 平台边界:API 头 framework/kernel + 后端 ports/freertos(消"零 include 自相矛盾");③skill(evolution-roadmap/coding-contract)同步时点从 B12 **提前到批准时**(现行条款与 B1/B2/B7 正面冲突);④**B1 拆分**:B1(地基)/B1b(工程风格批:clang-format+naming+注释 §2+证据迁移,合入窗口执行,规避双线格式冲突);⑤msg 多订阅者修正(机制已存在于 message_center 链表,保留勿删;深度>1 才是新能力);⑥T01 扩 `h*` 句柄扫描(referee_task.c:22,82 直发例外,B6b 删);⑦工期数字校正:档一 21~31/**档二 49~72**(含 B3)/档三 57~85;B0a 9 条;§11 14 项(A12 一致)。
- 已核实成立项(HEAD/体积/闸门计数/message_center 三缺陷/FreeRTOSConfig for(;;)/IWDG 2s/CMake GLOB/CI 双构建)与 §0.2 基线快照一致。

## v2.1(2026-11-22)——用户四项裁决落地

- **范围 = 档二(到 M3 迁移收官,含 B11/B12)**:43~63 会话、实体 5~7 次;B9(裸机)/B10(Linux/ROS2)/M5 全量验收标记不执行,G-C ②④⑤ 结项按 G-C' 登记能力缺口;G-B 改为"infantry_skeleton 装配达标(heavy/sentry 缺口登记)"。
- **硬日期暂无**:按会话节奏走,比赛急务按 §7.1;若赛季中途出现硬日期,按 §7.2 处理。
- **DEC-⑥ 裁决**:0 字节占位文件(unicomm/standard_cmd)保留至 B12 统一裁决(不推翻重构总方案 §9.4),B0 删除清单同步。
- **check-links = 方案 A 已执行**:`tools/refactor_gate.py` LINK_SKIP_DIRS 增加 `.Doc/review`(审查报告留仓作证据链),实测 PASS。

## v2.0(2026-11-22)——六域审查整合版

- 整合 `.Doc/review/` 六域审查(00-总报告 + A 事实核验 / B 架构 / C 批次闸门 / D 规则映射 / E 文档一致性 / F 方向自审):**P0 阻断 8 条全部落地**,采纳状态见正文附G。
- 结构:新增 §0.1 文档状态机(A/B/C/D 四态)、§0.2 现状基线快照、§0.3 权威两轴、§0.4 赛季倒排与三档范围;M↔B 映射表收口(正文只用 B 编号,目录删除时点挂批次);版本史拆出至本文件。
- 事实修正(A 域,三条承重断言被证伪后复核采纳):
  - `#if` 系使用点 186 → **业务层 193**(含 tests 202、含 CubeMX 376);其中 78 处为 include guard,**真条件编译点 115**(186 = 漏计 7 处 `#elif` 的口径错误);
  - malloc"21 处/5 活文件" → **全仓 47 处/17 文件**:kalman_filter 27 处(启动期一次性,豁免)+ 活路径 **7 处/5 文件**(bsp_can:153、bsp_pwm:40、buzzer:52、message_center:54,66,72、dji_motor:221);user_lib 的 zmalloc 无调用者(死);**稳态已零 malloc**,无一处位于周期任务;
  - `while(1)`"11+"/"白名单 4 处" → **全仓 33 处 while(1) + 6 处任务体 for(;;)**:注册/初始化 18(纯 bsp 9)、运行期传输 12、复位 1、INS fail-fast 1、任务体 6;白名单 = 4 类共 10 个代码点;servo 无 while(1)(原列举有误)。
- 其余口径修正:`ONE_BOARD` = 7 文件 29 处(定义 1 + 裁剪 5 + 注释 1);tools/ 读符号文件 = 根目录 8 个 .js **+ tools/lib/preflight.js**(第 9 个)+ analyze_*.py 的 CSV 列别名;伞头"顺序不敏感"有反例(ax650.h 的 #if 依赖 chassis.h),宏治理目标改为"消除 config 内跨子文件 #if";G3 现状红(RM-012);check_source_lists FAIL(缺 bsp_fault.c/bsp_iwdg.c)。
- 批次:B0a 重写为"当前树稳定化 + 8 条未验证行为改动实车验证 + 红旗裁决 + RM-012 修复 + 基线重锚"(旧内容已结项:实车回归 2026-11-20 PASS、R5b 完成、R4b 决定不做);B0b 更名 **B0**(删除分 3 提交 + check_symbol_contract.py + host 整链 harness + 冻结基线入库 `.Doc/gate/arch-baseline/` + 注释乱码修复 + 0 字节占位改 DEC-⑥ 待批);**B7 前移到 B2 之后**(消灭 5 批双 API 并存);**B6 拆 B6a(敏感:IMU/INS/EKF)/B6b(其余)**;B8 上修 8~12 会话;总工期 38~56 → **58~86 会话**(到 M3 41~60),实体验证 3 次 → **8~10 次**。
- 闸门:G1 拆 **G1a(config 逐字节)/G1b(框架宏账本)**;G2 加编译单元数/build type/无 `-flto` 断言并强制重跑 cmake;G3 加 check_symbol_contract 与测试源列表一致性;G4 换**自基线执行器**(文件清单+sha256 快照,解除对 audit_baseline 的依赖);G5 拆 **a(纯搬家)/b(替换档预算)/c(分区对比 CCM)**;G6 加 `--paths` 必填;G7 check-tags 自动化;G8a/G8b 容差 rtol/atol ≤1e-6 + 模式字精确相等,金标准必须用**迁移前代码**生成;G9 自动化(`-DDG_TEST_HANG` + RTT `hang`,≤4.2s + IWDG=1 日志 + 急停态)并增反向判据"不接裁判 60s 不复位";新增 **G10 构建一致性**。
- 白名单重写:B4a FIFO 显式化、B7 BASEPRI 临界区(最坏关中断 ≤20µs 验收)、B8 板型运行期化(若做)、`while(1)`→`err_report` 转换归属 **B3**;兜底条款"未列入本表的任何改动一律视为行为变化"。
- 风险册新增 5 条🔺:工具符号静默失效、CubeMX 覆盖 Inc/FreeRTOSConfig.h、CCM 84% 预算、工具链漂移、双构建漂移(Makefile 带 -DDISABLE_LOG_SYSTEM 而 CMake 不带,两个产物日志行为不同)。
- 设计修正:framework 承诺改为"**零平台 include + port 供给表**"(DWT/段属性/RTT sink 属 port);kernel 15+3 原语(补 is_in_isr/current_task/start;优先级数值钉死 3/4/5 + 静态断言;mutex/queue 推迟至有调用者);msg_publish 非 ISR 断言 + MSG_TOPIC_MAX=8 + 适配头保留返回 1;err 四档分投递(LOG_ONLY 计数环/SAFE_STATE 原子标志任务侧消费)+ 临界区禁 err_report + configASSERT 改造;diag **必活/可选集合** + diag_watch_unregister(UI 合法离线不喂狗,修"不接裁判 2s 复位循环"毁车缺陷);遥测改 **arena 注入式**(port 定义对象,framework 拿指针,SECTION_CCM 退居 port 内部);FIFO 金标准 dump 先行;`sender_assignment` 伪实例→bus_tx_frame。
- 范围:三档供选(§0.4;档二为建议默认,**待用户确认**);附录 C 按 D 域核验修正(适用赛制列、42mm 16.5m/s、功率=新建、模块在线性=代理观测、SHOOT_RATE_ECO 硬编码证伪"零改动跨季"、1V1 专属行、遗漏 9 项)。
- 用户指示落地:代码风格统一(clang-format + clang-tidy + 注释规范,T15 扩为"命名与格式");**证据资产迁开发史**(config 参数卡片注释可瘦身,完整证据迁 `.Doc/spec/参数证据表.md` 与 RM_DEVLOG,B1 执行);`@author 小猫大王`(v1.9)维持,搬移文件保留原文件头、实现替换批统一更新。

## v1.9(2026-11-22)

- 新增 **附录 F 注释治理**与 **T16**——更正:注释重构 program 已存在且基本完成(.Doc/spec/注释规范.md 397 行,批次 0~5 ✅,126/137 文件头),本方案只做 @author 政策修订、残余清零、批次映射;**@author = 小猫大王**(队伍 IP 署名),上游原作者行/版权行保留;config 注释为证据资产(该红线于 v2.0 按用户指示放宽,见上)。

## v1.8(2026-11-22)

- 新增 **附录 E 命名规范与坏名映射表**及 **T15**——grep 实测拼写错误渗透工具链(`gimba_IMU_data` 代码 21 处 + 工具 7 个)、四动词混乱(send29/recv39/fetch11/feedback7)、缓冲区 5 种命名 97 处;规范 N1~N11;迁移批"搬家即改名";M3 验收加"全仓命名达标"。

## v1.7(2026-11-22)

- 新增 **附录 D 重复/近义宏清单**(grep 实证)——TRUE/FALSE×3、SMOOTH_COEF×3、user_malloc×2 纯重复;**PI 双定义且值不一致**;限幅四套(两套零使用)/开方三套/双日志闸/双时间源/换算三档精度/分配三入口;user_lib 死函数 8 个 + 死宏 2 个;B0b 删死项、B1 收口、B12 勿动合法相似。

## v1.6(2026-11-22)

- 新增 **§7.1 多人并行协作纪律**——main(可参赛线)/framework(批次分支)双线,批间隙为合入窗口,批中队友提交不合入(G4/G1' 会误判);自瞄调参、USART1 交接、新增功能三类处置表;B0a~B4 与自瞄调试零交集可并行,B5/B6/B8 为协调窗口。

## v1.5(2026-11-22)

- **去除框架品牌前缀与工作名**——API 命名统一为模块名前缀 + 动词(kernel_/bus_/msg_/err_/diag_/mem_,段宏 SECTION_*,profile 特征宏 USE_*),与仓库既有"模块名前缀"风格同构;目录随之改名。

## v1.4(2026-11-22)

- 新增 **§3.9 宏管理(四类账本)**与 **T14**;B1 交付 `tools/check_macros.py` 与宏账本初版,G1' 升级为分类校验;RTT 1024B 日志预算表入账。

## v1.3(2026-11-22)

- 吸收外部代码级审查,10 条采纳 2 条驳回:新增 T13 工具契约、§3.7 ISR 上下文契约、§3.8 一等不变量清单(FIFO 归属=注册顺序)、CAN 续传定死 ISR 续传、遥测环独立对象 + SECTION 宏、T07 口径、B0 拆分、B4 拆分、G9 补急停判据、RAM 风险、B10 串口边界;**驳回**"kalman_filter.c 是死文件"(实为 QEKF 骨架)与"DM/HT 可删"(实为活桩)。

## v1.2(2026-11-22)

- 三份 profile 统一"骨架"命名,步兵 profile 改名 `apps/infantry_skeleton`;重申原地绞杀式路线(对比空白重建的判据)。

## v1.1.1(2026-11-22)

- 自审修订:apps 目录树层级错位、规则手册路径全角括号、G5 闸门分档 + 补偿控制、超电"已装则须在线"、隧道增益待协议核对、回血/发弹量行补齐。

## v1.1(2026-11-22)

- 按《RMUL 2027 比赛规则手册 V1.0.0》修订全兵种阵容(取消英雄/工程 → 重装/步兵/哨兵 + 自定义终端);新增附录 C(规则→框架映射)。

## v1.0(2026-11-22)

- 初版:目标、T0 判据、架构、子系统设计、上游脱离、闸门 G1'~G9、批次 B0~B12、风险、纪律;四项决策(C 核心/全跨平台/原地绞杀/先收尾旧重构)经用户拍板。
