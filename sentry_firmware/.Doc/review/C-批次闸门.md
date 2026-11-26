# 域C评审：批次计划、闸门体系与风险登记册的可执行性

> 审查者：plan-gate-reviewer（子代理），2026-11-22。
> 对象：`.Doc/plan/自主研发框架升级总方案.md`（v1.4，358 行）§5 闸门、§6 批次（B0a~B12）、§7 衔接、§8 风险、§9 纪律、§10 记录。
> 对照：`.Doc/plan/重构总方案.md`、`AGENTS.md`、`RM_DEVLOG.md`、`tools/refactor_gate.py`、`tools/check_source_lists.py`、`.github/workflows/c-cpp.yml`、`CMakeLists.txt`、`Makefile`、`tests/run_*.ps1`。
> 口径：**以本地工作树为准**；所有数字为本会话实测（命令见附A）。只读审查，未改任何仓库文件，未跑 `save-baseline`、未编译、未烧录。
> 事实更新：审查期间 HEAD 前进到 `84d51d7`（2026-11-22 12:43，含 DEV-20261122-01/02），`git status` 现只剩本方案文档与 `.Doc/review/` 未提交。前提里"HEAD 大幅落后"已部分失效，但下文的实质结论不受影响（8 条行为改动仍未实车验证）。
> 与域F的分工：域F 已就"G1' 与 2027 参数切换冲突""G4 执行器与 G-E 退役矛盾"给出战略层结论，本文不重复立论，只在 §三 给执行层替代方案。

---

## 一、总体判断（10 行）

1. **方案的方法论是对的**（绞杀式 + 逐批零行为闸门 + 前一批基线），但**闸门目前只实现了"结构未变"，没有实现"行为等价"**；G5 降级档、G8、G9 三者都还是待建工程，却已被写成 B4/B8 的强制判据。
2. **三个阻断级缺陷**：G2 不重跑 cmake 导致新目录的代码可能根本没被编译却报 0 error；基线放在 `build/` 且是覆盖式，"冻结基线"与"每批重锚"抢同一个目录；G8b 要的"迁移前快照"在 B0b 时无法采集（sim 到 B4a 才有）。
3. **批次顺序有一处该改**：B7（消息切换）应在 B2 之后立刻做，而不是等 B6 之后——否则 5 个批次双 API 并存，且 B5/B6 迁完的文件在 B7 要再改一遍（25 处生产调用点 + 8 处主机测试调用）。
4. **行为变化白名单漏了 3 项**：B8 的"编译期 ONE_BOARD → 运行期 profile 字段"、B4a 的 FIFO 归属显式化、B7 引入 BASEPRI 临界区；另外 `while(1)→err` 这批转换**根本没有归属批次**，而它是行为变化。
5. **B0a 的范围与事实脱节**：方案说"R5b/R4b"，但 R5b 已完成、R4b 已决定不做；真正欠的是 8 条 2026-11-21/22 的行为改动的实车验证，外加 2 个新红旗。
6. **工作量整体低估约 1.5 倍**：38~56 → **58~86 会话**；另需 **8~10 次实车/台架**（方案只写了 3 次实车）。
7. **§8 风险册不合格**：7 条里只有 1 条带量化阈值，没有责任动作与退出判据；最该在册的三条（CubeMX 覆盖 `Inc/FreeRTOSConfig.h`、CCM 84% 隐性预算、工具符号漂移）不在册。
8. **M 编号体系残缺**：M1/M4/M6 被引用但无定义、M2 不存在、M5 没有排期行，而"application/bsp/modules 何时删"全靠它们。
9. **结论**：方案可以批准开工，但**必须先做"§三 的闸门与批次修订"，再开 B0a**；否则闸门会从"证据"退化成"仪式"，而这正是绞杀式路线最怕的失效模式。
10. 优先级建议：先修 G2/G5/G8b 三个执行器缺陷（约 2~3 会话），再开 B0a；不要先写更多文档。

---

## 二、问题清单

严重度定义：**阻断**=不修就会让闸门给出假绿/工期不可信；**高**=会造成返工或隐患进入迁移；**中**=可接受但需登记；**低**=措辞/一致性问题。

### 2.1 批次排序与依赖（评审点 1）

#### C-01【阻断】G2 只跑 `ninja -C build`，新目录的代码可能从未被编译却报 0 error
- 方案行号：L225（G2）、L252（B1 新增 framework/）、L255（B4 新增 ports/）、L259（B8 新增 apps/）。
- 证据：`tools/refactor_gate.py:157` 的 `run_build()` 只执行 `ninja -C build`，不重跑 cmake；`CMakeLists.txt:63-69` 的 include 递归与 `CMakeLists.txt:72-79` 的 `file(GLOB_RECURSE SOURCES ...)` **硬编码只列了 `Drivers/Src/Middlewares/bsp/modules/application`**，没有 `framework/`、`ports/`、`apps/`、`components/`。`AGENTS.md:57` 写的正确命令是 `cmake -G Ninja -B build && ninja -C build`，与脚本实现不一致。
- 为什么是问题：B1 新增 `framework/{err,kernel,mem}` 后，若只跑 ninja，**这些 .c 不在 build.ninja 的源列表里，编译器根本不会打开它们**，G2 却报 "0 error / 第一方 0 warning"。整套"零行为闸门"里最基础的一道会给出假绿；而且 B1 的分解验收（附B 第 6 条 L319"整车未接线任何新代码"）在此前提下完全不可判。
- 修改建议：G2 判据改为 ①`cmake -G Ninja -B build && ninja -C build`；②**断言 `build/build.ninja` 里出现的编译单元数 == 磁盘上第一方 `.c` 数**（`tools/check_source_lists.py` 已有解析 Makefile 的能力，扩成双向即可）；③断言 `CMAKE_BUILD_TYPE=Debug` 且编译期 FLAGS 无 `-flto`（理由见 C-12）；④在 `CMakeLists.txt` 一次性把 `framework ports apps components` 加入 include 递归与 GLOB，并在 B1 交付里显式登记这一条（附B 只写了"增 framework 静态库 target"，不覆盖 include/GLOB）。

#### C-02【阻断】基线存放在 `build/` 且是覆盖式：滚动基线与冻结基线抢同一个目录
- 方案行号：L214（"B0 起 save-baseline 成为唯一基线"）、L231（G8b 要"B0b 金标准样本"）；`AGENTS.md:60`（"只在每批开工前重锚一次"）。
- 证据：`tools/refactor_gate.py:36-38` 定义 `BUILD = ROOT/"build"`、`BASE = BUILD/"refactor_baseline"`；`cmd_save()`（:177-214）用 `write_text` 覆盖写入。`build/` 是未跟踪、随时可 `rm -rf` 的目录。当前基线记录的 `git_rev` 是 `9f84e08`（R7），而工作树已到 `84d51d7`，中间隔了 R8 + 2026-11-21/22 的全部改动——基线**已经过期**（实测 G1 新增 30/消失 12，G4 计数 295/104/61/61/4 vs 基线 301/98/61/57/2）。
- 为什么是问题：12 个批次跨数月、跨多台机器/多次 clean build。①任何一次 `rm -rf build` 或换机就丢基线，而 `save-baseline` 只能用**当前树**重建——丢了就再也造不出"上一批"的基线；②G8b 需要的是一份**永不覆盖**的"迁移前快照"，但如果每批开工前重锚，这份快照会被覆盖掉；③基线里没有 gcc 版本与 build type 的强校验，只有 `env.txt` 一行文本。
- 修改建议：拆成两个目录。①`build/gate-baseline/`（滚动，每批重锚，不提交）；②`.Doc/gate/arch-baseline/`（**B0b 冻结一次并提交入库**，存 `macros/symbols/size/sections/golden-vectors/input-seq` + `git_rev` + `gcc --version` + `build_type`）。G4/G5 比滚动基线，G8a/G8b 比冻结基线。另给 `save-baseline` 加 `--tag` 与"不覆盖已存在目录（除非 `--force`）"的护栏。

#### C-03【阻断】G8b 的"迁移前快照"没有可执行采集路径
- 方案行号：L167（"B4 内嵌 sim-min"）、L231（G8b"迁移前快照"、判据"sim 跑 10s 标准输入序列"）、L251（B0b"采 G8 金标准样本"）。
- 证据：仓库内**没有 sim**（`ports/sim` 不存在，`Get-ChildItem ports` 无），主机侧只有单文件桩测试 `tests/run_chassis_tests.ps1:6-16`（编译 `robot_cmd.c` 等 11 个源文件 + `tests/native_stubs`），无法承载"cmd→chassis→gimbal 整链 10s"。
- 为什么是问题：B0b 在 B4a 之前，sim 尚不存在，所以"B0b 采整链金标准样本"这件事**在当前工具集下无法执行**。这不是低估工作量，是判据与工具时序自相矛盾：G8b 的"迁移前快照"若在 B8 才补采，那时迁移已经做完了，采到的是**迁移后**的行为，"对拍"退化为自证。
- 修改建议（三选一，推荐 ①）：①**B0b 交付 host 整链 harness**：沿用现有"编真源码 + 桩"的成熟做法，把 `robot_cmd/chassis/gimbal/shoot` 的纯控制路径在 x86 上跑 10s 脚本输入（遥控/键鼠输入序列），输出 `golden_chain.csv` 提交入库；B8 在 sim 里重放同一输入序列比对。②退一步：整链基线在 **B4a 之前**用 `arch-before-B4a` 标签 + 探针 RTT 抓取（200Hz 遥测环已有 5 通道），但这是侵入式且需用户在场。③最差：明确写"G8b 降级为 B8 自身两次运行一致"，并承认它不证明行为等价。
- 附带：G8a 不需要 sim。G8a 对象是 PID/运动学/斜坡这些**纯函数**，现有 host 测试模式（`tests/run_chassis_tests.ps1`）就能编真源码跑向量。所以"B4a 内嵌 sim-min 为了让 G8a 就绪"这个理由不成立（见 C-22）。

#### C-04【高】B7 排在 B6 之后是错的：要么双 API 并存 5 批，要么迁移文件被改两遍
- 方案行号：L107/§2.4、L154（"适配头 B2~B7 存续"）、L258（B7）、L266（关键路径）。
- 证据：实测 `message_center` 四 API 全仓 38 处出现，去掉 `message_center.c` 自身 5 处与主机测试 8 处，**生产调用点 25 处**（`robot_cmd.c` 12、`shoot.c` 5、`chassis.c` 4、`gimbal.c` 4），6 个话题 12 个注册点；`tests/chassis_remote_test.c` 另有 8 处。`message_center.h:21` 已是 `QUEUE_SIZE 1`，所以 L234 说的"B7 深度默认 1 = 现语义"是**正确**的（这点方案没错）。
- 为什么是问题：①B5/B6 会把 `dji_motor`/`ins_task`/`referee`/`telemetry` 等迁进 `components/`，这些文件里的 `PubRegister/PubPushMessage` 调用要用**旧 API + 适配头**写一遍，到 B7 再改成新 API 一遍——每处改两次，且第二次是在刚通过闸门的文件上动刀；②适配头存续 B2~B7 共 5 批，期间任何"顺手在旧 API 上加个参数"的诱惑都无闸门可拦。**但"两套实现并存"的担心可以排除**：适配头是"旧 API → 新实现"的转发（L154），真队列只有一套，腐化面小于直觉。
- 修改建议：把 B7 前移，顺序改为 `B0a→B0b→B1→B2→B7→B4a→…`。理由：msg 零硬件依赖、已有主机测试覆盖 `message_center`（`tests/run_chassis_tests.ps1:15` 就把它编进去了）、`QUEUE_SIZE=1` 语义已一致——它是全计划里**最适合早落地**的一块。前移后 B5/B6 直接按 msg API 迁移，25 处调用点只改一次，过渡窗口从 5 批缩到 1 批。B7 工作量 2→2~3（要同时改 `tests/chassis_remote_test.c` 的 8 处）。

#### C-05【高】B5/B6 迁移期间的"行为等价"没有动作级兜底，且金标准用例有自证风险
- 方案行号：L237-239（补偿四道：①B5/B6 给纯函数补金标准用例 ②台架冒烟 ③G8a 自 B4、G8b 自 B8 ④M3）；L256（B5）、L257（B6）。
- 证据：`G8a` 只覆盖"PID/运动学/斜坡等纯函数"（L231），而 B5 迁 `dji_motor.c`(347 行)/`controller.c`(180 行)、B6 迁 `BMI088driver.c`(349)/`ins_task.c`(331)/`QuaternionEKF.c`(434)/`kalman_filter.c`(437)/`remote_control.c`(244)/`referee_*`(~1200)/`ax650_*`(~530)——合计约 **3800 行**，主体是状态机、ISR 交互、CAN 收发时序。G8b 到 B8 才开，所以 B5、B6 两批**没有任何动作级对拍**。更关键：补偿①说"B5/B6 给纯函数补金标准用例"，如果用例的期望值是在迁移后代码上生成的，就是**用迁移后的实现给自己背书**。
- 修改建议：①金标准用例放在**每批批首**、用旧源码生成期望值并随批入库（与代码同 commit）；②B5、B6 各自在批尾跑一次 C-03 的 host 整链 harness（输入序列 + 输出对比，容差见 C-12），把"动作级等价"从 B8 提前到 B5；③B6 拆分（见三、批次表 B6a/B6b），因为把 IMU/INS/EKF 与裁判/UI/AX650 塞一批，失败时无法隔离是"敏感件"还是"顺手改坏的"。
- 对"B6 与 B5 并行还是串行"的判断：**保持串行**。两者文件域虽不重叠，但共用同一套新接口（bus/err）与同一份《工具符号冻结清单》，且单人独岗时并行无收益、只有合并冲突。要压缩的是 B6 内部（拆两段），不是 B5∥B6。

#### C-06【中】B0b"先删 2100 行"与 trace 脚本失效的先后顺序：**前提不成立，风险在 B5/B6**
- 方案行号：L251（B0b 删除清单）、L46（T13"8 个脚本直读 DWARF 字段，字段改名即静默失效"）。
- 证据：①删除目标全仓**零外部引用**（实测：`oled/ist8310/HC05/tfmini/unicomm/std_cmd/bsp_spi/bsp_iic/bsp_gpio/LK9025` 只被自身 include，`application/` 内 0 命中）；②8 个脚本实际依赖的 DWARF 路径是 `motor_lf/rf/lb/rb->measure.{real_current,speed_aps}`、`yaw_motor/pitch_motor->measure.{...}`、`pitch_motor->motor_controller.speed_PID.{Iout,Output}`、`gimba_IMU_data->{Gyro[],Pitch,Roll,Yaw,YawTotalAngle}`，共 42 条 `->` 表达式分布在 6 个脚本（`odom.js` 按全局名读 `motor_*`，`doctor.js` 只读 ELF 身份）——**全部落在 B5（电机/PID）与 B6（IMU/INS）的迁移面上，与 B0b 删除清单无交集**。
- 为什么仍是问题：方案把"工具失效"写成 B0b 的任务（出冻结清单），但真正的失效点在 B5/B6：B5 要重写 `motor_controller.speed_PID` 结构（5 个脚本读它），B6 要搬 `gimba_IMU_data`。清单在 B0b 出了却不接闸门，等于一张贴在墙上的表。
- 修改建议：①B0b 出清单的同时交付 `tools/check_symbol_contract.py`（输入：清单 `符号->成员路径`；实现：用 `readelf --debug-dump=info` 或探针 `variables.read` 逐条解析，任一条失败即非零退出）；②把该脚本挂进 **G3**（每批必跑），B5/B6 的验收项从"字段名未动"升级为"脚本全绿"；③清单条目直接采用本次实测的 42 条（不要只写示例）。

#### C-07【中】"B0 完成前不动任何架构文件"与 B0a/B0b 自相矛盾
- 方案行号：L270（§7.1）；对照 `AGENTS.md:27`（"结构级变更需批准：新增/删除 C/H …"）。
- 证据：B0b 的删除动作本身**就是**删除 C/H（`AGENTS.md:27` 明列）；B0a 若实车回归失败，修复必然落在 `application/` 等架构文件里。
- 修改建议：L270 改写为——"B0 完成前**不新建架构目录、不改调用链与接口**；仅允许两类动架构：①B0b 已批准的纯删除（只删不改，逐提交过 G5，text 必须 +0）；②B0a 回归中发现的缺陷在最小范围修复（单独提交 + 单独 DEV，不视为架构变更）。" 这样与 `AGENTS.md:27` 的"按操作批准"也不冲突。

#### C-08【中】B3 与 B8 会双重触碰 `robot_task.c`；"全员活才喂狗"缺未注册位语义
- 方案行号：L158（"profile 注册周期任务→心跳位掩码"）、L254（B3 与关键路径**并行**）、L259（B8 `robot_task→kernel+profile`）。
- 证据：`application/robot.c:169` 当前是 **RobotTask 单点 `IWDGFeed()`**，任务表在 `application/robot_task.c:33-54`（`osThreadDef` 硬编）；IWDG 超时 2s（`bsp/bsp_iwdg.c:31`：PR=/64、RLR=1000）。
- 为什么是问题：B3 说"可与关键路径并行（不同文件域）"，但心跳位掩码必须知道**完整任务集**，而任务集恰恰是 B8 的 profile 才正式化的；B3 在此之前只能靠临时任务表，B8 再改一遍。更要紧的是判据 L36/L232 的"全员活才喂狗"需要定义"某个位从未被注册"时算死还是算活——若算死，B3 一上线就复位循环；若算活，漏注册的任务就永久失去监督（闸门形同虚设）。
- 修改建议：①B3 内加"注册完成断言"：启动后 100ms 校验预期位掩码全部被置过，缺位 log+蜂鸣码；②B3 的 DEV 里登记"临时任务表将在 B8 由 profile 取代"，B8 的验收项加"心跳位集合 == profile 任务表"；③B8 不再改 `robot_task.c` 之外的喂狗路径，避免第三处单点。

### 2.2 闸门可证伪性（评审点 2）——逐闸核对

先给结论表，再给逐条问题。

| 闸 | 方案写的能力 | 现有实现能否支撑 | 缺口 | 谁执行 / 失败怎么办 |
|---|---|---|---|---|
| G1' | 宏集合为上批超集（只增框架宏） | **不能**：只预处理 config 伞头（`refactor_gate.py:61,87`） | 框架宏不在集合内；内建宏使集合随编译器变 | 脚本；FAIL→本批不合入 |
| G2 | ARM 0E0W | **不能**：只 ninja，不 cmake（C-01） | 新目录可能未编译；无编译单元数断言 | 脚本；FAIL→不合入 |
| G3 | 主机测试 + 新增 msg/err | 能，但源列表硬编码 | B5/B6/B7 移动/删文件后脚本会断 | 脚本；FAIL→先修测试基建 |
| G4 | "上批→本批自比" | **不能**：`audit_baseline.py` 比的是上游 manifest（460 条） | 与 G-E"退役 manifest"冲突（域F F6）；当前红旗已 2→4 | 需新脚本；FAIL→不合入 |
| G5 | 搬家批逐字节 / 替换批总量 +0.2% | **一半**：符号名+尺寸做了；**总量阈值没有**（size 差只打 INFO，`refactor_gate.py:304-310`） | bss 是全域合计，抓不到 CCM 回迁；"逐字节"不可实现 | 脚本；FAIL→不合入 |
| G6 | 纯搬家行多重集 | 能，但只能整体 diff 一个 ref | 无 `--paths` 文件清单；剥掉所有 `#` 行 | 脚本；FAIL→不合入 |
| G7 | DEV + 回退标签 | **未实现**（脚本无此检查） | 无自动校验 | 需新子命令 |
| G8a | 纯函数逐位一致 | **未实现** | 无基线、无对拍器；"逐位"与 `-Og`/布局冲突 | 需新建；FAIL→不合入 |
| G8b | 整链 10s 逐拍一致 | **未实现**，且基线不可采（C-03） | 同上 | 需新建；FAIL→不合入 |
| G9 | 挂任务→2s 复位→急停态 | **可自动化**（见 C-14），方案写成"台架" | 判据缺"≤2s"的余量与自动化路径 | 用户+探针；FAIL→回退 B3 |

#### C-09【阻断】G1' 用现有工具无法实现"只增框架宏"，也无法区分 A/B/C/D 四类
- 方案行号：L203-207（`check_macros.py`、G1' 分类校验）、L224（G1'）、L47（T14）。
- 证据：`dump_macros()`（`refactor_gate.py:85-90`）只执行 `gcc -E -dM -Iconfig config/robot_infantry_2026.h`。实测该命令输出 **628 行**（其中工程宏 220 条、编译器内建 408 条）；而 `loader` 里 `CONFIG_H` 是常量（:61），**framework/components 的头文件永远不在预处理树里**。`--allow-new-macros`（:443-444）能实现"只增不删"，但只对 config 伞头有效。
- 为什么是问题：①"只增框架宏"里的框架宏，G1' 根本看不见——B1 新增 40 个 `XR_*` 宏、G1' 也照样 PASS，反过来有人误删一个 framework 宏 G1' 也照样 PASS；②内建宏让集合绑定编译器版本（`__GNUC__` 等），跨工具链必然 FAIL（见 C-13）；③派生宏在 `-dM` 里以**未展开文本**输出（实测 `#define PITCH_GRAVITY_FF_LIMIT (1.2f * ((PITCH_GRAVITY_A < 0.0f ? …)))`），所以"改上游值"只会体现为上游那一行的变化，下游派生行不变——集合 diff 能抓到值变，但**读不懂是哪个派生宏受影响**；④被 `#if 0` 排除或注释掉的宏本来就不在集合里，uncomment 会以"新增"出现，这正是想要的行为，但要写进判据说明，否则会被当成误报。
- 修改建议：拆成 G1a/G1b。`G1a`＝现状（config 伞头逐字节，含内建），B0a/B0b 期间它就是唯一可执行形态；`G1b`＝新增 `tools/macro_probe.c`（只 include config 伞头 + `framework/**` 与 `components/**` 的公开头），`-dM` 后与上批比较：**新增允许、删除 FAIL、同一条定义值变 FAIL**。四类账本的分类校验（A 类逐字节/B 类声明/C 类位置白名单）由 B1 的 `check_macros.py` 承担，并明确写"B0a/B0b 阶段 G1' 按 G1a 执行，分类校验自 B1 起生效"——否则"G1' 适用全部批"与"执行器是 B1 才交付"在时序上自相矛盾。

#### C-10【阻断】G5 的降级档在当前实现里**永远不会失败**
- 方案行号：L228（G5 分档、"超 +0.2% 须逐项解释"）、L236-240（补偿四道）。
- 证据：`refactor_gate.py:304-310` 把 `text/data/bss` 差值算出后只 append 一条 `INFO`，没有任何阈值比较；`:228` 的 `--allow-linkage` 一旦打开，`resized` 非空也只是打印（`:292-302`）。也就是说"实现替换批降级为总量预算"这句话，**代码里不存在**。
- 为什么是问题：G5 是 B4~B8 唯一的结构闸。现在它只有两种结果：符号名集合变化 → FAIL；符号尺寸变化 → 靠 `--allow-linkage` 放行。迁移批必然两者都动，实操中只能全程挂 `--allow-linkage`，于是"逐项解释"变成 DEV 条目里的一句"链接性代价"，压不住任何回归。
- 修改建议：G5 改三档，并把阈值写成**按批声明**而不是固定 0.2%：①**纯搬家档**＝符号名集合相同 + 每符号 `(type,size)` 相同（地址不参与，见 C-11）；允许 `--allow-linkage` 直通但必须打印逐符号清单到 DEV；②**实现替换档**＝`--budget` 参数，默认 `text+rodata ≤ +1.0%`（当前 text 113,988 → 约 +1.1KB），**按批声明**（B4a/B4b/B5/B6 ≤+1%，B7/B8 ≤+2%），超出 FAIL 并要求逐项解释；③**分期预算**：全升级期 text 累计上限 +8KB、主 RAM 累计 +4KB，写进 §5 表尾。基准取**上批基线**（滚动基线），不取 R7 基线。

#### C-11【高】"既有符号逐字节一致"不可实现，且 bss 作为预算量是错的
- 方案行号：L228（"既有符号逐字节一致"）。
- 证据：①`parse_nm`（`refactor_gate.py:93-107`）实际比较的是 `(name, type, size)` 多重集，**地址被丢弃**（`_addr` 未使用），并归一化 static 的 `.N` 后缀——所以能比较的只有"名字+链接性+尺寸"，不是字节。Cortex-M 的 `BL` 是**相对偏移编码**，代码一挪、指令字节就变，`DEV-20261120-06:1699` 实测"763 个函数里 268 个指令序列不同"正是这个原因（该条把它归因给 `-flto`，但实测 `build/build.ninja` 的编译 FLAGS 是 `-Og`，`-flto` 只出现在 LINK_FLAGS，`CMakeLists.txt:34-38` 是 `add_link_options`——**全程序 LTO 的前提不成立**）。②`size` 的 `bss` 是**全 ELF 的 NOBITS 合计**：实测当前 `bss 118464 = .bss 22560 + .ccmbss 54944 + ._user_heap_stack 40960`（`readelf -S`），而 `.ccmbss` 是 CCM（`0x10000000`）。也就是说"遥测从 CCM 掉回主 RAM"这种事故，**bss 总量一个字都不会变**——恰恰是 §8 L279 列为"真正的预算风险"的那一条。
- 修改建议：①删掉"逐字节"措辞，写清判据＝**符号名集合相同 + 每符号 (type,size) 相同**；②G5 的预算只比 `text + rodata`（Flash 侧）；③RAM 侧改为**分区**：`.bss+.data`（主 RAM）与 `.ccmbss`（CCM）分别记，并新增 `check-sections` 子命令——断言 `.ccmbss` 段 VMA 落在 `[0x10000000, 0x10010000)`、`.ccmram` 存在、`arm-none-eabi-size -A` 的两区合计 ≤ 90%。这条建议同时覆盖 §8 L279 的对策。

#### C-12【高】G8a/G8b 的"逐位一致"对浮点不现实；容差与基线来源必须先定
- 方案行号：L231（"逐位一致"、"逐拍一致"）、L239（"G8 兜住控制行为"）。
- 证据：`CMakeLists.txt:22` 是 `Debug` → 实测编译 FLAGS 为 `-Og -gdwarf-2`（`build/build.ninja`），未开 `-ffast-math`，所以**同一份源码、同一编译器**下 FP 运算本身可复现；但迁移会改变函数归属的 TU 与内联决策，`-Og` 下不同内联会改变 x87/FPU 指令序列与中间舍入，末位差异是可预期的。`AGENTS.md:53-54` 也记录了"15.3.1 与 14.2.1 编出的 text 差约 600 B"——跨工具链连整型代码都不"逐位"。
- 修改建议：①G8a 判据＝`rtol ≤ 1e-6 / atol ≤ 1e-6` + **分支/枚举/模式字必须精确相等**（状态机迁移错一拍，数值容差抓不到，但模式字能抓到）；②G8b 同理，且额外要求"逐拍 tick 序列对齐"；③基线必须**同工具链、同 build type** 生成，写进 `arch-baseline/env.txt`；④如果以后有人把 `CMAKE_BUILD_TYPE` 改成 Release（`CMakeLists.txt:90-95` 会挂 `-Ofast`），G8 全部作废——所以 G2 要断言 build type（C-01 第③条）。

#### C-13【高】G1/G5 跨工具链不可比，而 CI 的工具链是浮动版本
- 方案行号：L222-231（G1'/G5 适用"全部批"）。
- 证据：`.github/workflows/c-cpp.yml:15` 用 `carlosperate/arm-none-eabi-gcc-action@v1`（**未锁 release**），本地是 15.3.1（`build/CMakeCache.txt`），PATH 上另有 14.2.1；`refactor_gate.py:41-56` 已从 CMakeCache 解析编译器（这点做对了），但**没有校验"当前 gcc 版本 == 基线 gcc 版本"**，`env.txt` 只是记事本。
- 修改建议：`save-baseline` 写 `gcc --version` + `CMAKE_BUILD_TYPE`；`check` 前置校验两者一致，不一致直接 FAIL 并提示"需重锚基线"；CI 侧锁 `15.3.Rel2`（或与本地一致的 release），并明确"CI 不跑 G1/G5，只跑 G2/G3/G10"（runner 上没有基线工件，这是一种诚实的降级，写进 CI 注释）。

#### C-14【中】G6 与"仅限声明清单"不匹配；默认参数下会静默跳过
- 方案行号：L229（"仅限声明逐字搬家文件清单；接口改写批不适用"）。
- 证据：`check_moved`（`refactor_gate.py:353-385`）签名是 `(ref, max_unmatched)`，无文件清单参数，`git diff -U0 <ref> -- "*.c" "*.h"`（:355）对**所有** .c/.h 生效；`:359-363` 的 `norm()` 丢掉所有以 `#` 开头的行（`#include` 变化对 G6 完全不可见）；`:339-340` 只在传了 `--since` 时调用——**不传就静默跳过**，最终仍打印 `ALL PASS`。也即"接口改写批不适用"这句话没有执行体：谁判定、按文件还是按批、判定错了怎么复核，代码里都没有。
- 修改建议：①`--since` 与 `--paths` 在"纯搬家批"里**必填**，缺失即 FAIL（B0a 这种无搬家的批可显式 `--no-move-check` 记账）；②新增 `--mode move|rewrite`，`rewrite` 时改为"逐文件声明摘要"（记录每文件增删行数，不做多重集比较），并把判定写进批次批准清单（谁判定＝用户批准该批时的那张文件清单，与 L289 的"清单式列出新增/删除/移动文件"合并成一张表）；③`#` 行不整体丢弃，改为"允许 `#include` 路径变化（记录到 DEV），其余预处理行参与比较"。

#### C-15【中】G7 完全未实现；回退标签体系与现有标签的实际状况被高估
- 方案行号：L230（G7）、L296（`arch-before-B{n}` 标签）。
- 证据：`refactor_gate.py` 无任何 DEVLOG/标签检查（全文 455 行，只有 G1~G6 + check-links）。现有标签实测为 `refactor-before-R{0,2,3,4a,5,5b,6,7,8}` 共 9 个（`git tag`），**没有 R1、没有 R4b**——因为 R0+R1 合在一个提交 `f4d47f6`，R4b 取消（`重构总方案.md:374`）。`RM_DEVLOG.md:22` 写的 `refactor-before-R{0,2,3,4a,5,6}` 也不完整。
- 修改建议：①新增 `refactor_gate.py check-tags`：校验本批 DEV 条目存在（`## DEV-YYYYMMDD-NN` 且正文含"AI"与闸门表）、`arch-before-B{n}` 标签存在且 `git rev-parse` 指向上一批收工的 commit；②标签沿用 `refactor-before-R*` 不动（只读历史），新体系从 `arch-before-B0a` 起；③承认"一批一标签"不是硬保证（合并提交就会出现 R1 那种缺口），判据里写"允许一个提交承载多批，但每批必须有独立 DEV 与标签指向该提交"。

#### C-16【中】G9 可以自动化，方案写成纯台架；判据"2s 内"与 IWDG 2s 零余量
- 方案行号：L232（G9）、L36（T03"2s 内复位"）。
- 证据：①IWDG 实测 **2s**（`bsp/bsp_iwdg.c:30-31`：PR=/64 → 500Hz，RLR=1000 → 2s），所以"2s 内复位"在"刚喂完狗就挂"的注入点上，理论值正好 2.0s，测量抖动就会判失败；②复位原因已经是可观测的：`application/robot.c:103` 打 `[boot] RST cause: IWDG=…` 且 `:112` 转成"3 声短鸣"（DEV-20261121-07）；③急停初态与解锁锁存**已经是现状**：`application/cmd/robot_cmd.c:137` `robot_state = ROBOT_STOP`（注释"上电先急停"）、`application/cmd/cmd_authority.c:55,71-73` "任一停止来源都进 ROBOT_STOP，须右上+左中+全回中才解除"——所以 G9 的后半句不是行为变化，是**回归保护项**；④"挂起任一任务"需要一个注入手段：本仓库已有 RTT 下行通道（`tools/rtt_read.py`、`bsp/log`），加一个仅 `DG_TEST` 构建可见的钩子即可。
- 修改建议：G9 判据改为可自动化三步：①构建加 `-DDG_TEST_HANG=1`；②RTT 发 `hang ins|motor|daemon|robot`，任务进入死循环（**不喂心跳**）；③判据：`≤2 × IWDG + 200ms`（即 4.2s 内，留足测量余量）检测到复位、复位原因日志为 `IWDG=1`、且复位后 `robot_state==ROBOT_STOP`、需解锁才出力。执行者：用户在场，探针跑脚本；失败动作：B3 回退并复查心跳位语义（C-08）。**同时把 G9 从"B3 专用"改为"B3 与 M3 各跑一次"**——喂狗策略是被后续批次最容易悄悄破坏的。

#### C-17【中】G4 的执行器与 G-E 退役冲突（与域F F6 同一问题，此处只给执行替代）
- 证据：`run_audit()`（`refactor_gate.py:143-153`）调用 `audit_baseline.py`，后者比对的是上游 manifest 460 条（`audit_baseline.py:4-9`）；而 L213/L215 要"manifest 封存/退役"、L227 要"自基线审计（上批→本批）"。
- 修改建议：`G4` 换成新实现：`save-baseline` 同时落盘**全仓第一方文件清单 + sha256**；`check` 与上批比 → 输出 `moved/added/deleted/changed` 四类，并断言"与批次批准清单一致"。上游 manifest 审计从 B0b 起降级为"历史记录，不进闸门"（封存后只跑一次出报告）。红旗（RedFlags）判据改为"不增加"，且**开工前必须归零或入白名单**（见 C-19）。

### 2.3 行为变化白名单（L234）完备性（评审点 3）

逐项判定（方案现有 3 项 + 你点名的 5 项 + 我补的 2 项）：

| # | 批/改动 | 方案是否列 | 判定 | 建议 |
|---|---|---|---|---|
| 1 | B3 喂狗策略 | ✅ | 行为变化 | 保留；补 G9 自动化与"注册完整性断言"（C-08/C-16） |
| 2 | B7 消息深度档位 | ✅ | 零行为（默认 1，且现 `QUEUE_SIZE 1`） | 保留；**但要补一条**：B7 引入 BASEPRI 临界区 → 中断延迟变化（C-18） |
| 3 | B10 ROS2 | ✅ | 整车无关 | 保留 |
| 4 | B4a FIFO 显式配置 | ❌ | **契约变更**（§3.8 不变量 1 本来靠"注册顺序"隐式成立） | 入白名单，并附"FIFO 归属等价表"（每个电机 ID → 现 FIFO / 新 FIFO，逐行相等）+ 台架 CAN 在线检查 |
| 5 | B4b USART 重构 | ❌ | 零行为目标成立，但**风险极高**：`bsp/usart` 刚在 DEV-20261121-06 重写成循环 DMA 且仍"待实车验证" | 不入行为白名单，但必须入 B0a 的回归清单（先实车验证再迁）；B4b 验收加"错误中断风暴回归"（关遥控器/拔线，`bsp_iic` 那类 `while(1)` 不得出现、CPU 不打满） |
| 6 | B6 遥测独立对象 + `XR_SECTION/CCM` | ❌ | 零行为（段属性宏替换，`TELEMETRY_CCM` 已在 `modules/telemetry/telemetry.h:33-37` 存在） | 不入白名单，但 G1b 必须能看见 `XR_SECTION_*` 新增（否则新宏无人管）；G5 `check-sections` 必须断言 `.ccmbss` VMA（C-11） |
| 7 | B8 profile 字段替代 `ONE_BOARD` | ❌ | **行为变化**：编译期裁剪（`#if`，代码不存在）→ 运行期分支（代码存在、走判断）。且板型实为**三值**：`ONE_BOARD / GIMBAL_BOARD / CHASSIS_BOARD`（`application/robot.c:141-149,172-180`），方案 L101 只写"单板/双板" | 二选一：**(推荐)** B8 阶段仍保留编译期选择（`profile.h` 里用 `#define` 选板型，运行期字段推到 B11），彻底零行为；或入白名单 + G8b + 台架冒烟。无论哪种，L101 的"单板/双板"要改成三值 |
| 8 | B1 新增 cppcheck/clang-tidy | ❌ | **构建变更**（按 `AGENTS.md:27` 需批准），非行为变化 | 不入行为白名单；但必须锁版本、且"仅新增目录、存量豁免"要写成可检查的路径白名单（否则第一天下 300 条存量告警） |
| 9 | `while(1)` → `err_report`（§3.1） | ❌ | **行为变化**（挂死 → 策略出口） | 入白名单；且必须**指定批次归属**（建议 B3 汇总做，见 C-19） |
| 10 | B13? 无 | — | — | 建议白名单表尾加一句"未列入本表的任何改动一律视为行为变化，须先改本表再开工" |

#### C-18【高】B7 的临界区引入没有登记
- 方案行号：L149（"每订阅者 `xr_enter_critical`（单核 BASEPRI）"）、L234（白名单只写"消息深度档位"）。
- 证据：现状 `message_center.c` **完全无锁**（`PubPushMessage`/`SubGetMessage` 全文无临界区），安全前提是"同拍消费"；B7 后每次 publish 会遍历订阅者并短暂关中断（BASEPRI），发布点在 `RobotCMDTask`(200Hz)、`GimbalTask`(200Hz)、`ShootTask`(200Hz)、`ChassisTask`(200Hz)、`INS_Task`(1kHz)。CAN RX 中断与串口 DMA 中断都可能被推迟。
- 为什么是问题：这是**时序行为**变化，G8b 只比控制量、看不见中断延迟；G5 只看体积。一旦最坏延迟吃掉 CAN 1ms 周期预算，症状是偶发掉帧，最难定位。
- 修改建议：入白名单，并加一条可执行验收——用探针/TIM 测"最坏关中断时长"（或最简单：publish 前后翻转一个空闲 GPIO，示波器/逻辑分析仪看脉宽），阈值 ≤ 20µs（CAN 位时间在 1Mbps 下 1µs，留足余量），超了就把临界区收窄到"只保护 ring 索引推进"。

#### C-19【高】`while(1)→err` 的转换没有批次归属，且现存量与方案数字不符
- 方案行号：L35-36（T02"bsp 注册路径 11+ 处裸 while(1)"）、L126（"其余现存 11+ 处全部改 `err_report`"、验收"仅剩白名单 4 处"）、L252（B1 造 err"不接线"）。
- 证据（实测首方站点，排除 B0b 将删除的文件）：`bsp_can.c` 2、`bsp_fault.c` 1、`bsp_pwm.c` 1、`bsp_usart.c` 3、`bsp_tools.c` 1、`buzzer.c` 1、`ins_task.c` 1、`dji_motor.c` 5、`HT04.c` 1 = **16 处**；另有 `Src/main.c` 2、`Src/stm32f4xx_it.c` 5（HAL/启动，可论点不算第一方）；B0b 将删除的 `bsp_iic`(8)/`bsp_spi`(7)/`ist8310`(1) 不计。`message_center.c:24,34` 还有 2 处（B7 删除）。
- 为什么是问题：①"11+"与实测 16 处不符，验收线"仅剩 4 处"要么达不到、要么被悄悄放宽；②更重要的是**没有任何批次负责这批转换**——B1 明说"不接线"，B3 只写"diag/监督"，B4/B5/B6 都宣称零行为。于是它要么被塞进某个"零行为"批（偷偷改行为），要么永远不做（T02 永远 PASS 不了）。
- 修改建议：把转换明确划给 **B3**（它已是行为变化批，白名单第 9 项），范围＝16 处 + `message_center` 2 处（随 B7 消失）；启动期 fail-fast 白名单固定 4 类并逐点加注释锚 `// XR_WHITELIST: <理由>`，使 `grep XR_WHITELIST` 可计数。这样"仅剩白名单 4 处"变成可验证。

**综合判定**：L234 的白名单在"有没有想到"这个层面覆盖了最明显的三项，但在"执行层会不会被绕过"这个层面不够——**缺 4 项（B4a FIFO、B7 临界区、B8 板型、err while(1)）**，且缺一条"兜底条款"。

### 2.4 与现行纪律的冲突（评审点 5）

#### C-20【中】批准粒度：批次批准 ≠ 结构级变更批准，需写明从属关系
- 方案行号：L289（"每批开工前用户明确批准该批（清单式列出新增/删除/移动文件）"）；`AGENTS.md:11`（"每批开工前需用户对该批明确批准"）与 `AGENTS.md:27`（"结构级变更需用户对该次操作明确批准"）。
- 判定：**粒度一致，但方案漏写了从属关系**。`AGENTS.md:27` 在升级期依然生效——批内任何超出批准清单的结构级操作（新增消息字段、改接口、改构建文件）仍须单独批准。
- 修改建议：L289 补一句"批内若出现清单外的结构级变更（`AGENTS.md` 禁区口径），暂停本批并单独申请；不得以'批已批准'覆盖。"

#### C-21【中】B0a 的范围写的是已完成的活，真正欠的活没写
- 方案行号：L250（B0a"实车回归（重构总方案 §9 清单）→ R5b/R4b → 再实车回归"，预估"1 会话+实车×2"）。
- 证据：①R5b 已完成（`RM_DEVLOG.md` DEV-20261120-11）；R4b **已决定不做**（`重构总方案.md:374`"R4b 决定不做"）；②真正的未验证项在 2026-11-21/22：`DEV-20261121-04`(串口半死态修复+0x0208 闸门)、`-05`(错误中断风暴修复)、`-06`(串口循环 DMA 重构 + IWDG)、`-07`(复位原因鸣叫码 + 射速护栏)、`-08`(解析递归栈溢出/HardFault)、`-09`(堆扩容 + 故障详情 ASCII + 静态守护池)、`-11`(射速律/战术档位/UI)、`DEV-20261122-02`(CCM 搬迁)——全部标注"待实车验证"；`RM_DEVLOG.md` 中"待实车/待验证"字样 43 处；③实测审计红旗已从 2 涨到 4（见 C-22）。
- 为什么是问题：把这些未验证的行为改动和后续 12 批迁移混在一起，M3 实车回归若失败，**无法区分是迁移引入的还是 11-21 就带着的**。这是"零行为等价"的前提被污染。
- 修改建议：B0a 重写为"**当前树稳定化 + 8 条未验证改动的实车验证**"：①先把工作树合成可烧录提交（已由 84d51d7 完成大部分）；②按 `DEV-20261121-04~11, 20261122-02` 逐条实车验证并回填；③红旗裁决（C-22）；④修 `RM-012`（`tests/run_ax650_tests.ps1` 桩缺失，CI 持续红）；⑤收工 `save-baseline` 重锚 + 打 `arch-before-B0b`。预估 2~3 会话 + 实车 2 次（失败则 +1 次返修）。

#### C-22【高】两个新红旗必须开工前裁决，否则会被搬进新架构
- 证据（实测 `audit_baseline.py` 输出 `RedFlags: 4`）：`application/chassis/chassis.c: huart1`（新增：`chassis.c:142` `.usart_handle = &huart1` 在 `CHASSIS_VOFA_UART1_ENABLE` 下直触句柄）、`application/gimbal/gimbal.c: huart6`（旧）、`modules/imu/ins_task.c: while(1)`（已裁决 fail-fast）、`bsp/fault/bsp_fault.c: while(1)`（新增，`:61` `NVIC_SystemReset()` 后的兜底 `while(1)`）。
- 为什么是问题：`AGENTS.md` 的句柄口径是"Init 注入允许 / Task 内直接访问禁止"，config 里传 `&huart1` 属允许侧（初始化期），但审计脚本按 token 命中就当红旗——**问题不在代码违规，在于"红旗不归零就无法用 G4 的'红旗不增加'判据"**。B8 会把 `chassis.c` 搬进 `apps/infantry_skeleton/`，红旗跟着搬家，此后永远有一个"历史遗留红旗"豁免，闸门钝化。
- 修改建议：B0a 出口加"红旗 4→2 或全部入白名单"：①`bsp_fault.c:61` 的 `while(1)` 加 `// XR_WHITELIST: reset 后不可返回` 注释（与 ins_task 同款处理），或改 `__builtin_unreachable()`；②`chassis.c` 的 `&huart1` 确认属 Init 注入后，在 AUDIT 侧加路径白名单（改 `audit_baseline.py` 属工具改动，需批准）或在代码处加锚注释；③把"红旗清单 = 2 条已裁决 + 0 条新增"写进 G4 判据。

### 2.5 工作量预估（评审点 4）

标定依据（`git log --date=format:%H:%M` + DEVLOG）：重构期 R0~R8 共 10 批、9 个标签、提交时间 19:31→20:53（约 82 分钟），每批 1 个 DEV 条目、1 个会话。但 **R 批的规模是"1~2 个文件/1 个模块"**（R2 拆 823 行 config、R3 拆 903 行 cmd、R5a 改 11 个 include、R8 删 2 个文件），而 B 批是"新建子系统 + 无测试网 + 需要新验证基建"。所以不能线性外推，但可以定住"**1 会话 ≈ 一个边界清晰、有现成闸门可跑的改动**"这个单位。

#### C-23【高】三个最大失准点
1. **B4（4~6 → 8~11，最大失准）**：一行里塞了 6 件事——CAN 逐函数搬运（`bsp_can.c` 220 行，含双 FIFO/发送队列/ISR 续传）、FIFO 归属显式化+等价证明、USART 循环 DMA 语义搬运（`bsp_usart.c` 237 行，刚重写未验证）、pwm/flash/usb/dwt（`bsp_pwm` 108 + `bsp_flash` 275 + `bsp_usb` 27 + `bsp_dwt` 105）、内嵌 sim-min、G8a 对拍就绪。**其中 sim-min 与 G8a 根本不需要 sim**（C-03），该从 B4 移走；剩下的 B4a/b/c 三步各自仍要 3~4 会话。
2. **B8（5~7 → 8~12）**：它不是"搬迁批"，而是**新交付**：`apps/infantry_skeleton` 装配 + `profile.h` + `robot_task→kernel` + 全链 sim + G8b 自动化 + `ONE_BOARD` 三值改造 + CMake/include/GLOB 扩张（C-01）。sim 整链本身就是一个独立项目（B9 才写"sim 从对拍工具完善为日常回归环境"，说明方案自己也认为 B8 的 sim 只是最小版）。**"整链对拍"与"整车搬迁"压在同一批，是双重高风险叠加**。
3. **B0b（2~3 → 3~5）**：删除只是其一（实测 1870 行 .c/.h，方案写 ~2100 行含文档），还有《工具符号冻结清单》（8 脚本 42 条路径要落表 + 要有检查器，否则是张墙上的表）、G8 金标准样本（C-03 说它需要先建 host harness）、manifest 封存、重锚基线。**四项里有两项是"造新工具"，不是"删代码"。**

补充：**B6（4~6 → 7~10）不算"时间失准"，算"粒度失准"**——9 个组件、约 3800 行、含最敏感的 IMU/INS/EKF，单批失败无法隔离；它需要的是拆（B6a/B6b），不是加时间。B0a 则是**性质失准**（C-21）：活变了，不只是变多。

其余批：B1 3~5 → 4~6（220 宏账本 + 470 处条件编译点的人工分类是真内容，不是脚本一跑就有）；B2 2~3 → 3~4；B3 2~3 → 3~4（要造 G9 注入钩子）；B5 2~3 → 4~6（要先建金标准用例）；B7 2 → 2~3（若前移，含改 8 处主机测试）；B9 2~3 → 3~5；B10 4~6 → 5~8；B11 3~4 → 3~5（三份 profile + 板型三值改造）；B12 2~3 → 3~4。

详细表见 **§五**。

### 2.6 M3/M5 语义（评审点 7）

#### C-24【中】M 编号体系残缺，且与"目录何时删"耦合
- 方案行号：L58（M1→M3）、L80-83（`application/`→M3 后删、`bsp/`→M4 后删、`modules/`→M6 后删）、L261（M3 是批次表里的一行）、L17-24（M5 验收）。
- 证据：全文只定义了 M3（L260-261"整车在新架构上行为等价"）与 M5（L17"出口判据"）；M1（L58）、M4（L82）、M6（L83）无定义，M2 从未出现；`重构总方案.md` 全文无 M 编号（grep `\bM[1-6]\b` 零命中，它是 R 体系）。M5 在批次表里**没有行**（表止于 M3 与 B12）。
- 修改建议：①删掉 M1/M4/M6 的引用，改成 B 批锚点："`application/` 于 B8 平移后删除（保留一个标签 `arch-before-B8` 的完整树）；`bsp/` 于 B4c 收工后删除；`modules/` 于 B7 删除 message_center、B12 删除其余";②M5 在批次表补一行：`| M5 验收 | G-A~G-F 逐条判定 | 全量 | §0 出口判据表 | 用户+脚本 |`，排在 B12 之后；③§0 的 G-A~G-F 每条补"证据形式"（脚本输出/实车记录/DEV 编号），否则 M5 无法判定——现在只有 G-F 有明确证据形式（闸门+M3），G-B/G-C 是"可编译/跑通"，G-D/G-E 是"存在性"，粒度不齐。
- 具体缺口：G-C 要求"④Linux socketCAN/termios 端口编译通过；⑤ROS2 桥最小可用"（B10）、"②裸机后端"（B9）、"③x86 仿真 cmd→chassis 闭环对拍"（B8/B9），所以 **M5 必须在 B9/B10/B11 之后**，而批次表把 B9~B12 排在 M3 之后、M5 没有位置——按现在的表读完，读者会以为 M3 过了就快结束了。

### 2.7 §7 衔接（评审点 8）

#### C-25【中】插队/暂停/恢复机制不可操作
- 方案行号：L272（"比赛/调试急务可插队，插队须记'升级暂停/恢复'DEV 条目，回退标签体系不受影响"）。
- 证据：没有定义谁判定、暂停点在哪、恢复后闸门怎么办。而现实中 B0a 已经演过一次：2026-11-20 重构收尾后紧接着 11-21 一整天插入 8 条行为改动（`DEV-20261121-04~11`），直接把"零行为基线"冲掉——**这就是插队的真实代价，方案没有从这次事故里吸取规则**。
- 修改建议（三条硬规则，可直接贴进 §7）：
  1. **插队前必须先把当前批推进到可编译、可烧录、闸门全绿的提交**（不允许留半截工作树），打 `arch-pause-B{n}` 标签；未到可编译状态时插队请求默认拒绝。
  2. **插队改动若触碰"本批迁移中的文件"**（由该批批准清单判定），恢复后该批的 G5 基线作废，必须重锚并整批重跑闸门；DEV 条目必须写明"哪些文件被插队改过"。
  3. **插队期间的功能改动视同"对相应功能的临时解冻"**，须用户单独批准；若产生行为变化，恢复后必须补进 §5 白名单并补验证，否则该行为改动不得进入迁移基线（`arch-before-B{n}` 只允许建立在已验证的树上）。

### 2.8 风险登记册与数字可复现性

#### C-26【高】§8 现有 7 条普遍缺"触发阈值/退出判据"
- 方案行号：L276-284。
- 证据：7 条里只有 L279（RAM）有量化数字，且该数字的解读有误（见 C-27）；其余是"对策名"（"每批零行为闸门 + G8a/G8b"、"排 M3 后"、"框架 API 面刻意小"）。按你的判据（责任动作/触发阈值/退出判据）逐条核：**0 条完整、2 条有动作无阈值、5 条只有方向**。
- 修改建议：保留原 7 条，但每条补三列（触发阈值/责任动作/退出判据），并新增 §四 的 8 条。

#### C-27【高】RAM 风险"降级为低"的结论只对一半：CCM 已 84%
- 方案行号：L279（"RAM/Flash 预算｜**低（降级）**：CCMRAM 搬迁后 91%→49%（DEV-20261122-02，待实车）"）。
- 证据（实测当前 ELF）：`.ccmbss = 54944 / 65536 = 83.8%`（CCMRAM 区），主 RAM `= .data 852 + .bss 22560 + ._user_heap_stack 40960 = 64372 / 131072 = 49.1%`，Flash `= 114824 / 1048576 = 10.95%`。`._user_heap_stack` 的 40KB 来自链接脚本 `_Min_Heap_Size = 0x5000` + `_Min_Stack_Size = 0x5000`（`STM32F407IGHx_FLASH.ld:58-59,205-211`），是纯预留（FreeRTOS 自带堆在 CCM），**可回收**。
- 为什么是问题：①"91%→49%"只描述了主 RAM，把预算约束从"主 RAM"搬到了 CCM，**CCM 现在只剩 12KB**，而 CCM 里放的是遥测环（27KB）+ FreeRTOS 堆（28KB），这两者任何一个膨胀都会撞墙；②`size` 的 bss 合计抓不到 CCM/主 RAM 的互换（C-11），所以"段属性丢失"这条风险目前**没有任何自动检测**；③把风险降为"低"会让后续批次不再看这个数。
- 修改建议：风险等级改"**中**"，触发阈值写死：CCM > 90%（即 >59KB）或 `.ccmbss` 段 VMA 越界或 `.ccmbss` 段消失 → 该批不合入；主 RAM > 70% → 告警。对策加 `check-sections`（C-11）+ 每批 G2 附 `--print-memory-usage` 两区数字进 DEV。

#### C-28【中】§3.9 的关键数字不可复现（"186 个 #if 点"与"6 个文件"）
- 方案行号：L7、L191（"config 220 宏/全仓 186 个 `#if` 点/开关散布至 6 文件"）。
- 证据（实测）：config 宏 **220** ✔（与 `-dM` 非下划线项 220 一致）；但全仓条件编译点：`#if/#ifdef/#elif`（排除 `#ifndef` 守卫）= **470**，含 `#ifndef` = **657**；`ONE_BOARD` 引用 = **7 个活文件 29 处**（`robot.c` 6、`robot.c/h` 1、`robot_def.h` 3、`chassis.c` 11、`chassis_kinematics.c` 1、`robot_cmd.c` 6、`board.h` 1），不是 6 个文件。
- 为什么是问题：`T14` 的账本与 `check_macros.py` 要以这些数字为验收基线；数字不可复现，工具写出来也没法判"全绿"。R2 的教训（`参数证据表.md` 写"85 个宏"实际 154）已经发生过一次。
- 修改建议：把口径写进方案——"#if 点 = `^\s*#\s*(if|ifdef|elif)\b`（不含 include 守卫）"并给复现命令；文件数与处数改为 `check_macros.py` 的输出为准（B1 交付时空表，跑出来填）。220 这个数可以保留（已可复现）。

### 2.9 三条"必须进册但方案完全没提"的工程风险

#### C-29【高】CubeMX 再生成会覆盖 `Inc/FreeRTOSConfig.h` 里三处手改，且都在 USER CODE 之外
- 方案行号：L283（§8 现有条目"`.ioc` 再生成冲突 ports 层"，对策"ports 只经 USER CODE 区/注册制引用句柄"）。
- 证据：`Inc/FreeRTOSConfig.h:67` `configTOTAL_HEAP_SIZE ((size_t)28000)`、`:71` `configAPPLICATION_ALLOCATED_HEAP 1`、`:85` `configCHECK_FOR_STACK_OVERFLOW 2`——**三处都不在 `USER CODE BEGIN/END` 标记内**（该文件的 USER CODE 标记只有 `Header/Includes/MESSAGE_BUFFER_LENGTH_TYPE/1/Defines` 五处）；仓库根有 `basic_framework.ioc`（25434 B），CubeMX 重生成时按模板重写非 USER CODE 区。
- 为什么是问题：①`configAPPLICATION_ALLOCATED_HEAP` 回 0 → heap_4 内部再定义一个堆，而 `Src/freertos.c:52` 的 `ucHeap` 仍在 `.ccmbss`，**两个 28KB 数组/符号**，要么链接冲突，要么主 RAM 悄悄 +28KB（DEV-20261122-02 的收益全丢）；②`configTOTAL_HEAP_SIZE` 回默认（15360）→ 任务/队列创建在启动期失败，"启动即 HardFault"这类症状会重演（DEV-20261121-09 就是为它扩容两次）；③方案现有对策只覆盖 ports 层句柄，**不覆盖 Inc/ 里的配置常量**——这是对策错位。
- 修改建议：B1 在 `Src/freertos.c`（USER CODE 区）加两道守卫：`_Static_assert(configTOTAL_HEAP_SIZE >= 28000, "CubeMX regenerated FreeRTOSConfig.h")` 与 `#if !configAPPLICATION_ALLOCATED_HEAP #error "..." #endif`；`.ioc` 每次再生成后**强制跑 G2+G5c**，并把 `git diff --stat Inc/` 非空作为"需人工复核"的触发条件。

#### C-30【中】测试脚本的源列表/包含路径硬编码，迁移批会打断 G3
- 方案行号：L226（G3）、L252-259（B1~B8 都会移动/删除文件）；附B 第 5 条（L318）只写"CMake 增 target；Makefile 同步登记"，**没提测试脚本**。
- 证据：`tests/run_chassis_tests.ps1:6-16` 硬编码 16 个 include 目录与 11 个源文件（含 `modules/message_center/message_center.c`、`modules/telemetry/telemetry.c`、`modules/daemon/daemon.c`、`application/cmd/*.c`）；`tests/chassis_remote_test.c` 自身还有 8 处 message_center API 调用（C-04 实测计数）。
- 为什么是问题：G3 失败是"响亮的失败"（好），但方案没有把它写成"每批必做项"，实操中会被当成"测试基建坏了、先跳过"——而 G3 是 B5/B6/B7 唯一的自动化行为网。B7 删 `message_center` 时若只改固件不改测试，`run_chassis_tests.ps1` 直接编不过，整条 G3 失效。
- 修改建议：把"**测试脚本源列表/包含路径与本批文件布局一致**"写进 G3 判据（三、已给）；B1 顺手把两个 ps1 的源列表改为**目录 glob**（与 CMake 同口径），从根上消掉手工同步。

#### C-31【高】CMake/Makefile 双构建漂移没有被收口，而它直接影响"零行为"的定义
- 方案行号：L225（G2"ninja"）、L319（附B"Makefile 同步登记"）；`AGENTS.md:57`（推荐 CMake+Ninja）。
- 证据：`Makefile:229` 带 `-DDISABLE_LOG_SYSTEM`（RTT 日志整体编译掉），CMake **不带** → 两个产物**日志行为不同**（`bsp/log/bsp_log.h` 在 `DISABLE_LOG_SYSTEM` 下 LOGINFO 为空）；`Makefile:35` 输出 `build_make/`（R6 已分离），但 CI 仍然先 `make -j12` 再 cmake+ninja（`.github/workflows/c-cpp.yml:19-24`），两套都算"通过"；`tools/check_source_lists.py:20` 的 `TREES` 硬编码 `Drivers/Src/Middlewares/bsp/modules/application`，**新目录永远不在检查范围**。
- 为什么是问题：①"零行为"必须指定**以哪个构建为准**——现在 `.vscode/tasks.json` 走 make（且本机无 `mingw32-make`，DEV-20261120-09 已记录），闸门走 cmake，用户烧的可能是日志被编译掉的那一版；②`bsp_log` 的 `%f` 禁令、`RttUpBufferSane` 健全性检查、`[heat]/[chcal]/[tel]` 判据全部依赖日志存在——用 make 产物做实车回归，判据根本观察不到；③新目录不进 TREES 意味着 B1 之后"源列表一致性"检查形同虚设。
- 修改建议：二选一，**推荐退役 Makefile**（仓库已 CMake 收口、CI 也在跑 cmake+ninja、本机没有 make）：B6 之前把 `.vscode/tasks.json`/烧录脚本切到 `build/`，Makefile 标记 archived；若必须保留，则加 `check_build_parity.py`（比对两边 `-dM` 的关键开关集合 + 目标文件集合），并让 CI 只在**同一开关集**下判定通过。同时 B1 把 `check_source_lists.py` 的 `TREES` 扩成参数化。

---

## 三、建议的批次/闸门修订

### 3.1 批次表（可直接替换 §6 的 L248-264；最后一列"改动"粘贴时删掉）

| 批 | 目标 | 范围要点 | 闸门 | 预估 | 改动/理由 |
|---|---|---|---|---|---|
| **B0a** | 当前树稳定化 + 未验证改动实车验证 | ①按 `DEV-20261121-04~11`、`DEV-20261122-02` 逐条实车验证（8 条行为改动，全部"待实车"）；②裁决 2 个新红旗（`chassis.c:142 &huart1`、`bsp_fault.c:61 while(1)`）；③修 `RM-012`（ax650 测试桩）；④**收工 `save-baseline` 重锚 + `arch-before-B0b`** | G1a~G7；红旗 4→2 | 2~3 会话 + 实车×2 | ▲ 原写"R5b/R4b"（R5b 已完成、R4b 已取消），改成真实欠账；基线必须在此重锚（C-02） |
| **B0b** | 清理与锚定（删除 3 提交 + 工具 1 提交，各自过闸） | ①**删除分 3 提交**：空桩（unicomm/standard_cmd）、bsp 三件套（spi/iic/gpio）、OLED+IST8310+蓝牙+TFmini+LK（实测 1870 行 .c/.h，非 ~2100 行文档口径）；每提交按 R8"四边取证"（`DEV-20261120-14`），text 必须 +0；②《工具符号冻结清单》= 实测 42 条 `->` 路径（6 脚本）+ 2 脚本全局名 + `odom.js`/`doctor.js`；③**`tools/check_symbol_contract.py`**（清单可执行化）；④**host 整链 harness + 输入序列 + 输出 CSV**（G8b 基线，替代"sim 采样本"）；⑤manifest 封存；⑥冻结基线入 `.Doc/gate/arch-baseline/` 并提交 | G1a+G4+G5+G7；新增 G3 符号契约 | 3~5 | ▲ 拆 3 提交、加 2 个工具、基线入库（C-02/C-03/C-06/C-22） |
| **B1** | 框架地基 | 目录骨架；`err`+`kernel`(FreeRTOS)+`mem`；`check_macros.py`+宏账本（口径见 C-28）；cppcheck/clang-tidy 入 CI（锁版本、仅新目录）；定稿 §3.7/§3.8 契约表；**CMake 扩 `framework/ports/apps/components` 到 include 递归与 GLOB；`check_source_lists.py` 扩 TREES**；`Src/freertos.c` 加 `_Static_assert(configTOTAL_HEAP_SIZE>=28000)` 与 `configAPPLICATION_ALLOCATED_HEAP` 守卫；**不接线** | 零行为（G1a 逐字节 + 编译单元数断言） | 4~6 | ▲ 加 CMake/TREES/FreeRTOSConfig 守卫（C-01/C-29/C-31） |
| **B2** | msg v1 | 框架+主机单测+适配头（旧 API 转发），**不切源码** | 零行为 | 2~3 | 不变 |
| **B3** | diag/监督 + err 接线 | 心跳位掩码+IWDG 切换；daemon 并入；**16 处 `while(1)`→`err_report`（含白名单 4 处加锚注释）**；注册完整性断言；台架 G9 **自动化**（`-DDG_TEST_HANG` + RTT `hang` 命令） | **行为变化批**：G9 + 白名单登记 | 3~4 | ▲ 明确接管 while(1) 转换 + G9 自动化（C-16/C-19） |
| **B4a** | CAN 总线 | bus 接口 + `ports/stm32f4` CAN 逐函数搬运；FIFO 归属显式配置 + **等价表**；`SECTION_CCM/XR_SECTION` 段宏落 ports；bsp_can 的 2 处 `while(1)` 归 err（随 B3 规则） | 零行为（语义清单逐条）+白名单第 4 项 | 3~4 | ▲ 去掉 sim-min（移 B0b/B9）；补段宏与 FIFO 等价表（C-11/C-18/C-19） |
| **B4b** | USART 总线 | 循环 DMA 消费语义逐条搬运；§3.8 不变量 3/4/6 入清单；**错误中断风暴回归**（关遥控/拔线） | 零行为 + 台架 | 3~4 | ▲ 加风暴回归（§2.3 第 5 项） |
| **B4c** | 其余总线 | pwm/flash/usb/dwt（SPI/IIC/GPIO 已于 B0b 删除）；收工后 `bsp/` 可删 | 零行为 | 2~3 | ▲ 明确 bsp/ 删除时点 |
| **B5** | 组件 I | **批首：用旧代码生成金标准用例并入库**；DJI/GM6020/PID/蜂鸣迁 `components/`；注册改 bus/err；§3.8 不变量逐条；**`check_symbol_contract.py` 全绿**；批尾跑 host 整链 harness | 零行为 + G8a + 契约 | 4~6 | ▲ 金标准前置 + 契约脚本化（C-05/C-06） |
| **B6a** | 组件 II-敏感 | IMU/BMI088 + INS + EKF（QuaternionEKF/kalman，启动期 malloc 豁免登记）；`gimba_IMU_data` 字段路径冻结 | 零行为 + G8a + 契约 | 4~5 | ▲ 拆批（C-23） |
| **B6b** | 组件 II-其余 | 遥控/裁判(含 UI)/AX650/遥测(独立对象+XR_SECTION)/标定/超电桩；`telemetry` 段宏替换需 `check-sections` | 零行为 + 契约 | 3~5 | ▲ 拆批（C-23） |
| **B7** | 消息切换（**前移到 B2 之后**） | 25 处生产调用点 + `tests/chassis_remote_test.c` 8 处切 msg；删适配头与 `message_center`；深度默认 1；**测最坏关中断时长（≤20µs）** | 零行为 + 白名单第 2/新增临界区项 | 2~3 | ▲ 顺序前移（C-04）；B5/B6 因此只需迁一次 |
| **B8** | 应用装配 | `robot_task→kernel+profile`；`application/`→`apps/infantry_skeleton/`；`profile.h` 装配表（**板型三值**）；**CMake GLOB/include 扩 apps**；sim 整链 + G8b（比 B0b 的输入序列与 CSV）；`RCC->CSR` 复位原因回归 | 零行为+G8b（+白名单第 7 项，若运行期化） | 8~12 | ▲ 最大失准点，单独拆 B8a 装配 / B8b sim 更佳（C-01/C-23） |
| **M3** | 实车回归 | `重构总方案 §9` 同款清单 + **G9 复验** + trace 脚本全绿 | 实车 | 用户执行 | ▲ 加 G9 与契约复验（C-16） |
| **B9** | 裸机后端 + sim 完善 | kernel baremetal；sim 从"对拍工具"→日常回归（脚本化场景注入） | sim 无整车风险 | 3~5 | 工作量上修 |
| **B10** | Linux/ROS2 | ports/linux（socketCAN/termios）；ROS2 桥最小可用；**只走 socketCAN/以太网**（`bsp/usart.h:24` `DEVICE_USART_CNT 3` 已满） | 编译+桥自测 | 5~8 | 工作量上修 |
| **B11** | 三份骨架 profile | infantry/heavy/sentry；**板型运行期化（若 B8 未做）** | 三 profile 编译 | 3~5 | ▲ 接住 B8 推迟的运行期化 |
| **B12** | 脱离上游收官 | 死桩裁决、AGENTS/Skill/code-map v2、新基线锚定、版权审查；**b12 收尾 `check-tags` 全套** | G1a+G4+G5+G7+check-links | 3~4 | 上修 |
| **M5** | 验收 | G-A~G-F 逐条判定 + 证据清单（脚本输出/实车记录/DEV 编号） | §0 出口判据表 | 用户+脚本 | ▲ **新增行**（C-24） |

**关键路径（修订后）**：`B0a → B0b → B1 → B2 → B7 → B4a → B4b → B4c → B5 → B6a → B6b → B8 → M3`；B3 仍可与关键路径并行，但**必须在 B8 之前收工**（心跳位要接 profile）。B9/B10/B11 在 M3 后并行，M5 最后。

**依赖补充（原表缺的 4 条）**：
1. `B1 → 所有后续批`（err/kernel 是 bus/diag 的前置，原表隐含但没写"B3 依赖 B1"）。
2. `B4a → B4b/B4c`：三段共用 `ports/stm32f4` 的实例头与注册；原表写成"B4a→B4b/c"没错，但 B4b/B4c 之间**无依赖**，可任意顺序。
3. `B2/B7 → B5/B6`：迁移批必须在新 msg API 上写（这正是 B7 前移的收益）。
4. `B0b → B0a 的基线重锚`：B0b 冻结基线必须在 B0a 实车验证**通过之后**采，否则冻的是未验证行为（C-02/C-21）。

### 3.2 闸门表（可直接替换 §5 的 L222-232；最后一列粘贴时删掉）

| 闸 | 内容 | 适用 | 通过判据 | 改动/理由 |
|---|---|---|---|---|
| **G1a** | 参数宏逐字节一致（config 伞头 `-dM`，含内建） | 全部批 | 与基线 `env.txt`/`macros.txt` **逐字节相等**；工具链版本不一致 → FAIL（提示重锚） | ▲ 原 G1' 拆分；B0a/B0b 可用形态（C-09/C-13） |
| **G1b** | 框架宏账本守恒 | B1 起 | `tools/macro_probe.c`（include config 伞头 + framework/components 公开头）`-dM`：**新增允许、删除 FAIL、同一定义值变 FAIL**；四类账本由 `check_macros.py` 校验（A 类逐字节/B 类声明/C 类位置白名单） | ▲ 新增；原 G1' 对框架宏无效（C-09） |
| **G2** | ARM 全量编译 | 全部批 | `cmake -G Ninja -B build && ninja -C build`；0 error/第一方 0 warning；**编译单元数 == 磁盘第一方 .c 数**；`CMAKE_BUILD_TYPE=Debug` 且编译期无 `-flto` | ▲ 加三条断言（C-01/C-12） |
| **G3** | 主机测试 + 工具契约 | 全部批 | 现有两脚本 + `run_fwk_tests.ps1`(msg/err/mem) ALL PASS；**测试脚本的源列表/包含路径与本批文件布局一致**；**`check_symbol_contract.py` 全绿** | ▲ 加后两条（C-06/C-30） |
| **G4** | 自基线文件审计 | 全部批 | 与上批"文件清单+sha256"比：`moved/added/deleted/changed` 与本批批准清单一致；**RedFlags 不增加**（白名单 2 条已裁决）；manifest 审计 B0b 后仅作历史记录 | ▲ 换执行器，红旗硬判据（C-17/C-22） |
| **G5a** | 纯搬家档 | 声明 move 的文件 | 符号名集合相同 + 每符号 `(type,size)` 相同（地址不参与）；`--allow-linkage` 须逐符号打印进 DEV | ▲ 删"逐字节"措辞（C-11） |
| **G5b** | 实现替换档 | B4~B8 | `--budget` 按批声明（默认 `text+rodata ≤ +1%`；B4a/B4b/B5/B6 ≤+1%，B7/B8 ≤+2%），超出须逐项解释；全期累计上限 text +8KB | ▲ 现在没有阈值（C-10） |
| **G5c** | 分区与段属性 | 全部批 | `check-sections`：`.ccmbss` VMA∈[0x10000000,0x10010000) 且存在；`size -A` 两区合计 CCM ≤90%、主 RAM ≤70% | ▲ 新增（C-11/C-27） |
| **G6** | 纯搬家行多重集 | 声明 move 的文件清单（`--paths`） | `--mode move` 时 `--since`+`--paths` **必填**；多重集相等；`#include` 变化单列到 DEV；`--mode rewrite` 时改记逐文件增删行数 | ▲ 加参数与必填（C-14） |
| **G7** | 记录与回退 | 全部批 | `check-tags`：本批 DEV 条目存在（含模型名、闸门表、未执行项）且 `arch-before-B{n}` 指向上一批收工 commit | ▲ 自动化（C-15） |
| **G8a** | 纯函数对拍 | B0b 建基线；B5/B6/B8 强制 | 期望值来自**迁移前旧代码**（B0b 入库）；`rtol/atol ≤1e-6` + **分支/枚举/模式字精确相等**；同工具链同 build type | ▲ 容差取代"逐位"（C-12） |
| **G8b** | 整链对拍 | B8 强制 | 基线 = B0b 入库的输入序列 + 输出 CSV；sim 重放逐拍对齐；容差同 G8a；tick 序列必须一致 | ▲ 基线来源写死（C-03） |
| **G9** | 看门狗注入 + 急停初态 | B3、M3 各一次 | `-DDG_TEST_HANG` + RTT `hang <task>`；`≤2×IWDG+200ms`（≈4.2s）内复位；复位原因日志 `IWDG=1`；复位后 `robot_state==ROBOT_STOP` 且需右上+左中+全回中解锁 | ▲ 自动化 + 余量 + 复验（C-16） |
| **G10** | 构建一致性 | 全部批 | `check_source_lists.py --trees bsp,modules,application,framework,components,ports,apps` 无漂移；Makefile 与 CMake 的关键开关一致，**或 Makefile 已退役** | ▲ 新增（C-01/C-31） |

**行为变化批白名单（替换 L234）**：

> **行为变化批白名单**（不能宣称零行为，须单独授权 + 单独验证）：
> B3（喂狗策略；`while(1)`→`err_report` 策略出口）、
> B4a（CAN FIFO 归属从"注册顺序推导"改为显式配置，须附等价表）、
> B7（消息深度档位 opt-in 保持默认 1；发布路径引入 BASEPRI 临界区，须实测最坏关中断 ≤20µs）、
> B8（仅当板型改为运行期 profile 字段时；若保留编译期选择则不算）、
> B10（ROS2 新功能，无整车影响）。
> **未列入本表的任何改动一律视为行为变化，须先改本表再开工。**

---

## 四、风险登记册补充条目（可直接粘贴进 §8；🔺 为新增行）

| 风险 | 等级 | 对策（触发阈值 → 责任动作 → 退出判据） |
|---|---|---|
| 🔺 **工具符号静默失效**（B5 重写 `motor_controller.speed_PID`、B6 搬 `gimba_IMU_data` → 6 个 trace 脚本按 DWARF 成员路径直读，读不到不报错） | 高 | 触发：`check_symbol_contract.py` 任一清单路径解析失败，或 trace 脚本 `variables.read` 返回 `NO_SAMPLES`。动作：该批不合入，先改工具或先切遥测环（`§3.5`）。判据：42 条 `->` 路径 + `motor_*`/`yaw_motor`/`pitch_motor` 全局名全部可解析。清单来源：B0b 实测（`chassis_trace`/`follow_trace`/`gimbal_axis_probe`/`gimbal_trace`/`pitch_calib`/`pitch_trace` 6 个脚本） |
| 🔺 **CubeMX 再生成覆盖 `Inc/FreeRTOSConfig.h`**（手改项都在 USER CODE 之外：`configTOTAL_HEAP_SIZE 28000`(:67)、`configAPPLICATION_ALLOCATED_HEAP 1`(:71)、`configCHECK_FOR_STACK_OVERFLOW 2`(:85)；`.ioc` 在仓库根） | 高 | 触发：`.ioc` 再生成后，`configAPPLICATION_ALLOCATED_HEAP` 回 0（`Src/freertos.c:52` 的 `ucHeap` 与 heap_4 内部堆**双定义**→链接失败，或堆回到主 RAM → RAM 弹回 91%）。动作：B1 在 `Src/freertos.c` 加 `_Static_assert(configTOTAL_HEAP_SIZE >= 28000)` + `#if !configAPPLICATION_ALLOCATED_HEAP #error`。判据：`.ioc` 每次再生成后必跑 G2+G5c；`configTOTAL_HEAP_SIZE` 不得 < 28000 |
| 🔺 **CCM 预算耗尽 / 段属性丢失**（`.ccmbss` 已 54944/65536 = 83.8%，内含遥测 27KB + FreeRTOS 堆 28KB；`size` 的 bss 合计看不出来） | 高 | 触发：CCM >90%（>59KB）或 `.ccmbss` 段 VMA 越界或段消失。动作：G5c `check-sections` 阻断，回退本批。判据：`.ccmbss` 在 0x10000000 起、`--print-memory-usage` 两区数字进 DEV |
| 🔺 **工具链漂移使 G1/G5 失效**（本机 15.3.1，PATH 另有 14.2.1，text 差约 600B；CI 用未锁版本的 `arm-none-eabi-gcc-action@v1`） | 高 | 触发：`gcc --version` ≠ 基线 `env.txt`，或 CI 与本地版本不一致。动作：闸门拒绝比对，提示重锚；CI 锁 15.3.Rel2 且只跑 G2/G3/G10（不上 G1/G5）。判据：每批 DEV 记录工具链版本 |
| 🔺 **CMake/Makefile 双构建漂移**（Makefile 带 `-DDISABLE_LOG_SYSTEM`，CMake 不带 → 两个产物**日志行为不同**；`check_source_lists.py:20` 的 TREES 硬编码，新目录永不检查；CI 两条都跑） | 高 | 触发：两边 `-dM` 关键开关不一致，或新目录不在 CMake GLOB/TREES。动作：B1 扩 TREES + 加 `check_build_parity.py`；或**在 B6 之前退役 Makefile**（推荐：仓库已 CMake 收口，`.vscode/tasks.json` 用的 `mingw32-make` 本机不存在）。判据：单构建出口或两出口开关一致 |
| 🔺 **过渡期日志带宽打满 RTT 1024B**（`[tel]` 5 环 × 2Hz + `[chcal]` + `[heat]` + `[FAULT]` ASCII 3 连发） | 中 | 触发：一次 RTT 会话内 `[tel]` 行计数不连续（丢行）或 RTT up buffer 溢出。动作：过渡期 `TELEMETRY_LOG_DIV` 100→200、只留 yaw/pitch 两环日志；把字节率写进 B 类账本（B1）。判据：连续 60s 无丢行 |
| 🔺 **`while(1)` 行为转换被塞进"零行为批"**（16 处首方站点无批次归属） | 高 | 触发：某"零行为"批的 diff 中出现 `err_report` 替换 `while(1)`。动作：拒绝合入，移交 B3 并按白名单登记。判据：`grep -c 'XR_WHITELIST'` = 4；`err_report` 只出现在白名单批 |
| 🔺 **单人独岗 + 验证积压**（当前 43 处"待实车/待验证"；连续多批无实体验证时"零行为"只是纸面） | 中 | 触发：连续 3 批无台架/实车验证。动作：冻结开新批，先还验证债。判据：每批 DEV 必须写"未执行项"；≥3 批积压即停 |
| 行为漂移（原 L278） | 高 | **补阈值**：G8a/G8b 任一 FAIL、或台架冒烟三项（CAN 反馈在线/零指令零输出/`err_cnt` 不增长）任一异常 → 回退本批。判据：M3 通过 |
| B3 误伤（原 L280） | 高 | **补阈值**：G9 复位时间 >4.2s 或启动期误复位 → 回退；DBGMCU 冻结路径必须在调试器暂停时不复位。判据：G9+B3 台架 PASS |
| `.ioc`/ports 冲突（原 L283） | 中 | **补阈值**：再生成后 `git diff Inc/` 非空 → 人工复核三项（堆/栈溢出检测/堆分配宏）。判据：再生成后 G2+G5c PASS |

---

## 五、工期重估表

单位＝AI 协作会话（沿用 L246 口径）；"✅"＝本次维持，"▲"＝上修，"✦"＝拆分/新增。

| 批 | 原估 | 重估 | 依据 |
|---|---|---|---|
| B0a | 1 会话 + 实车×2 | **2~3 会话 + 实车×2（失败 +1）** ▲ | 范围实质变了：8 条待验证行为改动 + 2 红旗 + RM-012（C-21/C-22） |
| B0b | 2~3 | **3~5** ▲ | 删 1870 行（分 3 提交）+ 冻结清单 + 契约检查器 + host 整链 harness + 基线入库（C-03/C-06） |
| B1 | 3~5 | **4~6** ▲ | 220 宏/470 条件编译点分类是真内容；加 CMake/TREES/FreeRTOSConfig 守卫（C-01/C-28/C-29） |
| B2 | 2~3 | **2~3** ✅ | msg+单测+适配头，边界清晰 |
| B3 | 2~3 | **3~4** ▲ | 加 16 处 `while(1)` 转换 + G9 注入钩子（C-16/C-19） |
| B4a | 4~6（B4 合计） | **3~4** ✦ | 去掉 sim-min，只做 CAN + FIFO 等价 + 段宏 |
| B4b | ↑ | **3~4** ✦ | USART 搬运 + 风暴回归 |
| B4c | ↑ | **2~3** ✦ | 其余总线 |
| B5 | 2~3 | **4~6** ▲ | 金标准用例前置 + 契约脚本 + 整链 harness（C-05） |
| B6a | 4~6（B6 合计） | **4~5** ✦ | IMU/INS/EKF 单独一批 |
| B6b | ↑ | **3~5** ✦ | 遥控/裁判/UI/AX650/遥测/标定/超电 |
| B7 | 2 | **2~3** ▲ | 前移后含改 8 处主机测试调用（C-04） |
| B8 | 5~7 | **8~12** ▲▲ | 装配 + kernel + profile + sim 整链 + G8b + CMake 扩张（最大失准点，C-23） |
| B9 | 2~3 | **3~5** ▲ | sim 从对拍工具→日常回归环境 |
| B10 | 4~6 | **5~8** ▲ | Linux 端口 + ROS2 桥 |
| B11 | 3~4 | **3~5** ▲ | 三份骨架 + 板型运行期化（若 B8 未做） |
| B12 | 2~3 | **3~4** ▲ | 死桩裁决 + 文档全面改写 + 基线收尾 |
| **合计（B0a~B12）** | **38~56** | **58~86** | ≈ ×1.5；不含实车/台架等待日 |
| 其中到 M3（关键路径） | 未给 | **41~60 会话** | B0a,B0b,B1,B2,B7,B4a/b/c,B5,B6a/b,B8 |

**实车/台架事件（方案只写了 3 次实车，实际需要 8~10 次实体验证，每次都要车+探针+用户在场）**：

| # | 事件 | 批次 | 判据 |
|---|---|---|---|
| 1~2 | 实车回归 | B0a | `重构总方案 §9` 9 项 + 8 条 DEV 逐条 |
| 3 | 台架 G9 | B3 | 注入→≤4.2s 复位→急停态→需解锁 |
| 4 | 台架串口风暴 | B4b | 关遥控/拔线：不风暴、CPU 不打满、急停有效 |
| 5 | 台架冒烟 | B5 | CAN 在线/零指令零输出/`err_cnt` 不增 |
| 6 | 台架冒烟 | B6b | 同上 + trace 脚本可读 |
| 7 | 台架冒烟 | B8 | 同上 + G8b 一轮 |
| 8 | **实车 M3** | M3 | 全清单 + G9 复验 |
| 9~10 | 预留返修 | B0a/B8 | 失败后复测 |

**日历估算（给两个假设，便于选排期）**：
- 假设 A（每周 4 个会话 + 每周 1 次实体验证）：**约 15~21 周（3.5~5 个月）**到 B12；到 M3 约 **10~14 周**。
- 假设 B（每天 1 个会话、实体验证看车的可用性，比赛季 2026-12~2027-03 有插队）：**5~7 个月**到 B12；M3 落到赛季后。
- 若只求"能上车跑的新架构"（砍 B9/B10/B11/B12 的非必要项）：**41~60 会话 + 3 次实车**，约 2.5~3.5 个月。

---

## 附A　复现命令（全部只读）

```powershell
# 基线状态与过期证据
Get-Content build\refactor_baseline\git_rev.txt -Encoding utf8      # → 9f84e08(R7)
Get-Content build\refactor_baseline\size.txt -Encoding utf8        # → text 110960/data 27776/bss 80144
python tools/refactor_gate.py check --no-build --no-tests           # → G1 新增30/消失12;G4 295/104/61/61/4

# 宏集合口径(628 行 = 220 工程宏 + 408 内建)
& C:/Toolchain/bin/arm-none-eabi-gcc.exe -E -dM -Iconfig config/robot_infantry_2026.h

# 当前体积与分区(.ccmbss 54944 / 主 RAM 49.1% / CCM 83.8%)
& C:/Toolchain/bin/arm-none-eabi-size.exe build\basic_framework.elf
& C:/Toolchain/bin/arm-none-eabi-readelf.exe -S build\basic_framework.elf

# 编译期 FLAGS 无 -flto、build type=Debug
Select-String -Path build\build.ninja -Pattern '^  FLAGS = ' | Select-Object -First 1

# 删除清单实测 1870 行 .c/.h；全仓零引用
#   modules\oled,ist8310,bluetooth,TFminiPlus,unicomm,standard_cmd; bsp\spi,iic,gpio; modules\motor\LKmotor
# 死代码在 Makefile 的登记(删除时同步)
Select-String -Path Makefile -Pattern 'oled|ist8310|HC05|tfmini|unicomm|std_cmd|LK9025|bsp_spi|bsp_iic|bsp_gpio'

# message_center 调用点：38 处(生产 25 / 自身 5 / 主机测试 8)；话题 6 个
Get-ChildItem -Recurse -Include *.c | Where-Object { $_.FullName -notmatch '\\\.agents\\|\\build|\\Drivers\\|\\Middlewares\\' } |
  Select-String -Pattern 'PubRegister|SubRegister|SubGetMessage|PubPushMessage'

# trace 脚本 DWARF 依赖(42 条 -> 路径，6 个脚本)
Select-String -Path tools\*.js -Pattern "(['\`"])([a-zA-Z_][a-zA-Z0-9_]*->[^'\`"]+)"

# while(1) 站点(删除目标之外仍有 16 处首方)
Get-ChildItem -Recurse -Include *.c | Where-Object { $_.FullName -notmatch '\\Drivers\\|\\Middlewares\\|\\build|\.agents' } |
  Select-String -Pattern 'while\s*\(\s*1\s*\)'

# 重构节奏标定(R0~R8 全部在 2026-11-20 19:31~20:53)
git log --pretty=format:"%h %ad %s" --date=format:"%Y-%m-%d %H:%M" -18
git tag --list | Sort-Object     # refactor-before-R{0,2,3,4a,5,5b,6,7,8}，无 R1/R4b
```

## 附B　与其它域的接口（避免重复计数）

- **域F F5**（G1' 与 2027 参数切换冲突）：本文 C-09 只处理"框架宏怎么导出"，不重复其战略性结论；若采纳域F 结论（赛季切换需改参数），G1a 的"逐字节"必须允许"赛季配置目录切换"这一**显式声明**的例外。
- **域F F6**（G4 执行器与 G-E 退役矛盾）：本文 C-17 给执行替代（自基线文件审计），不改其立论。
- **域A（数字）**：本文 C-28 的 470/657/7 文件 29 处、C-27 的分区用量、C-21 的 43 处"待验证"，请以域A 复核为准。
- **域B（技术）**：C-18（BASEPRI 临界区对中断延迟）、C-19（while(1) 转换清单）与技术方案强相关，若域B 给出不同实现，以域B 为准、本文只保留闸门要求。
