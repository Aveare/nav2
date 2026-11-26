# RM 工程开发记录

<!-- rm-ec-dev:handoff schema=1 -->
<!-- 使用说明:本文件是 JIT-RM 1029 的实例化版本,直接复制到工程根改名 RM_DEVLOG.md 即可接手。
新工程使用时填写实际内容,移除未使用的示例条目;已有记录保留原作者与编号。
配置、接口及构建已有独立文档时直接链接,避免重复维护。
当前摘要链接对应 DEV 条目/问题 ID,完整证据在历史或详情中维护。 -->

## 0. AI 冷启动摘要

<!-- AI 会话开始时只读本节即可接手;只放结论与链接,证据留在对应条目。人工接手同样建议先读这里。 -->

| 项 | 内容 |
| --- | --- |
| 基线身份 | 仓库内 `.agents/skills/rm-electrical-dev`(上游 basic_framework @1a136eb1,460 条;审计脚本随仓库走,见 DEV-20261116-06) |
| 工程目标 | **当前交付目标 = 哨兵 Sentry**:`apps/sentry/` + `config/sentry_2026/`;必须显式执行 `build.ps1 sentry -Current`,哨兵烧录优先使用明确命名的 `firmware/sentry.elf`,CMake 无参数默认 infantry 仅作历史兼容(DEV-20261126-01/03) |
| 有效配置 | 哨兵 ONE_BOARD 单板;`config/sentry_2026/config.h`(伞头 + `config_<域>.h`);装配入口 `apps/sentry/app/assembly.c`,消息合同 `apps/sentry/app/sentry_def.h`;infantry/heavy profile 仅作共享回归与兼容 |
| 功能状态 | 哨兵底盘/三轴云台/发射/遥控/安全机制已实装;**AX650 已关闭,USART1 专用于三轴编码器 VOFA JustFloat,USB CDC VCP 保留为独立上位机物理链路且应用帧待定义**;DEV-20261126-25 已按本地 RM_shaobin2 Sentry 工作树迁移遥控/PID:右杆平移,左纵杆 pitch,左下+左横杆小 yaw,其他左挡+左横杆底盘转向;大 yaw 普通挡锁机械正前、小陀螺挡做世界角速度阻尼;大小 yaw 均恢复输出。此前大 yaw 10 N·m 饱和/线圈过温是未关闭的历史风险,本次仅完成编译与主机验证,必须架空短测 |
| 最近验证 | **2026-11-26 Sentry RM_shaobin2 移植(DEV-20261126-25)**:infantry/sentry/heavy 三构建成功，chassis/fwk/AX650/profile 四套主机测试 PASS，fullchain 700 拍确定性通过，11 项静态检查 PASS；Sentry ELF SHA-256 `8BD68019…E467100`。仓库既有 skill 快照 `LICENSE` 哈希不匹配使 baseline audit 不可靠，未冒充通过；未烧录，实车待架空回归 |
| 当前问题 | 无(RM-001 已关闭) |
| 下一步 | 用户架空验证 RM_shaobin2 迁移固件:先核对急停、三轴方向/限位和 USART1 VOFA,再短时测试大 yaw 并监测 DM 状态/温度;另行定义 USB CDC VCP 上位机帧、坐标系、超时、安全态与模式仲裁;M3 整车回归和热量守卫实测仍由用户执行 |
| 演进待办 | flash 标定流程(Yaw 零位等持久化,代码只读语义名),实车回归通过后评估 |
| 重构状态 | **自主研发框架升级 B0a~B12 已完成(2026-11-22,DEV-20261122-21/22/23)**:framework/{kernel,err,mem,msg,diag} + ports 全总线/FreeRTOS 后端 + components/ + apps/{infantry,heavy,sentry}_skeleton(CMake `ROBOT_PROFILE`);板型运行期化按 DEC-009 不做;**`application/`、`modules/`、`bsp/` 全部删除**,AGENTS/Skill/code-map v2/版权页收口。**2026-11-23 工程统一化(DEV-20261123-01)**:目录语义化(app/、config_*)、函数/类型/宏/变量/枚举全量 snake_case + 模块前缀、API 统一 `module_verb_object`、注释头 171/171、命名规范与 `check_naming.py`/`.clang-tidy` 落地。下一里程碑 = **M3 实车回归(用户)→ M3′ 结项验收**(tag 复核点 `arch-before-M3p`)。2026-11-20 零行为变化重构 **R0~R6 已完成**(9 提交,DEV-20261120-03~09):闸门工具、文档五类重组、config 分轴、cmd 拆分、chassis/gimbal 函数搬出、切断 modules→application 反向依赖、构建/CI 收口。**功能与参数零改动,text 仅 +56 B(+0.05%),bss 不变**。排程入口 `.Doc/plan/重构总方案.md`(已结项:R0~R6+R5b 全部交付,实车回归 2026-11-20 PASS;R4b 经裁决不做),回退标签 `refactor-before-R{0,2,3,4a,5,6}`。该重构无遗留待办;**当前结构轴待办 = M3 实车回归 → M3′ 结项验收**。功能队列仍冻结。 |
| 文档结构 | 2026-11-20 重组为 `plan/ spec/ detail/ guide/ history/`,2026-11-23 补 `review/`、`gate/` 索引;规范新增 `spec/命名规范.md`,组件登记 `components/README.md`。索引见 `.Doc/README.md`。**本文件 §6 历史 DEV 条目里的 `.Doc/xxx.md` 是当时路径**,已移动的文件按文件名在 `.Doc/` 下检索(新条目请写全路径)。 |

哨兵当前串口路由:**USART1 = 云台三轴编码器 VOFA(921600 8N1);USB CDC VCP = 独立上位机物理链路(应用协议待定义);USART6 = 裁判系统**。旧 AX650 UART1 链路已关闭,见 DEV-20261126-02。

哨兵兵种已进入**手动整机实装**(DEV-20261123-13):`apps/sentry/{cmd,chassis,gimbal,shoot,autonomy}` + `config/sentry_2026/` 全部落地,CMake `-DROBOT_PROFILE=sentry` 可构建;默认构建仍为步兵。三轴零位/方向/PID 均为 `[待实测]`,未烧录。

U3/U4 已收口:组件层直达 RTOS/HAL 与具体句柄清理,IMU 增加 `ports/stm32f4/imu_port`,G4 扩到当前分层并有新的 `arch-after-U3U4` manifest;四套测试、Debug/RelWithDebInfo 构建和 G4 均有本轮证据。

历史 AX650 USART1 `0x1021` 方案见 DEV-20261116-01,当前哨兵配置已停用,不得作为现行接线依据。

编码核验：DEV-20261116-01 提交的 18 个文件均为 UTF-8 无 BOM；已固定编辑器使用 UTF-8 与 LF，见 DEV-20261116-02。

## 1. 当前状态

| 项目 | 内容 |
| --- | --- |
| 当前目标与验收 | 哨兵三轴云台/底盘/发射台架回归,并单独定义 VCP 上位机协议 |
| 机器人与任务范围 | 2026 哨兵 Sentry,RoboMaster RMUC,单板整车 |
| skill | rm-ec-dev v1.1 |
| 工程源码基线 | jit-rm-1029.manifest.json(见 §0);git 为字节历史事实源 |
| 有效配置 | ONE_BOARD;`config/sentry_2026/`;VISION_USE_VCP;USART1=云台 VOFA;AX650=OFF |
| 功能状态 | 生效:哨兵底盘/三轴云台/发射/仲裁/看门狗/遥测、UART1 VOFA;VCP 应用协议待定义;预留:超电/ist8310 |
| 最近验证 | 见 §0 |
| 板上固件身份 | 待确认(用户烧录后回填) |
| 当前问题 | 无 |
| 最近开发条目 | DEV-20261126-25 |
| 下一步 | 见 §0 |

## 2. 设计与配置

<!-- 详细接线/几何/标定值见 README.md 与 .Doc/spec/本车调参结论.md,此处只记变更与待确认项。 -->

- 机械拓扑:麦克纳姆轮(45°全向),四轮 CAN1;云台 yaw GM6020(CAN1)/pitch M3508(CAN2)。
- 零位:YAW_CHASSIS_ALIGN_ECD=3564 / PITCH_HORIZON_ECD=3412(拆装传动须重标)。


<!-- 后续条目按此格式追加;同一主记录编号连续递增。 -->

## 3. 问题台账

| ID | 现象与范围 | 状态 | 下一步 | 关闭条件/证据 |
| --- | --- | --- | --- | --- |
| RM-002 | 发射机构硬件问题无法正常运行(单发/三发/射速代码已实装,DEV-20261030-01) | **挂起(硬件问题)** | 硬件修复后按原计划架空标定 | 单发逐发出弹无连击/无卡弹;连发射速与堵转保护实车确认(用户回填) |
| RM-006 | `power_control.c` 是 `dji_motor.c` 的整套副本(同组帧/rx_id/半圈判据),`PowerControlInit` 无调用者;dji 侧后加的残帧检查、CAN1 总闸、停机清积分**未同步**过去 | **已关闭** | — | R8 多边取证后删除 `power_control.c/.h`:3 个公开 API 不在 ELF、两处 include 零符号使用、两处同名函数是 static 不冲突;删除后 text +0(DEV-20261120-14) |
| RM-007 | gimbal 手动 yaw PID 在 Init 与运行时各写一份(同值),改 Init 静默无效 | **已关闭** | — | 2026-11-23 深审定案:运行时每拍无条件覆盖 yaw 的 6 个字段(角度 Kp/Kd/DeadBand、速度 Kp/Ki/IL),Init 同名字段是死初值(仅在 ROBOT_STOP 窗口存在,PID 不参与输出);修法=删除 6 个死字段+注释声明运行时唯一写者,**pitch Init 字段为真唯一写者,明示不动**;整链 700 拍 golden 逐字节一致(DEV-20261123-07) |
| RM-008 | `shoot.c` 堵转判据在连发分支与单发状态机各写一份,只改一处会漂移 | **已关闭** | — | 2026-11-23 深审修正:两处判据与"转起来就清零"**逐字相同**(原记的"单发缺清零隐患"经读码证伪),差异只在锁存后果(连发保持锁存/单发丢队列,系有意语义);抽取 `shoot_loader_jam_tick()` 单点实现,判据+累计+清零内聚,后果留调用点;**零行为**(整链 golden 逐字节一致),text −84;台架卡弹验证归 RM-002 硬件窗口(DEV-20261123-07) |
| RM-009 | 双 ms 时基:components 层 ax650 用 `HAL_GetTick`,其余用 DWT/kernel 原语,超时判定存在异源隐患(ist8310 已随 B0 删除,范围缩至 ax650 ×3) | **已关闭** | — | 2026-11-23:ax650 三处改走 `kernel_tick_ms()`(framework 时间原语,整数 ms 回绕语义与 HAL_GetTick 一致);判据修正为"**apps/components/framework 层 HAL_GetTick=0**,ports 启动期/桩路径豁免登记"(iwdg/usart 在 ports 属合法平台资源);ax650 主机桩补 kernel.h,套件含 wrap 断言全绿;全工程不存在跨时基比较(实测 grep);AGENTS Boundaries 已固化红线(DEV-20261123-06) |
| RM-010 | `chassis_internal.h` 与 `chassis_kinematics.c` 各有一份 `LF/RF/LB/RB_CENTER` 宏(R4a 拆分残留) | **已关闭** | — | R8 删除 `.c` 里那份(内部头已提供),text +0 |
| RM-011 | seasky 旧视觉栈:`seasky_protocol.c` 的 CRC 函数已死(不在 ELF、仅自用),但 `master_process.c` 的 `VisionSetAltitude` **仍被 ins_task.c 调用** | **已关闭** | — | 2026-11-23 整组删除 `master_process.c/h`、`seasky_protocol.c/h` 与 host 桩:核实 `VisionInit/VisionSend` 无调用者,`VisionSetAltitude` 只写无人读的 `Vision_Send_s`;删除后 text −40 B、bss −24 B,四套主机测试 exit 0(DEV-20261123-01) |
| RM-012 | `tests/run_ax650_tests.ps1` 编译失败:cbb90e3(串口循环 DMA 重构)改写 `bsp_usart.c` 用到 HAL DMA API,`tests/ax650_stubs` 未同步缺桩(HAL_UART_AbortReceive/DMA_CIRCULAR/`__HAL_DMA_GET_COUNTER` 等);CI 因此持续红 | **已关闭** | — | 补齐循环 DMA 测试桩并改为任务侧 `USARTProcessAll` 消费模拟；RX=1/0、O2/Ofast 全部通过(DEV-20261122-18) |

### 已关闭问题

| ID | 结论 | 日期 | 证据 |
| --- | --- | --- | --- |
| RM-003 | pitch偶发抖动三场景消失:速度环反馈改欧拉pitch角速度(DEV-20261030-02)后用户实车确认"没什么问题" | 2026-10-30 | 用户实车确认 |
| RM-004 | pitch粘滑/异响/松杆续滑全项收敛:积分降档(Ki=10,IL=1200)+输出低通(RC0.005)+角度死区(0.3°)+ref前馈(DEV-07~11整定路径,沉淀至调参结论§2),用户实车确认"ok了感觉很不错" | 2026-10-30 | 用户实车确认 |
| RM-005 | 裁判系统一期链路联通:主控接C板USART6、连上引擎服务器、图传(VT02/VT12)在裁判系统中正常出图,用户确认"裁判系统已经搞好"(UI视觉待二期真实数据后一并确认) | 2026-11-02 | 用户联调确认(引擎面板机器人上线+图传出图) |

### 决策与否决台账(DEC-NN)

<!-- 只追加不删除;翻案新增条目并引用旧编号。被否决的方案必须入账,防止后续 AI 会话重新提出。 -->

| 编号 | 决策/被否方案 | 理由 | 日期 |
| --- | --- | --- | --- |
| DEC-001 | 架构与配置决策:APP→Module→BSP→HAL 分层;本车参数唯一改动点收敛于 config/robot_infantry_2026.h;协议合同独立于参数放 robot_def.h(带布局守卫);记录体系采用冷启动摘要+DEV/DEC/RM 台账 | 换车/换赛季不改业务代码;参数与协议分离防止误动合同;记录轻量化但满足 AI 冷启动与追溯 | 2026-10-29 |
| DEC-002 | 小陀螺采用恒定自转速度(CHASSIS_ROTATE_SPEED),否决"瞄准时自转让速"调度方案 | 用户体感否决("太狠");恒速自转行为可预期,调度路径不保留死代码 | 2026-10-29 |
| DEC-003 | 基线审计以本工程 manifest 为基线(不对外部提交做比对) | 字节历史由 git 承担;审计只回答"工程相对固定基线改了什么" | 2026-10-30 |
| 调参否决记录(详见 .Doc/spec/本车调参结论.md) | 摩擦前馈 B=18 否决(失稳);D 项限幅 12 否决;Kd-FAR 加大否决;0x2FE 电流帧暂不启用 | 实测证据,详见踩坑记录 | 2026-10 |
| DEC-004 | pitch手感三轮实验(ref前馈/阈值低通/重力前馈)全数回滚至初始Gyro[0]反馈方案,恢复丝滑接受偶发抖;保留解锁pitch重锚定与DEV-02分析结论 | 三轮盲改无遥测数据支撑,实车均无效;初始版无粘滑跳步。后续改动必须先取telemetry波形 | 2026-10-30 |
| DEC-005 | 热量管理采用"估计器+前馈射频",否决"裁判热量直连射速"的反馈式限速 | 对象死区100~200ms、停扣瞬间仍有1~2发在膛,反馈必然过冲且调参无法消除;裁判ΔQ只用于校正Q̂。同构参照:中科大2024全向步兵crt_booster.cpp(余量30、超阈值射速=c/10),但其直接用10Hz原始值、无估计器 | 2026-11-19 |
| DEC-006 | 底盘 VOFA 调试占用 **USART1(视觉口)**,调试期 AX650 链路整体停用;否决"底盘 VOFA 走 USART6" | CubeMX 只初始化 USART1/3/6 三路:USART3=DBUS、USART6=裁判系统,无空闲口。USART6 已有过 yaw VOFA 先例,代价是 UITaskInit 被编译掉→`RefereeGetInfo()` 为 NULL→`[chcal]` 功率/缓冲能量恒 -1、热量守卫直通(枪口无保护),底盘功率标定恰好需要这两个量。故临时让出视觉口,两个宏互斥并由 `#error` 兜底;比赛前换回 AX650=1/VOFA=0 | 2026-11-20 |
| DEC-007 | AX650 的 USART1 所有权与下行解析拆成 `AX650_UART1_ENABLE` / `AX650_UART1_RX_ENABLE` 两级开关；当前两者均开启 | 允许手眼标定等场景保留 0x1021 姿态上报、单独关闭 0x0503 下行；RX 不能脱离链路总开关，非法组合由 `#error` 拦截 | 2026-11-22 |
| DEC-008 | 每次代码/config/构建改动必须同步相关 Markdown（至少 `RM_DEVLOG.md`），并提交、推送当前跟踪分支 | 用户要求把文档与 Git 同步固化为默认交付动作；推送失败必须如实报告，不得称已同步 | 2026-11-22 |
| DEC-009 | 板型运行期化(B11 行"若做"子项)**不做**:无双板硬件/双板代码未实装(当前唯一固件 ONE_BOARD),运行期字段无消费者,强行字段化只增风险;保持编译期三值守卫(ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD 互斥检查),`assembly_t` 运行期表随双板硬件窗口引入 | §3.3 允许"若做"分支;零行为约束下无硬件无法验证双板分支;M3′ 按 G-B 只要求三骨架编译 | 2026-11-22 |
| DEC-010 | 模块在线检测 daemon 落 **components/daemon/**(附H 原写"framework/diag 并入 B3") | B3 只并入**心跳监督**;模块 daemon 仍被 5 个组件使用,且依赖 `log_port/dwt_port` 与 `buzzer` —— 放进 framework 会破坏"零平台 include";components 是唯一满足依赖方向的落点 | 2026-11-22 |
| DEC-011 | 全工程命名风格定为 **snake_case + 模块前缀 + 动词**(`module_verb_object`);类型 `_t`、枚举 `_e`、宏全大写带域前缀 | 用户 2026-11-23 明确"命令风格要最标准"并全权批准;选 C 社区最通用的 snake_case,避免 Pascal/下划线混写导致 grep 失效;公共 API 一次性改名比长期双轨成本低 | 2026-11-23 |
| DEC-012 | config 子头统一 `config_<域>.h` 前缀,伞头移入 `config/<robot>_<year>/config.h`;跨域一致性 `#if` 上移伞头 | 扁平化后 `config/gimbal.h` 与 `apps/*/gimbal.h` 同名,include 顺序决定语义(实测主机测试拿错头);前缀化彻底消除歧义,子头不再互相 include | 2026-11-23 |
| DEC-013 | 旧视觉栈 `master_process.*` + `seasky_protocol.*` **整组删除**(RM-011 结案) | 核实 `VisionInit/VisionSend` 零调用者,`VisionSetAltitude` 只写无人读结构体;`Enemy_Color_e`/`Bullet_Speed_e` 是唯一被引用的类型,已收敛到 `robot_def.h` | 2026-11-23 |
| DEC-014 | DM/HT/servo 电机、`super_cap`、`can_comm` 保留为 `OPTIONAL/DORMANT`,不按"整洁"删除 | 下赛季可能换电机/上超电/双板;删除会丢能力。改为在 `components/README.md` 显式登记状态与接入条件,`OPTIONAL` 禁止被现行 apps 引用 | 2026-11-23 |
| DEC-015 | `YAW_SPEED_KP_HIGH` 维持 40,电流/电压两分支同值;"电流模式回落 20"推演**搁置** | 域换算 ×0.6554 下电流模式等效增益≈电压域 65.5%,"高速区变硬"未经实测;先完成 0x2FE 复验(YAW_ESC_SELFTEST),凭实测波形再议,不凭推演改值 | 2026-11-23 |

## 4. 构建与恢复

- 工具链:arm-none-eabi-gcc 14.3.1(STM32CubeCLT bundle)+ Ninja 1.13.2 + CMake;主机测试 gcc(mingw64)。
- 构建:`mingw32-make -j24` 或 `cmake -G Ninja -B build && ninja -C build`;-Werror(Makefile)。
- 烧录:tasks.json `download_dap`/`download_jlink`;日志:`JlinkRTTClient`。
- 恢复顺序:本记录 §0 → 问题台账 → DEV 条目 → 源码/config → 重跑审计与测试 → 板上固件核验。

## 5. 资料与设计依据

- 架构:README.md + .Doc/spec/架构与开发指南.md(§1.5 为相对上游框架的架构改造清单);调参:.Doc/spec/本车调参结论.md + .Doc/spec/PID参数整定.md;纪律:.Doc/spec/必须做&禁止做.md。
- 架构框架:HNUYueLuRM/basic_framework(架构来源);模块协议:各 modules/*/*.md。

## 6. 开发进展

### DEV-20261029-01｜2026-10-29｜工程初版开发

- **目标与触发**:完成 2026 步兵单板整车电控初版:底盘/云台/发射/遥控/安全机制全部落地,建立可维护的分层架构与记录体系。
- **功能变化**:实现 cmd 控制权仲裁与解锁回中、底盘跟随/小陀螺/斜坡、云台双闭环+双前馈+Kd/Kp 调度、发射到速互锁+堵转锁存、cmd 断流看门狗、遥测环形缓冲、主机单元测试;拒绝"瞄准让速"自转调度方案(见 DEC-002);运行期未知模式一律 LOGERROR+安全态。
- **文件与符号**:application/(robot 接线、robot_task 任务、cmd/chassis/gimbal/shoot)、config/robot_infantry_2026.h、modules/(电机/IMU/遥控/消息中心/守护/遥测)、bsp/、tests/。
- **接口与调用链**:消息话题 chassis_cmd/gimbal_cmd/shoot_cmd 三路;电机接口 DJIMotor*;任务频率 INS/MOTOR 1kHz、DAEMON 100Hz、ROBOT 200Hz。
- **配置/API/协议影响**:协议结构体 pack(1) 并以 _Static_assert 布局守卫(arm 21/3,host 24/9);本车参数全部收敛于 config 头。
- **验证**:主机测试 PASS(cmd_watchdog 14 断言+仲裁全场景);ARM gcc14.3.1 全量 131/131,0 error 第一方 0 warning;FLASH 9.88%/RAM 72.41%。烧录实车执行功能正常。

### DEV-20261030-01｜2026-10-30｜发射机构闭环:单发/三发状态机+连发射速

- **目标与触发**:实车回归通过后(RM-001 关闭),补齐发射机构:单发/三发模式、连发"攻速"概念、拨弹与摩擦轮互锁闭环;顺带清理打包前发现的死宏。
- **功能变化**:1) 单发/三发由"零参数禁用占位"改为"待发弹数"状态机——cmd 持续发布 load_mode,shoot 端按模式切换沿登记弹数(1/3),逐发切角度环 ref=total_angle+一发角,每发起步前置=摩擦轮双轮在线且在速(复用连发互锁),到位/超时/堵转三重保护,发完自动回速度环;2) 连发射速落地:ω=R×每发角×减速比,R=SHOOT_RATE_DEFAULT(6发/s≈1852RPM,替代原固定 LOADER_TARGET_RPM=2000);3) 拨轮输入重映射:>300 急停(不变)、-240~-460 单发(请求保持)、<-460 连发(持续)、回中 LOAD_STOP;4) 堵转锁存解除条件由"收到LOAD_STOP"改为"拨轮回中且无待发弹";5) 死宏 PTICH_HORIZON_ANGLE 删除。三发机制就绪、输入待图传/键鼠接入。**结构级变更已获用户批准(2026-10-30 会话确认)**。
- **文件与符号**:config/robot_infantry_2026.h(ONE_BULLET_DELTA_ANGLE 重定义=360/7输出轴角+SHOOT_RATE_DEFAULT+DIAL_SINGLE/BURST_ZONE+LOADER_ANGLE_TOL_DEG+LOADER_SINGLE_TIMEOUT_MS;LOADER_TARGET_RPM 删除)、application/cmd/robot_cmd.c(RemoteControlSet 发射段)、application/shoot/shoot.c(loader_pending 状态机+射速换算)、tests/run_chassis_tests.ps1(补 UTF-8 BOM,PS5.1 无 BOM 按 GBK 解析中文注释致语法崩)。
- **接口与调用链**:协议结构体零改动(load_mode/shoot_rate 字段既有);cmd→shoot 语义变更:LOAD_1_BULLET 由"单周期事件"改为"拨轮段状态",事件沿检测移至 shoot 端(last_load_mode)。
- **配置/API/协议影响**:LOADER_TARGET_RPM 引用清零;ONE_BULLET_DELTA_ANGLE 单位明确为输出轴 deg(原 36 系上游图纸遗留,量纲与 7 槽不符,实车标定修正)。
- **验证**:ARM 增量编译 0 error 第一方 0 warning(FLASH 9.98%);主机测试 ALL PASS(仲裁+看门狗全场景);基线审计 4 Changed(均为本条目标文件)/0 Added/0 Missing/0 红旗。**实车未回归——单发角度标定(ONE_BULLET_DELTA_ANGLE/LOADER_ANGLE_TOL_DEG)与射速手感待上车**。
- **遗留**:RM-002 单发标定与实车回归;LOAD_REVERSE 退卡弹待实现。
- **下一步**:架空单发标定→实弹校一发角→三发用连发时序验证。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-02｜2026-10-30｜pitch偶发抖动根因修复:速度环反馈改欧拉pitch角速度

- **目标与触发**:实车报告pitch偶发抖动,三场景(解锁回正时/上下移动时/旋转云台时)。发射机构因硬件问题挂起(RM-002)。
- **功能变化**:pitch速度环反馈由`Gyro[0]`(芯片X轴)改为欧拉pitch角速度`θ̇=Gyro[1]·cosφ−Gyro[2]·sinφ`。**根因**:BMI088驱动无安装换轴,Gyro[]=芯片原始体轴;ZYX欧拉下θ绕体Y轴,`Gyro[0]`(欧拉roll轴)物理内容≈φ̇−ψ̇sinθ——**不含pitch自身角速度**(速度环无阻尼→移动抖),且yaw旋转时−ψ̇·sinθ漏项直接进速度环(300°/s×sin20°≈100°/s假速度→回正抖/旋转抖)。旁证:yaw环用Gyro[2](芯片Z=竖直轴)实车正常→平装;pitch能定位→机械pitch轴=芯片Y轴。上游框架即如此(本工程照抄),属继承性bug。
- **文件与符号**:application/gimbal/gimbal.c(新增static pitch_speed_feedback,GimbalTask开头按欧拉式计算,PITCH_SPEED_FB_DIR宏翻转方向,telemetry_pitch的fb改用新反馈)、config/robot_infantry_2026.h(PITCH_SPEED_FB_DIR=+1)。
- **接口与调用链**:other_speed_feedback_ptr改绑静态变量;该变量200Hz更新、1kHz电机任务解引用(最坏5ms数据龄,若阻尼不足再上移INS_Task计算)。单写者=GimbalTask。
- **配置/API/协议影响**:协议零改动;chassis.c的Gyro[2]跟随前馈严格说含cosθ近似(pitch大角度偏差),实车已验证暂不动,后续单独评估。
- **验证**:ARM增量编译0 error第一方0 warning(FLASH 9.98%);审计5 Changed(=DEV-01四文件+本条gimbal.c)/0红旗。**实车未回归**:架空顺序①静态稳定②手拨pitch松手应无振荡回位③小陀螺+pitch抬起观察旋转耦合;若正反馈疯甩→PITCH_SPEED_FB_DIR改-1。遗留:pitch无重力前馈(模板TODO)按单变量纪律留下一轮。
- **下一步**:实车验证RM-003→视结果决定重力前馈。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-03｜2026-10-30｜pitch推杆跟手度:补ref前馈(RM-003实车验证通过后)

- **目标与触发**:RM-003实车确认抖动消失("没什么问题"),用户报pitch推杆"不丝滑、很顿"。
- **功能变化**:pitch补目标角速度前馈(与yaw同款机制):`pitch_speed_feedforward=Δref/dt`,绑定speed_feedforward_ptr+feedforward_flag=SPEED_FEEDFORWARD。**根因定量**:角度环纯P(Kp=8×D2R,Ki=0),恒速跟踪稳态滞后=目标速度/Kp——满杆60°/s滞后7.5°、半杆3.75°,推杆永远拖着误差爬、松杆再追回,即"顿"。前馈直接给出目标速度,角度环只修零点几度。目标钳位(PITCH_MIN/MAX)或跳变时Δref超限自动给0(照抄yaw跳变保护)。yaw当初即靠同款前馈治好推杆滞后(DEV-93),pitch漏配。
- **文件与符号**:application/gimbal/gimbal.c(static pitch_speed_feedforward/pitch_ref_last/last_ms+GimbalTask计算块+pitch电机配置)、config/robot_infantry_2026.h(PITCH_REF_FF_GAIN=1.0/MAX_RATE=60/MAX_DELTA)。
- **接口与调用链**:仅pitch电机控制器配置变化;协议零改动。
- **配置/API/协议影响**:无。GRAVITY前馈仍缺(下一轮,单变量纪律)。
- **验证**:ARM增量编译0 error第一方0 warning(FLASH 10.00%);审计5 Changed/0红旗(改动集同前,含本条gimbal.c+config)。**实车未回归(RM-004)**。
- **下一步**:实车推杆验证;仍顿再评估重力前馈与增益。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-04｜2026-10-30｜pitch"一顿一顿"修复:前馈阈值按DBUS帧周期重算+前馈低通

- **目标与触发**:DEV-03烧录后用户报pitch上抬/下抬"一顿一顿"断续,且"之前虽然概率性抖但是丝滑的"——两条线索合并定位。
- **功能变化**:1) **前馈跳变阈值基准周期错误修正**:`MAX_DELTA`原按cmd周期5ms算(60×0.005×1.1=0.33°),但杆量按DBUS帧周期14ms(≈70Hz)更新、每2~3个cmd周期集中到来,满杆单帧Δref≈0.84°>0.33°——**杆量推过约4%行程前馈即被判"跳变"恒0**,前馈从未真正工作(DEV-03等于白修)。改为按帧周期算:RATE×0.014×3(pitch=2.5°,yaw=7.6°)。2) **前馈一阶低通**(`PITCH_REF_FF_LP=0.3`,200Hz截止≈11Hz):70Hz阶跃前馈直接进速度环是力矩纹波,即"一顿一顿"的直接来源;低通抹成近直流,相位滞后≈17ms可忽略。3) 跳变(重锚定/超时)时前馈立即清零不走低通拖尾。4) **yaw同源缺陷顺带修正**:YAW_REF_FF_MAX_DELTA原0.99°,满杆(180°/s)单帧Δref≈2.5°同样超限失效——中低杆量下yaw一直工作所以未暴露;放宽后满杆前馈才真正生效,行为更接近设计意图。
- **为什么之前"抖但丝滑"**:旧反馈Gyro[0]不含pitch自身速度,速度环近乎开环,电机惯性把70Hz杆量台阶磨平(无阻尼假丝滑);DEV-02换成真实反馈后闭环有阻尼,70Hz阶跃目标+大Ki直接表现为断续。
- **文件与符号**:application/gimbal/gimbal.c(pitch前馈计算块:低通+跳变立即清零)、config/robot_infantry_2026.h(两个MAX_DELTA重算+PITCH_REF_FF_LP)。
- **接口与调用链**:无接口变化;yaw前馈行为有变(满杆时从失效变生效),实车需顺带确认小陀螺推杆手感。
- **配置/API/协议影响**:无。
- **验证**:ARM增量编译0 error第一方0 warning(FLASH 10.00%);审计5 Changed/0红旗。**实车未回归(RM-004)**:推杆应丝滑跟手;若仍有细微纹波→PITCH_REF_FF_LP降至0.2;若起步变迟钝→LP升至0.4。
- **下一步**:实车验证;通过后RM-004关闭,pitch剩余短板仅重力前馈。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-05｜2026-10-30|pitch"一顿一顿"续修:重力前馈实装+解锁pitch重锚定

- **目标与触发**:DEV-04烧录后用户仍报"一顿一顿"。结合"修复反馈前抖但丝滑"重定位:旧开环恒流硬推碾过静摩擦(假丝滑),真闭环下小目标电流不足以克服静摩擦+重力矩→粘滑跳步(stick-slip)——机械摩擦被真闭环暴露,前馈阈值/低通不是主因。
- **功能变化**:1) **pitch重力前馈实装**(调参结论遗留的模板TODO):`pitch_gravity_ff=GAIN·cos(pitch)`,经current_feedforward_ptr输出侧叠加(feedforward_flag补CURRENT_FEEDFORWARD位);GAIN=500初值,标定法写config(架空松杆读telemetry out稳态,GAIN≈|水平稳态输出|,反向改负,欠补偿原则宁小勿大)。2) **解锁沿pitch重锚定**(真bug修复):原只重锚定yaw,pitch目标恒为上电0°(IMU水平),云台不在水平时解锁会先甩向水平;cmd解锁沿补`pitch=当前IMU Pitch`。
- **文件与符号**:application/gimbal/gimbal.c(static pitch_gravity_ff+计算块+配置绑定)、application/cmd/robot_cmd.c(解锁pitch重锚定)、config/robot_infantry_2026.h(PITCH_GRAVITY_FF_GAIN=500)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 10.01%);审计5 Changed/0红旗。**实车未回归(RM-004)**:推杆丝滑度;抬升/下降力度对称性(不对称→GAIN符号/幅值);解锁无甩动。
- **下一步**:实车验证+GAIN标定回填;若仍粘滑→评估speed环Ki降档或机械摩擦排查。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-06｜2026-10-30|pitch手感实验全数回滚至初始方案(DEC-004)

- **目标与触发**:DEV-03/04/05三轮手感修复(推杆前馈/阈值低通/重力前馈)实车均无效或更差("一顿一顿"未解决,用户判定重力前馈"效果太差"),用户要求退回初始版本(该版本无粘滑跳步,仅偶发抖+恒速滞后)。回滚决策入DEC-004。
- **功能变化**:pitch速度反馈恢复Gyro[0];删除pitch ref前馈(变量/计算块/绑定)、重力前馈(变量/计算块/绑定)、telemetry fb改回Gyro[0];config删除PITCH_SPEED_FB_DIR/PITCH_REF_FF_*/PITCH_GRAVITY_FF_GAIN,YAW_REF_FF_MAX_DELTA恢复0.005×1.1。**保留**:①DEV-02的欧拉反馈分析结论仍成立(入DEC台账待数据复核);②解锁沿pitch重锚定(正确性修复:原上电目标恒0°非水平解锁会甩,与手感无关);③发射机构DEV-01全量保留。审计确认gimbal.c符号级=基线,config差集仅剩发射宏。
- **教训(写给自己和后来者)**:三轮修复全部是"理论推演→改码→实车试错",无遥测数据支撑,迭代成本由用户烧录时间承担——违反调参结论§7"单变量回放"精神。**后续任何pitch手感改动必须先取telemetry_pitch数据(ref/fb/out波形)再动代码**。
- **验证**:ARM编译0 error第一方0 warning(FLASH 9.98%);审计5 Changed(发射4+gimbal.c注释级差异,无符号差集)/0红旗。实车:用户确认恢复"不顿"状态后RM-004改挂起。
- **下一步(数据驱动,不再盲改)**:复现粘滑→Ozone读telemetry_pitch波形→按特征分流:a)out锯齿爬升+fb平台=stick-slip摩擦→摩擦前馈/速度环Ki重整;b)out与杆量70Hz同步纹波=输入台阶→cmd端pitch目标斜坡;c)fb噪声→反馈滤波。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-07｜2026-10-30|pitch粘滑新方向:速度环积分降档(文献+yaw对照,单变量)

- **目标与触发**:回滚版确认恢复丝滑后,重新查资料定位(RM-004挂起中)。用户要求"重新思考,多查资料"。
- **诊断更新**:"一顿一顿"更符合**速度环积分喘振**(文献:FOC速度环实战"I太大出现低频喘振,转速忽快忽慢像呼吸,立即减小I";3508实战"带载低速抖动原因就是积分饱和,单独限幅后消失")。证据:①pitch Ki=20/D2R≈1146、IntegralLimit=1000=MaxOut的**25%**;②yaw Ki=114.6、IL=100=MaxOut的**0.4%**,恒稳;③喘振能源是积分器本身→解释重力前馈为何无效(方向错)、Gyro[0]时代为何丝滑(开环特性掩盖积分行为)。
- **功能变化**:pitch速度环 Ki 20/D2R→**2/D2R**(yaw同款)、IntegralLimit 1000→**200**(MaxOut的5%)。其余一律不动(单变量)。回退路径:仍顿→Ki再降或IL=0试纯P;抗扰变差(外力扰动恢复慢)→Ki升5/D2R档。
- **文件与符号**:application/gimbal/gimbal.c(pitch speed_PID两参数)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 9.98%)。**实车未回归(RM-004)**。
- **下一步**:实车验证;无论结果回填现象→决定下一档参数或转遥测数据定位。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-08｜2026-10-30|Ki折中档+恢复解锁回水平(DEV-07实车反馈两问题)

- **目标与触发**:DEV-07实车反馈两问题:①解锁后"回正"效果消失;②左杆上推云台反应延迟。
- **诊断**:①=DEV-06保留的"解锁pitch重锚定"把原版"解锁后pitch回水平"行为改掉了——用户认知的"回正"=yaw转正前+pitch回水平,重锚定属错误保留,已恢复原版(pitch目标保持上电0°);②=Ki=2/D2R降过头:积分建立力矩慢10倍,起步电流爬升慢→延迟感。**②同时验证喘振方向正确**(顿消失,换来了可调的延迟)。
- **功能变化**:①robot_cmd.c删除解锁沿pitch重锚定(恢复原版回水平);②pitch速度环Ki 2→10/D2R(折中),IntegralLimit 200→600(MaxOut 15%,需扛重力稳态电流,200扛不住)。
- **文件与符号**:application/cmd/robot_cmd.c、application/gimbal/gimbal.c(pitch speed_PID)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 9.98%)。**实车未回归(RM-004)**。
- **下一步档位**:仍喘振→Ki=5/D2R;仍延迟→Ki=15/D2R;两者都稳→RM-004关闭。若反复不收敛→转遥测数据定位(Ozone telemetry_pitch)。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-09｜2026-10-30|特定角度极限环+回水平不到位:角度死区+积分上限重取

- **目标与触发**:DEV-08实车反馈:①轻微抖动,疑似特定角度,手轻碰电机即消失(稳定小极限环特征);②解锁pitch回正(回水平)仍不到位。
- **诊断**:两症状统一指向**积分限幅600仍扛不住水平附近重力**(cos(pitch)最大→重力电流需求最大):①积分顶限幅边缘振荡=小极限环(轻碰踢出即不回归);②回水平末段推不到位。另角度环无DeadBand(原版即无),到位后小误差持续修正助长极限环。
- **功能变化**:①pitch angle_PID加DeadBand=0.3°(到位即停杀小极限环;yaw同款0.5°先例);②speed_PID IntegralLimit 600→1200(MaxOut 30%)。Ki保持10/D2R不动。
- **文件与符号**:application/gimbal/gimbal.c(pitch angle_PID.DeadBand、speed_PID.IntegralLimit)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 9.98%)。**实车未回归(RM-004)**。
- **判读**:抖消失+回水平到位→RM-004关闭;回水平到位但抖加重→IL回落至900并改加重力前馈方案(需先标定重力电流);回水平仍差→用户读Ozone稳态输出回填重力电流值精调。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-10｜2026-10-30|上抬异响:速度环输出低通(纹波抑制)

- **目标与触发**:DEV-09实车:振动只出现在上抬过程(对抗重力方向),下抬无;非持续抖,运动中异响,停即消;声音比DEV-08略大。
- **诊断**:上抬=高电流工作点(重力+摩擦全由电机出),速度环输出中陀螺噪声xKp(2865)的高频力矩纹波被机械啮合放大为可闻异响;下抬重力帮转、电流小、齿面受力松,纹波不显。声音略大与DEV-09的IL 1200相关(上抬中积分输出更活跃)。
- **功能变化**:pitch速度环启用**输出低通**(PID_OutputFilter位+Output_LPF_RC=0.005s,1kHz下截止约32Hz):推杆动态(<5Hz)无损,高频纹波衰减20dB级。注:chassis虽写Output_LPF_RC但未开使能位,此为本工程首次真正启用该机制。
- **文件与符号**:application/gimbal/gimbal.c(pitch speed_PID.Improve加PID_OutputFilter、Output_LPF_RC=0.005)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 9.98%)。**实车未回归(RM-004)**。
- **判读**:异响消→RM-004关闭;上抬变"肉"(动态迟钝)→RC降至0.003;异响仍在→RC升至0.008或转重力前馈方案(降上抬工作点电流)。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-11｜2026-10-30|松杆续滑:pitch ref前馈重新引入(环路稳定后)

- **目标与触发**:DEV-10实车异响消失(输出滤波起效);新现象=松杆后pitch朝原运动方向再滑一小段(上抬/下抬对称)。
- **诊断**:纯P角度环恒速跟踪滞后——运动中目标超前当前角(满杆7.5°=速度/Kp),松杆后目标定格、云台走完差距=续滑。即DEV-03最早诊断的问题,当时前馈因阈值bug未生效+Ki=20喘振主导无法评估而回滚;现环路已稳定(不喘/不顿/无异响),是重新引入的正确时机。
- **功能变化**:重新引入pitch ref前馈,全部按DEV-04教训取参:①阈值按DBUS帧周期14ms×3(MAX_DELTA=2.5°);②前馈一阶低通LP=0.3(抹70Hz阶跃);③跳变(重锚定/限位钳位)立即清零。绑定speed_feedforward_ptr+feedforward_flag=SPEED_FEEDFORWARD,与OutputFilter(输出端)分层不冲突。
- **文件与符号**:application/gimbal/gimbal.c(static三变量+计算块+配置绑定)、config/robot_infantry_2026.h(PITCH_REF_FF_GAIN/MAX_RATE/MAX_DELTA/LP)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 10.00%)。**实车未回归(RM-004)**。
- **判读**:松杆即停(跟手)→RM-004关闭;若运动中重新出现纹波/异响→LP降至0.2(前馈更平)或GAIN降至0.8;若起步变冲→GAIN降至0.8。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-12｜2026-10-30|裁判系统一期:链路接入(UI任务启用+注册)

- **目标与触发**:RM-004关闭后进入下一阶段。用户确认:硬件未接/分期实施/暂缓UI定制。一期=最小链路,烧录后接硬件即可联调。
- **功能变化**:①TASK_UI_ENABLE 0→1(UI任务启用,任务骨架StartUITASK本已写好);②chassis.c恢复UITaskInit(&huart6,&ui_data)注册(从#if 0拆出,超电部分保持#0),补referee_data/ui_data静态声明;③referee_task.c的MyUIInit等待循环加LOGINFO提示(硬件未接时1Hz提示"waiting for robot_id",接好自动继续,联调可见状态)。UI内容=跃鹿原版(RobotModeTest假数据循环保留,验证链路用,二期接真实数据后关闭)。
- **文件与符号**:application/robot_task.h、application/chassis/chassis.c(+2静态声明+注册)、modules/referee/referee_task.c(+bsp_log include+等待日志)。
- **接口与调用链**:UI任务100Hz?(osDelay(1)循环)独立于ROBOT任务;裁判串口=huart6(中断收);UI数据源=chassis.c的ui_data(一期全0,RobotModeTest假数据驱动)。
- **配置/API/协议影响**:协议referee_protocol.h已适配2026(HP=20B,热量=14B);FLASH 10.80%(+0.8%)/RAM 73.61%(+1.2%,UI任务栈512字)。
- **验证**:ARM编译0 error第一方0 warning;主机测试ALL PASS;审计8 Changed/0 Added/0红旗(新增变更:robot_task.h/chassis.c/referee_task.c)。**实车未联调(RM-005)**:待用户接裁判主控(USART6口,4pin)上电。
- **下一步(二期)**:硬件联调(看UI能否画出)→关RobotModeTest接真实模式数据(cmd三路模式喂ui_data)→热量/弹速进cmd→射速热量管理。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261030-13｜2026-10-30|键鼠控制实装(图传链路)

- **目标与触发**:用户确认键鼠输入自动激活+现在写好等图传。模块层解析全现成(DBUS帧含键鼠字段:鼠标XY/按键/键盘16键位掩码+沿计数),cmd层零键鼠逻辑为全部工作量。
- **功能变化**:①输入源分发UpdateInputSource():鼠标移动/点击或任意键盘键激活键鼠模式,3s无输入回落遥控,切换打LOG;图传未装时键鼠字段恒0,永不激活(天然安全)。②KeyboardMouseSet()映射:W/S/A/D平移(Shift×0.4慢速,chassis端斜坡平滑)、鼠标XY增量云台(KBM_*_SENS灵敏度,pitch限幅)、Q沿切换小陀螺/跟随、右键摩擦轮、左键按住连发、R沿单发(LOAD_1_BULLET,shoot端沿检测)、F沿触发yaw回正(复用解锁回中状态机)。③RobotCMDTask输入源分发接入。仲裁/看门狗与输入源无关,遥控急停挡键鼠模式下依然有效。
- **文件与符号**:application/cmd/robot_cmd.c(Input_Source_e/UpdateInputSource/KeyboardMouseSet+分发)、config/robot_infantry_2026.h(KBM_YAW/PITCH_SENS/SLOW_RATIO/TIMEOUT_MS/MOUSE_DEADZONE)。
- **接口与调用链**:三路cmd_send结构与协议零改动;shoot_cmd的load_mode复用LOAD_1_BULLET沿检测语义(键鼠R沿=单帧请求)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 10.89%);主机测试仲裁+看门狗PASS(gcc直跑);审计8 Changed/0 Added/0红旗。**实车未验证(RM-006):图传未装,键鼠数据恒0,代码就绪待图传联调**。
- **下一步(装图传后)**:键鼠在线确认(RTT看"input source: keyboard-mouse")→鼠标灵敏度手感→Q/R/F功能逐项→与裁判UI二期合并联调。
- **AI**:GLM(WorkBuddy) 2026-10-30。

### DEV-20261102-01｜2026-11-02|键鼠代码复审:修未解锁累积与切入补触发

- **目标与触发**:图传出图(RM-005关闭)后键鼠链路可测,用户要求代码复审。
- **功能变化(两处真bug)**:①**未解锁时鼠标累积云台目标**——KeyboardMouseSet的yaw/pitch是增量累加,原无解锁门控,停机晃鼠标会把目标角累积(pitch无重锚定,解锁后台会先甩到累积位置);补 robot_state==ROBOT_READY && RemoteControlIsOnline() 门控(与RemoteControlSet杆量累加一致)。②**切入键鼠瞬间补触发Q/R/F**——沿计数缓存仅在键鼠模式更新,遥控模式下按过的键在切入帧 count!=last 成立→误触发;改为切源瞬间同步三条沿计数基线。
- **文件与符号**:application/cmd/robot_cmd.c(UpdateInputSource切源同步基线、KeyboardMouseSet云台段门控)。
- **验证**:ARM编译0 error第一方0 warning(FLASH 10.89%);主机测试仲裁PASS;审计0红旗。**实车待验证(RM-006)**。
- **判据与地址(本次产物)**:input_source@0x2000BF0C(0=遥控1=键鼠)、spin_enabled@0x2000BF05、gimbal_cmd_send@0x2000BF54(yaw+0/pitch+4)、chassis_cmd_send@0x2000BF74(vx+0/vy+4/mode+16)、shoot_cmd_send@0x2000BF14;RTT关键日志"input source: keyboard-mouse"/"kb spin on/off"。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-02｜2026-11-02|键鼠链路打通(DBUS路径)与灵敏度初调

- **目标与触发**:图传/裁判系统就绪后调键鼠,端到端排查(emberprobe直读板上变量)定位到数据源缺失。
- **根因与结论(重要,写入知识库)**:2026赛季键鼠有两条下发路径,任选其一:①**DBUS路径(本车采用)**:Client采集键鼠→**电脑MicroUSB连NDJ6/DT7遥控器底部调参口**→2.4G→DR16→DBUS→C板,需装DJI_WIN_Driver_Installer、Client管理员权限运行、遥控器左上开关中挡;②图传链路0x0304:Client→图传→主控→C板USART6(协议头已有ID_remote_control=0x0304但rm_referee.c未实现解析,本车未用)。**实测:路径①打通后键鼠可用,代码零改动**(DBUS解析与cmd键鼠映射均为现成)。此前判断"必须走图传0x0304"有误,已纠正。
- **功能变化**:鼠标灵敏度过高,两级系数砍半:KBM_YAW_SENS 0.5→0.25、KBM_PITCH_SENS 0.3→0.15(Client"控制灵敏度"为另一级,实时可调)。
- **文件与符号**:config/robot_infantry_2026.h(两个灵敏度宏+注释澄清DBUS路径)。
- **验证**:编译0 error(FLASH 10.89%);实车键鼠可用(用户确认"可以用了"),灵敏度手感待复测。
- **排查方法论沉淀**:emberprobe MCP(agent-bridge HTTP 127.0.0.1:49671/v1/call)可直读板上变量;本次靠它定位到rc_ctrl键鼠字段恒0+robot_state=0(未解锁),避免盲改代码。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-03｜2026-11-02|鼠标手感柔化:输入低通+单周期角度限幅(摇杆感)

- **目标与触发**:键鼠链路打通后用户反馈鼠标"很冲、很快",希望接近遥控摇杆的手感。
- **诊断**:鼠标是位置型输入(单帧位移直接累加目标角),手一甩目标角瞬间跳几十度,云台以角度环MaxOut(180°/s)满速猛追→冲感;摇杆是速度型(杆量→目标转速,缓启动/松手即停)。
- **功能变化**:两级柔化——①鼠标增量一阶低通(KBM_INPUT_LP=0.35,约3周期收敛)抹掉手抖/甩动尖峰;②单周期目标角限幅(KBM_MAX_DEG_PER_CYCLE=0.6°≈120°/s@200Hz,低于角度环MaxOut留跟踪余量),等效"转速上限"像杆量限速。未解锁/失联时滤波器清零防残留突跳。灵敏度系数保持0.25/0.15。
- **文件与符号**:application/cmd/robot_cmd.c(static kbm_yaw_filt/kbm_pitch_filt+云台段重写)、config/robot_infantry_2026.h(KBM_INPUT_LP/KBM_MAX_DEG_PER_CYCLE)。
- **验证**:ARM编译0 error(FLASH 10.91%);主机测试仲裁PASS。实车手感待验证。
- **调参档**:仍冲→LP降0.2或MAX_DEG_PER_CYCLE降0.4;变"肉/拖"→LP升0.5或上限升0.9;准度不够→只调Client控制灵敏度不要动LP。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-04｜2026-11-02|鼠标曲线映射(借鉴开源quadratic思路)

- **目标与触发**:用户要求参考优秀开源资料的键鼠优化思路。查证:UT-RAS开源(deepwiki)鼠标→云台为"位置型+可配灵敏度",摇杆用dead zone+quadratic映射;键盘底盘用固定加减速率做惯性平滑(本车chassis端斜坡5000mm/s²等效覆盖);2025赛季开源热门为云台SMC滑模控制器(复旦星云EGA/川大火锅),属控制器架构级替换,记为赛季后选项。
- **功能变化**:鼠标→云台加曲线增益:gain=1+KBM_CURVE_QUAD*(|原始手速|/KBM_CURVE_SCALE),上限KBM_CURVE_MAX;慢手≈1倍保瞄准精度,快甩放大快速转身;纯乘法实现(无pow,零FPU开销);**增益基于低通前的原始|mouse|计算**、作用于低通后的增量,避免快甩增强被低通吃掉。QUAD=0可退回线性。
- **文件与符号**:application/cmd/robot_cmd.c(云台段增益计算)、config/robot_infantry_2026.h(KBM_CURVE_QUAD/SCALE/MAX)。
- **验证**:ARM编译0 error(FLASH 10.92%);主机测试仲裁PASS。实车手感待验证。
- **调参档**:转身慢→QUAD 1.5或SCALE 35;瞄准跳→QUAD 0.5或SCALE 70;完全线性→QUAD 0。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-05｜2026-11-02|键鼠方向修正(A左D右 + 鼠标上滑抬头)

- **目标与触发**:用户要求 A 左 / D 右、鼠标上滑枪口上抬。
- **功能变化**:①A/D 的 vy 符号对调(A +=、D -=),并引入 KBM_STRAFE_DIR;②pitch 取反(KBM_PITCH_DIR=-1,mouse.y上滑为负,取反后上滑=抬头)。**两个方向宏化**:反了改±1即可,不用改逻辑——本车 chassis 的 vx/vy 系数是整体取反实车标定的,平移方向无法纯理论推断,必须实车裁决。
- **文件与符号**:application/cmd/robot_cmd.c(平移vy符号+dpitch乘KBM_PITCH_DIR)、config/robot_infantry_2026.h(KBM_STRAFE_DIR/KBM_PITCH_DIR)。
- **验证**:ARM编译0 error(FLASH 10.92%);主机测试仲裁PASS。**实车方向待用户确认**:若A/D或pitch仍反,直接改对应DIR宏为相反值。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-06｜2026-11-02|小陀螺平移不稳:平移投影方向宏化(本车旋转矩阵相反)

- **目标与触发**:用户反馈小陀螺(CHASSIS_ROTATE)模式下平移不正常,要求平移稳定。
- **诊断**:三处证据指向云台系→底盘系平移投影方向反了:①麦轮分解四个wz项全为负号(标准应左右相反)→wz符号约定被取反;②调参结论§3"本车旋转矩阵约定相反"(跟随增益+1.5才稳);③投影矩阵仍是标准旋转(上游原样)。小陀螺时offset_angle以300deg/s大范围变化,方向错被放大→平移反向画圈;跟随模式offset小(cos≈1/sin≈0)故平时不显形。**注:输出限幅为四轮等比缩放(保比例),饱和只会整体变慢不会导致方向不稳,已排除。**
- **功能变化**:投影theta乘以CHASSIS_OFFSET_PROJ_DIR(默认-1.0即反向)。
- **文件与符号**:application/chassis/chassis.c(投影theta)、config/robot_infantry_2026.h(CHASSIS_OFFSET_PROJ_DIR)。
- **验证**:ARM编译0 error(FLASH 10.92%);主机测试仲裁PASS。**实车待验证**:小陀螺按W应沿云台指向直线前进不画圈;若跟随模式平移变斜走或小陀螺仍画圈→宏改+1。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261102-07｜2026-11-02|回滚DEV-06小陀螺投影方向改动

- **目标与触发**:用户要求退回DEV-06(小陀螺平移投影方向取反)。实测反馈不佳,具体现象待用户补充。
- **功能变化**:撤销chassis.c投影theta的CHASSIS_OFFSET_PROJ_DIR乘子、删除该宏;投影恢复上游标准旋转矩阵。其余改动(键鼠方向/曲线/限幅/柔化)全部保留。
- **文件与符号**:application/chassis/chassis.c、config/robot_infantry_2026.h。
- **验证**:ARM编译0 error(FLASH 10.92%);全工程已无CHASSIS_OFFSET_PROJ_DIR引用。
- **待办**:小陀螺平移不稳问题重新定性——需明确现象是"画圈/斜走/打滑/抖动"哪一种,再决定改跟随律/投影/wz符号。**在现象明确前不再盲改坐标系符号**(本轮教训:投影取反可能连带破坏跟随模式平移,单变量验证次序不对)。
- **AI**:GLM(WorkBuddy) 2026-11-02。

### DEV-20261103-01｜2026-11-03|小陀螺纯自转向右微漂:偏心参数试错与回退

- **目标与触发**:用户实测纯自转(OFFSET=0,无平移输入)时车向右缓慢漂移。纯自转时投影公式乘0不参与→漂移与坐标系/投影无关,定性与机械/几何相关。
- **诊断**:①CENTER_GIMBAL_OFFSET参数本义=旋转中心偏心(代码按偏心点建四轮速度场),OFFSET=0=假设"云台轴=几何中心";②用户自行试了OFFSET_X=-20、-40(效果未回报),当前回退至0基线。③麦轮厂商技术文/底盘打滑长文佐证:跑偏漂移主因=四轮接地压力不均、辊子磨损、车架形变、质心偏移,属机械层。
- **功能变化**:config回退CENTER_GIMBAL_OFFSET_X至0(用户要求)。
- **验证**:编译0 error(FLASH 10.92%)。
- **下一步(关键区分实验)**:①正反转对照:正转漂右,反转若漂左=几何偏心(参数可修,正向+20起试);若反转仍漂右=系统性偏差(打滑/接地不均,参数难修,机械排查:四轮着地纸测/辊子磨损/X型确认)。②降自转速度150对照:漂移角度减小=打滑;不变=偏心。
- **AI**:GLM(WorkBuddy) 2026-11-03。

### DEV-20261103-02｜2026-11-03|反转受限实锤(线束/限位)+OFFSET_X转正向试

- **目标与触发**:用户做了自转速度对照:ROTATE_SPEED=+300向右漂(已知);ROTATE_SPEED=-150出现左右摆头,且云台不稳定跟着底盘一起转。
- **诊断(重要)**:反向自转时"云台被拖转+底盘摆头"=**云台-底盘线束/机械限位到达余量极限被拉扯**——底盘反向转,云台被线束拽着走,拽到底弹回→摆头。正转(+300)方向线束余量充足,故正转时云台稳、键鼠可测。**结论:小陀螺只能正转,反向自转在本车机械状态下不可用**;正反转对照实验作废,偏心方向只能正向试错。
- **功能变化**:①ROTATE_SPEED恢复+300(用户改-150做实验后);②OFFSET_X 0→+20(负向-20/-40实测无效,转正向)。
- **文件与符号**:config/robot_infantry_2026.h(CHASSIS_ROTATE_SPEED恢复、CENTER_GIMBAL_OFFSET_X=+20)。
- **验证**:编译0 error(FLASH 10.92%)。
- **下一步**:正向二分:OFFSET_X=+20→漂右减轻→+40/+60;加重→说明漂移非对称性可补偿,转IMU投影或机械排查(四轮接地/磨损)。另:反向自转受限提示**小陀螺长时间使用会缠线**,后续需评估"周期性解线"或"限幅自转"机制(记入待办)。
- **AI**:GLM(WorkBuddy) 2026-11-03。

### DEV-20261103-03｜2026-11-03|小陀螺平移方向偏置参数(用户提议方案)

- **目标与触发**:用户提议"设一个参数让小陀螺平移时偏右一点点"——放弃继续标定OFFSET偏心(试错成本高),改为直接给平移指令叠加固定方向偏置,操作手可感知可描述。
- **功能变化**:①config新增CHASSIS_TRANSLATE_BIAS_ANGLE=-10deg(负=向右偏);②chassis.c在平移斜坡之前,ROTATE模式下将云台系平移指令旋转该角度(斜坡作用于补偿后指令)。仅在ROTATE模式生效,跟随模式平移不受影响。
- **与OFFSET_X的关系**:两种独立机制(OFFSET改轮速分配对称性/BIAS直接旋转平移指令),为避免双重补偿,OFFSET_X已回0,以BIAS为主调参数。
- **文件与符号**:application/chassis/chassis.c(斜坡前偏置旋转)、config/robot_infantry_2026.h(CHASSIS_TRANSLATE_BIAS_ANGLE)。
- **验证**:ARM编译0 error(FLASH 10.93%);主机测试仲裁PASS。实车待验证:小陀螺按W轨迹应从"偏左"转为"偏右10°"→继续调角度至轨迹与云台指向重合。
- **调参档**:偏左仍在→|角度|加大(-15/-20);变偏右过多→减半(-5);方向反了→改+10。
- **AI**:GLM(WorkBuddy) 2026-11-03。

### DEV-20261105-01｜2026-11-05|工程接入 git 管理,推送 GitHub 私有仓库

- **目标与触发**:用户要求将工程纳入 git 版本管理并上传 GitHub 远程仓库进行团队协作。
- **实施**:①winget 安装 gh CLI v2.103.3;②gh auth login 设备码授权(账号 youyucat),后补 workflow scope(首推被拒:OAuth token 无 workflow 权限不能推送 .github/workflows/);③git 全局身份 youyucat + GitHub noreply 邮箱(192050448+youyucat@users.noreply.github.com);④git init -b main;⑤初始提交 503 文件/356119 行;⑥gh repo create 私有仓库并推送。
- **远程仓库**:https://github.com/youyucat/RM-JIT-CR (PRIVATE,默认分支 main,main...origin/main 已同步)。
- **入库范围**:源码(application/modules/bsp/Src/Inc/config)、Drivers/Middlewares(第三方但编译必需)、文档(RM_DEVLOG/.Doc/README/AGENTS.md)、tests、CI workflow、.agents/skills;build/(12MB)被 .gitignore 排除。.workbuddy/memory(会话笔记)已入库,如不需要可加入 .gitignore。
- **行尾策略**:仓库内统一 LF(.gitattributes 既定策略);add 时 CRLF→LF 转换警告为预期行为,非异常。团队克隆后工作区统一为 LF,避免跨设备假 diff。
- **验证**:git status 干净(本地与远端一致);审计口径:初始提交=2026-10-29 基线+DEVLOG 已记录开发条目(8 文件),状态可追溯。
- **下一步**:队友在 GitHub 仓库 Settings→Collaborators 邀请;克隆用 `git clone https://github.com/youyucat/RM-JIT-CR.git`;拉取后跑基线审计核对状态。
- **AI**:GLM(ZCode) 2026-11-05。

### DEV-20261105-02｜2026-11-05|新增团队协作指南,修复 CI 触发分支与构建命令

- **目标与触发**:用户要求编写日常团队协作操作指南;顺带发现 CI 两处既有问题。
- **实施**:①新增《.Doc/团队协作指南.md》(克隆接入/分支策略/提交规范/每日流程/调参标准流程/冲突与撤销/禁令/FAQ),README 文档索引加入口;②CI 修复:触发分支 master→main(原配置与仓库实际分支不一致,永不触发);ninja 步骤 `cd build && cmake ..` 改为 `cmake -G Ninja -B build && ninja -C build`(CI 检出后无 build 目录,原命令必失败)。
- **文件与符号**:.Doc/团队协作指南.md(新增)、.github/workflows/c-cpp.yml、README.md。无源码/参数改动。
- **验证**:推送 main 后 GitHub Actions "C/C++ CI" run 42017496072 **completed/success**(make 与 CMake/Ninja 双构建均通过)。
- **AI**:GLM(ZCode) 2026-11-05。

### DEV-20261105-03｜2026-11-05|遥控拨轮改扳机式(短拨单发/长按连发)+键鼠E键三发

- **目标与触发**:用户反馈拨轮深度分段(浅段单发-240~-460/深段连发<-460)比赛时太难控,要求优化模式切换。
- **功能变化**:①遥控拨轮改**扳机式**:压过DIAL_TRIGGER_ZONE(-240)即触发区不分深浅,短拨松手=单发(松手沿,发布1周期),按住≥DIAL_SINGLE_HOLD_MS(250ms)=连发(持续,松手停);进/出阈值差20刻度构成回中迟滞滤边界抖动;摩擦轮关闭立即复位扳机状态。②键鼠新增**E键三发**(按下沿发布LOAD_3_BULLET,与R单发同构);注:DR16键值表无T键(16键W~B),E为R同排右邻。③config清理死参数LOADER_TARGET_RPM(连发已改shoot_rate换算,全仓库无引用)。
- **shoot端零改动**:扳机式产出仍是LOAD_1_BULLET(1周期)/LOAD_BURSTFIRE(持续)/LOAD_STOP,与原分段协议完全一致。
- **边界推演**:急停/仲裁期间静态变量残留,解锁后首周期时长差必超判别时长→判连发语义→STOP,无误触发单发;连发后松手不补发单发。
- **文件与符号**:application/cmd/robot_cmd.c(RemoteControlSet扳机状态机/KeyboardMouseSet三发/沿基线同步+last_e_count)、config/robot_infantry_2026.h(DIAL_TRIGGER_ZONE/RELEASE/SINGLE_HOLD_MS替换DIAL_SINGLE/BURST_ZONE,删LOADER_TARGET_RPM)、README.md(操作表)。
- **验证**:ARM编译0 error 0 warning第一方(FLASH 10.94%+136B,RAM+8B);主机测试ALL PASS。实车待验证:短拨出单发/按住250ms起连发/松手即停/E键三连出弹。
- **调参档**:连发起动延迟嫌慢→DIAL_SINGLE_HOLD_MS降至150-200;误触发连发→加大至350;扳机太灵敏→DIAL_TRIGGER_ZONE下移至-300。
- **AI**:GLM(ZCode) 2026-11-05。

### DEV-20261105-04｜2026-11-05|pitch水平及以上上抬异响复发:输出低通RC 0.005→0.008

- **目标与触发**:协作者反馈云台pitch上抬到水平及以上角度时连续嗡嗡异响,只上抬有/停手即消——DEV-10特征复发(高电流工作点下陀螺噪声xKp2865纹波被齿轮啮合放大)。
- **处理依据**:按《本车调参结论》§2整定路径档案(gimbal.c速度环注释)既定阶梯"异响仍在→RC升至0.008",属参数级改动,不新增结构。DEC-004约束已知悉:若RC加大后仍响,下一步转重力前馈前必须先架空松杆标定各角度稳态电流+取telemetry波形(DEV-05盲上否决先例),不再盲改。
- **改动内容**:gimbal.c pitch速度环 `Output_LPF_RC` 0.005→0.008(截止~32Hz→~20Hz,推杆动态<5Hz无损,上抬手感略增迟滞)。
- **验证**:gimbal.c主机gcc语法级检查0 error 0 warning(第一方);主机单元测试ALL PASS;ARM全量编译未执行(本机无arm-none-eabi-gcc,烧录前在开发机补跑)。**实车待验证:上抬过水平及以上角度异响消失、上抬无"肉"感、停手姿态保持无下坠。**
- **回退路径**:上抬变"肉"→RC回调0.006;异响仍在→架空标定重力电流后转重力前馈,或RC再升0.010。
- **AI**:Kimi(Moonshot AI,WorkBuddy) 2026-11-05。

### DEV-20261106-01｜2026-11-06|键鼠视角手感FPS化:线性1:1映射+角速度域限幅+灵敏度运行时调档

- **目标与触发**:用户要求键鼠视角手感对齐热门FPS(CSGO/无畏契约)。旧方案三宗罪:①加速曲线(快甩3倍增益)=FPS玩家最忌讳的"鼠标加速度",破坏肌肉记忆;②70Hz帧计数被200Hz控制周期重复施加~2.86次,有效灵敏度与帧率耦合飘忽;③输入低通0.35(~9ms群延迟)+单周期0.6°限幅(等效120°/s硬顶),大甩不跟手。
- **映射模型**:位置域"计数×灵敏度直接加目标角"改为**计数→角速度→按dt积分**:ω=滤波后计数×SENS/DBUS帧周期0.014s,每控制周期yaw+=ω×0.005——同帧驻留的计数恰好施加一次,灵敏度与帧率/控制率解耦;固定增益无曲线(CSGO式)。角速度域限幅替代单周期限幅:yaw 450°/s(贴速度环Kp调度HIGH区)、pitch独立150°/s(防甩过头)。
- **灵敏度运行时调档**:V降/B升(×0.85步进,范围0.2~2.5)G重置,全局倍率对config基准缩放,Float2Str回显RTT;沿基线同步补进UpdateInputSource(仿Q键防补触发)。
- **前馈联动**:YAW_REF_FF_MAX_RATE 180→450(否则大甩时Δref超0.99°阈值,前馈被判跳变静默失效,位置环硬拖手感发肉);pitch 150°/s→单周期0.75°<阈值2.52°无需动。
- **文件与符号**:application/cmd/robot_cmd.c(KeyboardMouseSet鼠标段重写+V/B/G调档块+last_v/b/g_count)、config/robot_infantry_2026.h(KBM_YAW_SENS 0.25→0.7/KBM_PITCH_SENS 0.15→0.43=旧有效增益2.86x基线,KBM_INPUT_LP 0.35→0.6,删KBM_MAX_DEG_PER_CYCLE/KBM_CURVE_*,新增KBM_DBUS_FRAME_S/KBM_CMD_DT_S/KBM_YAW_MAX_RATE/KBM_PITCH_MAX_RATE/KBM_SENS_SCALE_*,YAW_REF_FF_MAX_RATE→450)。
- **验证**:基线审计与git历史一致(无未记录改动);ARM编译0 error 0 warning第一方(FLASH 10.96%,RAM 73.64%);主机测试ALL PASS。**实车待验证(用户执行):①慢移线性跟手无加速感②快甩~180°一步到位不冲头③V/B/G调档RTT回显④解锁/切模式瞬间无跳变⑤telemetry_yaw对比改前甩枪波形(DEC-004:改前先取现固件基线)**。
- **回退路径**:git revert本条即可回到DEV-20261102-03/04方案;灵敏度倍率不合适实车V/B直接调,无需回退。
- **AI**:GLM(ZCode) 2026-11-06。

### DEV-20261109-01｜2026-11-09|修复单发/三发待发队列悬挂导致停火后补发

- **问题**:`loader_pending` 在摩擦轮关闭时不清零,而发射超时只在"已起步"后才计时。三发打到一半松开右键,剩余弹数永久挂在"等待互锁"分支;下次开摩擦轮时互锁一通过,残余弹数自动出膛——未瞄准的误射。同时因 `loader_pending != 0`,期间按 R/E 被静默忽略,表现为"按键失灵"且无任何报错。
- **根因**:状态机隐含假设"待发队列总由有效工况驱动",但摩擦轮开关是独立信号,可随时打断队列;三条终止路径(到位/堵转/超时)都要求"已起步",缺少"从未起步"的出口。
- **修复**:①摩擦轮持续关闭超过 `LOADER_FRICTION_OFF_GRACE_MS`(100ms)时丢弃队列并 LOGINFO;②新增 `loader_wait_ms`,等待互锁超过 `LOADER_WAIT_TIMEOUT_MS`(2000ms)时丢弃并 LOGERROR。
- **宽限期的理由**:按 R/E 与按右键常在同一帧,右键的摩擦轮标志可能比按键沿晚一个控制周期(5ms);若"一关就清"会把刚登记的请求误删,变成新的"按键没反应"。
- **为何不改在 LOAD_STOP 分支**:单发/三发是沿触发,cmd 只发一帧 LOAD_3_BULLET,下一帧即回落 LOAD_STOP;在该分支清零会让登记当场失效,一发都打不出去。
- **文件与符号**:application/shoot/shoot.c(新增静态量 loader_wait_ms / friction_off_ms;状态机前加停火清队列、等待分支加超时出口,各清零点同步复位)、config/robot_infantry_2026.h(LOADER_WAIT_TIMEOUT_MS、LOADER_FRICTION_OFF_GRACE_MS)。
- **验证**:ARM编译 0 error、第一方 0 warning(FLASH 10.96% / RAM 73.64%);逻辑走查覆盖 5 个场景(正常三发 / 中途关轮 / 同帧按键 / 摩擦轮不到速 / 先按R后开轮)。
- **实车待验证(用户执行)**:①三发中途松右键、再开轮,确认无弹丸补发;②同时按 R 与右键,确认不吞弹;③RTT 出现 `dropped` 提示时能对应到实际操作。
- **回退路径**:git revert 本条即回到原行为;注意原行为存在误射风险,不建议回退。
- **AI**:DeepSeek-V4.1-Flash 2026-11-09。

### DEV-20261111-01｜2026-11-11|yaw GM6020 切电流命令帧 + 空载自检固件

- **目标与触发**:用户要求把 yaw GM6020 换成电流控制模式(0x2FE),并先确认"电流帧到底能不能出力"。历史记录(《本车调参结论》§1、DEC 台账"0x2FE 电流帧暂不启用")显示 2026-10 曾实测电流帧零扭矩,故本轮同时提供可复现的判定手段,而不是只翻一个开关。
- **驱动层结论**:无需改动。`dji_motor.c` 已支持 `MOTOR_CONTROL_CURRENT`(分组自动切 0x1FE/0x2FE,同组混用 fail-fast),`motor_def.h` 已有 `Motor_Control_Mode_e`。本轮只动业务层参数。
- **功能变化**:①命令帧由常量改为 `YAW_ESC_CURRENT_MODE` 开关派生——`control_mode`、速度环 `MaxOut`(25000↔16384)、输出域换算系数(1.0↔0.6554)三处由同一开关绑定,避免"改一漏一";②补 `Ki` 乘域换算系数(原实现只缩放 Kp/前馈,切电流域后积分作用会相对强 1.5 倍);③新增 `YAW_ESC_SELFTEST_ENABLE` 自检:上电开环给 ±1638(0.3A)各 200ms,按转速峰值 + 实际电流判 PASS/FAIL,RTT 打印;自检后本固件不再接管云台。
- **判据为何这样定**:电流帧不被接受时反馈报文字段完全正常(角度/转速/温度都有),只有"出力"这一观测能证伪,故取转速峰值(门限 50deg/s,静止噪声 <5deg/s);实际电流用于区分"电流环生效"与"码值被当成电压解释"。
- **文件与符号**:config/robot_infantry_2026.h(新增 YAW_ESC_CURRENT_MODE / YAW_ESC_OUT_MAX / YAW_ESC_SELFTEST_ENABLE,YAW_CURRENT_DOMAIN_SCALE 改为派生)、application/gimbal/gimbal.c(YawEscSelfTest 状态机 + GimbalTask 入口接管、speed_PID.Ki 乘域系数、control_mode 三元、新增 bsp_log.h 依赖)。
- **接口与协议影响**:无协议改动;`DJIMotorInstance.motor_settings` 在自检期间被改成 OPEN_LOOP,不回填(自检固件不跑常规云台)。
- **验证**:用户 2026-11-11 自行编译通过。AI 侧 ARM 编译仅到 gimbal.c 单文件 0 error 0 warning 一轮,随后本机 `build/` 因 CMakeCache 源码目录失效(C:/RoboMaster/RM-JIT-CR)重建,编译器解析落到 C:\Toolchain(15.3.1)且沙箱内子进程无法写 .obj,未取得全量编译证据 —— 全量编译证据以用户侧为准。
- **实车待验证(用户执行)**:空载下烧录,RTT 应依次出现 `[esctest] feedback ok…` / `pos pulse` / `neg pulse` / 判决行。PASS = 电流帧可用;FAIL 且 avg_cur≈0 = 帧未被接受(电调固件/Assistant 设置)。对照实验:`YAW_ESC_CURRENT_MODE=0` 同法跑一遍应 PASS。
- **回退路径**:`YAW_ESC_CURRENT_MODE=0` 回到历史电压模式全套(三处自动跟随);`YAW_ESC_SELFTEST_ENABLE=0` 恢复常规云台控制。
- **AI**:DeepSeek-V4.1-Flash 2026-11-11。
- **实车结果(2026-11-11 晚,用户执行)**:mode=1 电流帧空载脉冲**完全无反应**;切 mode=0(电压帧,同一自检代码、同一脉冲)后电机**明显动了一下**——自检链路(任务/反馈/发帧/判据)全部走通,差异只剩帧类型。**初判:该电调不执行 0x2FE 电流帧,与 2026-10 记录一致。** 已回退:YAW_ESC_CURRENT_MODE=0、YAW_ESC_SELFTEST_ENABLE=0。旁证:台架 RTT 日志被 `[bsp_can] CAN Send FIFO full` 刷屏(未供电节点导致 CAN2 无 ACK 队列积压+全车离线告警),esctest 行被淹没未捕获,判定以肉眼观测为准;上车供电齐全不会出现此现象。
- **翻案(同日 20:30,用户在 RoboMaster Assistant 开启"电流环"后复测)**:电流帧自检 **PASS**(用户观测:先一方向快转再反向刹停,速度明显)。**根因不是固件版本,而是 Assistant 里"电流环"开关未开**——0x2FE 在开关关闭时被电调忽略。已在调参结论/证据表更新结论;按 0.6554 等效换算直接切闭环待实车回归(见 DEV-20261111-01 待验证项)。

### DEV-20261111-02｜2026-11-11｜yaw 电流模式优化第一刀:Kp 高速调度随帧类型派生 + 自检代码移除

- **目标与触发**:用户确认继续优化 yaw。电流模式下反电动势由电调顶电压抵消(DEV-20261111-01 已验证电流帧生效),电压模式的高速 Kp 调度(20→40,DEV-116)失去补偿对象,40 在高速区是纯过增益。另:用户要求删除自检代码。
- **功能变化**:①`YAW_SPEED_KP_HIGH` 改为 `(YAW_ESC_CURRENT_MODE ? 20.0f : 40.0f)`——电流模式恒 20(LOW==HIGH,300-450 过渡区自动失效),电压模式回退时调度原样保留;②整体删除 YawEscSelfTest 状态机、ESC_ST_* 常量、YAW_ESC_SELFTEST_ENABLE 宏与 bsp_log.h 依赖,gimbal.c 恢复纯云台逻辑。
- **文件与符号**:config/robot_infantry_2026.h(YAW_SPEED_KP_HIGH 派生、自检宏删除)、application/gimbal/gimbal.c(自检代码整体移除)。
- **验证**:ARM 全量编译 EXIT=0,0 error 0 warning(当前工具链为 C:\Toolchain 15.3.1,FLASH 11.06%)。
- **实车待验证(用户执行)**:小陀螺高速推杆——发飘/啸叫/过冲消失→恒 20 正确;若"软脚"说明摩擦负载比预期大,回调 25/30 逐档试。
- **回退路径**:YAW_ESC_CURRENT_MODE=0 即恢复原电压模式全套(含 Kp 40)。
- **AI**:DeepSeek-V4.1-Flash 2026-11-11。

### DEV-20261111-03｜2026-11-11｜底盘几何量实测补全:轴距/轮距 500×460,力臂 212→340mm

- **目标与触发**:2026-11-09 因整车装配后轮轴不可达,`WHEEL_BASE/TRACK_WIDTH` 一直沿用 300×300 设计值(证据等级 `[待实测]`)。本日用户卷尺实测解除。
- **测量数据(用户 2026-11-11)**:前后轮沿车长最外缘 500mm;左右轮外侧面距 500mm;单轮宽度 40mm;轮径复测 150mm(确认 `RADIUS_WHEEL=75`)。
- **换算依据(易错的几何不对称)**:车长方向的极值点是麦轮滚子包络的圆周最前点,正对轮心平面 → 读数即轴距,不减轮宽;车宽方向的极值是滚子端面 → 读数 = 轮距 + 一个轮宽(两侧各半个),故减一个轮宽而非两个(减两个会把轮距少算 40mm)。得 `WHEEL_BASE=500`、`TRACK_WIDTH=460`。
- **功能变化**:力臂 (500/2+460/2)×0.7071 = 340mm(原 212,+60%);与真实对角线 √(250²+230²)=340 差 <0.2%,原近似公式仍成立。
- **行为影响**:①同一自转指令下车身实际转速约 +60%(此前力臂被低估、轮速指令给少),`CHASSIS_ROTATE_SPEED` 体感需重对;②平移速度不受影响;③自转叠加平移更易触 `CHASSIS_MAX_MOTOR_RPM`(6000RPM)限幅,属物理后果非调参可解。
- **文件与符号**:config/robot_infantry_2026.h(WHEEL_BASE/TRACK_WIDTH 值 + 注释重写,并纠正旧注释"外缘距离减去轮径"的错误测法)、《参数证据表》§3 与待实测优先级行、.workbuddy/memory/MEMORY.md。
- **验证**:ARM 全量编译 EXIT=0,0 error 0 warning。
- **实车待验证(用户执行)**:小陀螺体感与自转实际转速;自转+平移叠加是否触限幅。
- **AI**:DeepSeek-V4.1-Flash 2026-11-11。



### DEV-20261112-01｜2026-11-12｜P0 装机前预备:前馈结构化、α 源可切、停机清积分、标定遥测

- **目标与触发**:按 `.Doc/yaw轴深化改造计划.md` 的 P0 四项,把 yaw 整改的**地基**先打好,不涉及任何标定与参数改动,装机后可直入 P1 基线。
- **功能变化(默认行为不变)**:
  1. **P0-1** 删除死宏 `YAW_ECD_GREATER_THAN_4096`(全工程零引用,仅 config 一行定义)。
  2. **P0-2** `DJIMotorControl` 的停止分支里对三环调用 `PIDClear()`:此前 `DJIMotorStop` 只置 `stop_flag`,积分跨停机保留,再次使能瞬间会带出残余力矩。复用现成 API(DM 电机模式切换处已有先例),**波及全部 DJI 电机**,不只是 yaw。
  3. **P0-3** 惯量+摩擦前馈从 `GimbalTask` 内联块抽成 `UpdateYawFeedforward()`;新增 `YAW_INERTIA_FF_SRC` 选择 α 源:0 = IMU 世界系实测差分(默认,与改造前逐表达式一致)、1 = 参考轨迹 α_ref(先零阶保持再低通再差分,默认关闭,[待实测])。**注意两种源的主语都是世界系 α**——底盘加减速时关节系建模需补 `+J·α_chassis`,本车不采用该记法。
  4. **P0-4** 标定期遥测:`telemetry_yaw.extra` 改为**关节转速**(`speed_aps`,摩擦前馈的自变量);新增 `YAW_FF_CALIB_LOG` 每 0.5s 打一行 `[yawcal] out/spd/cur/ff`,覆盖 P2 拟合所需四列。
- **文件与符号**:config/robot_infantry_2026.h(删死宏;新增 `YAW_INERTIA_FF_SRC`/`YAW_VREF_LP`/`YAW_FF_CALIB_LOG`)、application/gimbal/gimbal.c(`UpdateYawFeedforward`/`UpdateYawAlphaRef`、遥测 extra、标定日志)、modules/motor/DJImotor/dji_motor.c(停止分支清积分)。
- **接口与调用链**:无协议改动;新增两个 static 函数(文件内),`GimbalTask` 调用顺序不变;`dji_motor` 对外接口未变。
- **验证**:ARM 全量编译 EXIT=0,0 error 0 warning(FLASH 11.08% / RAM 73.65%);主机测试 `chassis_remote_test` PASS、`cmd_watchdog_test` ALL PASS(0 failures);死宏全工程 grep 0 命中;src=0 路径经 diff 逐表达式核对与改造前等价。
- **未验证项**:`YAW_INERTIA_FF_SRC=1`(α_ref 链)未上过实车,默认关闭;标定日志默认开启,会产生 0.5s/行输出,标定结束应与 extra 语义一并恢复。
- **AI**:DeepSeek-V4.1-Flash 2026-11-12。

### DEV-20261112-02｜2026-11-12｜pitch P0 装机前预备:重力前馈通路接通(默认关闭)+ 标定遥测 + 钳位核对

- **目标与触发**:按 `.Doc/pitch轴优化计划.md` 的 P0 执行。云台结构与材料整体更换,pitch 参数全部待重测,
  P0 只做**不改行为**的地基工作,装机后可直入 P1(机械基线)。
- **功能变化**:
  1. **P0-1** 接通 pitch 输出侧前馈通路:`current_feedforward_ptr = &pitch_gravity_ff`,
     `feedforward_flag` 由 `SPEED_FEEDFORWARD` 改为 `CURRENT_AND_SPEED_FEEDFORWARD`;
     新增 `pitch_gravity_ff`(文件内 static),由 `I(θ)=A·cos(θ−φ)+B` 在线计算,θ 取 **IMU 欧拉 Pitch**。
  2. **P0-3** 新增参数占位 `PITCH_GRAVITY_FF_GAIN`(0)/`PITCH_GRAVITY_A`/`PITCH_GRAVITY_PHASE`/`PITCH_GRAVITY_OFFSET`(均 0),
     以及 **`PITCH_GRAVITY_FF_LIMIT`(由 A/B 自动派生 1.2×(|A|+|B|))**。
  3. **P0-2** 新增 `PITCH_FF_CALIB_LOG`:每 0.5s 打 `[pitchcal] ang/out/spd/cur`;**telemetry_pitch.extra 暂不改**,
     P1 阶段保持 `Gyro[0]`(B7 敲击辨识要看振动通道,那时 out/cur 全为 0),P2 标定前再切电流码。
  4. **P0-4** 核对钳位:**无需改动**——遥控(`robot_cmd.c:331`)与键鼠(`:194`)两路写入后均
     `LIMIT_MIN_MAX(pitch, PITCH_MIN_ANGLE, PITCH_MAX_ANGLE)`,且 `chassis_remote_test` 已覆盖该断言。
- **关键实现约束(代码事实)**:`CURRENT_FEEDFORWARD` 在 `DJIMotorControl` 中是**速度环输出之后**才叠加到 `pid_ref`,
  前面没有任何钳位 → `speed_PID.MaxOut=4000` 管不到前馈,总电流可达 4000+FF。故前馈必须自带 `FF_LIMIT`,
  这条也写进了 `.Doc` 的风险矩阵。
- **文件与符号**:config/robot_infantry_2026.h(pitch 重力前馈参数段)、application/gimbal/gimbal.c
  (pitch_config 的 current_feedforward_ptr/feedforward_flag、`pitch_gravity_ff` 计算块、标定日志、bsp_log 依赖条件改为
  `YAW_FF_CALIB_LOG || PITCH_FF_CALIB_LOG`)、.Doc/参数证据表.md。
- **验证**:ARM 全量编译 EXIT=0,0 error 0 warning(FLASH 11.11% / RAM 73.65%);主机测试 PASS;
  `GAIN=0` 时 `g=0 → 钳位 0 → ff=0`,与整定前行为一致(逐表达式核对)。
- **未验证项**:重力模型三个参数全为 0(P2 才标定);`FF_LIMIT` 的派生式在 A=B=0 时为 0,属安全默认(前馈整体不动作)。
- **AI**:DeepSeek-V4.1-Flash 2026-11-12。

### DEV-20261112-03｜2026-11-12｜底盘 P0:轮速逆解实装 + 底盘反馈遥测 + 2027 协议核对

- **目标与触发**:按 `.Doc/底盘优化计划.md` P0 执行。核对手册时把"2026/2027 协议到底给不给功率"这件事查实。
- **协议结论(2027 通信协议 V1.0.0 / 20261029,用户提供)**:0x0202 长度 **14B**,前 8 字节为**保留位**,
  只有 `buffer_energy`(u16, J)、17mm/42mm 热量,且**发送频率 10Hz** → **2027 与 2026 一样不提供底盘电压/电流/功率**;
  0x0201 为 17B 且含 `chassis_power_limit`(u16)与 `bullet_speed_limit`(float),与本工程 `ext_game_robot_state_t` 完全一致。
  **结论:现有结构体本来就对,无需改协议**;但功率控制必须自建估算(电机功率模型),`buffer_energy` 只能做低频校正。
- **功能变化**:
  1. **P0-2** 实装 `EstimateSpeed()`:四轮实测转速 → 逆解 `est_vx/est_vy/est_wz`(文件内 static,刻意不进对板协议结构体,
     避免布局守卫与双板同步);`WHEEL_RATIO` 提为文件级宏,正解/逆解共用一份;新增 `_Static_assert`:
     设了 `CENTER_GIMBAL_OFFSET` 就得重推逆解,把"改了偏心忘了改公式"挡在编译期。
  2. **P0-3** 新增 `telemetry_chassis_fb` 环(第 4 环,tag `chf`,单写者=ChassisTask)与 `CHASSIS_CALIB_LOG`
     0.5s 一行 `[chcal] vx/vy/wz/gz/be`。**没有**写 `telemetry_chassis_cmd`——那条环的单写者是 RobotCMDTask。
  3. **P0-1** 仅更新注释(协议事实),无代码改动。
- **文件与符号**:application/chassis/chassis.c(EstimateSpeed 实装、WHEEL_RATIO、静态断言、遥测与日志)、
  modules/telemetry/telemetry.h/.c(第 4 环)、config/robot_infantry_2026.h(CHASSIS_CALIB_LOG)。
- **验证**:ARM 全量编译 0 error 0 warning;**RAM 73.65%→77.77%(+约 5.2KB),FLASH 11.67%**。
- **待实车(P1)**:V1 直线 / V2 横移(验符号)/ V3 **计时法转 10 圈**验力臂 / V4 滑移率基线。
  注意 `gz` 列**不能**当自转工况的底盘转速参照(小陀螺时云台世界系静止,IMU 读 0)。
- **AI**:DeepSeek-V4.1-Flash 2026-11-12。

### DEV-20261114-01｜2026-11-14｜发射摩擦轮方向反转(新结构安装镜像)

- **目标与触发**:用户确认新结构装机后摩擦轮方向反了(两轮轮面朝供弹方向推弹)。原配置 REVERSE 在右轮(`shoot.c` ShootInit),判断为电机安装镜像导致整组旋向翻转。
- **改动**:左摩擦轮 NORMAL→REVERSE、右摩擦轮 REVERSE→NORMAL(两处标志对调,`application/shoot/shoot.c`)。**reverse 只作用于输出,反馈是原始方向**——连发互锁的到速判定符号已同步互换(原"左 +目标/右 −目标"改为"左 −目标/右 +目标"),漏改会让互锁永不满足;注释写明回退方式(还原两处 REVERSE 位置 + 互锁符号)。
- **验证**:ARM 编译 0 error 0 warning(FLASH 12.21%、RAM 81.88%,含发射 P0 遥测环);主机测试 run_chassis_tests.ps1 PASS。touch 重编会暴露一个 libc_nano `_write` 链接警告——15.3.1 工具链环境产物(newlib syscall 桩),非第一方代码,RTT 日志不受影响。低速通电目测两轮接触面同朝枪口、装弹首发向前出弹——**待实车(P0-5 判据)**。
- **未验证项**:低速目测与实弹方向确认由装机后 S0/P0-5 判据承担;若低速目测仍反向,说明电机并非镜像安装而是单轮接反,回退后单独查线序。
- **配套文档**:`.Doc/发射模块优化计划.md` 新增 P0-5 并按审查修正 9 处(北科灵敏度转写错误订正/B3 判据放宽至 ±50 rpm/P4-3 摩擦轮堵转检测补位/退弹状态机联动/P4-0 对照组/实弹安全条款/遥测环 RAM 账/B4 引用作废/MaxOut=6000 补记)。
- **AI**:GLM(WorkBuddy) 2026-11-14。

### DEV-20261115-01｜2026-11-15｜底盘首测 B1:1434mm 判定为积分假象 + 轮端里程计工具

- **触发**:用户开始底盘首测「手推两米」。复查 11-15 已有的快推数据时发现结论不成立,先定案再开测。
- **发现(1434mm 是假象)**:`build/sample_straight.csv`(287 行,推车段 19609→23553ms,峰值 653mm/s)用梯形积分:
  只积到最后非零点 = **1444mm**(即此前记的 1434mm),但第 68→69 行时间间隔 **3475ms**(探针丢点),
  跨越空洞按梯形补齐 = **2287mm**,单是补齐那一段就多算 842mm。真空值落在 1444~2287 之间不可知,
  该次数据只能证明「推得很直」(航向 +0.2°、横向 +19mm),**不能作为 −28% 比例错的证据**。
  教训:3Hz 采样率下,积分不能跨丢点,跨了就必须作废。
- **替代量(已就绪)**:电机 `measure.total_angle`(转子累计角,`dji_motor.c:168-172` 每帧 CAN 反馈刷新,
  零力停止时照常累加)→ 推车前/后各一次快照求差,中间不采样,没有空窗。
  换算 `车身 mm = Δdeg/360/19 × π×150 / 0.70710678 = Δdeg × 0.097417`,2000mm 对应 **20523 deg/轮**。
  四轮差值的离散度同时给出拖轮/打滑的直接证据。
- **改动**:新增 `tools/odom.js`(主机侧只读脚本,固件零改动):`mark <标签>` 存起点快照、`delta <标签> --expect mm`
  出 `distanceMm`(四轮中位数)、`wheelSpreadPct`(离散度,>5% 判拖轮)、`errorPct`;常量运行时从
  `config/robot_infantry_2026.h` 解析 `RADIUS_WHEEL`/`REDUCTION_RATIO_WHEEL`,不复制数值。
  同步修订 `.Doc/底盘测试手册.md`:§1.2 工具表加 odom、B1 改为「odom 判决 + 采样仅作波形」、
  B1 背景订正为积分假象、B2 消除「比例错≈1434」的错误前提、B3 改用 odom 判符号、§7 记录表加 odom 列。
- **验证**:odom 换算核对 20523deg → 1999.6mm(与 2000 差 0.02%,换算链自洽);
  无调试会话时报错路径正常(`没有活动的调试会话`);当前 `probe.list` 无设备、`debug.status` none,
  实车读数待探针接入。固件未改,ARM 编译与主机测试无需重跑。
- **审计**:audit_baseline.py → Unchanged 110/199、Changed 89、Missing 0、Added 1(`tests/verify_comments_only.py`,此前已入)、**RedFlags 0**。
- **待实车(P0)**:上挡零力 → `odom mark` → 慢推 2m → `odom delta --expect 2000`;快推重复一次做 B2 判别。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

### DEV-20261115-02｜2026-11-15｜底盘首测 B 组实车完成:vx/vy/wz 三行全部验过 + 手推法精度边界定案

- **触发**:承 DEV-01,接探针(XR-DAP-V208,`1d50:6199`,serial `XRobot-XRDAP-CB9BB410535CD20839E339E3`)后完成 B1/B4/B5;
  全程遥控上挡零力手推,板子保持运行(`firmwareConsistency: match`)。
- **实测结果**(全部落盘 `.Doc/data/`):
  | 项 | 读数 | 真值 | 误差 | 四轮离散 |
  |---|---|---|---|---|
  | B1 直线 2m | 2000.2mm | 2000 | +0.01% | 6.4% |
  | 直线 2m 第二次(原计划反向) | 2065.2mm | 2000 | +3.26% | 9.1% |
  | B4 横移 2m(向车头左侧) | 2054.2mm | 2000 | +2.71% | 6.4% |
  | B5 原地转一圈(俯视逆时针) | +347.3° | 360 | −3.5% | **0.2%** |
- **结论 1(解算链正确)**:正解/逆解的比例(`RADIUS_WHEEL 75` → `WHEEL_RATIO 14.5149` → `×0.70710678`)、
  vx/vy 行的符号与镜像约定、wz 行的四轮同号组合全部与实车一致;**+vy = 车头左侧、+wz = 俯视逆时针**(RM 标准)。
- **结论 2(方法精度边界,重要)**:同一条线两次「推 2 米」读出 2000.2 与 2065.2,**相差 3.25%** ——
  手推法的重复性就是 ±3%,只能判「比例与符号对不对」(≈5% 分辨率),**不能**标定亚百分点误差,
  也**不能**用来分「打滑 vs 比例错」。据此推翻 DEV-01 遗留的 B2 设计前提,并撤回"B4 偏高 2.7% = 辊子打滑"的推断。
- **结论 3(B3 降级)**:`EstimateSpeed()` 是四轮转速的齐次线性组合,四轮同时取反则 `est_vx` 必然取反,
  手推验证反向符号在数学上多余(实测也证实:所谓反向推车读出的轮子符号与前进一位不差,车是车头朝前动的)。
  B3 改为「只在能发速度指令时做」。
- **两个致命坑(已修)**:
  ① **核心暂停时 CAN 中断不跑,`total_angle` 一个数都不累加**——read_chassis 的 `continue` 曾静默失败,目标一直 paused,
     车推了 2 米读数一位没变。两个工具现在都强制校验:恢复后 `chassis_last_ms` 两次读数必须增长,否则 `ok:false` 不出数;
     且 `resumeAndVerify()` 循环重试 continue(本工程 `variables.read`/`memory.read` 是侵入式读取,偶发残留暂停);
  ② **遗留 `openocd.exe` 占着探针**,报错却长得像板子没电(`Error connecting DP: cannot read IDR` /
     `CMSIS-DAP failed to connect in mode (1)`)。实为 11-14 16:43 起的僵尸进程(PID 49952),杀掉即恢复。
     已写入手册 §9 排查表。
- **改动**:`tools/odom.js` 扩为三轴(`--axis vx|vy|wz`,wz 输出 `yawDeg`,力臂 `CENTER=√(250²+230²)=339.7mm` 运行时解析
  `WHEEL_BASE`/`TRACK_WIDTH`);`tools/read_chassis.js` 重写恢复校验;`.Doc/底盘测试手册.md` 改 B3/B4/B5、§9/§10/§11;
  `.Doc/本车调参结论.md` 新增 §3.1 整节(四行实测表 + 精度边界 + 两个坑)。
- **未验证(诚实边界)**:指令→实际速度(逆解 + 速度环,必须驱动才能测)、负载/加速度打滑、云台装机后的机械状态。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

### DEV-20261115-03｜2026-11-15｜修通 MCP `mcp__emberprobe__*` 通道(BRIDGE_UNAVAILABLE 根因)

- **现象**:整个会话里所有 `mcp__emberprobe__*` 工具返回 `BRIDGE_UNAVAILABLE`(`details.searchedFrom` 为
  `C:\Users\ZhuanZ\.dsh\profiles\desktop`,`registryEntries:1`),而同一台机器上 `.agents/skills/` 的脚本通道一直正常。
- **根因**:MCP server 进程的 cwd 是 dsh profile 目录,扩展的 workspace 推导(`dist/mcp-server.js` 里
  `function wI(e,t=process.env)` → `t.EMBERPROBE_WORKSPACE`)在没有显式参数时退回 cwd,于是去 profile 目录下搜
  agent-bridge descriptor,搜不到 → 判定 bridge 不可用。脚本通道由 `.agents/skills/_emberprobe/bridge-locator.js`
  按 registry 匹配,所以不受影响。
- **改动**:`C:\Users\ZhuanZ\.dsh\profiles\desktop\cordis.patch.yml` 的 `mcp-emberprobe` 条目 `env` 增加
  `EMBERPROBE_WORKSPACE: "C:\\RoboMaster\\RM-JIT-CR-Infantry"`(备份 `cordis.patch.yml.bak-20261115`);
  YAML 用块式 env 书写避免 flow 换行歧义。
- **生效方式**:`plugin_manager set_plugin include:mcp-emberprobe enabled=false` → `true` 重载插件,**无需重启 DSH**。
- **验证**:`mcp__emberprobe__probe_list` → `{available:true,devices:[]}`(探针当前未插,属正常);
  `config_get` 返回完整工程配置(elf/SVD/openocd/上次探针 UID `002F003F3034510C30343537`);
  `debug_status` → `state:"none"`。三条都通了,不再是 BRIDGE_UNAVAILABLE。
- **副作用/边界**:该路径硬编码本工程,换工程要同步改;本次只改用户 profile,不涉及工程代码,审计计数不变。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

### DEV-20261115-04｜2026-11-15｜新增 `tools/read_yaw.js`:云台 yaw 零位标定工具(待云台装好后使用)

- **背景**:拨遥控中挡时 yaw 电机持续自转。根因链已读实:`CalcOffsetAngle()`(`robot_cmd.c:283-292`)
  用 `offset = yaw_motor_single_round_angle − YAW_CHASSIS_ALIGN_ECD × 0.043945` 算偏差,该 offset 同时驱动
  解锁回正(`robot_cmd.c:492-514`,90°/s、600×5ms=3s 超时、上限 270°)与中挡底盘 FOLLOW(`chassis.c:258-288`)。
  云台未装时 yaw 单圈角≈0,而宏值 `YAW_CHASSIS_ALIGN_ECD 3564`(`config/robot_infantry_2026.h:22`)=156.62°,
  于是 offset≈−156.6°,回正 3s 追不上 → 一直转。**重标一个宏即可同时修好回正与跟随。**
- **新增**:`tools/read_yaw.js` —— 读 `yaw_motor` 全局指针(`gimbal.c:224`)→ `memory.read` 464 字节 →
  按 `DJIMotorInstance` 偏移解码(ecd +2 / angle_single_round +4 / total_angle +16 / speed +8 / dt +456,
  与 `tools/read_chassis.js:33` 同一套);输出当前宏值算出的 offset 残差、建议宏值(对齐时的 ecd)、
  以及"写进去之后 offset 会变成多少"。带在线判据(dt<5ms,否则 ecd 冻结 → `ok:false`)与恢复后
  `chassis_last_ms` 新鲜度校验(同 read_chassis.js 的坑)。`node --check` 通过;无会话时优雅报错并给出
  该跑的命令(实跑验证)。
- **文档**:`.Doc/底盘测试手册.md` 新增 §11.3(完整标定步骤 + 输出判读表 + "方向反了改
  `GIMBAL_RECENTER_DIR` 不动 ecd" 的边界),§11.2 把"重标 ecd"列为装好云台后的第 0 步。
- **状态**:待云台装好、探针插上后执行;标定出的新 ecd 属于参数级改动,可直接写入 config,烧录由用户执行。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

### DEV-20261115-05｜2026-11-15｜新增 CAN1 电机总闸(总线级失能/使能)+ 修掉测试里写死的旧标定值

- **目标与触发**:用户要求"加个 CAN1 电机全部失能和使能的控制宏"。台架需求:推车、架空、手转云台时
  不想让四轮/yaw 出力,也不想每次都靠遥控上挡(kill switch)才能做到。
- **功能变化**:
  1. `modules/motor/DJImotor/dji_motor.h` 新增总闸 API 与宏:`g_dji_can1_enable`(非 static,调试器/MCP 可直接写)、
     `DJIMotorSetCAN1Enable(0/1)`、`DJIMotorGetCAN1Enable()`、`DJI_MOTOR_CAN1_DISABLE_ALL()` / `DJI_MOTOR_CAN1_ENABLE_ALL()`;
     编译期初值由 `DJI_MOTOR_CAN1_ENABLE_BOOT`(默认 1)决定。
  2. `modules/motor/DJImotor/dji_motor.c` 的 `DJIMotorControl()` 里逐帧判断
     `bus_disabled = (!g_dji_can1_enable && motor->motor_can_instance->can_handle == &hcan1)`:
     命中则把 `set` 压成 0,**并复用原有停机分支**(槽位清零 + 三环 `PIDClear`),因此使能瞬间不会带出停机前的残余力矩。
  3. **防疯车硬锁 `DJI_MOTOR_CAN1_FORCE_DISABLE`(用户 2026-11-15 追加需求"让电机始终失能")**:=1 时
     - `DJIMotorControl()` 的 `bus_disabled` **不看** `g_dji_can1_enable`,CAN1 电机永远零输出;
     - `DJIMotorSetCAN1Enable(1)` 被拒绝并 `LOGWARNING` 一次("enable request refused"),调试器直接写
       `g_dji_can1_enable=1` 也无效(控制环只认宏,不认该变量);
     - 构建时打 `#pragma message` 提醒当前是安全版,免得忘了自己烧的是什么。
     **当前仓库内该宏=1(安全版)**;跑 S1/S2/E1 等需要电机出力的测试前改回 0 重烧。
- **关键实现约束(代码事实)**:
  - 总闸是**最后一级**强制零输出,与上层指令无关 → 不会被底盘/云台任务"顶回去";`DJIMotorStop()` 做不到这点,
    因为它是单电机标志且会被同周期内的重新下指令覆盖。
  - **不采用"停止发控制帧"**的失能方式:C620/C610 丢帧约 1s 后自行停机并进入保护,想恢复得断电重来;
    持续发 0 帧才可随时恢复。
  - 反馈侧完全不受影响:电调照发反馈帧,编码器/温度/在线判据(dt<5ms)照常,**所以总闸关闭时 `odom`/`read_chassis` 仍可读数**。
  - 只作用于 CAN1(四轮 + yaw);CAN2 的 pitch/摩擦轮/拨盘不受影响。
- **顺带修复(测试假失败)**:`tests/chassis_remote_test.c:137` 写死了 `3564 * 0.043945f` 作为云台反馈桩。
  本次标定把 `YAW_CHASSIS_ALIGN_ECD` 改成 5892 后,桩角度与宏差 +104° → 解锁回正去追假目标 →
  `gimbal_cmd.yaw != 0`,第 4 步断言失败。改为 `YAW_CHASSIS_ALIGN_ECD * ECD_ANGLE_COEF_DJI` 现算(并 include `dji_motor.h`),
  以后标定值怎么改测试都自洽。
- **文件与符号**:modules/motor/DJImotor/dji_motor.h(总闸 API/宏/说明)、modules/motor/DJImotor/dji_motor.c
  (`g_dji_can1_enable`、两个函数、`DJIMotorControl` 内的 `bus_disabled` 分支)、tests/chassis_remote_test.c(桩值改用宏)。
- **验证**:ARM 全量编译 `ninja -C build` EXIT=0,0 error 0 warning(FLASH 12.14% / RAM 81.88%,基线内);
  主机测试 `tests\run_chassis_tests.ps1` **ALL PASS (0 failures)**(修复前是该断言失败)。
- **未验证项**:总闸开关本身未上实车(需烧录后拨 `g_dji_can1_enable` 实测一轮:0 → 四轮手推无阻力、反馈仍在线;
  1 → 恢复出力)。**烧录由用户执行**,本次只保证编译与主机测试。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

### DEV-20261115-06｜2026-11-15｜云台 pitch 低头抬不起来的实测诊断 + 限流判据实验(MaxOut 4000→8000)

- **触发**:车头加装自瞄模块后,"pitch 一旦低头过低就抬不起来"(m01170);用户判断"是力矩不够"(m01180),
  并要求直接放宽限流(m01404)。按"先测后调"的顺序,先做实测诊断再动参数。
- **新增工具(主机侧,零固件改动)**:
  1. `tools/read_motor.js` —— 通用电机读数(`node tools/read_motor.js pitch_motor yaw_motor [--json]`),按
     `DJIMotorInstance` 偏移(ecd +2 / angle +4 / speed +8 / current +12 / temp +14 / totalAngle +16 / feedCnt +452 / dt +456)
     解 464 字节,带 `dt<5ms` 在线判据与 `resumeAndVerify()`(5 次循环校验核心确实恢复运行)。
  2. `tools/pitch_calib.js` —— 重力前馈标定与"卡住性质"判定工具,子命令 `add`(静态采点)、`jam`(连续采样判性质)、
     `show`/`fit`(三模型拟合:常数 / 余弦 / 带相位余弦,输出 `PITCH_GRAVITY_A/PHASE/OFFSET` 建议值与残差 RMS)、`clear`。
     采样表达式走 `debug.evaluate`:`ang=gimba_IMU_data->Pitch`、`ref=gimbal_cmd_recv.pitch`、
     `out/iout=pitch_motor->motor_controller.speed_PID.Output/.Iout`、`gyro=gimba_IMU_data->Gyro[0]`;
     数据落 `build/pitch_calib.csv` / `build/pitch_jam.csv`(12 列)。
- **实测记录(2026-11-15 20:59–21:13,探针 XR-DAP + 固件 ELF `206030d9…`)**
  1. 静止保持态:`θ=8.10° / 目标 12.52° / 误差 4.42°`,`Output=2978`、`Iout=1200(顶 IntegralLimit)`、
     实际电流 2985 码、温度 25℃ —— 积分顶满仍追不上目标。
  2. 零力手扫(上挡):θ 从 −27.57° 到 +10.35°,后一轮同样手推可达 **+42.69°**,两次差 32° →
     **+10.35° 不是机械键**,是"电机顶不动、人也没使劲"的位置(首轮据此提出的 `PITCH_MAX_ANGLE 25→8.5` 建议已撤回)。
  3. **底部抬升(判据核心)**:θ=−26.45° 恒定,连续 12 s `Output=4000(顶 MaxOut)`、`Iout=1`、实际电流 4018 码,
     **θ 一个数字没变** —— 满电流彻底顶不动,不是"慢慢爬"。
  4. 离开底部的角度全部正常:+16.79° 时 Output 3364 正常爬升、+19.26° 时 Output 2811 稳住、+27.49° 时 Output −932 往回压
     —— 因此故障局限在低角度段。
  5. 速度反馈符号校验:θ 增大 ↔ `Gyro[0]` 为正(+0.339),θ 减小 ↔ 为负(−0.279),**同向 → 速度环反馈未接反**。
- **改动(参数级)**:`application/gimbal/gimbal.c:206` pitch `speed_PID.MaxOut` **4000 → 8000**(满量程 16384≈±20A,
  即 ±4.9A → ±9.8A,逼近 M3508 连续额定 10A),注释写明这是判据实验、以及两条回退路径。
  `IntegralLimit` 保持 1200 不动 —— 一次只改一个变量,便于归因。
- **验证**:ARM 全量编译 `ninja -C build` EXIT=0、0 error 0 warning(FLASH 12.14% / RAM 81.88%,基线内);
  主机测试 `tests\run_chassis_tests.ps1` **ALL PASS (0 failures)**;新 ELF SHA256 `B1159382F83BA1647E41F27D6D56F4F9E2565A2CC7091823F2AE9A6AACFF37AC`;
  探针已 `debug stop` 释放,无残留 openocd 进程。
- **未验证项 / 下一步**:8000 档实车行为待用户烧录实测(判读:能动→缺力属实,前馈接管后回退峰值;
  仍不动→机械楔死/供给限流,回退并查机械;C620 自带限流则查电调配置与母线电压)。
  `PITCH_GRAVITY_*` 前馈仍未标定(`GAIN=0`),须在确认"能动"后按 `pitch_calib.js add/fit` 扫点标定。
- **AI**:DeepSeek-V4.1-Flash 2026-11-15。

## DEV-20261116-05｜2026-11-16｜读数链路两级加固:固件身份预检 + doctor 一条命令体检

- **触发**:用户要求"读一下 IMU 状态,感觉 pitch 的解锁回中效果不好"(m01480/m01529)。我花 **15 分 05 秒**
  没读到数,用户问"都在干什么、时间占比"(m02257);复盘后提出两个工具化改进,用户回"可以"(m02262)。
- **根因(实测证据)**:板上固件 ≠ `build/basic_framework.elf`。
  - 片上 flash @0x08000000 = `00000220 c5360108`(SP `0x20020000` / Reset `0x080136C5`);
  - ELF 同址 = `00000220 e13d0108`(Reset `0x08013DE1`,与 `e_entry` 一致);**全文件搜不到 `c5360108`**;
  - 而烧录记录与 `firmwareConsistency` 都报 `match` / `flashedAt 2026-11-15T13:15:48Z` → **"烧录成功"是假象**。
  - 同期垃圾读数:`motor_lf=0x0`、`gimba_IMU_data=0x3`、`chassis_last_ms` 恒 `1.1210387714598537e-44`、
    `gimbal_cmd_recv.pitch=1.0929329e-19` —— 全是错地址上的内存,不是"故障数据"。
- **改动(主机侧,固件零改动)**:
  1. 新增 `tools/lib/preflight.js`:`readElfVector()`(解析 program header 找覆盖 `0x08000000` 的 LOAD 段,
     不假设向量表在文件头)/ `readChipVector()`(暂停窗口内 `memory.read` 8 字节)/ `checkFirmware()`
     (按 workspace + ELF mtime/size 做进程内缓存,ELF 重编译会自动重校验)/ `assertFirmware()`
     (不一致抛 `FIRMWARE_MISMATCH` 并打印两侧向量表)/ `checkGlobals()` / `withTargetPaused()`。
  2. 新增 `tools/doctor.js`:八段体检 —— ①工程文件(sha256) ②桥接(描述文件/端口 TCP 探活/工作区指针)
     ③进程(VS Code 窗口、残留 openocd) ④调试会话(无则 attach,结束时还原并释放探针)
     ⑤**固件身份(向量表比对)** ⑥供电时钟(`RCC.CR` HSE/PLL、`RCC.CSR` 复位来源) ⑦关键指针 +
     `chassis_last_ms` 是否增长 ⑧收尾。支持 `--json` / `--no-attach` / `--keep`,退出码 0/1。
  3. 五个读数工具统一接入预检(读变量**之前**抛错,拒绝出数):`read_chassis.js`、`read_motor.js`、
     `read_yaw.js`、`odom.js`、`pitch_calib.js`(后者两个求值入口都接了)。
  4. `tools/pitch_calib.js` 新增 `ENUM_NUM` 映射:修掉 `state`/`auth` 两列显示 `null` ——
     `debug.evaluate` 对 enum 类型回的是字面量(`ROBOT_STOP` / `AUTH_RC_LOST`),原数字解析全部落空。
- **验证**:`node --check` 七个文件全过;`node tools/doctor.js` 实跑结论"可用,但有 2 项提醒":
  - 固件身份 ✓ ELF `00000220e13d0108` == 片上 `00000220e13d0108`(用户已于本地 11:28 重烧正确 ELF,
    `flashedAt 2026-11-16T03:28:14.191Z`),`elfSha256 b1159382…`;
  - 关键指针 ✓ `motor_lf=0x20011858` / `gimba_IMU_data=0x2000f36c` / `yaw_motor=0x20010d50` / `pitch_motor=0x20010f78`;
  - 活性 ✓ `chassis_last_ms 245137.21875 → 245652.28125`;
  - 供电 ✓ `RCC.CR=0x03038183`(HSE 起振、PLL 锁定)、`RCC.CSR=0x1E000000`(POR+PIN+BOR+WWDG,**无 IWDG**);
  - 提醒项:窗口探测为空(桥接已连,不影响)、"烧录记录 match 不代表片上内容"。
- **顺带得到的 IMU 实读(11:33–11:34,`node tools/pitch_calib.js state --times 2`)**:
  `pitch −27.60° / roll −1.54° / yaw −5.05°`,目标 pitch `0.00°`、误差 `27.60°`,
  **`Output=0` / `Iout=0` / 实际电流 ±30 码**,状态机 **`AUTH_RC_LOST(遥控丢失) / ROBOT_STOP`**。
  → "pitch 解锁回中效果不好"的真相是**根本没解锁**:遥控未连接 → `ROBOT_STOP` → `ApplySafeOutputs()` 把云台
  置零力,枪管被重力自然垂到 −27.6°,目标 0° 一直在但没有任何力矩去执行。不是缺力,也不是回中算法问题。
- **未验证 / 下一步**:遥控开机对频后重读 `state`,应显示 `RUN(已解锁)`;若 `RUN` 下 `Output≠0` 却仍回不到 0°,
  才回到"缺力 / 死区 / IMU 零位不在物理水平"这三条分支继续查。pitch 重力前馈仍 `GAIN=0` 未标定。
- **AI**:DeepSeek-V4.1-Flash 2026-11-16。

## DEV-20261116-02｜2026-11-16｜pitch 稳态力矩不足:IntegralLimit 1200→3000(判据实验)

- **触发**:用户报"pitch 轴解锁后自己上升、下降"(m02386),要求放宽限幅(m02396"提吧")。
- **实测证据**(`node tools/pitch_calib.js state --times 14 --interval 900`,11:40:10–11:41:00,固件 `b1159382…`):
  | 时刻 | pitch | 目标 | 误差 | Output | Iout | 电流码 | 状态 |
  |---|---|---|---|---|---|---|---|
  | 11:40:17 | −26.99 | 0.00 | 26.99 | 0 | 0 | 36 | STOP_LATCHED/STOP |
  | 11:40:21 | −9.46 | 0.00 | 9.46 | 4292 | 567 | 4299 | RUN/READY(解锁) |
  | 11:40:24 | −7.57 | 0.00 | 7.57 | 4221 | **1200** | 4218 | RUN |
  | 11:41:00 | −6.80 | 0.00 | 6.80 | 3923 | **1200** | 3941 | RUN |
  - 解锁后 4 s 内从 −27° 冲到 −9.5°,之后 **40 s 只爬 0.8°**,卡在 −6.8°;`Iout` 全程顶死 `IntegralLimit=1200`;
  - 目标恒 `0.00` → 排除"摇杆未回中/目标漂移";温度 22→26℃ 缓慢上升(4A 堵转)。
- **判读**:"上升+下降"不是失控,是**够不着 0° 的极限环** —— 枪管越接近水平重力矩越大,静止保持靠积分项,
  积分被 1200 锁死 → 停在"力矩≈重力"的平衡角;静摩擦一蹭一停 + `MaxOut=8000` 挣脱那一下很猛,视觉上就是上下动。
  与 `gimbal.c` 旧注释记载的 DEV-09 现象(600 时"特定角度极限环+回水平不到位")同款,即加装自瞄模块后 1200 再次不够。
- **附带观察**:`yaw` 恒 −109.7° 不动 —— yaw 挂 CAN1,而当前固件 `DJI_MOTOR_CAN1_FORCE_DISABLE=1`,
  解锁后 yaw 回正流程在跑但电机零输出,属预期(要看 yaw 回正须把该宏改 0 重烧)。
- **改动(参数级,一次只改一个)**:`application/gimbal/gimbal.c:204` pitch `speed_PID.IntegralLimit` **1200 → 3000**
  (仍 < `MaxOut 8000`),注释写明历史、本次证据、两条回退路径。`tools/pitch_calib.js` 的 `INT_LIMIT_REF` 同步 3000
  (该常量参与 jam 判据,不同步会误判"没顶积分")。
- **验证**:`ninja -C build` EXIT=0、0 error 0 warning(FLASH 12.14% / RAM 81.88%,仅 libc `_read/_write` 存根警告,基线内);
  `tests\run_chassis_tests.ps1` **ALL PASS (0 failures)**;新 ELF SHA256
  `d3ff15f072553d304091687dbf8bae4b5f31b629a00a9f32ef670becd1578b46`(mtime 2026-11-16 11:43:24)。
- **未验证 / 下一步**:待用户烧录后解锁实测 —— ①能爬到 0°±2° 并稳住 → 稳态缺力确诊,接着标定重力前馈
  (`GAIN 0.5` 起符号检查),前馈接管后把 IntegralLimit 回退到 1200~2000;②3000 仍到不了 0° → 低角度段机械干涉,
  回 `jam` 判据查机械并回退参数。
- **AI**:DeepSeek-V4.1-Flash 2026-11-16。

## DEV-20261116-03｜2026-11-16｜pitch 重力前馈标定闭环(非侵入采样 + 三参数拟合 + 全量前馈验收)

**背景**:装自瞄模块后 pitch 在低角度抬不动。排查中先踩到一个致命观测坑 —— 用 `memory.read`/`debug.evaluate`
读变量必须暂停核心,GimbalTask 停发 CAN 帧,C620 约 100ms 无帧即按 0 输出,枪管被重力砸下来;恢复后又抬起。
用户观察到的"pitch 一直在自己动"、以及 11:40 那张"积分顶死 + 极限环"的表,**全部是读数伪影**,
据此做的 `IntegralLimit 1200→3000` 判据实验作废(参数保留,理由见 gimbal.c 注释)。

**采集通道(本次的根本改进)**:改用 EmberProbe 的 `variables.sample` 非侵入采样(DWARF 指针路径,
不暂停核心),一次 RPC 拿整条曲线,实测 24 Hz;单请求过长会被打断,故分块 100 点(≈4s)、每块重试 3 次、
块间失败不中止,且**拿到任何数据都落盘**(只丢整个空请求)。

**数据**(带前馈 0.5 的固件上采,500 点/38.5s/12 个角度箱,`|角度误差|≤1°` 的准静态点):

| 角度 | −22° | −10° | −4° | +8° | +20° | +26° |
|---|---|---|---|---|---|---|
| 实际总电流码 | 3901 | 3688 | 3618 | 2978 | 2521 | 2662 |
| 闭环 Output | 1864 | 1839 | 1860 | 1386 | 1069 | 1341 |
| 积分 Iout | 1306 | 1843 | 1600 | 1397 | 1493 | 1667 |

**方向/结构结论**:抬枪管时 Pitch 由 −22° 升到 +26° → **Pitch 正 = 抬头**。电流需求随角度**单调下降**
(越往下越费),与"重力在前方"的直觉相反 —— 自瞄模块装在**枪管上方**,质心既在轴前又在轴上方,
越往上抬质心越靠近轴线,所需力矩越小;拟合相位 φ≈−92° 是结构决定的,不是异常。

**拟合方法(两个坑)**:
1. **必须用 `measure.real_current` 而不是速度环 Output**:前馈开着时 Output 只剩残差,拿它拟合会把已补上的量再算一遍。
2. 同一角度箱若有多趟(上推/下推),摩擦偏差近似等大反号,先按 2° 分箱平均再拟合,否则会把 ±摩擦拟合成假重力/假相位。
3. 修掉工具里一个真实 bug:`ls2(rows,f,()=>0)` 传入零基函数导致矩阵奇异、余弦模型恒为 null,故此前只输出常数模型;
   新增两参数 `ls1()`,并把相位网格从 ±40° 扩到 ±180°。

**改动**:
- `config/robot_infantry_2026.h`:`PITCH_GRAVITY_FF_GAIN 1.0f`、`PITCH_GRAVITY_A 1927.0f`、`PITCH_GRAVITY_PHASE -92.0f`、`PITCH_GRAVITY_OFFSET 3371.0f`(拟合残差 RMS 119 码≈0.15A,行程 −21.4°~+25.8°)。
- `tools/pitch_calib.js`:新增非侵入 `readBatch()`、`sweep`(连续扫掠分箱入库)、`rebuild`(从保存的原始波形换门限重算,免重推)、`state`;`fit` 支持 `--use out`、分箱平均、`ls1`。

**验收(全量前馈烧录后,625 点/48.3s/14 箱)**:

| 角度 | −20° | −10° | 0° | +8° | +16° | +26° |
|---|---|---|---|---|---|---|
| 闭环 Output | −420 | −119 | ~+90 | −64 | −139 | +319 |
| 积分 Iout | −194 | −117 | ~+50 | 65 | 203 | 321 |
| 实际总电流 | 3491 | 3546 | ~3450 | 3003 | 2600 | 2832 |

- **闭环 Output 从 ±1900~4200 降到 ±420 以内(均值≈0)**,**积分从长期顶死 3000 降到 ±320**,前馈接管了稳态力矩;
- 总电流仍在 2600~3965 区间 → 补的是力矩,不是叠加;温度 26~27℃(此前同工况 31~33℃);
- 编译 `ninja -C build` 0 error 0 warning(FLASH 12.15% / RAM 81.88%);`tests\run_chassis_tests.ps1` ALL PASS;ELF `4081DB11CEB9B169C6A17699D6FF3141F94B38BC5DFBBFF4195C75BAB969E3E7`。

**未验证 / 下一步**:①手感验收(任意角度静止不下滑、上下跟手、外力扰动回正);②前馈只按角度查表、不辨运动方向,
静摩擦仍需 PID 处理,若某角度仍有"蹭一下"需评估速度环 `Output_LPF_RC` 与 `MaxOut`;③`IntegralLimit` 现为 3000,
前馈接管后可回退到 1200~2000(一次只改一个变量);④行程外(>|26°|)曲线未验证,`PITCH_MIN/MAX_ANGLE` 已夹在 −20/+25。

**AI**:DeepSeek-V4.1-Flash 2026-11-16。


## DEV-20261116-04｜2026-11-16｜pitch 前馈配套收敛(IL 1500 / MaxOut 5000)+ 台架手感验收

**改动(一次一个变量,两个都基于前馈已接管稳态)**:
- `application/gimbal/gimbal.c:204` pitch `speed_PID.IntegralLimit` **3000 → 1500**:前馈 GAIN 1.0 后实测积分只有 ±320,大余量只会在大角度阶跃时积满并吞吐造成超调。
- `application/gimbal/gimbal.c:212` pitch `speed_PID.MaxOut` **8000 → 5000**(16384≈±20A,5000≈±6.1A):8000 是当初"靠积分硬顶仍不够"时的判据实验值,前馈接管后它只决定瞬态/挣脱静摩擦那一瞬的峰值,过冲与"一顿一顿"由此而来。行程内前馈最大 ≈3970 码(−20° 处),合计仍在满量程 40% 内。

**验证**:`ninja -C build` EXIT=0、0 error 0 warning(FLASH 12.14% / RAM 81.88%);`tests\run_chassis_tests.ps1` **ALL PASS (0 failures)**;ELF `768BFBAAC8C1AAA6F6F7BAE9092138317962A712A6BEE96FCCCF2399C5D7B02E`。
**台架验收(用户实测)**:①最低位起步顺畅 ②大角度快速推拉跟得上、无冲过头回荡 ③−15°/0°/+15° 停 5s 无缓慢下滑 —— **三项均无问题**。

**结论沉淀**:已写入 `.Doc/本车调参结论.md` §2.1(方法、两个必守铁律:拟合用 `measure.real_current` 而非 Output、禁止用暂停核心的方式做动态观测;结构结论:模块在轴上方 → 电流需求随角度单调下降,φ≈−92° 属正常)。
**回退路径**:起停变肉 → MaxOut 6000/4500;某角度缓慢下滑 → IntegralLimit 2000;整体变差 → `PITCH_GRAVITY_FF_GAIN 0`。

**AI**:DeepSeek-V4.1-Flash 2026-11-16。
## DEV-20261116-01｜2026-11-16｜USART1 接入 AX650 的 0x1021 姿态上报

- **请求与范围**：用户明确将 STM32 修改目标改为 `D:/BaiduNetdiskDownload/jit`，要求 USART1 发送符合 AX650 的消息。本次完成真实姿态上报；未烧录、未控制实车、未接入下行指令执行。
- **协议**：921600 baud、8E1、无流控，每 10 ms 尝试发送一帧。帧为 `A5 | length=17(u16LE) | seq(u8) | CRC8 | cmd=0x1021(u16LE) | payload[17] | CRC16LE`，整帧 26 字节。数据为四个 IEEE754 float32（yaw/pitch/roll 单位 deg、弹速 m/s）及一个模式字节，全部小端；CRC8 反射多项式 0x8C/初值 FF，CRC16 反射多项式 0x8408/初值 FFFF，均无最终异或。
- **姿态来源**：`INS_Task()` 在 EKF 更新并复制 `INS.q` 后调用独立模块，由机体到导航四元数 `[w,x,y,z]` 转为严格 `Rz(yaw)Rx(pitch)Ry(roll)`。不照搬原有另一种顺序的欧拉角字段；非法、非有限、零四元数及 ZXY 奇异附近的输入直接跳过，不伪造零姿态。
- **串口与调度**：沿用 PA9 TX/PB7 RX，在模块初始化时经 BSP 将 USART1 重新配置为 8E1（HAL 9B+EVEN），不修改 `.ioc` 或 CubeMX 生成区。发送使用静态缓冲区和 IT；先检查忙状态再封包，避免覆盖未发送完的数据；HAL 成功启动后才递增序号，忙时丢弃该次机会，不排队旧姿态；初始化失败按周期重试，不阻塞 INS。修复 BSP 原 `USARTIsReady()` 的按位或恒忙判断。
- **配置**：`AX650_UART1_ENABLE=1`、`AX650_POSE_PERIOD_MS=10`、`AX650_TX_SPEED_MPS=0`、`AX650_TX_MODE=0`，集中在 `config/robot_infantry_2026.h`。速度与模式目前是手眼标定占位值；模式 0 为 IDLE，正式自瞄的枚举为 1，真实弹速尚未接入。旧 Seasky/VCP 编译选项保留，未启用旧发送调用。
- **文件**：新增 `modules/master_machine/ax650_pose.c/.h`（纯封包）和 `ax650_uart1.c/.h`（发送模块）；修改 `application/robot_task.c`、`modules/imu/ins_task.c`、`bsp/usart/bsp_usart.c/.h`、本车配置与 Makefile。新增独立主机测试及 HAL 桩；CMake 通过既有 GLOB 自动收集新文件，Makefile 同步登记。
- **主机验证 PASS**：`tests/run_ax650_tests.ps1` 在 `-O2` 与 `-Ofast` 下验证固定 26 字节零姿态帧、225 组三轴组合、四元数归一化与正负号、字段/CRC/序号、非法数与容量拒绝；真实发送模块与真实 BSP 仅替代 HAL，验证 8E1、初始化重试、100 Hz、忙缓冲区保护、HAL 发送失败、最新姿态及计时/序号回绕。既有 `tests/run_chassis_tests.ps1` 控制权仲裁和看门狗回归全部通过。
- **ARM 编译 PASS**：`cmake -G Ninja -B build`、`ninja -C build -j8`，GNU ARM 14.3.1，第一方编译 0 error/0 warning。既有 CMake CMP0175 开发者警告与 newlib `_close/_lseek/_read/_write` 链接警告保留，不属于本次第一方代码诊断。FLASH 127884 B（12.20%），RAM 107360 B（81.91%），CCMRAM 0 B。
- **BIN SHA256**：`de5ec318db75640de38117e9b04ddfe86e8d869f857973ad4eb89a907cf1df47`，产物 `build/basic_framework.elf/.bin/.hex`。
- **审计边界**：AGENTS 指定的 `C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/scripts/audit_baseline.py` 在本机缺失，已实际尝试并得到文件不存在，不能声称固定历史基线审计通过。改前 190 文件快照与 hashes 位于 `C:\Users\Avear\AppData\Local\Temp\jit-uart1-before-rb0klca9`。相对该快照为 7 个修改、11 个新增、0 个删除；原云台零位与限位之后的全部本车参数相同，`Src/Inc`、`.ioc` 未变，未编辑第三方目录。详细清单见 `build/uart1-verification.json`。
- **实机待验证**：用户烧录后核对 AX650 收包 CRC、实际频率和移动云台时的角度；编译/主机测试不代表实体串口已经验证。
- **AI**：Codex（GPT-6）。

### DEV-20261116-02｜2026-11-16｜核验 UART 提交的 UTF-8 编码并固定编辑器设置

- **请求与范围**：用户要求将已提交代码改为 UTF-8 后再次推送。逐一读取提交 `7ca9ea0` 的 18 个文件，严格 UTF-8 解码均成功，均无 BOM、无 U+FFFD 替换字符且为 LF 行尾；中文内容人工抽查正常。按 UTF-8 无 BOM 重新编码后，字节与原提交一致，无需转换或修改串口实现。
- **改动**：新增 `.editorconfig`，指定 UTF-8 无 BOM 和 LF；在 `.vscode/settings.json` 显式设置 `files.encoding=utf8`、`files.autoGuessEncoding=false`。未改变串口协议、控制参数或运行逻辑。
- **验证**：提交前严格检查上述 18 个文件及本次 3 个改动文件的 UTF-8、BOM 和行尾，并校验 VS Code 设置的 JSON 语法；核对 UART 提交的全部代码仍与 `7ca9ea0` 字节一致。仅编码设置和记录变化，无固件逻辑变化，未重复编译或烧录。
- **审计边界**：本次相对 `7ca9ea0` 为 2 个修改、1 个新增、0 个删除。AGENTS 指定的历史基线审计脚本在本机缺失，已再次实际调用确认文件不存在，未声称该审计通过。扩大只读检查发现旧文件 `bsp/flash/bsp_flash.c` 不是严格 UTF-8，另有两个旧头文件含替换字符；这些文件不属于本次 UART 提交，未进行有损转码或猜测修复。
- **AI**：Codex（GPT-6）。

## DEV-20261116-06｜2026-11-16｜审计基线切到仓库内 skill(460 条)+ 上游配图本地化

**现状**:此前基线审计依赖用户级目录 `C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/`,协作者机器上不存在该路径;其基线段是 199 文件的旧清单,与上游 basic_framework 不同源。

**变化**:
- 用户新增 `.agents/skills/rm-electrical-dev`(上游 basic_framework @1a136eb1 快照 + 审计脚本 + 15 篇 references),随仓库分发,协作者开箱可用。
- 为控体积,快照内 `.assets/` 的 61 张配图(约 41.1 MB)已剥离;按磁盘实况手工裁剪 manifest `521 → 460` 条,`file_count` 同步,并写入 `snapshot_strip_note` 记录裁剪原因与覆盖率;原 manifest 备份为 `yuelu-basic-framework.manifest.json.bak-20261116`,裁剪脚本 `build/prune_manifest.py`。
- 配图从 `C:\Users\ZhuanZ\Desktop\JIT_yaw_bench\.assets` 复制回工程根 `.assets/`(61 文件/41.1 MB,逐字节校验 0 不一致),`.gitignore` 予以忽略,只在本机保留;`.Doc/` 内 8 篇上游文档的 46 处图片引用现已全部可解析。
- `AGENTS.md`:审计命令、代码地图、基线路径全部改为仓库内路径;并登记 RedFlags 中的 `modules/imu/ins_task.c` `while(1)`(:106-113,超 50 次重试后 LOGERROR 挂起)为**已裁决的启动期 fail-fast**,不再逐次解释。

**复核计数**(`python .agents/skills/rm-electrical-dev/scripts/audit_baseline.py --project .`):`Unchanged 313/460、Changed 99、Missing 48、Added 32、RedFlags 1`,exit 0。Missing 48 = 19 个 `application|bsp` 文档 + 23 个 `modules` 文档 + 3 个 PDF + 3 个 PNG,均为从未搬入的上游模块说明,不影响源码审计结论。

**未验证**:PowerShell 孪生脚本 `audit_baseline.ps1` 本次未跑;两版脚本文件哈希不同(仓库内 `331285EC` / 用户级旧版 `ED30090D`),结论口径以仓库内版本为准。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261116-07｜2026-11-16｜版本归档清点:10 个压缩包 + git 历史拼出开发时间线

**目的**:用户提供 `C:\RoboMaster\` 下历次归档(10 个包,10-26 ~ 11-16),要求串出一条开发时间线,并判断"哪个是最新"。

**方法(只读)**:逐包解出 `RM_DEVLOG.md` / `config/robot_infantry_2026.h`,比对 DEV 条目编号与日期跨度、关键宏值、特征文件存在性;未解全部内容、未改动任何原包。脚本 `build/scan_versions.py`、`build/scan_versions2.py`、`build/scan_versions3.py`,原始输出 `build/timeline_scan*.txt`,结论文档 `.Doc/版本归档时间线.md`。

**时间线骨架**:
- 10-26 上游 `basic_framework-master` 下载;同日打包 skill `rm-electrical-dev-0.7.0-draft`。
- 10-05 ~ 10-29 **JIT-RM 1027**:DEV-20261027-007..129 共 107 条,末条为"摩擦前馈 B=18 失稳回退 15"(RM-032/033)。
- 10-29 **1029 立项**(DEV-20261029-01 工程初版开发);10-30 单日 13 条(发射状态机/pitch 抖动/极限环/松杆续滑/裁判一期/键鼠实装)。
- 11-02 键鼠逻辑(21 条,末条回滚小陀螺投影方向);11-05 小陀螺平移校正 + 功率限制三层防线(25 条)。
- 11-05 19:04 `5ebd8ae` 建 git 仓库,归档时代结束;此后只有提交:11-06(3)、11-09(16)、11-11(1)、11-12(5)、11-14(12)、11-15(16)、11-16(6),共 66 个提交。

**三个坑**:
1. **11-05 前的包底盘参数是错的**:全部为 `RADIUS_WHEEL 60`、`WHEEL_BASE 300`、`TRACK_WIDTH 300`;仓库为 75/500/460,修正分别在 `9079a7d`(11-09 轮径 60→75)与 `0f686db`(11-11 轴距/轮距 300→500 / 300→460)。本车 B1 实测(2000mm ↔ 20529° ↔ 14.5149 deg/mm)证明轮半径 75mm 成立,按 60mm 只能折出约 1130mm → **旧包参数不可直接抄**。
2. **两个"11-16 打包"的文件其实是历史快照**:`JIT-RM 1029.rar`(13:32)包内 DEVLOG 仅 1 条(10-29 初版),`JIT-RM 1029-pitch轴修复.rar`(13:46)只到 DEV-20261030-11;二者均无 `.agents/skills`、`tools/`、AX650 模块。当前最新代码是仓库 `b1d588e`,不是这两个包。
3. **skill 版本号不可信**:`0.7.0-draft.zip`(10-26)版本号最高但 SKILL.md 仅 3.4 KB、references 11 篇(缺 `ai-constraints`/`baseline-diff`/`code-map`/`evolution-roadmap`);`_archive\...v0.9.7-draft\` 与 `1029\skill\rm-electrical-dev\` 逐字节相同且 manifest 仍写 `0.6.1-draft`。仓库内那份(15 篇 / 10.6 KB SKILL.md)是现存最全。

**未验证**:各包与 git 提交之间的内容等价性只做了末条 DEV 与宏值抽样,未逐文件 diff;11-06 ~ 11-14 无任何归档,该区间结论全部来自 git 提交信息。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261116-08｜2026-11-16｜《JIT-RM 1029 开发史》成书(1811 行 / 17 章)

**触发**:用户要求"写一个类似史书的东西,把这个工程的完整开发过程,包括错误和修正、优化等等,以及效果全部写进去,记录的很详细"。

**产出**:`.Doc/开发史.md` —— 228,790 B / 1811 行 / 17 个顶层章节(序 + 十章 + 附录 A-F),UTF-8 无 BOM、LF、`U+FFFD`=0,文件头带编号目录。

**分期与章目**:
- 前史 10-05~10-29(第一章,DEV-20261027-007..129 共 107 条)
- 1029 立项 10-29~10-30(第二章)、键鼠与功率 11-02(第三章)、小陀螺方向 11-03~11-05(第四章)
- git 化 11-05~11-09(第五章)、yaw 电流帧 11-11~11-12(第六章)、装机前 P0 11-14(第七章)
- 底盘首测 11-15(第八章)、云台装到 pitch 前馈 11-15~11-16(第九章)、规范化 11-15~11-16(第十章)
- 附录 A 决策与否决台账 / B 事故与坑索引(20 条) / C 参数演进表 / D 主机侧工具清单 / E AI 协作大事记 / F 材料来源与未验证清单

**写法**:统一叙事骨架"问题/现象 → 尝试与错误(含否决与 DEC 编号)→ 根因 → 修正(文件:行号,参数旧→新)→ 效果(实测数字原样)→ 教训(一句)";保留 DEV/DEC/RM 编号作为史料坐标;材料按 `[实测]/[理论]/[待实测]` 分级,无证据处留白而不补写。

**方法**:三段式 —— ①清点史料(工程 `RM_DEVLOG.md` 854 行 40 条、10 个归档包内 5 版日志、git 66 次提交、`.Doc` 27 份计划与结论);②按阶段分章,三个子代理并行读 1027 时期 276 KB 日志、1029 早期四版日志、11-05~11-14 的 DEVLOG+提交,产出第一/二~四/五~七章;③本人写序、第八~十章与六个附录,合并后生成目录。

**成书时发现、此前未单独记录的问题**(均已写入对应章节):
- 执行器已贴限幅时调增益毫无意义(1027 时期三次重复:误差 1311° 顶 720°/s、刹车 Output 停在 5000 限幅的 94%、D 限幅放大到 14153 仍过冲 14.0°,真正解法是 `MaxOut 5000→15000` + `B=15`)。
- 门限/时间常量未绑定真实调用周期与单位:毫秒当秒用(`dt=5ms > 0.1` 恒假 → 四轮零输出)、`0.3deg` 与 5 ms 周期增量比较使门限退化成 60°/s。
- 分段统计前未切净工况:甩杆离群段把过冲均值抬到 3.11°(p99=171°)、自转切入切出 ±15.6° 瞬态并进稳态得出"漂移 1.15°/s"(真实净漂移 0.023°/s)、整场混合中位 754 标定摩擦前馈(自转稳态真值 6461,差 8.6 倍)。
- `Gyro[0]` 轴错误(继承自上游)伪装成机械抖动:`Gyro[0]` 约等于 φ̇−ψ̇·sinθ,不含 pitch 自身角速度,转云台时 300°/s×sin20°≈100°/s 假速度直接进速度环。
- RoboMaster Assistant"电流环"开关关闭时电调忽略 `0x2FE`:反馈全部正常,唯一可证伪的是出力;先用电压帧 A/B 对照证明链路,才没掉进改 PI 的坑。
- 改 `MOTOR_DIRECTION_REVERSE` 只改连发互锁一处符号 → 单发/三发全超时而连发正常(最易漏测)。

**验证**:
- 一致性脚本:各章 DEV 引用无重复、无越界(part2∩part3=0、part3∩part4=0、part4∩part4b=0;第五章零 10 月条目);顶层章节 17 个、`###` 节 100+、数据表 50+。
- 编码:UTF-8 无 BOM、LF(CRLF=0)、`U+FFFD`=0。
- 基线审计:`Unchanged 313 / 460、Changed 99、Missing 48、Added 32、RedFlags 1(ins_task.c while(1),已批准启动期 fail-fast 例外)`,`exit 0` —— 本条目只增文档,固件源码未动。

**未验证**:
- 全书史料来源于 DEVLOG 与 git,未对 11-05 前的压缩包做逐文件 diff(以各版 DEVLOG 末条号判定),个别现场细节按"日志未记细节"留白。
- 第八章的底盘实测数据引自本会话实测记录,与 `.Doc/data/*.json` 一致,但原始波形未随书归档。

**AI**:DeepSeek-V4.1-Flash。

**下一步**:①git 提交 `.Doc/开发史.md` + `.Doc/版本归档时间线.md` + 本条目;②底盘 S1/S2 驱动测试(需先把 `DJI_MOTOR_CAN1_FORCE_DISABLE` 改回 0 重烧);③语文校对(可选,按章抽查事实与措辞)。

## DEV-20261116-09｜2026-11-16｜底盘驱动层三次标定:F1 跟随律 / C2 小陀螺 / F2 转速相容性

**触发**:用户 m03340"可以测试底盘了";m03409"其实如果你只是测功能正不正常的话可以跳过了，如果是标定什么参数的另说"→ 按"是否产出参数"重排,砍掉纯功能验证(T1 指令→速度、T2 落地短冲、T4 热与安全),只留 **F1 跟随律**、**F2 转速相容性**,并顺带做 **C2 小陀螺**(静止、无需场地)。

**前置核查**:
- 板上固件与 `build/basic_framework.elf` 一致,`g_dji_can1_enable = 1`,`authority_source` 解锁后为 3(RUN)→ 不需重烧即可驱动。
- **功率控制模块事实上未接入**:`PowerControl()` 与 `SetPowerLimit()` 全工程无调用者(`motor_task.c:6` 注明"本车未接裁判/超电,暂不调用"),`chassis_max_power` 恒 0 → "T6 功率上限标定"不存在,接入属结构级改动。

**新增工具(均为非侵入,走 `variables.sample`)**:
- `tools/chassis_trace.js`:9 变量 `est_vx/est_vy/est_wz + 四轮 speed_aps + robot_state + authority_source`,50 点/块、每块重试 8 次;输出 CSV + 平台段统计。
- `tools/follow_trace.js`:F1 专用 4 变量 `chassis_cmd_recv.wz / est_wz / chassis_cmd_recv.offset_angle / gimba_IMU_data->Gyro[2]`,实测 ~21 Hz。

**F1 跟随律(六次否决,全部劣于现状)**:

| # | 方案 | 反摆均值 °/s | 最大 | off 峰值 |
|---|---|---:|---:|---:|
| — | **现状(KP 1.0 / FF 0.9)** | **+11.8** | **15.1** | **8.96°** |
| 1 | KP 1.5 → 1.0(采纳) | 原 +14.4 → +8.3(单脉冲) | — | 8.96° |
| 2 | KP 1.0 → 0.6 | +14.9 | 20.6 | 11.51° |
| 3 | 速度环 KD 0.5 | +17.1 | 23.6 | 10.02° |
| 4 | FF 0.9 → 0 | 发散 | — | 16.70° |
| 5 | wz 修正量斜坡 1200°/s² | +22.2 | 42.4 | 9.71° |
| 6 | 只刹不反门控 40°/s | +33.3 | 64.3 | 14.85° |
| 7 | FF 0.9 → 1.0 | +18.3 | 32.0 | 12.66° |

- 采纳的一步:`config/robot_infantry_2026.h` `CHASSIS_FOLLOW_KP 1.5 → 1.0`,单次回中反摆 `+14.4 → +8.3 °/s`(−42%),代价是收尾慢 84 ms(334→418 ms)。
- **现象定位**:松手瞬间 `chassis_cmd_recv.wz` 在一个 5 ms 周期内从 −57 跳到 +26、再 +50;车体因此反向摆 8~15 °/s、持续 0.24~0.33 s、偏航行程约 3°。静止段 `|est_wz|` 34/49 点为 0 → 静止不抖,用户所述"回中之后抖一下"就是这一记反摆。
- **死区改不动它的实证**:t=13044 时 `offset_angle = 2.81°`(已进 3° 死区)、位置项≈0,但指令仍 −9.9 °/s = `spd(−14.6) + ff(+3.9)` —— 阻尼项与前馈**都绕过死区**,与用户"改死区不能消除"的实测一致。
- **否决机理(逐条)**:降增益只是拖慢收敛、让反摆在更大 off 上发生;速度环 KD 在"wz 是速度目标"的架构里等于主动加一脚反向(+85 °/s);FF=0 丢掉跟随速度主来源;任何"推迟修正"(斜坡/门控)都在攒更大误差;FF=1.0 因四轮速度环滞后导致底盘"超跑"、offset 反面积累。
- **根因**:`offset_angle` 来自"云台 IMU 姿态 + yaw 编码器"(`application/cmd/robot_cmd.c:286-291`),天生比车体真实偏航滞后一拍;要彻底消除需底盘独立 IMU 做绝对偏航闭环(硬件+结构级,建议赛季间)。
- 附带取证:静止 `gz` 均值 0.0088 °/s、std 0.209 → 前馈零偏累积不构成问题(FF=1.0 也就 0.53°/min)。

**C2 小陀螺(12 s 级)**:`build/chassis_c2_rotate.csv`,400 点/0 失败块。
- `est_wz` 平台均值 **300.1 °/s**(指令 300,偏差 0.0%)、std 3.58、峰值 306.2。
- 四轮 `speed_aps` 均值 −4302 / −4308 / −4287 / −4295 RPM(离散 0.5%),符号全同向。
- 平台内 `est_vx/est_vy` 均值 −0.3 / +4.7 mm/s → 自转不带平移。
- 测后温度 21~22 ℃、回零电流 ±55 码 → 12 s 级自转无热问题。

**F2 转速相容性(架空)**:预算与实测一致。
- 预算:`CHASSIS_ROTATE_SPEED 300` 单独需 4299 RPM;`CHASSIS_MAX_SPEED 1500` 单独需 2566 RPM;两者叠加需 **6865 RPM**;旧 `CHASSIS_MAX_MOTOR_RPM 6000` 只用到 3508 机械上限(469×19 = 8911)的 67%。
- 实测(旧 6000):`build/chassis_f2_airborne.csv` 平台段 `est_vx` 峰值 **1278**、`est_wz` **288**,左前/左后轮顶在 **6073 / 5994 RPM**(= 限幅 36000 deg/s),`vx/wz` 比值 4.90 vs 指令 5.00 → **等比例限幅工作正常,方向不失真,只整体降速 12.6%**。
- 改 `config/robot_infantry_2026.h:197` `CHASSIS_MAX_MOTOR_RPM 6000 → 7000`(占机械上限 78%)后复测:`build/chassis_f2_rpm7000.csv` 平台段 `est_vx` 峰值 **1501**、`est_wz` **300**,四轮峰值 7088 / 6440 / 6774 / 7101 RPM(最高超出限幅 1.4%,为速度环超调)→ **满速平移 + 满速自转不再被削**。
- 回退路径:6000(保守,接受 12.6% 削速)/ 8000(仅当确认 24V 与温升余量后)。

**验证**:
- `ninja -C build`:EXIT=0、0 error 0 warning(仅基线内 libc `_read/_write` 存根警告);FLASH 128632 B / 12.27%,RAM 107360 B / 81.91%。
- `tests\run_chassis_tests.ps1`:**ALL PASS (0 failures)**。
- 最终 ELF SHA256 `5FD6A911AD435A63B862635298069481137DE8D158C695E8D9B56429084C864A`,已烧录并核对 `firmwareConsistency: match`。

**未验证**:
- 7000 RPM 下的功率与温升余量(功率控制未接入、无裁判系统数据),落地长时间满速平移+小陀螺未测。
- 组合工况落地的打滑量:架空测得的 `est_vx/est_wz` 不含滑移误差,落地时 `est` 会高估真实运动。
- 跟随律在真实对抗节奏下的手感(只在脉冲阶跃下量化过)。

**AI**:DeepSeek-V4.1-Flash。

**下一步**:①把六次否决实验同步进 `.Doc/本车调参结论.md` 的跟随律小节;②落地复测 F2(确认 7000 RPM 下不打滑、不超功率);③提交本轮参数与工具。

## DEV-20261116-10｜2026-11-16｜文档回写:底盘/pitch/S4 计划与实车现状对齐

**触发**:用户 m00304"现在去给我解决优化计划和现在不符的情况"。此前已确认计划文档停留在 11-12/11-14,而 11-15/11-16 的实车结果没有回写,继续按旧文档排程会重复已完成的工作、并按已失效的判据做判断。

**改动(只改文档,零代码)**:
1. `.Doc/底盘优化计划.md`:§1.2 参数表按实况重写(7000RPM/`CHASSIS_FOLLOW_KP 1.0`/`RATE_KD 0/RAM 81.91%` 等);控制链图 6000→7000;
   §4 新增"现状总览"表;§5 Checklist 按实际完成度逐项勾选/注明(P0 与 P1-V1/V2 打勾,P1-V3 注明只有内部自洽、
   P2~P5 未做、P6 仅完成限幅那一项)。
2. **解决 V3 与手册的冲突**:计划原判据"力臂必须计时验、<10%",而 `.Doc/底盘测试手册.md` §10 已定案
   "自转法被滑移污染 10~20% > 门限 → 不测,取几何值 339.7mm"。回写按后者定案:V3 降级为"符号+量级",
   H4 门禁降级,不再作为阻塞项。
3. `.Doc/pitch轴优化计划.md`:§0 加状态回写段(四件未做事里重力标定/前馈已完成);§1.1 控制链写上前馈已实装与真实
   IL/MaxOut;§1.2 表右列全部换成实际值(含 `PITCH_HORIZON_ECD 47` 与 `MAX/MIN 25/−20` 标注为"待补证");
   §1.3"重力前馈未做"改为已完成;§4 新增"现状总览"表;§5 Checklist 勾选 P0/B1/B5/P2/P3;§7 参数登记表按实车值回填。
4. `.Doc/参数证据表.md`:`PITCH_HORIZON_ECD 47 [待补证]`、`PITCH_MAX/MIN [待补证]`、`CHASSIS_MAX_MOTOR_RPM 7000`、
   `CHASSIS_FOLLOW_KP 1.0`、`CHASSIS_MAX_SPEED 1500 [实测-架空]`、`PITCH_GRAVITY_*` 实值、`PITCH_FF_CALIB_LOG 0`。
5. `.Doc/本车调参结论.md` §2 开头删掉"重力前馈仍未做"这句与同级 §2.1 的矛盾。
6. `.Doc/装机后总计划书.md` §1 表格加"2026-11-16 更新"段(RAM 81.91%、前馈已启用、限幅 7000、零位变化),原表保留作对照。

**未改动**:`gimbal.c:202` 的 `Output_LPF_RC = 0.01f` 与同段注释"0.005→0.008"仍不一致;`dji_motor.h:155` 注释仍写
"当前=1"而宏实为 0。两处都是注释/参数注释级,留待单独一次改动,避免与本次纯文档回写混在一起。

**验证**:纯文档改动,未触碰 C/H/config,无需编译与主机测试;基线审计已重跑(见回话中的计数)。**未验证项**:无。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-01｜2026-11-17｜跟随模式参数回退到 11-05 基线(KP 1.5 / 限幅 6000)

**触发**:用户 m00342"把 CHASSIS_FOLLOW_KP、CHASSIS_MAX_MOTOR_RPM、CHASSIS_FOLLOW_RATE_KD、CHASSIS_FOLLOW_RATE_MIN 都改成之前的"。背景:对比 `5ebd8ae`(11-05 入库快照)后发现,跟随模式相对 11-05 只有 KP 与限幅两处实质参数变化;用户更认可 11-05 的上场手感。

**改动(参数级,一次一轴)**:
- `CHASSIS_FOLLOW_KP` **1.0 → 1.5**(11-16 曾由 1.5 降到 1.0);
- `CHASSIS_MAX_MOTOR_RPM` **7000 → 6000**;
- `CHASSIS_FOLLOW_RATE_KD` **保持 0.0**、`CHASSIS_FOLLOW_RATE_MIN` **保持 25** —— 这两项在 11-05 版本里根本不存在,且 `KD=0` 使 `ref += 0×(ref−est_wz)` 恒等于 `ref`,即该环数学上不介入;宏保留是为了将来改到力矩域复用(注释已写明)。
- `YAW_CHASSIS_ALIGN_ECD 5892 → 3564` 由**用户本人**修改(本条不重复记录)。

**影响与代价(必须知道)**:
1. KP 1.5 时回中反摆均值 **+14.4 °/s**(1.0 时 +8.3),收尾 334ms(1.0 时 418ms)——即"更快但更摆";
2. 限幅 6000 时,"满速平移 1500 + 满速自转 300"叠加需 6865 RPM,**会被等比例削速 12.6%**(架空实测 est_vx 1278、est_wz 288,比值不失真);
3. 跟随的"削顶"故障与这两项无关,根因仍是前馈 `0.9×gz` 未单独限幅后撞 ±300(总计划书 P0)。

**验证**:`ninja -C build` EXIT=0、0 error 0 warning(RAM 107480 B / **82.00%**,FLASH 131456 B / **12.54%**);`tests\run_chassis_tests.ps1` **ALL PASS(0 failures)**。**未烧录**,实车行为待用户烧后确认。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-02｜2026-11-17｜pitch 角度环死区 0.3°→0.6°(转 yaw 时 pitch 刺响的判据实验)

**触发**:用户报告"转 yaw 时 pitch 没给指令也会偶发刺响",且**pitch≈0 时同样出现**(排除了随俯仰角变化的 `ψ̇·sinθ` 投影项)。

**归因(当前排序)**:
1. **陀螺轴污染(不随 θ 变的那部分)**:IMU 若有偏角 β,yaw 速率以 `gz·sinβ` 漏进 `Gyro[0]`,速度环 Kp≈2865 码/(rad/s) 直接放大;
2. **角度环死区往复**:快转 yaw 时欧拉 `Pitch` 抖过死区边界,而 `controller.c:139-179` 在死区内是 `Output=0; ITerm=0` → 每次穿越都是一次速度目标阶跃,听感即"咔/刺";
3. **惯性/背隙机械撞击**:yaw 加速带动枪管切向惯性,俯仰机构在背隙内相对微动。定量估算惯性力矩仅 ≈0.03 N·m(重力矩 1.2 N·m 的 2~3%),**不足以独立解释 5000 码级出力**。

**改动(参数级,一次一个变量)**:`application/gimbal/gimbal.c:185` pitch 角度环 `DeadBand 0.3f → 0.6f`。若 2 成立,加倍死区可把小扰动整段吞掉、不再触发修正。

**代价(必须先知道)**:①准位偏差上限由 0.3° 放宽到 0.6°;②边界阶跃幅值由 `Kp×0.3≈2.4°/s` 变为 `Kp×0.6≈4.8°/s` —— 即**触发时更重、次数更少**。所以若刺响变"少但更响",说明 2 成立但方向反了,应回退 0.3 并改走反馈域去耦。

**验证**:`ninja -C build` 0 error 0 warning(RAM 82.00% / FLASH 12.54%);主机测试 ALL PASS;ELF `72031A0C…`。**未烧录、未实车验证**。

**下一步判据**:烧录后在中挡下 pitch≈0 反复快转 yaw —— ①刺响消失 → 死区往复成立,再评估准位误差是否可接受;②刺响变少但更响 → 回退 0.3,转"反馈域去耦";③毫无变化 → 转测"泄漏比 `|gx|/|gz|`"与 pitch 电流尖峰,区分陀螺污染与机械背隙。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-03｜2026-11-17｜pitch 刺响:用户定位为「电机与齿轮之间的虚位」,暂按已处理挂起

**触发**:两轮采集后用户给出关键现象:转 yaw 时 pitch 偶发刺响,**手摸电机剧烈震动,但齿轮/输出轴没有转动**,且**枪管没有下坠**。用户判断为**电机与齿轮之间存在虚位(背隙)**,处理后暂时不再出现,后续观察。

**为什么这条解释与数据吻合**:
1. 角度/速度反馈**全部取自 IMU**(`gimbal.c:219/220`),编码器只喂遥测 → IMU 装在**输出侧**,转子在背隙内来回摆动时 IMU **看不到**,速度环因此收不到任何阻尼反馈 → 持续满电流推 → 转子在间隙里撞来撞去;
2. 表现为:电流在零与满之间来回(实测 `pOut` 单拍 0→5000、`pCur` 到 **9045~9114 码 ≈ 11 A**)、**输出轴不动**、声音是齿面高频撞击的"刺",手摸电机发麻;
3. 与采集到的两个形状都能对上:事件 A(137.5 s)零出力静止后单拍满电流猛拽;事件 B(79.8 s)满电流 0.3 s 内 6° 的来回。

**与"传感器尖峰/干扰"的区分判据(已留档,复现时用)**:同时记录 `Pitch` 与 `pitch_motor->measure.total_angle`(编码器是独立证人)——`Pitch` 跳而 `total_angle` 不跳 = 估计/传感器问题;两者同步 = 真运动;两者都不动而电流在打 = **背隙**。

**当前处置**:用户已处理虚位,**暂挂起观察**;`DeadBand 0.3→0.6`(DEV-20261117-02)的实验性质不变,一并待实车结论。

**未验证/待办**:①虚位是否真的消除,需观察若干次上车;②若复现,按上面判据三分;③`pOut` 峰值 9045~9114 码 ≈ 11 A 已超 M3508 连续 10 A,与总计划书 P1-4 的堵转/温升复核同一件事。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-04｜2026-11-17｜跟随阻尼 0.9→0.4(回到 1027 稳定值):"刹车力度"这条历史线索

**触发**:用户回忆"上次稳定的时候改的是刹车力度",要求先深挖历史台账再动手。翻 `.Doc/开发史.md` 命中:

> `:241` | 跟随阻尼 | 无 → `CHASSIS_FOLLOW_DAMP` **0.2 → 0.4** | 外环纯 P 无刹车项,用 `offset_last` 差分算变化率、一阶低通系数 0.1(≈3Hz)再乘阻尼(DEV-60/63)

**关键发现**:1029 入库即 `0.9`,`0.4 → 0.9` 这一步**没有任何调参记录或依据**(原注释只写"经验值",证据等级 `[待实测]`)。
它是跟随参数里唯一一处"相对历史稳定值漂移 2.25 倍且无台账"的项;KP/FF/死区/线性区/SPD_MIN 都与 1027 一致。

**数据侧的两个相反预期**(`build/follow_pulse_kp10.csv` 重建三项,60Hz 采样):
- 三项占比:|ff| 34.7 / |damp| 9.1 / |pos| 5.5 → FF 70%、damp 18%、pos 12%;
- `r(damp, est) = −0.46` → **damp 当前确实在刹车**(与底盘运动反向),降到 0.4 会让收尾少一半制动力,过冲可能**更大**;
- 但"过刹 → 反向再刹"本身就是 2~3Hz 收尾抖动的来源之一,少刹可能反而更干净。
两种效应方向相反,**必须实车裁决**——这也正是本条作为"判据实验"而不是"修复"的原因。

**改动**:`config/robot_infantry_2026.h` `CHASSIS_FOLLOW_DAMP 0.9f → 0.4f`,注释写明历史依据、两个相反预期与回退路径。

**验证**:`ninja -C build` 0 error 0 warning(RAM 82.00% / FLASH 12.54%);主机测试 **ALL PASS**;ELF `70777555…`。**未烧录**。

**判据(与 DEV-20261116-09 同口径)**:松杆后 ①`est_wz` 反向峰值(基线 +11.8 均值 / 单次 +21)②`wz` 抖动幅值(基线 ±35°/s、2~3Hz、约 0.5s)③收敛时间(基线 334~418ms)。任一项变差 → 回 0.9,转 `RATE_KD` 路线。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-05｜2026-11-17｜跟随阻尼 0.4 实车否决 → 回到 0.9(两轮对照数据)

**做法**:18:22 烧录 `DAMP=0.4`(ELF `70777555`,与板上一致)后连续采 253 s / 8000 样本(29 Hz),
其中跟随态 4941 样本;剔除每次解锁后 3 s 的回中瞬态后剩 **2270 样本 / 77.4 s** 参与统计,与 16:48 的
`DAMP=0.9` 基线(679 样本 / 29.3 s)用同一套指标对比。

**结果(同口径,均剔除解锁瞬态)**:

| 指标 | DAMP=0.9(基线) | DAMP=0.4(实验) | 判定 |
|---|---|---|---|
| `\|off\|` 中位 | 1.14° | **1.89°** | 变差 +66% |
| `\|off\|` P95 | 4.13° | **7.51°** | 变差 +82% |
| `off` 峰峰 | 15.9° | **21.6°** | 变差 |
| `\|off\|>2°` 占比 | 27% | **49%** | 超死区时间近翻倍 |
| `wz` 范围 | −184~194 | −197~208 | 相当(两轮均未撞 300) |
| `r(wz,est)` | 0.88 | 0.90 | 执行层正常 |

**结论**:0.4 **更差**,`CHASSIS_FOLLOW_DAMP` 回 **0.9**。历史记忆(1027 用 0.4)不适用于 1029——
推断原因是 1027 整定 0.4 时外环还**没有** `CHASSIS_FOLLOW_FF=0.9`(FF 是 DEV-84/85 才加的),
前馈接管主要跟随量后,阻尼不再是唯一刹车,0.4 就只剩"刹不住"。

**顺带记录(用户现场观察,本次未采到数据)**:期间多次解锁是因为 **pitch 回正效果不好**;
本轮的采集变量里没有 pitch,无法量化。另:采集中出现 `auth=0`(RC_LOST)累计 2754/8000 样本,其中
183.4 s 之后持续约 70 s,遥控链路在多次开关机后掉线,后续做 pitch 回正要先排除这一点。

**验证**:`ninja -C build` 0 error 0 warning(FLASH 12.54%);主机测试 **ALL PASS**。**未烧录**。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-06｜2026-11-17｜跟随阻尼改为按转速调度(0.4↔0.9):用户体感与静态指标的统一

**触发**:DEV-20261117-05 按静态指标把阻尼回退到 0.9 后,用户反馈"**感官上 0.4 好一点**"。补充分段分析后发现两者都对——只是描述的工况不同。

**分段对照(两轮同口径,剔除解锁后 3 s 回中瞬态)**:

| 工况 | 指标 | DAMP=0.9 | DAMP=0.4 | 更优 |
|---|---|---|---|---|
| **运动段**(`\|gz\|`>100°/s) | 云台-底盘速度差 | −6.5 °/s | **+3.0 °/s** | 0.4 |
| | 底盘角加速度抖动 `\|d est/dt\|` | 816 °/s² | **657 °/s²**(−20%) | 0.4 |
| | 指令抖动 `\|d wz/dt\|` | 985 °/s² | **821 °/s²**(−17%) | 0.4 |
| **静态段** | `\|off\|` 中位 / P95 | **1.14° / 4.13°** | 1.89° / 7.51° | 0.9 |
| | 底盘角加速度抖动 | **38 °/s²** | 67 °/s² | 0.9 |

即:"运动中 0.4 更顺(用户开起来感受到的就是这个);停下来 0.9 更准更稳。"单一取值必然牺牲一头。

**改动(控制逻辑,用户已批准)**:
- `config/robot_infantry_2026.h`:新增 `CHASSIS_FOLLOW_DAMP_SLOW 0.9` / `CHASSIS_FOLLOW_DAMP_FAST 0.4` /
  `CHASSIS_FOLLOW_DAMP_LO_SPD 20` / `CHASSIS_FOLLOW_DAMP_HI_SPD 60`;旧名 `CHASSIS_FOLLOW_DAMP` 保留为
  `_SLOW` 的兼容别名。
- `application/chassis/chassis.c`:新增 `FollowDampBySpeed()`(按 `chassis_IMU->Gyro[Z]` 绝对值在 20~60°/s
  之间线性过渡,边界连续);跟随分支的阻尼项改用该调度。与 yaw 的 `UpdateYawDamping()` 同一套思路。

**预期**:运动中保留 0.4 的顺滑(速度差、抖动),静止时恢复 0.9 的残差水平(P95 4.13°)。

**验证**:`ninja -C build` 0 error(仅基线内 libc `_write` 存根链接警告);RAM 82.00% / FLASH 12.55%;
`tests\run_chassis_tests.ps1` **ALL PASS**;ELF 较上一版 +132 B。**未烧录**。

**验收(与前面同口径)**:①运动段速度差回到 ~3°/s ②静态段 `|off|` P95 回到 ~4° ③两段抖动均不劣化。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-07｜2026-11-17｜跟随阻尼改为「三折点无极曲线」(0.9 → 0.7 → 0.4)

**触发**:用户否掉"固定 0.6"的两头妥协,要求把阻尼做成**多级/无极变化**。用户判断这是好方法,要求试。

**背景数据(四个固定值的实测)**:

| 配置 | 静止 `\|off\|` P95 | 运动段速度差 | 运动抖动 | 静止抖动 |
|---|---|---|---|---|
| 0.9 | **4.13°** | −6.5 °/s | 816 | **38** |
| 0.4 | 8.17° | +3.0 °/s | 657 | 67 |
| 调度 0.4↔0.9(20~60 过渡) | 6.20° | **+0.9 °/s** | 654 | 72 |
| 固定 0.6 | 7.25° | −2.2 °/s | **602** | 55 |

结论:静止要 0.9、高速要 0.4,而 20~60°/s 的窄过渡带切得太早、曲线太陡,导致"低速区"仍在用接近 0.4 的阻尼。

**改动(控制逻辑,用户已批准)**:
- `chassis.c: FollowDampBySpeed()` 改为**三段折线**:`0°/s → SLOW`、`SPD1 → MID`、`SPD2 → FAST`,超出 `SPD2` 保持 FAST;
  两段在折点处取值相等,曲线连续、无跳变。等价于"多级 + 无极"。
- `config` 折点:SLOW **0.9** / MID **0.7** / FAST **0.4**,SPD1 **50 °/s**、SPD2 **120 °/s**(实测低速微操 5~40、
  正常跟随 100~220、运动段样本基本在 100 以上)。曲线形状只改这 5 个宏。

**预期**:低速段比上一版"20~60 过渡"更早拿到高阻尼(50°/s 处仍是 0.7),高速段照旧 0.4;
目标是把静止残差往 4~5° 收,同时保住运动段的 650/700 级抖动。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.55%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **690863A2**。**未烧录**。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-08｜2026-11-17｜跟随环重写:前馈单独限幅 + 角速度伺服改用 `(ff − est_wz)` 残差

**触发**:用户明确提出目标——"云台转底盘转,云台停底盘停,不要松杆了还在抖"。要求直接改 FF 方向。

**为什么之前的写法做不到"停就停"**:
1. **前馈没单独限幅**:`ff = 0.9×gz`,gz 实测峰值 400~455°/s,单项即超 ±300 总上限 → 三项加完才削顶,
   前馈吃满把位置项/阻尼项份额整段挤掉,三项互相顶;
2. **角速度闭环写法缺陷**:旧式 `ref += KD×(ref − est) = (1+KD)·ref − KD·est`,**把目标放大 1+KD 倍**,
   KD=0.5 时一给指令就撞限幅、松杆瞬间命令 +85°/s 正转(DEV-20261116-09 第 3 项 +85°/s 的真正来源,
   不是"速度控制不能刹车"这个结论);
3. **门控选错变量**:旧门控是 `|est_wz| ≥ 25`,底盘快停下时反而把刹车关掉,与需求正相反。

**改动**:
- `chassis.c`:前馈单项 `LIMIT_MIN_MAX(ff, ±CHASSIS_FOLLOW_FF_LIMIT)`;角速度伺服改为
  `wz += KD × (ff − est_wz)`,门控改为对**残差**生效;旧写法删除。
- `config`:`CHASSIS_FOLLOW_FF_LIMIT 270`(=0.9×300,前馈最多占 90%,留余量给修正项);
  `RATE_KD 0.0 → 0.5`;`RATE_MIN 25 → 8`(语义变为残差门限)。

**机理**:云台停的瞬间 `ff→0` 而 `est_wz` 仍是残余转速 → 立刻产生 `−0.5×est` 的反向刹车,**不必等
`off` 建立**;云台起步时 `est≈0` → 同一项助起步;稳态 `ff≈est` → 修正项归零,无稳态偏差。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.56%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **6707F97D**。**未烧录**。

**判据**:松杆后 ①`est_wz` 反向峰值(基线均值 +11.8 / 单次 +21)②`wz` 抖动持续时长(基线 0.25~0.5s)
③运动段速度差(基线 +0.9~+3.0)。若出现反向尖峰或底盘"抖",先把 `RATE_KD` 降到 0.3;若仍过冲则升到 0.8。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-09｜2026-11-17｜位置项单独限幅 ±50,压掉停稳后的尾巴

**触发**:DEV-20261117-08 采集(3000 样本/94.8s,固件 `6707f97d`)显示"停"已经明显改善,
但停稳后仍有 ±7~10°/s、约 0.4s 的尾巴。逐点定位到位置项。

**实测(统一检测器,同口径六组对照)**:

| 配置 | 反向峰值 均值 | 反向峰值 最大 |
|---|---|---|
| 0.9 固定 | 79.2 | 174.6 |
| 0.4 固定 | 41.0 | 191.5 |
| 调度 | 37.6 | 186.7 |
| 0.6 固定 | 20.4 | 180.0 |
| 三折点 | 27.5 | 173.2 |
| **FF 限幅 + 残差伺服** | **15.7** | **56.5** |

最强一次动作(t=71.9 s):`est` 从 **−196 → 0 用 0.25 s**,瞬时过冲仅 **+1.9°/s**;但刹车过程中
`off` 冲到 **9.8°**,平方律给出 `1.5×7.8² ≈ 91°/s`,一脚把底盘拽回 → 之后 ±7~10°/s 抖约 0.4s。

**改动**:`chassis.c` 位置项计算后加 `LIMIT_MIN_MAX(wz_pos, ±CHASSIS_FOLLOW_POS_LIMIT)`;
`config` 新增 `CHASSIS_FOLLOW_POS_LIMIT 50.0f`。限幅只在 `off > 8°` 时生效(停稳后 off 稳定在 5~6°,
此时平方律给 26°/s),故**只压刹车瞬间的过冲一脚,不影响正常收敛速度**与用户选定的 KP=1.5。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.56%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **9BEB6950**。**未烧录**。

**判据**:停稳后 ①`est_wz` 残余抖动峰峰值(本次约 ±10)②尾巴持续时长(本次约 0.4s)
③反向峰值最大(本次 56.5,不能回升到 100 以上)。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-10｜2026-11-17｜小陀螺:电流域换算被证伪,`YAW_SPEED_KP_HIGH` 20→30

**触发**:用户提出小陀螺下"云台要稳、顺着转不能冲过头、逆着转不能被抓回"。查台账发现该问题在 §七/§八 已被系统研究过(DEV-90~129),但当前配置与当年定型的结论有漂移。

**用一次小陀螺采集(`exports/top_20261117.csv`,1439 样本)回归出两项域参数**:

| 量 | 实测 | 含义 |
|---|---|---|
| `real_current ≈ 1.036 × Output − 4371` | 斜率 **1.036** | 输出码与电流码 **1:1**,不是 16384/25000=0.6554 |
| `r(α, I) = 0.88`,斜率 `I/α = 4.1` 码/(deg/s²) | — | 惯量前馈满补偿参考值 4.1；现 FF 1.875 只补 46% |
| 回归截距 −4371 码 | — | 纯自转稳态维持世界静止的摩擦/背压直流分量 |

**结论**:`YAW_CURRENT_DOMAIN_SCALE` 按"量程比"乘 Kp 的假设不成立,电流模式等效 Kp 只有电压模式的 65.5%
(20 → 751,电压模式 20 → 1146)。这正是"逆着自转方向打杆被拉住"的量化来源(历史电压模式 HIGH=40 时
负向滞后 23.0→15.1)。

**改动**:`YAW_SPEED_KP_HIGH` 电流模式 **20 → 30**(等效 Kp 751→1127),先补一半;只作用于相对转速
300~450 区间,纯自转稳态不受影响。未动 `YAW_CURRENT_DOMAIN_SCALE` 与前馈增益——它们要和摩擦/惯量项
一起整体重标定,不能单独改。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.56%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **2739C3AB**。**未烧录**。

**待办**:①上电后采"顺/逆各 3 次固定幅度打杆",按判据看负向滞后与 `Output` 峰值;<90% 量程且无啸叫则可上 40;
②按南理工口径补**库仑摩擦项** `τc·tanh(ω/ε)`——当前只有粘性 `B·ω`,这是唯一缺失的前馈项。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-11｜2026-11-17｜电流域三参数标定 + 前馈重标定(惯量 1.875→3.0, 摩擦 15→8.7)

**做法**:一次采集(`exports/calib_20261117.csv`,6000 样本 / 196 s)同时覆盖小陀螺顺逆打杆与跟随慢速段,
用回归直接标出三个参数:

| 参数 | 实测 | 方法 | 旧配置折算 | 判定 |
|---|---|---|---|---|
| 惯量 `J` | **4.0 码/(deg/s²)**,r=0.84,196 样本 | `real_current` vs `dGyro[2]/dt`(仅 \|α\|>200) | 1.875×0.6554=**1.23**,仅 31% | 欠补偿 |
| 粘性 `B` | **5.69 码/(deg/s)**,5418 样本 | `real_current` vs 相对转速(\|α\|<300) | 15×0.6554=**9.83**,过补 1.7× | 过度补偿 |
| 库仑 `τc` | **−9 码 ≈ 0** | 同上的 `sign(rel)` 项系数 | 无此项 | **不需要** |

**标定中发现的域问题**:`real_current ≈ 1.036 × Output`(1439 样本)说明输出码与电流码 1:1,
而 `YAW_CURRENT_DOMAIN_SCALE` 仍按量程比 0.6554 乘前馈与 Kp → 前馈被系统性削了 35%。

**改动**:`YAW_INERTIA_FF_GAIN 1.875→3.0`(折算 1.97,约为实测 4.0 的 50%,沿用 DEV-105 取半策略);
`YAW_FRICTION_FF_B 15→8.7`(折算 5.70,补满实测 5.69);新增 `YAW_COULOMB_FF_TC 0.0`(实测为 0,不启用)。

**实测效果(同动作对照,小陀螺 est≈299.4°/s)**:

| 指标 | 重标定前 `2739C3AB` | 重标定后 `E01FF2BF` | 变化 |
|---|---|---|---|
| **顺自转**松杆残余 `\|gz\|` 均值 | 102.3 °/s | **61.3 °/s** | **−40%** |
| **逆自转**松杆残余 `\|gz\|` 均值 | 62.1 °/s | **40.9 °/s** | **−34%** |
| 顺/逆比值 | 1.65 | **1.50** | 收窄 |
| `Output` 峰值 | 11875 | **10404** | −12%,`\|out\|>14700` 占比 0% |

**结论**:重标定方向正确,两个方向同时改善约 35~40%,且未引入饱和。**残余不对称仍存在(1.50)**,
顺向仍更容易冲——下一步应继续加大惯量前馈(3.0→3.5)或用转速相关的额外阻尼,但需先排除 α 尖峰引起
的反向尖峰(DEV-104 的历史教训)。

**备注**:B 轮采集因探针会话中断只取到 1 段(1000 样本 / 26 s 小陀螺),顺 5 次、逆 10 次;结论方向一致,
若要更实的统计需再采一轮 3 段以上。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-12｜2026-11-17｜惯量前馈 3.5 + α 限幅 800:55 次打杆统计定型

**做法**:`49580834`(惯量 3.5 / α 限幅 800)采集 6000 样本 / 3 段,小陀螺 3058 样本,
**顺 27 次 / 逆 28 次**打杆(前两版只有 5~10 次,统计不可信)。

**三版对照(松杆后 0.6 s 内残余 `|gz|` 峰值,°/s)**:

| 版本 | 顺 P50 | 顺 P90 | 顺 max | 逆 P50 | 逆 P90 | 逆 max | P50 比值 |
|---|---|---|---|---|---|---|---|
| A 惯量 1.875 / α1000 | 91 | 140 | 140 | 83 | 88 | 88 | 1.11 |
| B 惯量 3.0 / α1000 | 48 | 116 | 116 | 18 | 80 | 103 | 2.69(*) |
| **C 惯量 3.5 / α800** | **50** | **109** | 190 | 44 | 83 | 132 | **1.14** |

\* B 只有 5 次顺向,2.69 是抽样偶然,不作数。

**判定**:**C 定型**。中位数顺向 91→50(−45%)、P90 116→109,顺/逆比值 1.50→**1.14**,
`Output` 峰值 10205、`|out|>14700` 占比 0%(无饱和)。
唯一变差项是顺向极值 190(1/27 次),但 B 版仅 5 次抽样无资格比较;判断为分布尾部而非系统性回归,
**不再在实验室单动作上继续抠**。

**遗留**:①顺向极值 190 未追因(策略:上场实战,若实战仍感到"顺着转甩"再回头查);
②`YAW_SPEED_KP_HIGH` 30 尚未上到 40(待 C 版实战手感后再定);
③`Output→current` 的 1:1 关系已证伪 0.6554 域换算,`YAW_CURRENT_DOMAIN_SCALE` 仍是 0.6554 —— 
   **有意保留**:它现在同时乘在 Kp 与前馈上,若要改成 1.0 必须同步把前馈按绝对值折算,属整包重标,留待赛季间。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-13｜2026-11-17｜速度环 Kp 双路调度(补"顺向滞后"这一侧)+ Kp 30→40

**触发**:用户问"顺向滞后能不能也压下去"。按历史同口径把本次数据换算成角度量后定位到根因。

**根因(单位换算后的铁证)**:Kp 调度量是 `|speed_aps|`(电机相对转速),而顺向打杆时
`rel = |−300 + ω|` **反而变小**(ω=+200 → rel=100),永远落在 `rel ≤ 300` 区间 → 只拿 `LOW=20`;
逆向打杆 rel 爬到 450 → 拿 `HIGH`。**两个方向吃到的增益差一倍,而改 `HIGH` 对顺向完全无效。**

**实测对照(300°/s,按历史角度口径换算)**:

| 指标 | 电压模式 DEV-119(历史最优) | 电流模式 Kp30(本次) |
|---|---|---|
| 顺向滞后 | 10.5° | 13.4°(max 17.2) |
| 逆向滞后 | 15.1°(16.1) | 13.6°(max 20.3) |
| 顺/逆差 | 4.6° | **0.2°** |
| 松杆过冲 | 中位 **12.8°** | **2.1~2.4°**(max 7.2) |
| `Output` 峰值 | 19642 | 10205(62%) |

即:过冲已降 80%、对称性已解决,唯一落后项是顺向滞后。

**改动(控制逻辑,用户已批准)**:
- `gimbal.c` 速度环 Kp 改**双路调度取 max**:①按相对转速(原逻辑原样保留)②按**云台世界角速度**
  (`|Gyro[2]|`,60→150 °/s 线性)。稳态自转时 ω≈0 → 仍拿 LOW(鸡头零影响);任一方向打杆 ω 变大 → 拿 HIGH。
- `config` 新增 `YAW_SPEED_KP_WORLD_LO 60` / `_HI 150`;同时 `YAW_SPEED_KP_HIGH` 电流模式 30→40
  (承接上一条的待办;40 折算 Kp 1502 ≈ 历史定型 2292 的 66%)。

**预期**:顺向滞后 13.4° → ≤10.5°(追平历史电压最优);逆向与稳态鸡头不变;过冲不劣化。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.57%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **F900E66F**。**未烧录**。

**风险与回退**:①若出现高频啸叫/发飘 → 先降 `YAW_SPEED_KP_WORLD_HI` 或把第二路整体撤掉(去掉 max 即可);
②稳态自转精度必须复测(硬约束:静止 `Gyro[2]` 标准差仍应 ≈2.4°/s)。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-14｜2026-11-17｜"小陀螺→跟随"直接切换补触发 yaw 回正

**触发**:用户反馈"小陀螺从下档拨到中档时 yaw 回正特别缓慢"。先按"跟踪慢"假设分析,被用户否掉("时间肯定不对"),重新定位后发现是**根本没触发**。

**实测证据**(`exports/spin_rc2_20261117.csv`,6000 样本 / 195 s,固件 `46067e2e`):

| mode 切换 | 时刻 | 切换时目标角 | 之后行为 |
|---|---|---|---|
| 1(小陀螺) → 3(跟随) | **58.67 s** | **−70.1°** | 目标停在 −70.1°,直到 75 s 用户打杆才变 |
| 3 → 1(再切回小陀螺) | 113.73 s | 0.0° | — |
| 1 → 3 | **156.27 s** | **58.3°** | 目标停在 58.3°,直到 184.6 s 仍是 58.3° |

**两次切换后目标角纹丝不动十几秒 —— 不是慢,是回正压根没执行。**

**根因**:`yaw_recentering = 1` 只出现在两处(`robot_cmd.c`)——
① L227 **F 键**(走 `rc->key_count[KEY_PRESS][Key_F]`,键鼠通道;DR16 遥控没有 F 键);
② L466 **解锁**(上挡零力 → 中挡)。
**从下档小陀螺直接拨回中档,既不经过上挡也不按 F,两个触发点都绕过了。**

**改动(控制逻辑,用户已批准)**:`RobotCMDTask` 在回正块之前新增"ROTATE → FOLLOW"跳变检测,
置位 `yaw_recentering = 1` 并清零 `recenter_cnt`,复用现有 90°/s 渐进闭环,不引入新逻辑。

**另**:`GIMBAL_RECENTER_SPEED` 自查 git 历史确认**从 11-05 入库起一直是 90**(`180°约2s,不甩`),
不是本轮改坏的;若觉得慢可后续调到 150。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.59%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **3499DCA6**。**未烧录**。

**判据**:切换后目标角应以 90°/s 匀速转向 0°,180° 用时约 2 s;不再"纹丝不动"。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261117-15｜2026-11-17｜回中重构:只动底盘、云台保持世界方向(修掉"转云台 + 与跟随环互抢")

**触发**:用户明确回中的定义——"**回中只有底盘动,云台不允许参与,解锁/陀螺回跟随触发回中时云台都要保持当前方向**";并反馈回中"一顿一顿",要求查逻辑 bug。

**旧实现的三处错误**(`robot_cmd.c` 原回中块):

1. **执行器错了**(致命):`gimbal_cmd_send.yaw -= delta` 改的是**云台世界目标**,于是回中期间云台自己在转。
   实测 `Gyro[2]` 冲到 ±200°/s(2026-11-17 21:26 采集),与"云台不许参与"完全相反。
2. **与跟随环互抢**:改云台目标 → 关节角 offset 变化 → 跟随环同时驱底盘 → 两者互相追,
   `|off|` 永远进不了 2° 死区,只能跑满 `GIMBAL_RECENTER_TIMEOUT`(600×5ms = 3 s)。
   实测两次回中:−400.39°→−670.39°、−668.14°→−934.99°,**各转 270° = 90°/s × 3 s**,都是超时值。
   → 这就是"一顿一顿"的机理。
3. **改符号无效**:DEV-20261117-14 之后我把 `GIMBAL_RECENTER_DIR` 改 −1 只是让跑飞方向反过来,
   属治标;结构错了,符号无解。该宏已删除。

**重构(用户已批准)**:

| 层 | 改动 |
|---|---|
| `robot_def.h` | 新增 `chassis_mode_e::CHASSIS_ALIGN` |
| `chassis.c` | 新增 `case CHASSIS_ALIGN`:恒速 120°/s 朝减小 `|offset|` 方向转,`|offset|<3°` 时线性降速收尾;不动 vx/vy |
| `robot_cmd.c` | 回中块**不再写 `gimbal_cmd_send.yaw`**,改为置 `chassis_mode = CHASSIS_ALIGN`;退出条件改为"已回中 / 超时"两种 |
| `config` | 新增 `CHASSIS_ALIGN_SPEED 120`、`CHASSIS_ALIGN_SOFT 3.0`;删除 `GIMBAL_RECENTER_DIR`;`GIMBAL_RECENTER_SPEED` 标记废弃存档 |

**"回中期间允许平移"**:ALIGN 只写 `wz`,vx/vy 及平移斜坡逻辑保持原样(switch 之后处理),故平移不受影响;
同时取消了旧实现里"左杆出死区即中止回中"的判据(那条会误杀平移)。

**未受影响的功能**:跟随/小陀螺/不跟随三条既有分支一行未动;急停与看门狗仍在 switch 之后统一处理;
`CHASSIS_ROTATE` 的平移偏置仍只作用于 ROTATE。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.58%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **B50DF41B**。**未烧录**。

**判据**:①回中期间 `Gyro[2]` 应保持在 ±3°/s 内(云台不动)、`est_wz` 应达 ±120°/s(底盘在转);
②`|offset|` 单调收敛到 2° 内,**不再跑满 3 s 超时**;③回中时推平移杆,底盘应能正常平移。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261118-01｜2026-11-18｜跟随前馈 0.9→1.0:实测"两方向一致落后 45°/s"

**触发**:用户报"推满杆底盘追不上云台,而且向右转时云台偏左像被底盘超过"。先质疑是回中改动引起(排除:ALIGN 是独立模式),后按数据定位。

**实测**(`exports/sym_20261118.csv`,2000 样本 / 63 s,静止10s→顺时针10s→静止10s→逆时针10s→静止10s):

| 阶段 | `gz` | `wz` | `est_wz` | `off` |
|---|---|---|---|---|
| 静止 0–10 s | ≈0 | ≈0 | 0 | **+0.7→+1.0**(零位准确) |
| 顺时针 10–21 s | −160~−180 | −120~−130 | −120~−126 | +1 → **+23.7** → +2.5 |
| 逆时针 31–41 s | +164~+166 | +116~+128 | +120~+126 | −6.5 → −7.7 |

**结论**:①`YAW_CHASSIS_ALIGN_ECD` 零位准确(静止 off≈+0.8°),排除零位偏置假设;
②**两个方向都稳定落后 43~45 °/s,方向无差别** → 系统性欠驱动,不是符号问题;
③执行层正常(`est−wz` 仅 1.5°/s),缺口在**指令**侧;
④拆项:wz = ff(0.9×gz) + 位置项(撞 POS_LIMIT 50) + 伺服 0.5×(ff−est) ≈ −117,与实测 −125 吻合。
即 0.9 结构性地少 10%(17°/s),位置项补不上还削弱前馈 → `off` 累积 24°,即用户看到的"云台偏一边"。

**改动**:`CHASSIS_FOLLOW_FF 0.9 → 1.0`。与 DEV-20261116-09 否决 FF=1.0 时环境不同:
现已有 `FF_LIMIT 360` 单独限幅、残差伺服已改为 `0.5×(ff−est)`(非放大目标的 `(1+KD)ref`)、
跟随总限幅已由 300 提到 400;且当时只有"超跑"体感,现有"两方向一致落后 45°/s"的量化证据。

**验证**:`ninja -C build` 0 error(RAM 82.00% / FLASH 12.64%);`tests\run_chassis_tests.ps1` **ALL PASS**;
ELF 前缀 **A36EE682**。**未烧录**。

**附注**:本次 FLASH 比上一版大 552 B,但工作区除本宏外无改动 —— 疑为上一次构建是用户自己
全量重编(其烧录的 `9bb6a4b2` 不在本仓库任何提交里),缓存状态不同所致;功能上无影响,已记录备查。

**判据**:①顺时针/逆时针满杆时 `wz` 与 `gz` 的差应从 −45 收敛到 0 附近;
②`off` 不再累积到 20°+,应稳定在死区附近;③若出现"底盘冲过云台"(`off` 反向且增大) → 回 0.95。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261116-11｜2026-11-16｜新建唯一执行入口《.Doc/总计划书.md》,其余计划书降级归档

**触发**:用户要求"把现有的计划书全检查一遍,搞一个总计划书,只执行这个计划书"。此前五份分计划各自带阶段编号与勾选状态、彼此还有跨轴依赖,交叉执行时反复出现"已完成项被重做""按已失效的判据判断"的问题。

**做法**:
1. 逐份检查 yaw / pitch / 底盘 / 发射 / 跟随 五份计划的现状,按**代码 + DEVLOG** 重新判定"已完成 / 部分 / 未做 / 已定案不做",不照抄文档里的勾选。
2. 新建 `.Doc/总计划书.md`:五轴现状表 → P0~P5 单一执行队列(每项带层级/判据/回退)→ 烧录批次 F-A~F-E → 硬依赖 D1~D6 → 不做清单 → 文档分工 → 变更记录。
3. 六份旧文档顶部加归档横幅(yaw/pitch/底盘/发射/跟随 = 技术细节库;旧《装机后总计划书》= 作废),并在 `AGENTS.md` 的 Map 段写明"执行入口唯一"。

**关键判定(与旧文档不同之处)**:
- **跟随环定为 P0** —— 唯一有实测证据的活跃故障:18:40 采样(2000 样本/80.7s)中 `wz` 撞 −300 削顶、`gz` 峰值 −455.6°/s、`r(wz,est_wz)=0.99`(执行层正常)、`r(off,wz)=−0.86`;主因是前馈 `0.9×gz` 未单独限幅,|gz|>333°/s 时单项即超上限。
- **力臂验收正式关闭** —— 按 `.Doc/底盘测试手册.md` §10 定案,自转法被滑移污染,取几何值 339.7mm,V3 只保留"符号+量级"。
- **pitch 刺响降级为 P1** —— 偶发不可复现,但已有"角度峰峰 0.05° 而 `Gyro[0]` 抖 17~23°/s"的旁证,先做 B7 再谈滤波/陷波。
- **功率控制降级为 P5 条件触发** —— 前置 `buffer_energy` 从未采样,"要不要做"尚无依据。
- **发射放 P4** —— 受 RM-002 硬件限制,先判硬件再标定弹速。

**未验证**:纯文档改动,未触碰 C/H/config;`ninja` 与主机测试本轮不必重跑(ELF 仍是 DEV-20261116-09 那一版)。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261116-12｜2026-11-16｜补记:机械零位 Flash 标定模块(calib)+ ECD 回正 + 限位实测落 config

**触发**:提交前核对工作区发现 `modules/calib/`、`gimbal.c/h`、`robot.c`、`robot_cmd.c`、config 有成组未记录改动,但 **18:07 已编译并烧录**(ELF `45E2B6A6…`,与板上 `firmwareConsistency: match`)。按 AGENTS「发现未记录改动先问人/先补证」,本条为**补记**,内容以代码与实测为准。

**功能变化**:
1. **新增 `modules/calib`(Flash 持久化零位)**:记录含 `magic/version/size/crc32`,任一不符即判无效并回退编译期宏;存放 sector 11(`0x080E0000`),`Calib_FlashSafe()` 在 Init 时检测代码是否越界到该扇区,越界只告警不写。解决"零位每次编译都要改宏重烧"。
2. **pitch 回正改 ECD 定零位**:`GimbalPitchRecenterTarget()` 由标定记录解算目标,IMU 只用于**判圈**(单圈 ECD 在行程内绕多圈,用 IMU 粗测消歧);未标定时返回 0,保持旧的"解锁回 IMU 水平"。安全钳制:目标必须落在 `PITCH_MIN/MAX_ANGLE` 内且与当前姿态相差 ≤90°,否则回退旧行为并 `LOGERROR`。
3. **标定触发**:遥控上挡零力(`ROBOT_STOP`)+ 左纵杆推满(`CALIB_TRIGGER_ROCKER_L1=600`)保持 3 s → 把当前 pitch 姿态写为水平零位。**必须零力**:擦除期间 CPU 冻结 1~2 s,控制环与 CAN 全停,有力矩时云台会摔。
4. **config**:新增 `PITCH_REDUCTION_RATIO 19.0f`、`PITCH_ECD_SIGN +1.0f`、`CALIB_TRIGGER_ROCKER_L1/HOLD_MS`;`PITCH_MIN_ANGLE −20 → −22` 且上下限注释改为 `[实测]`(即用户 2026-11-16 所说"限位已测"的落地)。
5. **pitch 输出低通 `Output_LPF_RC` 0.01 → 0.012**(呼应总计划书 P1-2 的刺响方向,尚未验收)。
6. 主机测试解耦:`tests/native_stubs/gimbal_stub.c` + `run_chassis_tests.ps1` 增加 `application/gimbal` 包含路径与 stub 源。

**验证**:`ninja -C build` → no work to do(工作区即当前 ELF),0 error 0 warning;`tests\run_chassis_tests.ps1` **ALL PASS(0 failures)**;ELF `45E2B6A669765368168A0D5DDDC0067EE579DC1F45ECCC627BA0799C248EF8AA`,18:07 已烧录且板上校验一致。

**未验证(明确记录)**:
- Flash 擦写/回读在实车上**没有跑过一次**(尚未触发过标定),丢弃时回退路径未实测;
- `PITCH_ECD_SIGN=+1` 方向未实测(判法:抬头时 `pitch_axis` 与 IMU `Pitch` 同号);
- 标定触发(上挡 + 推满 3 s)未实车验证;
- `Output_LPF_RC 0.012` 的手感/刺响效果未验收;
- `PITCH_REDUCTION_RATIO=19` 假定无额外减速,未实测确认。

**AI**:DeepSeek-V4.1-Flash。

## DEV-20261118-02｜2026-11-18｜跟随模式整段回退到 1027 `DEV-20261027-88` 版(按用户要求)

**触发**:用户对照 1027 工程 2026-10-28 的跟随手感(收尾台账 `DEV-20261027-88`,19:14,松杆过冲中位 **3.18°**、回稳 0.26~0.31s),要求"只把跟随模式回调到 88 这一版,不能影响其他任何东西"。

**范围(唯一)**:
- `application/chassis/chassis.c` 的 `CHASSIS_FOLLOW_GIMBAL_YAW` 分支;
- `config/robot_infantry_2026.h` 中**仅被该分支引用**的 6 个宏。
- **未动**:yaw/pitch 云台 PID(影响瞄准与小陀螺)、`CHASSIS_ALIGN` 回中、`CHASSIS_ROTATE` 小陀螺、发射、协议、其它任何层。

**回退掉的 11-17/11-18 五项增益**(88 版本来没有):
| 项 | 回退前(11-18) | 回退后(88) |
|---|---|---|
| 阻尼 | 按转速三折点调度 0.9/0.7/0.4(`FollowDampBySpeed()`) | 固定 `CHASSIS_FOLLOW_DAMP 0.4` |
| 位置项 | 单独限幅 `CHASSIS_FOLLOW_POS_LIMIT ±50` | 无限幅 |
| 前馈 | `CHASSIS_FOLLOW_FF 1.0` + `FF_LIMIT ±360` 单独限幅 | `0.9`,无单独限幅 |
| 角速度伺服 | `wz += RATE_KD(0.5)×(ff − est_wz)`,门限 8 | 无 |
| 总限幅 | `CHASSIS_FOLLOW_WZ_MAX 400` | 复用 `CHASSIS_ROTATE_SPEED 300` |
| 死区 | 死区内 offset_rate_f **照常累计** | 死区内**清零**(DEV-75 原样) |

**为什么 88 的"强"不能整体照搬(记录,避免以后重复讨论)**:88 的收杆过冲从 12.9~15.2° 降到 3.18°,**决定性改动是 yaw 速度环 `MaxOut` 5000→15000**(刹车力矩打满→不再贴限幅),不是这套跟随律结构。本车 yaw 已切**电流帧**、`YAW_ESC_OUT_MAX=16384` 满量程(见 `DEV-20261117-11`),刹车权限本来就 >88 的等效值,**故本次不动云台 PID**——动了就会影响瞄准与小陀螺,违反"只改跟随模式"。

**已知代价(88 版固有,复刻即复现)**:
1. 前馈无单独限幅 + 总限幅 300:云台快转时 `0.9×gz` 单项顶穿上限 → 三项互顶的 ±300 削顶(`DEV-20261116-09` 已实测:`off` −7.73~+2.81°、wz 撞 −300);
2. 本车云台满杆 `Gyro[2]` 可到 400+°/s,总限幅 300 → 满杆跟随约 25% 缺口("推满杆追不上");
3. 阻尼固定 0.4(现行静止档 0.9),静止残差与末端摆会变大。

**验证**:`ninja -C build` 全量 clean 重建 **0 error**,第一方警告仅 2 条既有 `-Wswitch`(`chassis.c:253` ZERO_FORCE、`referee_task.c:188` CHASSIS_ALIGN,与本次无关);`tests\run_chassis_tests.ps1` **ALL PASS(0 failures)**;基线审计 `Unchanged 313 / Changed 99 / Missing 48 / Added 35 / RedFlags 1`(与改动前一致)。**未执行**:烧录、实车。

**回滚**:`git revert <本提交>` 一次恢复 11-17/11-18 的全套跟随环(位置项限幅 + 前馈限幅 + 残差伺服 + 阻尼调度 + 400 上限)。

**AI**:DeepSeek Harness (deepseek-flash),2026-11-18.

## DEV-20261119-01｜2026-11-19｜左中挡接入 AX650 自瞄，左下挡保留手动云台

**目标与行为**。用户明确授权修改控制逻辑。左拨杆中挡切换 AX650 自瞄，直接使用 USART1 `0x0503` 下行命令中的世界系 yaw 与 IMU pitch 目标角。左拨杆下挡退出自瞄，保留左摇杆的 yaw/pitch 增量控制。自瞄挡硬性禁用发射，协议 `shoot` 字段与遥控拨轮均不能启动摩擦轮或拨弹；手动挡保留原拨轮发射逻辑，左上挡急停与锁存语义保持不变。

**接口与调用链**。`ax650_uart1.c` 沿用已独占的 USART1 921600 8E1，不重复注册串口。DMA/IDLE 回调把字节流交给 `ax650_pose.c`，解析 RMCV `0x0503` 的 30 字节载荷并处理分片、粘包、CRC 错帧与重同步。最近有效命令以中断保护快照提供给 `RobotCMDTask()`。左中挡同时将姿态上报 `ProgramMode` 切到 `AX650_AUTO_AIM_MODE=1`，其他挡恢复 `AX650_TX_MODE=0`。命令有效期为 `AX650_COMMAND_TIMEOUT_MS=100`，超时或 `target_id=0` 时保持上一云台目标，不把 AX650 的 `shoot` 字段接入发射机构。

**修改位置**。`application/cmd/robot_cmd.c`、`config/robot_infantry_2026.h`、`modules/master_machine/ax650_pose.c/.h`、`modules/master_machine/ax650_uart1.c/.h`、`README.md`，并扩展 AX650 与 CMD 主机测试。`tests/run_chassis_tests.ps1` 的两条中文注释改成 ASCII，修复 Windows PowerShell 5 对无 BOM UTF-8 脚本的解析失败，测试逻辑未变。

**验证证据**。`tests/run_ax650_tests.ps1` 在 O2 与 Ofast 两档全部通过。覆盖姿态发送、模式切换、下行命令新鲜度、分片、粘包、CRC 错帧恢复及序号回绕。`tests/run_chassis_tests.ps1` 全部通过，新增左中自瞄、左下退出自瞄、摩擦轮默认开启断言。`cmake -G Ninja -B build` 与 `ninja -C build` 成功，0 error。本次修改文件 0 warning。构建仍报告两条既有第一方枚举 switch warning，以及四条 newlib 系统调用链接 warning。ELF SHA-256 为 `A1D81D579974680D55D6E17113244736CB3871705130047E910C42FE1746E619`，FLASH 132600 B，RAM 107784 B。

**审计与遗留**。仓库快照 `LICENSE` 为 LF，manifest 记录 CRLF 哈希，直接审计会报 snapshot corrupted。本次仅在临时 skill 副本恢复 CRLF 后运行只读审计，工程文件未改。结果为 Unchanged 313、Changed 99、Missing 48、Added 36、RedFlags 1。唯一红旗仍是已裁决的 `modules/imu/ins_task.c while(1)` 启动期 fail-fast。未烧录、未转动实车。`0x0503` 的角度符号与本车 IMU 坐标仍需台架核对，若方向相反，应在 AX650 输出坐标配置或集中配置宏修正。

**AI**。Codex GPT-5.6，2026-11-19。

## DEV-20261119-02｜2026-11-19｜遥控摩擦轮改为默认关闭、拨轮触发

**目标与行为**。用户修正要求为默认关闭摩擦轮。手动遥控模式空闲时发布 `FRICTION_OFF`；拨轮下压进入扳机区时发布 `FRICTION_ON`，短拨松手仍登记单发，按住不少于 250 ms 仍为连发。拨轮松开后保持摩擦轮 2500 ms 再自动关闭，为单发预转和发射机构最多 2000 ms 的等待到速留出余量。AX650 自瞄挡只控制云台目标，整个发射链强制关闭；左上急停、左中自瞄、左下手动云台语义不变。键鼠右键控制摩擦轮的逻辑不变。

**配置与安全**。新增 `DIAL_FRICTION_KEEP_MS=2500.0f`，标为 `[理论]`、待实车验证。机器人不处于 `ROBOT_READY` 时清除拨轮扳机和摩擦轮保持状态，避免急停或重新解锁后因旧计时自动恢复摩擦轮。

**验证证据**。`tests/run_chassis_tests.ps1` 全部通过，覆盖手动挡空闲默认关闭、视觉挡协议 `shoot=3` 仍关闭发射、视觉挡拨轮下压仍关闭发射，以及 AX650 yaw/pitch 目标跟随。`tests/run_ax650_tests.ps1` 在 O2 与 Ofast 两档全部通过。`cmake --build build --clean-first --parallel 8` 全量编译成功，0 error；保留两条既有枚举 switch warning和四条 newlib 系统调用链接 warning。ELF `build/basic_framework.elf` SHA-256 为 `52B45FF287363D134519A4B84655E155D690030059B7B3D242F4EF8BBE1EE0BA`，FLASH 132740 B，RAM 107792 B。基线审计脚本因仓库现有 `LICENSE` 快照哈希损坏而退出，未把该结果作为代码正确性证据。

**审计与遗留**。仓库快照 `LICENSE` 的换行与 manifest 不一致，直接审计仍会报 snapshot corrupted；使用仅修正换行的临时 skill 副本执行最终只读审计，结果为 Unchanged 313、Changed 99、Missing 48、Added 36、RedFlags 1。唯一红旗仍为已裁决的 `modules/imu/ins_task.c while(1)` 启动期 fail-fast。未烧录、未转动实车。2500 ms 保持时间和摩擦轮实际到速时间需由用户在台架确认。

**AI**。Codex，2026-11-19。

## DEV-20261119-03｜2026-11-19｜枪口热量守卫：估计器 + 两级闸门 + 断流降级

**目标与行为**。任何时候都不越过枪口热量上限 Q0：越过即锁枪并断图传，且解锁要等热量归零（`Q0/c` 量级，典型 6 s）；越过 Q0+100（17mm）为整局永久锁，窗口只有 10 发。新增 `application/shoot/heat_guard.c`，在发射链路唯一决策点回答两件事：**能不能发**（NORMAL / SINGLE / HOLD 三级闸门）与**该发多快**（按余热预算的射速律，稳态收敛到 `c/10`）。

**控制结构（与"热量高了就减速"的根本区别）**。裁判 0x0202 的 ΔQ 只用于校正热量估计 Q̂，不直接参与射速决策；射速由 Q̂ 前馈算出。原因是该对象死区 100~200 ms 且停扣瞬间仍有 1~2 发在膛，反馈必然过冲且调参消不掉。

**估计器四条规矩**。①记账点前移到"放行"瞬间（单发在闸门内部、连发在拨盘槽沿过一发角时），不等裁判确认；②冷却按 100 ms 阶梯扣 `c/10`，不用连续 dt 积分，否则每周期开头系统性低估最多 `c/10`；③不对称校正：裁判值更高立刻跟随，更低需连续 3 帧（300 ms）才跟随；④Q0 与 c 每帧从 0x0201 重取，不缓存（升级瞬间上限跳变）。判"新帧"用新增的 0x0202 帧号，不能比数值。

**余量与闸门**。连发余量 `HEAT_GUARD_MARGIN_BURST=30` 点（≈3 发，6 发/s 下 500 ms 在途），单发 `HEAT_GUARD_MARGIN_SINGLE=15` 点（角度环待发数确定，最多 1 发在途）——"到上限只允许单发"因此不是纯保守，而是**买回 15 点可用热量**。两级阈值 `Q0-30` / `Q0-15`，退出各带 5 点回差；余量按 `Q0×0.25` 封顶，避免低等级 Q0 很小时把枪封死。

**接口与调用链**。新增 APP 模块 `heat_guard.c/.h`，不碰电机与硬件句柄。`modules/referee/referee_task.c` 增加只读访问器 `RefereeGetInfo()`；`rm_referee.h/.c` 增加 `PowerHeatTick` 帧号。`shoot.c`：`HeatGuardUpdate()` 放在任务首行（停火时冷却照常推算）；连发射速改为向 `HeatGuardAllowBurst()` 申请，并在槽沿过一发角时 `HeatGuardCommitShot()`；单发/三发起步前过 `HeatGuardAllowSingle()`，三发在 SINGLE 态降级为一发，HOLD 态丢弃未起步的待发弹（已起步的那发不撤，避免停在槽中间压弹）。

**失效与降级**。从未收到有效 0x0201 → OFF 全放行（台架无裁判时不存在热量，不能因此让枪不响）；曾上线但 0x0202 断流超 300 ms → DEGRADED，禁止花余热、射速压到 `c/10`、单发带间隔。二者判据不同，不可混为一谈。

**修改位置**。`application/shoot/heat_guard.c`、`heat_guard.h`（新增）；`application/shoot/shoot.c`；`config/robot_infantry_2026.h`（新增"枪口热量守卫"段 14 个宏）；`modules/referee/referee_task.c/.h`、`rm_referee.c/.h`；`Makefile`（登记 `heat_guard.c`）；`.Doc/热量管理设计.md`（新增）。CMake 为 GLOB，新增文件需重跑 `cmake -B build` 才会进构建。

**验证证据**。`ninja -C build` 增量编译 0 error，`heat_guard.c` 与 `shoot.c` 均为 0 warning（仅剩既有第一方 `-Wswitch` 与 newlib 系统调用链接告警）。FLASH 130092 B（+2972 B），RAM 106416 B（+40 B）。ELF SHA-256 `0818B598432EE78B3B5E8BD1B27F14CDE379974B4B3BEE1387122E318B82E235`。基线审计 `Unchanged 312 / Changed 100 / Missing 48 / Added 38 / RedFlags 2`；两条红旗为 `application/gimbal/gimbal.c` 的 huart6 与 `modules/imu/ins_task.c` 的启动期 fail-fast，均不在本次修改文件内。**未执行**：烧录、台架、实车；未做主机单元测试（本模块依赖 `RefereeGetInfo()` 与 DWT，主机侧需桩，未搭）。

**遗留与下一步**。四项待实测：`T_commit`（槽沿→ΔQ 时差，用于收紧 30 点余量）、冷却速率自辨识（校验 `cooling_buff` 是否直加，`HEAT_GUARD_COOLING_USE_BUFF`）、单发实际耗时、双轮转速跌落阈值（卡弹交叉判据尚未实现）。**弹速锁（V1 > V0）仍未守**：V0 在 0x0201 `shooter_speed_limit`、V1 在 0x0207 `bullet_speed`，协议层已解析但应用层无人使用，下次单独开条目。公开实现对照见 `.Doc/热量管理设计.md` §8。

**AI**：DeepSeek-V4.1-Flash，2026-11-19。

## DEV-20261120-01｜2026-11-20｜0x0202 功率字段实名化 + 判别式保护 + RTT 落盘工具 + tools/文档清理

**目标与行为**。三件事:①把裁判 0x0202 里那三个"读不到"的字段变成可读的底盘功率;②补回功率控制里缺失的判别式保护;③建立"上电解锁即开始、下电即结束"的 RTT 日志落盘通路。另外按用户要求删除了全部会暂停核心的读参脚本与过期计划文档。

**0x0202 字段实名化**。`ext_power_heat_data_t` 的三个 `reserved` 改为 `chassis_voltage`(mV)/`chassis_current`(mA)/`chassis_power`(W, float),布局不变(2+2+4+2+2+2 = 14 B),`LEN_power_heat_data` 与 `sizeof` 均为 14,已加 `_Static_assert` 作布局守卫——协议再变时编译失败优于静默错位。**注意这不是"新增解析"**:字节一直在 memcpy,只是没人读该字段。

**判别式保护**。`power_control.c` 补回 XJTLU 原版 `if (inside < 0) continue`(跃鹿移植时漏掉)。目标功率低于该转速下的损耗地板 `k2·ω² + constant` 时二次方程无实根,`sqrt(负数)` = NaN,强转 `int16_t` 是未定义行为。`continue` 跳过的是反解电流那一步,`power_control_out[i]` 保留 PID 原始输出——无实根意味着"这一档功率物理上达不到",退回 PID 比发随机数合理。

**模块禁则**。`power_control.h` 写明:本模块自带 `static CANInstance sender_assignment[6]`,与 `dji_motor.c` 同 ID 双发帧,**禁止同时启用**;启用须全部改 `PowerControlInit()` 且不再调 `DJIMotorControl()`。当前 `PowerControl()` 仍为零调用点,本次不接线。

**RTT 落盘通路**。新增 `tools/rtt_log.py`:常驻一个 OpenOCD,telnet 4444 周期读控制块与环形缓冲,增量写 `log/<时间戳>.log`。会话起点 = 固件解锁标记 `[CMD] reinstate, robot ready`,终点 = 目标下电(连续 3 次读失败)或 Ctrl-C。三个关键点:①**每轮必须回写 `RdOff`**,否则目标端认为缓冲满,`NO_BLOCK_SKIP` 会把后续日志全丢;②落盘剥 ANSI 颜色码;③`--loop` 常驻 + 占住本地 4456 端口做单实例互斥。配套 `capture_rtt.bat`(纯 ASCII,自动找 python)与 `stop_capture.bat`(`netstat` 按 4456 取 PID,并清理残留 `openocd.exe`——`taskkill /F` 时 `finally` 不执行,孤儿 OpenOCD 会继续占探针)。

**日志扩列**。`[chcal]` 追加 `p=`(底盘功率 W)与 `be=`(缓冲能量 J);`[shootcal]` 追加 `icl/icr`(双摩擦轮速度环输出 = 电流指令码,上限 6000 码≈7.3A)与 `ifl/ifr`(电调回传实际电流码)。指令与反馈分列,两者长期不一致即电调没吃满指令。

**配置切换**。`YAW_AUTO_VOFA_USART6_ENABLE` 1→0、`CHASSIS_CALIB_LOG` 0→1。CubeMX 只初始化 3 路串口(USART1=AX650、USART3=DBUS、USART6=裁判),VOFA JustFloat 与裁判系统抢同一路;置 1 时 `UITaskInit(&huart6)` 被编译掉,`[chcal]` 的 p/be 恒为 -1,且**热量守卫恒为 OFF 直通(枪口无保护)**。

**删除(tools/)**。`read_motor.js` / `read_chassis.js` / `read_yaw.js` / `mem_read.js` / `sample_scalars.js` / `monitor_motors.py`——全部走 `variables.read` / `memory.read` / gdb halt,会暂停核心 → motor_task 停发 CAN → C620 约 100ms 无帧 → 信号丢失保护输出 0,车运动中会突然顿一下;且 `variables.read` 有"偶尔不恢复"的历史(`doctor.js:293`)。只有 `variables.sample` 是非侵入的。保留 `odom.js`/`doctor.js`/`pitch_calib.js`/`flash_manual.ps1`(同样会暂停,但定位是位移测量/体检/标定/烧录,前提本就是静止)。**能力缺口**:四轮温度与电流从此没有不打断的读法(`[chcal]` 只有整体 p/be,`[shootcal]` 只覆盖摩擦轮与拨盘)。

**删除(.Doc/,11 份)**。`底盘测试手册`、`底盘首测步骤卡`(依赖的读数脚本已删,步骤不可执行)、`跟随与小陀螺优化计划`(实测无问题)、`装机后总计划书`(总计划书自称旧版)、`Bug_Report`、`TODO`、`版本更替`、`架构介绍与开发指南`(与 `架构与开发指南` 重复)、`让VSCode成为更称手的IDE`(被 `VSCode+Ozone使用方法` 取代)、`合理地进行PID参数整定`(与 `PID参数整定` 重复)、`工程审查报告-20261030`(一次性审查)。同时修正 `总计划书`(3 处)、`AGENTS.md`(1 处)与四份各轴计划书(5 处)指向已删文档的指针,其中 5 处 `.Doc/装机后总计划书.md` 改指 `.Doc/总计划书.md`。保留 yaw/pitch/底盘/发射四份计划书——它们被 6 处代码注释当作技术依据引用(`chassis.c:416`、`gimbal.c:105`、`gimbal.c:516`、`dji_motor.c:368`、`robot_infantry_2026.h:669`、`tools/gimbal_trace.js:7`)。

**文档合并**。`.Doc/注释重写规划.md`(2026-11-09)整份并入 `.Doc/注释规范.md`,原文件删除。去向:五个问题与"保留决策现场"的判据 → §0;三条不变量与五道闸门 → §11(其中 `tests/check_comment_style.py` **实际未实现**,已在文档内如实标注,闸门 3 暂由人工自检替代);分层覆盖表与批次进度 → §12(实测第一方 `.c/.h` 共 137 个、126 个已有文件头,余 11 个为 3 个 0 行空壳 + 8 个未接入模块);风险对策 → §13;完成判据 → §14。`README.md` 文档索引里原本的两行合并为一行,指向 `注释规范`。

**新增文档**。`.Doc/RTT日志与调试通道.md`:RTT 配置参数表(10 项带行号)、带宽预算(三条日志约 370 B/s,1024 B 缓冲零排空可撑 2.8 s)、全部日志行的字段字典、开关矩阵、落盘与自启/关闭流程。

**验证证据**。`ninja -C build` 0 error;仅剩既有第一方 `-Wswitch`(未处理 `CHASSIS_ALIGN`/`CHASSIS_ZERO_FORCE` 枚举)与 newlib 系统调用链接告警。FLASH 138692 B / 13.23%,RAM 107920 B / 82.34%。ELF SHA-256 `3786ADFC952E2445887BE41F9C4868B8C30591E5551A5EBEBA0D41856B7E618E`。`rtt_log.py` 已实测:参数解析、`mdw` 输出解析(含跨行)、ANSI 剥离、解锁标记匹配、无探针时快速失败、`--loop` 重试与 60 s 节流、单实例锁拒绝第二进程、`stop_capture.bat` 端到端。**未执行**:烧录、台架、实车;`rtt_log.py` 未在真实探针+目标上跑通。

**遗留**。①RTT 落盘需真机验证(探针必须空闲);②四轮温度/电流的替代读法:建议给 `[chcal]` 加四轮温度+电流列,或单开 1Hz `[wheel]` 日志(不碰调用链、零停顿);③`variables.sample` 与非侵入性的关系需实测确认——`read_motor.js` 记录的"LTO 后 DWARF 不可用"经 `readelf` 核对不成立(布局完整:464 B,`feedCnt@452`、`dt@456`),真正的限制在桥接侧解析,但四个 `*_trace.js` 仍在用 `->` 表达式,需一次 `variables.sample ['motor_lf','motor_lf->feed_cnt']` 对照来定案;④热守卫四项待实测与弹速锁(V1>V0)仍未做,见 DEV-20261119-03。

**AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-02｜2026-11-20｜台账与代码一致性复核：5 处过期值 + 1 处夹带改动

**触发**。补写 `.Doc/开发史.md` 第十一~十五章时，需要把 11-17~11-20 的参数变更落到附录 C，逐个回代码核对时发现 `.Doc/参数证据表.md` 有 5 行与当前 `config` 不符。

**改正的 5 处**（依据 git blame + 提交内容，非记忆）：

| 参数 | 证据表原值 | 实际值 | 变更出处 |
|---|---|---|---|
| `CHASSIS_FOLLOW_DAMP` | 0.9 | **0.4** | 11-17 试过 0.9/调度/三折点，11-18 `8280f6c` 整段回退后全部移除 |
| `YAW_INERTIA_FF_GAIN` | 1.875 | **3.5** | DEV-20261117-12，`6c6a7f6`（55 次打杆统计） |
| `YAW_INERTIA_FF_ALPHA_MAX` | 1000 | **800** | 同上 |
| `YAW_FRICTION_FF_B` | 15.0 | **12.0** | 11-17 电流域重标 15→8.7，`32f21b0` 回 12 |
| `YAW_SPEED_KP_HIGH` | "20(电流)/40(电压)" | **40** | 宏已写成 `(YAW_ESC_CURRENT_MODE ? 40.0f : 40.0f)`，两支同值 |

**一处夹带改动（本次最值得记的）**。`YAW_CHASSIS_ALIGN_ECD` 的变更史：

```
3564  (5ebd8ae, 11-05 入库)
5892  (9b0e152, 11-15 云台装机后重标)
3564  (d85fd44, 11-17 回退到 11-05 基线)
1835  (33dd6e8, 11-17 18:17)  ← 当前
```

`33dd6e8` 的提交信息只写"跟随阻尼 0.9->0.4，回到 1027 稳定值"，**没有一个字提到 yaw 对齐零位**；同一提交的 `config` diff 里却把该宏从 3564 改成 1835（相差 −1729 码 ≈ −76°），且源码注释仍写 `[实测 2026-10-28]`。这不是"文档抄错"，是**一次参数变更没有留下任何可检索的记录**。

**为什么必须记下来**：这个宏决定"yaw 对准底盘正前方"的物理朝向，是跟随律 `offset` 的零点。它被埋在无关提交里，等于把一条标定结论变成了化石——三个月后没人能从 `git log` 或 DEV` 号找回它的依据。纪律：**改标定类宏必须单独一条 DEV，哪怕只改一个数**。

**修改位置**。`.Doc/参数证据表.md`（5 行改值 + 新增 2026-11-20 复核声明：config 已从 85 个宏增至 154 个，本次未对新增 69 个逐条评级，§0 统计仍是 11-09 口径）；`.Doc/开发史.md` 附录 C（改正该行出处，此前误记为 `8280f6c` 所致）。

**验证证据**。纯文档改动，未编译（不影响固件）。核对手段：`git show <commit>:config/robot_infantry_2026.h`、`git log -G "^#define <MACRO> [0-9]"`、逐项 `grep` 当前 config。

**未执行 / 待确认**。**`1835` 是否为有意重标，需用户确认**。若它并非有意，则 yaw 对齐零点存在约 76° 偏置——但 11-17 之后实车跟随/回正均正常，倾向于认为是有意改动（例如拆装过 yaw 传动）。在确认前不回改任何值。

**AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-03｜2026-11-20｜重构 R0：零行为变化闸门（六项可执行证据）

- **目标与触发**：用户批准 `.Doc/plan/重构总方案.md` 的 R0——把"零行为变化"从口头承诺变成可执行闸门。本批**不碰任何源码**。
- **功能变化**：无。只新增只读校验工具。
- **文件与符号**：新增 `tools/refactor_gate.py`（G1 宏集合逐字节、G2 编译、G3 主机测试、G4 基线审计、G5 逐符号机器码尺寸、G6 纯搬家行多重集、`check-links` 路径校验）；`AGENTS.md` Checks 段加入闸门命令与 PowerShell 5.1 编码坑提示；无 `.c/.h` 改动。
- **接口与调用链**：无。
- **配置/API/协议影响**：无。
- **验证（实测锚点，全部来自本机）**：干净重建后 `save-baseline` + `check` 全 PASS；`check-links` PASS。宏 **557** 条、符号 **1215** 个、RAM **82.34%** / FLASH **13.23%**（text 110904 / data 27780 / bss 80144）、审计 305/460 未变 / 101 改 / 54 缺 / 38 增 / 2 红旗、主机测试 chassis+ax650 全 PASS。
  - **可复现性已证**：同一树连续两次干净重建，`nm -S` 逐符号尺寸**完全一致**（1215 个，text/data/bss 差 +0）。这是 G5 可信的前提。
- **发现（未改动，留给后续批次）**：①构建体系漂移——`Makefile` 带 `-DDISABLE_LOG_SYSTEM` 而 `CMakeLists` 不带，两者**共用 `build/`**（同名 elf/map 互相覆盖）；`.vscode/tasks.json` 仍用 `mingw32-make` 烧录，而本会话 PATH 中无 `mingw32-make`，实际可用的编译路径是 CMake+Ninja。②`tests/build/shoot_ctrl_test.exe` 无对应源码，是遗留产物。③`bsp_log.h` 的 `#if DISABLE_LOG_SYSTEM` 意味着**若真用 make 烧录，RTT 日志会被整体编译掉**——与 DEVLOG 里大量依赖 `[heat]/[chcal]` 日志的判据矛盾，需在 R6 一并裁决。
- **遗留**：Makefile/CMake 漂移与 `build/` 共用属 R6（改构建文件为结构级，需单独批准）。
- **下一步**：R1 文档体系。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-04｜2026-11-20｜重构 R1：文档体系重组（五类目录 + 索引）

- **目标与触发**：R1；消除 `.Doc` 18 份平铺、重复内容与悬空引用。纯文档改动，零代码。
- **功能变化**：无（未重新编译）。
- **文件与符号**：`git mv` 18 份 `.md` → `plan/`（总计划书、重构总方案）、`spec/`（架构与开发指南、注释规范、必须做&禁止做、本车调参结论、参数证据表、热量管理设计、PID参数整定）、`detail/`（四份轴计划——仍是"技术细节库",不进 history）、`guide/`（如何定位bug、RTT日志与调试通道、VSCode+Ozone使用方法、团队协作指南、嵌入式教学大纲）、`history/`（开发史，只读归档）。新增 `.Doc/README.md` 索引。全仓 `.Doc/` 引用改为带目录路径：`AGENTS.md`、`README.md`、`config/robot_infantry_2026.h`、`chassis.c`、`gimbal.c`、`heat_guard.h`、`dji_motor.c`、`tools/capture_rtt.bat`、`tools/gimbal_trace.js`。修悬空引用 2 处：`底盘优化计划.md` 指向已删的《底盘测试手册》、`tests/verify_comments_only.py` 指向已并入注释规范的《注释重写规划》。RM_DEVLOG 只改 §0/§2/§5 的活指针，**§6 历史 DEV 条目保留原路径**并在 §0 说明。
- **接口与调用链**：无（只改注释里的路径字符串）。
- **配置/API/协议影响**：无。`gcc -E -dM` 宏集合 **557 条逐字节一致**；ELF 逐符号尺寸 **1215 个完全一致**（text/data/bss +0）——证明注释路径修改没有产生任何代码变化。
- **验证**：`check --expect-audit unchanged=303,changed=100,missing=57,added=38,redflags=2` 全 PASS；`check-links` PASS；主机测试全 PASS。
- **审计计数为何变化**：被移动的 3 份文档（`必须做&禁止做.md`、`如何定位bug.md`、`VSCode+Ozone使用方法.md`）本身在上游基线里，移动后旧路径计入 Missing（54→57），故未变 305→303、改 101→100。**这是一次性预期变化，已重锚基线**；之后批次应回到"计数不变（除本批声明的新增文件）"。
- **未改动（有意）**：`.agents/skills` 与 `assets/yuelu-basic-framework` 上游快照**一个字节没动**（快照须逐字节等同上游，manifest 校验）；`参数证据表.md` §0 仍写"85 个宏"（实际 154）——改正需逐条评级，不属结构重构，留给内容批次。
- **下一步**：R2 参数配置拆分（待批准）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-05｜2026-11-20｜重构 R2：参数配置按轴拆分（154 宏零改动）

- **目标与触发**：R2；`config/robot_infantry_2026.h` 已长到 823 行 / 154 宏 / 18 节，一个"唯一改动点"变成难查的大抽屉。拆成伞头 + 7 个分轴头。
- **功能变化**：无。**只搬行，不改任何宏的名字、数值与注释文字**。
- **文件与符号**：
  - `config/robot_infantry_2026.h`：823 行 → 28 行伞头（文件名与路径不变，故 4 个 include 点零改动：`robot_def.h:15`、`robot_task.h:16`、`heat_guard.c:16`、`tests/ax650_stubs/robot_def.h:8`）。
  - 新增 `config/robot_infantry_2026/`：`board.h`(21) `chassis.h`(170) `gimbal.h`(312) `ax650.h`(69) `shoot.h`(129) `heat.h`(80) `kbm.h`(94) `protect.h`(17)。
  - 拆分方式：按原文 18 个小节的行区间整段搬运（`build/_split_config.py` 一次执行，脚本自检"154 宏全部分配、0 未分配、0 重复"）。
- **接口与调用链**：无。业务代码仍只 include 伞头；子头之间不互相 include（宏在包含点展开，顺序不敏感，故不存在前向引用问题）。
- **配置/API/协议影响**：
  - `gcc -E -dM` 宏集合 **557 条逐字节一致**（G1）。
  - ELF **1215 个符号尺寸完全一致**，text/data/bss 差 **+0** —— 配置拆分连机器码都没动。
  - 协议布局守卫不受影响（`_Static_assert` 仍在 `robot_def.h`，未改）。
- **验证**：`check --expect-audit unchanged=303,changed=100,missing=57,added=46,redflags=2` 全 PASS；主机测试 chassis+ax650 全 PASS。审计 Added 38→46，恰为新增 8 个头文件，符合声明。
- **未改动（有意）**：`CALIB_TRIGGER_ROCKER_L1`/`CALIB_TRIGGER_HOLD_MS` 仍留在 `gimbal.h`（原文位置在"云台零位与限位"节内，实际语义属 cmd/calib）；跨节搬迁会改变注释的组织语境，留给后续内容批次，不在零行为变化批次里做。
- **下一步**：R3 拆 `robot_cmd.c`（唯一有主机测试兜底的大文件）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-06｜2026-11-20｜重构 R3：robot_cmd.c 拆分（15 个函数体逐字符一致）

- **目标与触发**：R3；`robot_cmd.c` 903 行里混了 11 个内聚簇（仲裁/输入源/键鼠/遥控映射/AX650 滤波/回中/标定/Init/遥测/编排/对外读数），是最大的单文件，也是**唯一有主机测试兜底**的大文件——先用它验证"搬家 + 闸门"方法论。
- **功能变化**：无。搬走的函数体**逐字符一致**，只去掉 `static` 关键字。
- **文件与符号**：
  - `application/cmd/robot_cmd.c`：903 → 260 行；保留 状态定义 / `RobotCMDInit` / `RobotCMDTask` 编排 / `CalcOffsetAngle` / `CalibTrigger` / `RobotCMDGetAx650RawYaw`。
  - 新增 `cmd_authority.c`(121)：控制权枚举 + `AuthorityArbitrate` + `ApplySafeOutputs`（保持 static）。
  - 新增 `cmd_remote.c`(225)：`RemoteControlSet`（函数内 static 状态随函数整段搬迁）。
  - 新增 `cmd_kbm.c`(198)：`UpdateInputSource` + `KeyboardMouseSet`（`last_*_count`/`kbm_*` 状态私有）。
  - 新增 `cmd_autoaim.c`(206)：`YawAutoKalman*` + `YawAutoSmooth*`（滤波状态私有）。
  - 新增 `robot_cmd_internal.h`(46)：跨 TU 的 extern 声明（**过渡手段**，未包 setter，避免改变读写时序）。
  - `Makefile` 登记 4 个新 .c；`tests/run_chassis_tests.ps1` 源文件列表同步。
- **接口与调用链**：`robot_cmd.h` 三个对外 API 不变；消息话题与发布点不变（仍由 `RobotCMDTask` 统一发布）。**16 个符号由 static 变 extern**（5 个函数 + 9 个共享状态 + 输入源），是拆分类改动的确定代价。
- **配置/API/协议影响**：无。宏集合 562 条（15.3.1 工具链口径）逐字节一致；协议结构体与 `_Static_assert` 未动。
- **验证（逐项）**：
  - G1 宏：562 条逐字节一致；G2：0 error；G3：chassis + ax650 主机测试全 PASS。
  - G5 符号：**1215 个符号名与基线一致**；16 个仅链接性变化（已声明）；2 个函数尺寸变化 `KeyboardMouseSet 996→1012B`、`RemoteControlSet 976→992B`（变成 extern 后失去内联机会，指令序列不变）。
  - 尺寸：text 110936 (**+32 B，+0.029%**)、data 27776(−4)、bss 80144(+0)。
  - **源码级证明**：`build/_body_cmp.py 49145fe` 逐一比对 15 个函数体（含留在原文件的 5 个），结果 **ALL SAME**（只允许去 `static`）。
  - 审计：303/460 未变、100 改、57 缺、**51 增**（+5 = 4 个新 .c + 1 个新 .h），2 红旗。
  - 反汇编逐函数比对**不可用**：`-flto` 全程序优化下，拆分改变了内联与段布局，763 个函数里 268 个指令序列"不同"（含与本次改动无关的 CAN 中断处理）——该手段在 LTO 工程里不构成证据，已放弃。
- **顺带发现（未改功能）**：①本机有**两版 arm-none-eabi-gcc**：`build/CMakeCache` 用的是 `C:/Toolchain/bin` 的 **15.3.1**，PATH 上是 **14.2.1**，两者 text 差约 600 B；闸门已改为从 CMakeCache 解析实际编译器，不再受 PATH 影响。②`cmake` 不在 PATH，需用 STM32Cube bundle 的绝对路径（已写进 `AGENTS.md`）。③`git worktree` 检出的树跑审计会报 `snapshot corrupted: LICENSE`（换行符归一化所致），故工作树只用于编译比对，审计必须在主工作区跑。
- **遗留**：`robot_cmd.c` 拆块处残留几行连续空行（无害，待清理）；内部头的 extern 是过渡手段，真正解耦属功能批次。
- **下一步**：R4a 云台/发射/底盘纯文件搬家（无主机测试，靠符号 + 逐函数源码比对 + 一次实车回归）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-07｜2026-11-20｜重构 R4a：云台/底盘可独立函数搬出（8 个函数体逐字符一致）

- **目标与触发**：R4a；先做"不带出状态纠缠"的那部分搬迁，验证拆分在**没有主机测试**的文件上依然可控。`shoot.c` 经评估**没有可干净独立的函数**（`FrictionReady` 只被 `ShootTask` 用、读两个电机实例与命令结构），不动，留到 R4b。
- **功能变化**：无。搬出的函数体逐字符一致，只去掉 `static`。
- **文件与符号**：
  - `chassis.c` 443 → ~370 行；新增 `chassis_kinematics.c`：轮系几何宏（`HALF_*`/`PERIMETER_WHEEL`/`WHEEL_RATIO`/`LF..RB_CENTER`）+ `RampTo` / `MecanumCalculate` / `LimitChassisOutput` / `EstimateSpeed`。
  - 新增 `chassis_internal.h`：几何宏 + 9 个共享状态 extern + 4 个函数声明（运动学与 `ChassisTask` 共用同一份几何，避免"两处各写一份、几何一改静默走偏"）。
  - `gimbal.c` 598 → ~530 行；新增 `gimbal_recenter.c`：`GimbalGetPitchEcdDeg` / `GimbalGetPitchTotalAngle` / `GimbalSavePitchHorizonZero` / `GimbalPitchRecenterTarget`；新增 `gimbal_internal.h`（`pitch_motor`、`gimba_IMU_data` 两个 extern）。
  - `Makefile` 登记 `chassis_kinematics.c`、`gimbal_recenter.c`。
- **接口与调用链**：对外头文件（`chassis.h`/`gimbal.h`）与调用点零改动；话题与任务不变。**35 个符号**由 static 变 extern（本轮 +19：`motor_*`/`vt_*`/`est_*`/`chassis_vx/vy`/`chassis_cmd_recv`/`pitch_motor`/`gimba_IMU_data` + 8 个函数）。
- **配置/API/协议影响**：无。宏 562 条逐字节一致（几何宏只是搬到内部头，数值与展开结果不变——G1 验证）。
- **验证（逐项）**：
  - G1 562 条逐字节一致；G2 0 error；G3 主机测试 chassis + ax650 全 PASS。
  - G5：**1215 个符号名与基线一致**；35 个仅链接性变化；4 个尺寸变化 `ChassisTask 1320→1340B`、`MecanumCalculate 168→172B`、`KeyboardMouseSet 996→1012B`、`RemoteControlSet 976→992B`（都是失去内联机会的代价）。
  - 尺寸：text 110960（**+56 B，+0.050%**）、data 27776(−4)、bss 80144(+0)。
  - **源码级**：`build/_body_cmp2.py cc862d2` 比对 8 个搬出的函数体，结果 **ALL SAME**。
  - 审计：303/460 未变、100 改、57 缺、**55 增**（+4 = 2 个新 .c + 2 个新 .h）、2 红旗。
- **既有告警（非本轮引入）**：`chassis.c` 的 `switch(chassis_mode)` 未处理 `CHASSIS_ZERO_FORCE`（`-Wswitch`）；`referee_task.c` 未处理 `CHASSIS_ALIGN`。CMake 未开 `-Werror`，故不阻断编译；是否补 `default` 属功能/防御性改动，按功能冻结原则不动。
- **遗留**：`gimbal.c` 的 `GimbalTask`（193 行）与 `shoot.c` 的 `ShootTask`（321 行）仍是大函数，只能靠 R4b 的块抽取；`chassis.c` 的 `ChassisTask`（202 行）同理。
- **下一步**：R4b（Task 内块抽取）——**建议先只在 `chassis.c` 做一块**验证方法论，且需用户先定"不补测试 + 一次实车回归"还是"先补主机测试"。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-08｜2026-11-20｜重构 R5a：切断 modules → application 反向依赖（text +0）

- **目标与触发**：R5 第一步。盘点到的事实：**referee 全家桶 include 了 `application/robot_def.h`，却零符号、零 config 宏使用**（纯未使用 include）；另外 4 个模块只用 config 宏。
- **功能变化**：无。
- **文件与符号**：
  - 删除未使用 include（6 处）：`referee_task.c/.h`、`referee_custom.h`、`referee_vt.h`、`referee_vt_task.c/.h`。
  - 改为 include config 伞头（4 处）：`calib.c`、`ins_task.c`、`master_process.c`、`ax650_uart1.c`。
  - **保留 1 处（真实依赖）**：`referee/rm_referee.h` 的 UI 交互结构用了 `chassis_mode_e`/`gimbal_mode_e`/`shoot_mode_e`/`friction_mode_e`/`lid_mode_e`/`Chassis_Power_Data_s` 六个 APP 类型，是仅剩的反向依赖。彻底消除需把这些枚举下移到模块可包含的类型头（R5b，需单独批准）。
- **接口与调用链**：无。话题、任务、结构体布局均未动。
- **配置/API/协议影响**：无。宏 562 条逐字节一致。
- **验证**：
  - G1 562 条逐字节一致；G2 0 error；G3 主机测试全 PASS。
  - G5：**1215 个符号名一致、尺寸全同**；`text 110960 (+0)`、`data/bss +0`——**改 include 连一个字节的机器码都没动**，是"零行为变化"最强的形态。
  - 审计：未变 303→301、改 100→102（两个原本与上游基线逐字节相同的文件因删了一行 include 而变成"已改"）、缺 57、增 55、红旗 2。已按此重锚基线。
- **发现（未修）**：`bsp/flash/bsp_flash.c` **不是合法 UTF-8**（偏移 494 处 0xB2），与 DEV-20261116-02 "提交的 18 个文件均为 UTF-8 无 BOM" 的记录冲突。编译不受影响（字节落在注释里），但会破坏任何按 UTF-8 读取的工具；转码属文件级改动，留作单独小批。
- **台账**：`AGENTS.md` 补两条条文——依赖方向（modules/bsp 不得 include application 头，已知例外 rm_referee.h）、硬件句柄口径（Init 注入允许 / 运行期直接访问禁止）。
- **下一步**：R5b（合同头拆分消除最后一处反向依赖 + 死路径清单），需用户批准。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-09｜2026-11-20｜重构 R6：构建/测试/CI 收口

- **目标与触发**：R6。此前三处结构性隐患：①Makefile 是显式源列表、CMake 是 GLOB，两边集合已经漂移（4 个 .c 只被 CMake 编译）；②两套构建**共用 `build/`**，同名 `bare_framework.elf/map/bin` 互相覆盖；③CI 只验证"两种编译能过"，不跑主机测试与静态检查。
- **功能变化**：无。
- **文件与符号**：
  - 新增 `tools/check_source_lists.py`：解析 Makefile 的 `C_SOURCES`，与磁盘上 `Drivers|Src|Middlewares|bsp|modules|application` 下的 `.c` 集合双向比对，漂移即非零退出。
  - `Makefile`：`BUILD_DIR` 由 `build` 改为 **`build_make`**（烧录目标读 `$(BUILD_DIR)`，自动跟随）；登记此前漏掉的 4 个源文件 `bsp/adc/bsp_adc.c`、`modules/TFminiPlus/tfminiplus.c`、`modules/standard_cmd/std_cmd.c`、`modules/unicomm/unicomm.c`（前三个是 0 字节占位，`tfminiplus` 是未接入模块）。
  - `.gitignore`：加 `build_make/`。
  - `.github/workflows/c-cpp.yml`：新增 "host unit tests"（pwsh 跑两个测试脚本）与 "source list & doc links"（`check_source_lists.py` + `refactor_gate.py check-links`）两步。
- **接口与调用链**：无。
- **配置/API/协议影响**：无。宏 562 条逐字节一致。
- **验证**：
  - `tools/check_source_lists.py` → **PASS：138 个 .c 两边一致**（修复前报 4 条漂移）。
  - 闸门全 PASS：G1 一致、G2 0 error、G5 1215 符号名一致、`text 110960 (+0)`、主机测试全 PASS、审计 301/102/57/55。
  - 构建目录分离后，CMake 不再可能被 make 的产物污染——这一条对后续所有闸门比对的可信度是前提。
- **未执行（如实声明）**：①本机 PATH 无 `mingw32-make`/`make`，**make 构建未实测**（改动仅是一个变量 + 4 行源列表，语法由 `check_source_lists.py` 的解析间接验证）；②CI 改动**未在 GitHub runner 上跑过**，`pwsh` 在 ubuntu-latest 默认可用但未验证；③`make` 带 `-Werror`、CMake 不带，两边警告口径仍不一致，未统一。
- **遗留**：3 个 0 字节占位 `.c` 未清理（属上游快照残留，删不删需单独裁决）；CI 未接闸门的 G1~G5（需要 ARM 工具链与基线工件，暂不适合放 runner）。
- **下一步**：R5b（合同头拆分消除最后一处反向依赖 + 死路径清单）或实车回归。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-10｜2026-11-20｜重构后实车回归：运行正常（关掉"实体等价"这一环）

- **目标与触发**：R0~R6（9 个提交、text +56 B）完成后，用户烧录 §9.1 固化的 CMake 产物实车验证。
- **固件身份**：`build/basic_framework.bin` 138,744 B，ELF SHA256 `1b8cec7e4857df9102cf587c07b94f88bc1e35ea8bb9262824450f95f167c21a`，arm-none-eabi-gcc 15.3.1，对应提交 `8c66333`。
- **结果**：**用户确认"运行正常"**——底盘/云台/遥控/解锁流程/发射链路的体感与功能行为与重构前一致，无异常现象报告。
- **证据级别**：用户实车确认（**清单 §9.2 未逐项回报**）。因此第 7 项（热量守卫 `[heat]` 与 SINGLE/HOLD/断流降级）与第 8 项（AX650 自瞄）按"**未单独验证**"记录，不因总体"运行正常"推定通过。
- **意义**：闸门（宏 562 条逐字节、符号 1215 个一致、函数体逐字符、主机测试、text +56 B）只能证明**结构等价**；本次实车回归把结论升级为**实体行为一致**。至此 R0~R6 的"零行为变化"主张闭环。
- **遗留**：R5b（最后一处 modules→application 反向依赖 + 死路径清单）、R4b（Task 内联块抽取，无回归网）、`bsp_flash.c` 非 UTF-8、3 个 0 字节占位 `.c`、两条既有 `-Wswitch` 告警。
- **下一步**：R5b。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。
## DEV-20261120-11｜2026-11-20｜重构 R5b：共享类型下移（反向依赖归零）+ 死路径清理

- **目标与触发**：R5 收尾。R5a 后仅剩 `referee/rm_referee.h` 一处真实反向依赖（UI 交互结构用 6 个 APP 枚举）。
- **功能变化**：无。
- **文件与符号**：
  - 新增 `modules/app_types.h`：`chassis_mode_e` / `gimbal_mode_e` / `shoot_mode_e` / `friction_mode_e` / `lid_mode_e` / `Chassis_Power_Data_s` **逐字搬运**（枚举值即语义，未改一个值）。
  - `application/robot_def.h`：删除上述 6 个定义，改为 `#include "app_types.h"`；布局守卫（`Chassis_Ctrl_Cmd_s == 21`）继续由 `_Static_assert` 把关，编译通过即证明结构尺寸未变。
  - `modules/referee/rm_referee.h`：由 include `robot_def.h` 改为 include `app_types.h`。
  - 死路径：删除 `chassis.c` 的 `#if 0` 超电注册块（从未参与编译）、`gimbal.c` 的空 include `bmi088.h`（该文件不用任何 BMI088 符号）。
- **接口与调用链**：无。类型名/值/语义不变，所有使用者代码零改动。
- **配置/API/协议影响**：无——宏 562 条逐字节一致；**text 110960 (+0)**、data/bss +0；1215 个符号名与尺寸一致。
- **验证**：G1/G2/G3/G5 全 PASS；审计 added 55→56（+1 = app_types.h），其余计数不变。`modules/`、`bsp/` 下已无非注释性的 `application` 依赖。
- **明确保留（有意不删）**：
  1. `gimbal.c` 的 VOFA 死块（`#if YAW_AUTO_VOFA_USART6_ENABLE`）：是**功能开关**，置 1 即启用，不是死代码；
  2. `heat_guard.h` 的 `HeatGuardGetHeat`/`HeatGuardIsDegraded`：未使用的公开 API，属预留；
  3. `chassis_feed`/`shoot_feed` 两条**恒零话题**（`chassis_feedback_data` 从未赋值、`Shoot_Upload_Data_s` 是空结构且订阅方从不读取）：已证明零读者，但它们写在协议合同 `robot_def.h` 里，属预留反馈通道，删除需单独裁决（删除会改变消息中心注册数）。
- **下一步**：R4b（Task 内联块抽取，无回归网，需先定"补测试"还是"实车回归兜底"），或收尾。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。
## DEV-20261120-12｜2026-11-20｜重构收尾：混编码修复 + `-Wswitch` 清零 + 占位文件裁决

- **目标与触发**：R0~R6 + R5b 交付、实车回归通过后的收尾小尾巴。
- **功能变化**：无。
- **文件与符号**：
  - `bsp/flash/bsp_flash.c`：**混编码修复**——文件是 UTF-8 但夹着 GBK 片段（如 `B2 C1 B3 FD` = "擦除"，共 104 处非法位置），按"UTF-8 失败即 GBK 回退"逐段转码，现为纯 UTF-8（7,712 字符 / 7,909 字节）。
  - `chassis.c` 的 `switch(chassis_mode)` 与 `referee_task.c` 的 `switch(chassis_mode)`：补 `default: break;`（`CHASSIS_ZERO_FORCE` / `CHASSIS_ALIGN` 由电机层与底盘侧处理，本处不改行为）。两条既有 `-Wswitch` 告警清零。
  - **占位文件裁决**：先删了 3 组 0 字节占位文件（`bsp/adc`、`modules/standard_cmd`、`modules/unicomm` 的 `.c/.h`），审计随即显示 `Unchanged 301→295`、`Missing 57→63`——**这 6 个空文件本身就在上游基线里**。删除把"相对上游的偏离"变成永久噪声，收益为零，**已恢复**，并在 Makefile 保留其登记。
- **接口与调用链**：无。
- **配置/API/协议影响**：无。
- **验证**：闸门全 PASS——宏 562 条一致、`text 110960 (+0)`、data/bss +0、1215 符号名一致、主机测试全 PASS；`check_source_lists.py` PASS（138 个 .c）；`-Wswitch` 枚举告警 0 条。
- **结项**：R4b（Task 内联块抽取）**决定不做**——它改善的是三个 Task 函数的可读性，不是结构缺陷，且是唯一无法用现有证据证明、还必须再烧一次车的批次（方案 §4 R4b 原本就标注"可整体取消"）。
- **遗留（有意保留）**：`chassis_feed`/`shoot_feed` 两条恒零话题（协议预留，删除需改合同）；`gimbal.c` 的 VOFA 功能开关块；`heat_guard` 两个预留 API；`HeatGuard*` 未被测试覆盖。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。
## DEV-20261120-13｜2026-11-20｜R7 参数收敛：16 组可调量从代码搬进 config（text +0）

- **目标与触发**：用户要求"把可调参数都搞到 config"。先做只读盘点（3321 处数值字面量逐条判定），再搬其中真正可调的 16 组。
- **功能变化**：无。**宏体就是原来的字面量**，单位换算（`DEGREE_2_RAD` 等）留在代码里。
- **文件与符号**：
  - 新增 48 个宏，分布：`chassis.h`(12)、`gimbal.h`(18)、`shoot.h`(11)、`protect.h`(2)、`ax650.h`(1)、**新建 `remote.h`**(4:摇杆满杆角速率/死区/满量程)。
  - 代码侧替换 45 处：`chassis.c`(10)、`gimbal.c`(28)、`shoot.c`(11)、`cmd_remote.c`(4)、`cmd_authority.c`(6)、`ax650_uart1.c`(1)。
  - **关键修正**：`gimbal.c` 手动 yaw PID 的 Init 值原本每周期被同值字面量覆盖（"改 Init 不生效"），本轮让 Init 与运行时覆盖点共用同一宏，这个坑消失。
- **接口与调用链**：无。电机 PID 结构体、CAN ID、任务周期均未变。
- **配置/API/协议影响**：宏 562 → **610 条**（新增 48，无删除、无改名）；`text 110960 (+0)`、data/bss +0、1215 符号名一致 —— **参数搬家没有产生任何机器码变化**，这是"值逐字未改"的硬证据。
- **有意未搬（结构性常量，审计已逐条说明）**：`0.70710678f` 麦轮分量、KF 的 Q 矩阵系数、`±180/360` 回绕、C620/C610 协议满量程（10000/15000/20000）、功率模型 `K1/K2` 系数、CAN 过滤器与 `SlaveStartFilterBank`、栈深与 `osDelay`、`heat_guard.c` 的规则常量（10Hz 结算）、`bmi088.c` 驱动内标定常量。
- **已知耦合（写进宏注释）**：`RC_PITCH_MAX_RATE` 必须 ≤ `PITCH_REF_FF_MAX_RATE`；`RC_YAW_MAX_RATE` 受 `YAW_REF_FF_MAX_RATE` 跳变阈值约束 —— 单改任一会让前馈被**静默清零**。
- **事实冲突（只记录不改）**：`gimbal.c` 注释写 pitch 速度环 `RC=0.008`，代码实际 `0.012`；宏注释已注明"以代码为准"。
- **验证**：闸门 G1/G2/G3/G5 全 PASS；审计计数 301/102/57/**57**（+1 = `remote.h`）。
- **下一步**：R8 重复功能清理（审计已出清单，待批准）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。
## DEV-20261120-14｜2026-11-20｜R8 重复功能清理：删两套死副本（text +0，四边取证）

- **目标与触发**：用户要求做重复清理，但明确"**一定要审核多边，确保功能不受影响**"。因此每一项都过四道证据：①全仓调用者 ②头文件编译图 ③ELF 可达性 ④删除后机器码是否变化。
- **功能变化**：无。
- **删除（附四边证据）**：
  - `modules/BMI088/bmi088.c` + `bmi088.h`：13 个对外 API 外部引用**全为 0**；只有 `bmi088.c` 自己 include 自己；全部符号**不在 ELF**。
  - `modules/motor/power_control.c` + `power_control.h`：`PowerControl`/`PowerControlInit`/`SetPowerLimit` **不在 ELF**；`chassis.c` 与 `motor_task.c` 只有 include、零符号使用；与 `dji_motor.c` 同名的 `DecodeDJIMotor`/`MotorSenderGrouping`/`DJIMotorLostCallback` 两处都是 `static`，不构成重复定义。
  - `chassis_kinematics.c` 里重复的 `LF/RF/LB/RB_CENTER` 4 个宏（RM-010，R4a 拆分残留）。
  - 连带：`Makefile` 去掉 2 条源文件登记；两个文件的 `#include "power_control.h"` 一并删除。
- **明确不动（附理由）**：
  - `seasky_protocol.c`（CRC 栈已死）与 `master_process.c`：**`VisionSetAltitude` 仍在 ELF 中且被 `ins_task.c` 调用**，整组删除会断调用链 → 记 RM-011，改造需单独批准。
  - 手动/自瞄双 PID 集、跟随律 vs 回中律、`daemon` vs `cmd_watchdog`：审计确认为**有意重复**，不合并。
- **验证**：闸门全 PASS——`text 110960 (+0)`、data/bss +0、**1215 个符号名与尺寸一致**（删掉的两个 TU 本来就没贡献任何符号）；宏 610 条一致；主机测试 chassis+ax650 全 PASS；`check_source_lists.py` PASS（136 个 .c）。
- **审计计数**：301/460 未变、98 改、**61 缺**(+4: 4 个上游基线文件被删)、57 增、2 红旗。已按此重锚基线。
- **台账**：RM-006、RM-010 关闭；新增 RM-011（seasky 旧栈部分存活）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-15｜2026-11-20｜USART1(视觉口)改作底盘 VOFA JustFloat 调试口，AX650 链路暂停

- **目标与触发**：用户要求"用视觉的 uart1 通道去 VOFA 做底盘调试"。CubeMX 只初始化 3 路串口（USART3=DBUS、USART6=裁判系统、USART1=视觉），没有空闲口；USART6 那条旧 yaw VOFA 会顶掉裁判系统功率/热量，所以唯一可行的是**让视觉口临时改当调试口**。
- **功能变化**：新增编译期开关 `CHASSIS_VOFA_UART1_ENABLE`（当前 1）。开启后 USART1 不再走 AX650 8E1 私有帧，改为按 CubeMX 的 **921600 8N1** 发送 VOFA+ **JustFloat** 帧，200Hz（`CHASSIS_VOFA_SEND_DIVIDER`）。25 通道 104 B/帧，链路占用约 226 kbit/s（921600 的 24.5%），一帧 1.1 ms < 5 ms 任务周期。
- **互斥设计**：`AX650_UART1_ENABLE` 置 0（不初始化、不发姿态、不上报 ProgramMode）。两个宏同时为 1 时 `ax650.h` 里 `#error` 直接拦住——同一个口上混两套协议（8E1 私有帧 + 8N1 JustFloat）会让两边都解析错。恢复自瞄：`AX650_UART1_ENABLE` 回 1、`CHASSIS_VOFA_UART1_ENABLE` 回 0，其余代码零改动。
- **通道表**（全部 float，VOFA 端按此顺序加变量）：0 vx指令 / 1 vx斜坡后 / 2 vx逆解（mm/s）；3 vy指令 / 4 vy斜坡后 / 5 vy逆解；6 wz指令 / 7 wz逆解（deg/s）；8 云台-底盘夹角 deg；9 IMU 世界系 yaw 角速度 deg/s；10..13 四轮速度参考（转子 deg/s，LF/RF/LB/RB）；14..17 四轮实测速度；18..21 四轮电流原始码；22 底盘功率 W；23 缓冲能量 J；24 底盘模式枚举。22/23 来自裁判系统，未接裁判系统时为 -1。
- **文件与符号**：`config/robot_infantry_2026/chassis.h`（+2 宏）、`config/robot_infantry_2026/ax650.h`（`AX650_UART1_ENABLE` 1→0 + 互斥 `#error`）、`application/chassis/chassis.c`（`SendChassisVofa()`、`ChassisInit` 内注册 USART1 实例、`ChassisTask` 末尾调用）、`tests/chassis_remote_test.c`（自瞄挡"上报 ProgramMode"断言按 `AX650_UART1_ENABLE` 分支）。新增 1 个 static 函数、1 个 static 变量、1 个 static 帧缓冲（104 B bss）。
- **接口与调用链**：无新任务、无新消息话题、无协议结构体改动。USART1 句柄在 `ChassisInit` 注入 `USARTRegister` 配置，属 AGENTS 允许的"初始化期句柄注入"口径；任务内不碰寄存器/HAL。发送路径复用既有 `USARTIsReady`/`USARTTrySendIT`，先查忙再拼帧，忙则丢弃本帧，不排队旧数据。
- **验证**：ARM 全量编译 0 error、第一方 0 warning（text 109632 / data 27776 / bss 79952，FLASH 13.11%、RAM 82.18%）；主机测试 `run_chassis_tests.ps1` ALL PASS（含 `CHASSIS_M3508_FORCE_DISABLE=0` 变体）、`run_ax650_tests.ps1` 全 PASS（AX650 模块本身未改，仍覆盖 8E1/解析）；基线审计 301/98/61/57，**红旗 3 条**（新增 `chassis.c: huart1` = 上述初始化期注入；另两条为既有的 gimbal huart6、ins_task while(1)）。
- **未执行**：未烧录、未接 AX650 实车联调、**VOFA 波形未实测**（用户自测；VOFA 端设 JustFloat + 921600 + 8N1）。台架/整车始终由用户执行。
- **遗留/提醒**：①`[chcal]` RTT 日志与 VOFA 可同时开；②VOFA 与 AX650 物理互斥：调试时拔掉视觉板、把 USB-TTL 的 RX 接 PA9、GND 共地；③恢复自瞄前务必确认两个宏已换回，否则上场无自瞄。
- **发现（非本次改动）**：`config/robot_infantry_2026/board.h` 工作区有未提交改动 `CHASSIS_M3508_FORCE_DISABLE` 1→0（放开四轮输出），未在台账登记，本次未动它。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-16｜2026-11-20｜对照实验：去掉跟随律 wz 的 ±300 总限幅（待实测）

- **触发**：用户报"跟随模式匀速推杆自转时声音一顿一顿"，且**小陀螺模式不响**（说明速度环/机械在恒速指令下正常）。用户指定：把 `CHASSIS_ROTATE_SPEED` 那个限幅去掉试试。
- **假设**：跟随律 `wz = 位置项 + 阻尼项 + 0.9×gz`，原 88 版用 `CHASSIS_ROTATE_SPEED(300)` 当总上限。云台快转时 `0.9×gz` 单项即可顶穿 300（本车满杆 Gyro[2] 实测 400+°/s），吃满后位置项/阻尼项份额被整段挤掉 → "三项互顶"的削顶 + 偏角积分式堆积 → 松/换向时反向甩动，听着就是一顿一顿。
- **改动**：`CHASSIS_FOLLOW_WZ_LIMIT`（新宏，`chassis.h`）**0 = 不限幅**；`chassis.c` 跟随分支改为 `if (宏 > 0)` 才限幅。ROTATE 模式的 `wz = CHASSIS_ROTATE_SPEED` 是**转速常量、不是限幅**，未改。其余跟随律结构（0.9 FF / 0.4 阻尼 / 2° 死区 / 平方律）一字未动。
- **风险**：不限幅后大偏角（>约 14°）指令可远超 300，实际兜底只剩 `LimitChassisOutput` 的 `CHASSIS_MAX_MOTOR_RPM`（约 418 deg/s）；台架建议架空或手扶，地面测注意行程。
- **判读**：变干净 → 削顶是主因，下一步考虑"保留限幅但给前馈单独限幅"（88 版之后曾有过、被整段回退）；仍顿 → 排除削顶，回 300，转查速度环/机械。
- **回退**：`CHASSIS_FOLLOW_WZ_LIMIT` 改回 `300.0f`，一行，无代码回退。
- **验证**：ARM 全量编译 0 error、第一方 0 warning（text 109584，−48 B = 被删掉的限幅）。**未烧录、实车未测**。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-17｜2026-11-20｜实测定位跟随"一顿一顿"= 3.16 Hz 自激；限幅排除，改阻尼低通（待烧录验证）

- **触发**：DEV-20261120-16 烧录后按用户要求采集"跟随模式匀速推杆自转"的波形。
- **采集**：`tools/follow_trace.js` 4 通道（`cmd_wz/est_wz/off/gz`）**100 Hz**、1000 点 / 17.5 s、0 失败块（通道少→采样率从 18 Hz 提到 100 Hz，这是本次能看到 3 Hz 的前提）。
- **工作点**：`cmd_wz` 均值 123 deg/s（峰值 150.7）、`gz` 166.6 deg/s、`off` −5.4°。
- **结论 1（否掉削顶）**：`cmd_wz` 峰值 150.7，离 300 很远，**±300 限幅全程没有触发**，而顿挫照旧 → DEV-16 的削顶假设不成立，`CHASSIS_FOLLOW_WZ_LIMIT` 已恢复 **300**（宏保留，可配置）。
- **结论 2（真正的现象）**：**3.16 Hz 自激**。该频点幅值：`cmd_wz` 6.2（1σ 12.2）、`est_wz` 4.0（相位落后 cmd 20 ms）、`gz` 3.4、`off` 仅 **0.77°**。
- **放大机制**：偏角只抖 ±0.8°，但平方律位置项在该工作点的斜率 ≈ 3×eff ≈ **11 (deg/s)/deg**，把 0.8° 纹波放大成 ±8 deg/s 的指令纹波 → 四轮以 3 Hz 来回加/减速 → 听到"一顿一顿"。另外 0.9×gz 前馈把云台自身速率纹波直接搬进指令（gz 在该频点 ±3.4 deg/s），gyro 还有 7 个离群点（MAD σ 4.24、std 10.45）。
- **根因定位（低通截止正压在振荡频点）**：`CHASSIS_FOLLOW_RATE_LP=0.1` @200Hz ⇒ 截止 **3.2 Hz**，恰是振荡频率：阻尼项在这一频点已被衰减约 3 dB、滞后约 45°，**等于没有阻尼**。
- **改动**：`CHASSIS_FOLLOW_RATE_LP` **0.1 → 0.25**（截止 3.2 → 8 Hz），让 `spd = 0.4 × offset_rate_f` 真正参与。单变量，跟随律其余项（KP 1.5 / DAMP 0.4 / 2° 死区 / 平方律 / 0.9 FF）未动。
- **判据**：同一工况下 3.16 Hz 分量降 **≥50%**；稳态 |off| 不增大（基线 5.4°）；静止抖动不劣化（代价是 ecd 量化噪声更多进入阻尼项，靠 `CHASSIS_FOLLOW_SPD_MIN` 门限兜）。
- **回退**：仍抖 → 0.15；静止抖/异响加重 → 回 0.1。
- **验证**：ARM 全量编译 0 error、第一方 0 warning（FLASH 13.10%）。**未烧录**。
- **分析工具（新增，可复现）**：`tools/analyze_follow.py`（频谱/分割/逐点摘录）、`tools/analyze_follow_spikes.py`（离群点 + 通道间互相关时延）。
- **AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261120-18｜2026-11-20｜跟随律重写：平方律 → 线性 + 前馈额度分配

> **编号说明**：本条原写作 `DEV-20261120-03`，与重构会话已占用的 -03（重构 R0）重号，2026-11-21 复核时改为 **-18**。
> **与 DEV-16/17 的关系**：那两条走的是"修阻尼低通（`RATE_LP` 0.1→0.25，让 3.2 Hz 处的阻尼真正起作用）"这条路，
> 本条把阻尼项整条删掉、改走"降斜率 + 加前馈"。**两条是同一现象的两套不同假设，不是递进关系**；
> 若本条实测仍振，回 DEV-17 的方案（`RATE_LP=0.25` + 保留 `DAMP`）只需把本节改动整体回滚。

**触发**。用户要求"重写跟随律"。此前 11-17 把跟随环改得越来越复杂（前馈单独限幅、残差伺服、位置项限幅、阻尼三折点），11-18 整段回退到 1027 DEV-88 版；回退后 11-20 实测到 **3.16 Hz 自激**（cmd_wz 该频点 ±6 deg/s，而偏角只抖 ±0.8°），说明 88 版也不是稳态解。

**先做调研**（开源对照，详见"依据"）：

| 来源 | 控制律 | 参数 | 阻尼/前馈 |
|---|---|---|---|
| 官方 Development-Board-C `20.standard_robot` | `wz = -PID(relative_angle, angle_set)` | **Kp 40, Ki 0, Kd 0**, MaxOut 6 rad/s≈344°/s | 无 |
| 跃鹿 `xeno_framework`（本工程上游） | `wz = -1.5 × offset × \|offset\|` | 1.5 | 无 |
| 上交交龙 | 独立跟随 PID（kp/ki/kd + 各项限幅 + 总限幅，含扭腰 `twistGapAngle`） | 未公开 | 无 |
| Horizon-RM 教程 | 位置式 PID，目标 0，反馈 `-ANGLE_Relative`，`spinLittleRound` 取最短路径 | 未公开 | 无 |

**共同点**：全是一阶角度环，**没有一家用阻尼项**。本工程那个 `1.5 × offset × |offset|` 与跃鹿上游逐字符相同——是继承的魔数，不是本车标定。

**为什么没照抄官方的 Kp=40**。官方输入 rad、输出 rad/s，40 意味着"每度误差给 40°/s"。但本车的 3.16 Hz 自激恰好发生在旧平方律斜率 ≈11 (deg/s)/deg 的工作点（3.7° 处，d/deff(1.5·eff²)=3·eff）。**刚度再往上推，穿越频率就进 3Hz 以上，必振**。官方那个值配的是 4 m/s、力臂 0.2 m、500 Hz 的轻车；本车力臂 0.34 m、任务 200 Hz。故取 **8**，把穿越频率压到约 1.3 Hz，留 27% 裕度。

**新结构四处改动**（是重写，不是微调）：

1. **位置项统一为线性**：`wz_pos = KP × err`，删掉"小角度线性 + 大角度平方"两段。平方律斜率随误差变，稳定性无法用单一 Kp 描述；线性化后穿越频率与工作点无关。
2. **限幅改额度分配，且只在饱和时触发**：平时 `wz = ff + p` 直接相加（无稳态误差）；仅当 `|ff+p| > lim` 时，先把前馈压到 `lim×(1−P_RESERVE)`，给位置项留强制额度。88 版是三项加完统一截断，云台快转时前馈单项顶穿，位置项被整段挤掉 → 云台减速后靠积累的 offset 甩回来，这就是"互顶"。
3. **删除 offset 差分阻尼**（3Hz 低通 + ecd 差分）。一阶环里 D 项不提供稳定裕度，只把响应拖慢，且 200 Hz 下差分噪声大（约 8.8 deg/s）。需要阻尼时改用**实测相对速率** `(gz − est_wz)`，由 `CHASSIS_FOLLOW_KD` 打开，**默认 0 = 关**。
4. **前馈系数 0.9 → 1.0**。1.0 才是零滞后跟踪的唯一取值；低于 1 时剩余份额永远由位置环补，稳态落后 `(1−FF)×gz/KP`。11-18 试 1.0 失败的原因不是 1.0 本身，而是当时"先加后削"的结构；现在有额度分配兜底。

**新增功能**。`CHASSIS_FOLLOW_TARGET_OFFSET`（默认 0）：跟随目标相对角可配，非 0 即"侧身跟随/扭腰"，对应官方的 `chassis_relative_angle_set` 与上交的 `twistGapAngle`。官方本来就是 PID 的 setpoint，我们此前写死为 0。

**参数变更**（`config/robot_infantry_2026/chassis.h`）：

| 宏 | 改前 | 改后 |
|---|---|---|
| `CHASSIS_FOLLOW_KP` | 1.5（平方律） | **8.0（线性，(deg/s)/deg）** |
| `CHASSIS_FOLLOW_FF` | 0.9 | **1.0** |
| `CHASSIS_FOLLOW_KD` | — | **0.0（新增，默认关）** |
| `CHASSIS_FOLLOW_P_RESERVE` | — | **0.25（新增）** |
| `CHASSIS_FOLLOW_TARGET_OFFSET` | — | **0.0（新增）** |
| `CHASSIS_FOLLOW_LIN_RANGE` | 5.0 | **删除**（线性律已无分段） |
| `CHASSIS_FOLLOW_DAMP` / `_SPD_MIN` / `_RATE_LP` | 0.4 / 3.0 / 0.25 | **删除**（offset 差分阻尼整条移除） |

**修改位置**。`application/chassis/chassis.c`（跟随分支重写；删除 `offset_last`/`offset_rate_f` 静态量与斜坡段里的 3Hz 低通；ALIGN 分支注释同步）；`config/robot_infantry_2026/chassis.h`（上表）。

**验证证据**。`ninja -C build` 0 error，`chassis.c` 0 warning。FLASH 137408 B / 13.10%，RAM 107712 B / 82.18%（较改前 −1284 B / −208 B）。**已烧录**：2026-11-20 22:31 上板，探针回读片上 SHA-256 与 build ELF 一致（`4d709110…`）。**未采波形、未实车验证**——烧录时车处于零力（`chassis_mode=0`、`authority_source=1` 右上挡在上），跟随段尚未运行。

**遗留与下一步**。按顺序单变量试：① `KP` 6 → 8 → 10，各跑一次匀速推杆，量 3 Hz 分量与稳态 `|off|`（88 版基线 5.4°）；② 若 KP=10 出现 3 Hz 抖动，**不要降 KP**，先开 `CHASSIS_FOLLOW_KD` 0.1 → 0.2；③ 满杆急转若仍追不上，那是 `CHASSIS_ROTATE_SPEED 300 < 云台 400°/s` 的物理缺口，只能提上限或加超电，改律无用。侧身跟随（`TARGET_OFFSET`）等刚度定下来再试。

**AI**：DeepSeek-V4.1-Flash，2026-11-20。

## DEV-20261121-01｜2026-11-21｜跟随律 KD 标定：0.5 有效、1.0 反而更差；采集链三处缺陷修复

**背景**。DEV-20261120-18 把跟随律重写为"线性 P(4) + FF(1.0) + 额度分配"，并预留 `CHASSIS_FOLLOW_KD`（实测相对速率阻尼，默认 0）。用户反馈"一顿一顿没了，但单次大角度转向不好"，本轮量化并标定 KD。

**先修采集链（否则后面全是假结论）**。三处缺陷，逐条用数据否掉自己：

1. **分块采样留空洞**：`follow_trace.js` 原 `CHUNK_POINTS=50`，每块约 0.8 s + 块间约 0.37 s 空档，20 多块即有 20 多个洞（`follow_osc_20261120_2216.csv` 空洞占 43%）。而转向的峰值/松杆/过冲恰好都发生在那 0.3 s 里 → 溜车、回摆、收敛全部量不出来。**改 `CHUNK_POINTS=1000`（一次 RPC 取完，实测 1000 点 / 16.0 s / 空洞 0）**。
2. **FFT 假设等间隔**：`variables.sample` 实际间隔 9~46 ms 抖动（请求 10 ms），按等间隔做 FFT 频率轴是错的 → **此前"3.16 Hz 自激""2~5 Hz RMS 14.7 vs 2.2"等频域数字全部作废**，DEV-17 的结论同受影响。改频域分析前必须先插值到均匀网格。
3. **指标口径自相矛盾**：用 `|off|<2°` 判收敛，而死区就是 2°（必然判不收敛）；用 `cmd` 判松杆，而 `cmd` 是律的输出、松杆后是衰减不是归零（应看 `gz` 突降）。两个都改掉。

**KD 标定**。各烧一版，采"一次转向 + 松杆停 3 s"的连续数据（同一次动作，`gz` 峰值 200~230 °/s）：

| 指标 | KD=0.5 | KD=1.0 |
|---|---|---|
| 起步 off 建立 | 6.9°（0.17 s，云台 206/底盘 160 °/s） | 4.0° |
| 松杆后溜车 | **20.3°** | 32.3° |
| 底盘停住耗时 | **0.19 s** | 0.24 s |
| off 摆幅 | **17.1°** | 21.0° |
| 反向速率 | 17 °/s | 28 °/s |

**结论 1：KD=1.0 更差，已回滚 0.5**。机理：松杆瞬间 `wz = gz + P + KD(gz−est)`，KD=1 时衰减项 `(0−199)` 直接给出满额反向指令，车轮被命令倒转 → 打滑、多走 12°。

**结论 2：20.3° 已接近物理极限**。200 °/s 刹停用 0.19 s，平均 107 °/s，即 20° 位移；这段时间由轮胎摩擦与制动电流决定，不是控制律能改的。**起步滞后 8.3° 同理**（云台 206 °/s 时底盘只有 160 °/s，差值就是底盘加速能力）。

**改动**。`tools/follow_trace.js`：`CHUNK_POINTS 50→1000`；新增 `--trigger N` / `--wait S`（等 `|cmd|>N` 自动开录，解决"采集窗口与操作对不上"）；采集上限 1000→3000 点。`config/robot_infantry_2026/chassis.h`：`CHASSIS_FOLLOW_KD 0 → 0.5`（期间试过 1.0 已回滚）。

**验证证据**。ARM 编译 0 error，FLASH 137460 B / 13.11%；当前 ELF SHA-256 `aa81fc07…`（与 0.5 版逐位一致，可复现）。实车已烧 KD=1.0 与 KD=0.5 各一版并各采一组连续波形（`build/kd05_single.csv`、`build/kd10_single.csv`，均 1000 点 / 0 空洞）。

**未决**。①KD=0.3 未试，可能起步更灵、刹车略软；②"单次转向好不好"的主观判据仍缺——数据上 17° 摆幅已是物理极限附近，若体感仍差，瓶颈在执行层（轮速环增益、`CHASSIS_MAX_MOTOR_RPM`、轮胎）而非律；③频域分析全部需在插值后重做，暂不引用任何旧频率数字。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-02｜2026-11-21｜跟随律结构定版(线性P+前馈+单向刹车) + 提前刹车链路

**背景**。DEV-20261121-01 之后用户连续实车反馈:"起步/结束底盘超过云台,且顺逆时针手感差别大"。本轮把跟随律的结构、参数与信号通路一起改到位。

**查清的三件事**。

1. **AFF 的刹车判据方向错了**。原判据 `gz_rate_f < 0` 只对正向转向等价于"减速";负向转向时完全反过来 —— 加速段(gz −132→−240,α<0)被误判成刹车并给出**同向加力**,减速段(gz −245→−23,α>0)被误判成加速而罢工,off 一路冲到 31.7°。改为 `gz_rate_f * gz_dps < 0`(加速度与速度反向),与转向正负无关。**这条同时解释了"顺逆手感差别很大"**。
2. **刹车被绑在云台信号上**。AFF ∝ α,云台停稳后 α=0 → 刹车归零,而那时底盘还带着 200 °/s 惯性。实测松杆后多转 3~5°(100 °/s 时),按 s=v²/2a 反推制动能力约 1250 °/s²。
3. **底盘推重能力见顶**。架空正常、落地四轮跟踪只有 79~84%,把 `MAXOUT 10000→13000` 后电流峰值从 8900 涨到 10300(证明原限幅确实在顶),**跟踪比不变** → 已到 C620 电流边界,不是 PID 或限幅能解的。

**改动**。

- `chassis.c`:跟随分支重写为 `wz = FF×gz + KP×err + AFF×α + 单向阻尼`,并加额度分配(`P_RESERVE`);新增 `NO_OVERSHOOT` 硬钳位与 `BRAKE_ANT` 提前刹车。
- `robot_def.h`:`Chassis_Ctrl_Cmd_s` 新增 `yaw_rate_cmd`(云台目标角速度)。**必须放在 `chassis_mode` 之前** —— 该结构体紧凑排布,加在 u8 之后会落到奇地址 0x2000E951,float 读出的是 denormal 垃圾(实测踩到)。
- `robot_cmd.c`:`RobotCMDTask` 里对 `gimbal_cmd_send.yaw` 求微分(限幅 ±600、一阶低通 0.35)发布给底盘,松手瞬间归零,比 `gz` 早约 0.1 s。
- `chassis.h`:跟随律参数全部重排并补齐依据;`KD` 的生效条件改为 `|est|>|gz|`(原 `gap×gz<0` 在云台停住时恒不成立,形同虚设)。
- `tools/follow_trace.js`:`CHUNK_POINTS 50→1000`(消除块间 0.37s 空洞)、增加 `ycmd` 通道、`--trigger/--wait`。

**判据与结论**。①`est/cmd` 的 84% 是轮速环物理上限,不是控制律问题,**不要再从跟随律里补**;②"底盘超过云台"是偏角攒出后劲的后果,不是独立故障;③`KP` 上限在 4~8 之间(8 会重现 3.6Hz 自激),刹车只能靠非线性的 `BRAKE_ANT`,不能靠抬 `KP`。

**验证**。ARM 编译 0 error,FLASH 137824 B / 13.14%。实车已烧并采多组:`stop_*.csv` 真停事件 3 次(coast 3.0/4.7/4.0°)、`ant4.csv` 松手后 30ms 内 cmd 由 +144 翻至 −6。**未完成**:满速松手下的提前量验证、`BRAKE_ANT/ON_RATE` 手感标定(用户 13:44 停在 ANT=0.4、ON_RATE=5)。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-03｜2026-11-21｜底盘平移实现调研:上游差分 + 开源对照(仅文档,未改代码)

**背景**。用户要求"查工程里车子平移的逻辑"并"再看看开源的方案"。链条查清后,把对照结论沉淀为
`.Doc/detail/底盘平移实现对照.md`,避免下次重复调研。

**证据分级(重要)**。文档内每处结论标了 `[源码 已读]` / `[作者自述]` / `[二手]` / `[待核]`。
**只有本工程与上游 basic_framework 是逐行读源码**;304 战队读的是 README 全文、港科大为 DeepWiki 综述、
杭电只拿到论坛帖(未取到 zip)。文档里写明了这条边界,不把转述当实现断言。

**四条结论**。

1. **上游 baseline 不是"更完整的参考实现"**。跃鹿 `chassis.c` 的正解无 `0.7071`、无 `WHEEL_RATIO`
   (`:151-157`),`LimitChassisOutput()` 无任何限幅(`:163-174`),`EstimateSpeed()` 空函数体(`:181-186`),
   跟随律为平方律(`:223`),自旋 `wz=4000` 魔数(`:226`)。本工程在单位、限幅、逆解、看门狗四项已超上游。
2. **平移链的手感缺陷是"未按设计上限归一化"**:键鼠 `W+A` 四轮最大 30800 deg/s ≈ 5134 RPM,
   未触 36000 限幅 → 合成 2121 mm/s,超 `CHASSIS_MAX_SPEED 1500` 41%;遥控侧 `vx = MAX·f/√(f²+r²)`
   是恒幅值归一化,杆量只定方向不定大小("过死区即满速")。开源共识是"指令矢量先归一化,再解算"。
3. **功率限制双向空缺**:上游有 `SetPowerLimit()` 调用但本工程未移植 `power_control` 模块;
   数据链路已通(`referee_protocol.h:204-207` 已解析 voltage/current/power,2026-11-20 由 reserved3 更正)。
   建议照 304 的六项式(纯 C、解二次、已给 M3508 参数),插入点 `LimitChassisOutput()` 内。
4. **小陀螺漂移:开源的根因不适用于我们**。304 认为漂移源于功率限制导致的速度响应滞后 ——
   我们当前无功率限制,该因果链不成立,故**不能照抄其"θ 随 ω 前馈"的补偿形式**,须先落地实测归因。

**依赖顺序(文档 §0 的核心结论)**:`D1 归一化 → D2 功率 → D3 漂移补偿 ω 化`。
D3 必须在 D2 之后标定:功率缩放会改变轮速跟踪,先标的 `bias` 角会整体失效。

**改动位置**。新增 `.Doc/detail/底盘平移实现对照.md`;`.Doc/README.md` 的 `detail/` 行与"按问题查"表各加一条。
**未改任何代码、配置或参数。**

**验证**。纯文档变更,无编译影响,未跑 ARM 构建(不涉及)。
`tools/refactor_gate.py check-links` **未执行** —— 本机 `AGENTS.md` 登记的 python 绝对路径已失效(该路径是目录),
新文档只含 http 外链与行内路径引用,不引入相对链接。

**未决**。①0x0202 是否真的下发 `chassis_power`,`[chcal] p=` 实测核对(与 `底盘优化计划.md` §1.2 说法冲突);
②D2 的 `.bss` 增量预算(RAM 已 82.18%);③漂移真实根因待落地实测;④杭电/河北工大源码未取到。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-04｜2026-11-21｜串口接收半死态修复 + 0x0208 发弹额度闸门(待实车验证)

**背景**。用户实车报两个症状:①遥控器概率性失去控制权(RTT `[rc] remote control lost` 刷屏);②热量冷却未生效,连发全速。EmberProbe 实车取证(变量/寄存器/DMA 缓冲直读)定位到同一根因。

**取证实锤**。

1. **ORE 半死态**:USART6.SR 读到 `RXNE=1 + ORE=1` 且 `RxState=0x22(BUSY_RX)`、`ErrorCode=0`、无 `[rm_ref] lost` 日志 —— 字节在进、解析全停(`FrameHeader.Seq` 变而 `PowerHeatTick` 恒 1213)。机制(F4 HAL 源码核对):ORE 属 blocking error,IRQHandler 错误分支 EndRxTransfer + 异步 abort DMA 后**不清 ORE 标志**(清它需读 SR+读 DR);`HAL_UARTEx_ReceiveToIdle_DMA` 在 `RxState != READY` 时直接返回 HAL_BUSY,旧代码不检查返回值 → re-arm 静默失败。
2. **错位解析实锤**:ORE 丢字节后,`referee_info` 中 0x0201 字段被错位写入垃圾值(heat_limit=240/cooling=10/power_limit=16,与 DMA 缓冲完好帧不符);正确值(320/100/120)在重启后干净启动时解出,与裁判客户端日志 `热量:10.0|320` 吻合 → **协议布局定义无误,丢字节才是真凶**。
3. **热量守卫本身正常**:干净启动后 `heat_ready=1 / guard_state=NORMAL / heat_limit=320 / cool_per_s=100`。"全速射出"复核为参数下可持续射速:6 发/s×10 点=60 点/s < 冷却 100 点/s,闸门数学上不触发。

**改动**。

- `bsp/usart/bsp_usart.c`:新增 `USART_RearmRx(instance, clear_err)` 封装 —— RxState 非 READY 先 `HAL_UART_AbortReceive`;`clear_err=1` 时清 ORE/RXNE/IDLE 残留(读 SR+读 DR);检查 re-arm 返回值,失败限频 LOGERROR(每 32 次)。三处调用点(ServiceInit/RxEventCallback/ErrorCallback)统一走它。**clear_err 分寸**:仅 ErrorCallback/强制终结路径传 1,正常 IDLE/TC 路径传 0(此时 DR 里可能是下一帧首字节,读掉会丢字节)。
- `modules/referee/rm_referee.c`:`JudgeReadData` 增加 `buff_len` 参数并**错位自愈** —— SOF 不在包首时向后扫描下一个 0xA5 重新对齐(残帧最多损失一帧);新增 `judge_length > buff_len` 长度防御(DataLength 损坏时 CRC16 读越缓冲);递归调用传入剩余包长。
- `application/shoot/heat_guard.{c,h}` + `config/.../heat.h`:新增 0x0208 发弹额度硬闸门 —— `projectile_allowance_17mm < 1` 即禁发(AllowBurst/AllowSingle 双入口),优先级高于热量模型,做成独立标志避免与热量状态机互覆抖动。`HEAT_GUARD_ALLOWANCE_ENABLE` **默认 0**(台架/未开赛额度恒 0,开着会永久锁枪),**正式比赛上电前必须置 1**。

**验证**。`tests/run_chassis_tests.ps1` ALL PASS;ARM 编译 0 error,改动三文件 0 warning,FLASH 138440 B / 13.20%,RAM 107736 B / 82.20%(+1KB,heat_guard/裁判路径);ELF SHA-256 `8dd07f40…`。**未烧录、未实车验证** —— 验证判据:①上电跑 10 min,RTT 无 `[rc] remote control lost` 间歇刷屏、`PowerHeatTick` 连续增长不冻结;②ORE 复现时(如有)`[bsp_usart] rearm failed` 限频日志可见且接收自动恢复;③开赛前把 `HEAT_GUARD_ALLOWANCE_ENABLE` 置 1,连发至额度耗尽应看到 `[heat] allowance EXHAUSTED, hold fire` 并停发。

**未决**。①ORE 的物理来源未定(线束/共地/波特率边缘),固件自愈只是兜底,建议上台架用逻辑分析仪看 PG9 波形;②客户端日志 `允许发弹量:0|-N` 的规则语义(透支惩罚/锁定阈值)需对照规则手册确认;③若比赛中仍见错位,考虑在 bsp 层对 DBUS(18B 定长)单独走环形缓冲。

**补充(同日 16:51)**。应用户要求 `SHOOT_RATE_DEFAULT` 6 → **12 发/s**(shoot.h):6 发/s×10 点=60 点/s < 冷却 100 点/s,热量恒 0、守卫永不触发,热量管理无法验证;12 发/s 净 +20 点/s,守卫会真实进入 SINGLE/HOLD 并把射速收敛到散热平衡 10 发/s 附近。验证后若要恢复可持续射速改回 6(或定 8 留 20% 余量)。新 ELF SHA-256 `ebeb7a75…`。

**补充(同日 17:18)**。用户在裁判客户端将性能体系切至**爆发优先:Q0=230 / 冷却=14 点/s**(客户端截图;切换后 0x0201 下发值随档位变化,守卫每帧重取自动跟随,无需改逻辑)。重算:可持续射速 14/10=1.4 发/s,12 发/s 连发 ~1.9s 进 SINGLE、~2.0s 进 HOLD,爆发窗口约 2s/20 发,满热量冷却 16.4s。改动:`SHOOT_RATE_DEFAULT` 保持 12(作为爆发请求射速,节奏交守卫管理)并重写注释;兜底参数 `HEAT_GUARD_DEFAULT_HEAT_LIMIT/COOLING` 240/40 → **230/14**;单发间隔注释例子同步。新 ELF SHA-256 `f5c79d34…`。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-05｜2026-11-21|串口错误中断风暴实锤与修复:急停失效根因(待实车验证)

**背景**。DEV-20261121-04 修复后实车复测,用户报告更严重症状:发射后 1s 内"整车突然断电又自行恢复"、**遥控器关不掉、上档急停无效**。

**实车取证(EmberProbe,车运行中直读)**。

- `authority_source=3(AUTH_RUN)`、`robot_state=1(READY)`、`rc_data` 冻结(全 0 + 档位停在 `02 03` 运行挡)——用户按右上急停的帧从未写入;
- `USART3.SR = 0x1F8`:**ORE/RXNE/IDLE/LBD 全挂**;`CR1.PEIE/EIE/IDLEIE` + `CR3.EIE` **全部使能**;
- `DMA1.S1CR.EN=0`、`S1NDTR=0`(DBUS FIFO 是 DISABLE,排除 FIFO 滞留);
- huart3:`RxState=0x22(BUSY_RX)`、`ErrorCode=0`;`RemoteControlIsOnline` 的守护/无效帧逻辑(不喂狗、清 rc_ctrl)本身正确。

**机制**。遥控器关闭/线异常时,DBUS 线悬空毛刺产生持续假字节流(100000 8E1 采样下每个毛刺即一个假起始位)→ F4 HAL 错误分支**不清 ORE/RXNE/IDLE 标志**(清需读 SR+DR)→ 每次中断立即再次错误 → 形成"错误中断风暴":CPU 被打满、FreeRTOS 任务全部饿死 → authority/robot_state 冻结在最后一次的 RUN/READY → **急停、关机、断流保护全部失效**,表现为"断电又起来、怎么都停不下来"。DEV-20261121-04 的 ErrorCallback 修复对"经错误中断的单次错误"有效,但风暴中每次 re-arm 后立刻再错,修复本身被风暴吞掉。

**改动**。`bsp/usart/bsp_usart.c` ErrorCallback 改为两级处理:
- 偶发错误(距上次 >10ms):原地清标志 re-arm(维持 DEV-04 行为,无感恢复);
- **风暴(10ms 内第二次错误):终结接收(AbortReceive)+ 清 ORE/RXNE/IDLE 残留(读 SR+DR)后不再 re-arm —— 中断源被彻底掐断,风暴停止**;喂狗随之停止,DBUS 100ms/裁判 300ms 内 daemon 必触发 lost 回调(清控制量 + `USARTServiceInit` 重开接收),语义即"这条线已不可信,走离线保护"。恢复延迟最坏一个 daemon 周期,DBUS 丢 ≤10 帧,可接受。
- 新增 `last_err_ms[]` 风暴判定与 `bsp_dwt.h` 依赖(DWT 读寄存器,ISR 安全)。

**验证**。主机回归 ALL PASS;ARM 0 error/第一方 0 warning,FLASH 138636 B / 13.22%,RAM 107752 B / 82.21%;新 ELF SHA-256 `4c4c590b…`。**未烧录**。判据:①关遥控器(线悬空)10s,RTT 应出现 `error storm ... rx halted` 限频日志 + `[rc] remote control lost`,**上挡/关机应在 ≤200ms 内令整车零力**;②重新开遥控器,接收自动恢复、解锁正常;③12 发/s 连发时观察是否仍出现风暴日志(若频繁,指向电源/线束硬件问题)。

**未决**。①USART6 的 DMA 配置为 FIFO ENABLE + FULL 阈值(与 DBUS 的 DISABLE 不一致),P2M 包尾字节滞留 FIFO 是裁判侧错位/丢字的独立嫌疑,改配置涉及 .ioc/生成文件,待用户批准;②ORE 毛刺的物理来源(DBUS 线束/插头/遥控器接收机供电)待排查;③12 发/s 的电源冲击是否诱发毛刺待观测。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

**补充(同日 17:30)**。用户提供裁判服务端日志(16:20-17:29)裁决参数之争:**实际下发参数是 320/100,客户端 UI 的"爆发优先 230/14"未生效**。三处证据:①服务端日志 `热量常规冷却 热量:10.0|320|100`(17:00:21);②0x0201 实读 320/100;③17:00:20-24 连发 26 发,客户端"热量"字段 10→60,与 320/100 的热量账吻合(11.3 发/s×10 - 100 ≈ +13 点/s × 2.3s ≈ 30)。另两项实测:**12 发/s 机械跑得通** —— 26 发/2.3s = 11.3 发/s,弹速稳定 11.2~12.3 m/s(限 25),无卡弹无掉速(2.3s 窗口);允许发弹量透支至 -46。UI 参数生效条件待确认(疑似需开赛/重新上线)。改动:兜底参数 230/14 → **320/100**(回滚),shoot.h 注释更新为实测数据;`SHOOT_RATE_DEFAULT` 保持 12。新 ELF SHA-256 `6e58f815…`。

**补充 2(同日 17:33)**。用户确认:客户端性能体系已正确设置为**爆发优先 230/14**(此前是设置问题,非裁判下发 bug)。兜底参数改回 **230/14** 并修正注释;shoot.h 注释对齐(Q0/冷却每帧重取,0x0201 生效后守卫自动跟随,无需改逻辑)。二进制与 f5c79d34 版一致(仅注释差异)。上电后应读 0x0201 = 230/14 以确认下发链路同步。

**补充 3(同日 17:54)**。应用户要求 `SHOOT_RATE_DEFAULT` 12 → **1 发/s**:失控风暴仍在复现(DEV-05 修复待验证),排查期先用最小射速隔离变量(排除供弹链/大电流诱因)。1 发/s×10=10 点/s < 冷却 14,净散热为正,守卫不介入。风暴根因解决后按战术上调(可持续上限 1.4;冷却增益窗口可回 12)。新 ELF SHA-256 `62310452…`。

**补充 4(同日 18:11)**。射速最终定为 **6 发/s**(1 发/s 的隔离值作废,用户改回常规射速)。230/14 下的账:净 +46 点/s,连发 ~4.3s 进 SINGLE(累计 ~26 发),守卫压速至散热平衡 1.4 发/s;守卫预算峰值 8.1 > 6,请求不被削。新 ELF SHA-256 `186591db…`。

**补充 5(同日 18:13)**。应用户战术决策:`HEAT_GUARD_MARGIN_SINGLE` 15 → **10**(爆发优先 Q0=230 下,HOLD 闸门 = 220,即热量剩 10 点完全禁发;剩 30 点先切单发)。10 点 = 1 发整,覆盖单发在途弹 + 结算延迟;低于此值会盖不住在途弹。新 ELF SHA-256 `4283b936…`。

## DEV-20261121-06｜2026-11-21|串口接收架构重构:循环 DMA + 任务消费 + IWDG(待实车验证)

**背景**。DEV-20261121-04/05 都是给"NORMAL DMA + 每次 IDLE/收满后软件重启"打的补丁,但实车仍出现失控(16:54 与 18:18 两版固件均复现,后者已含风暴掐断)。调研开源后确认:官方 RoboRTS-Firmware、跃鹿哨兵开源、社区共识教程全部采用**循环 DMA/双缓冲 + FIFO + 任务侧解析**,从架构上不存在"重启失败"与"标志残留"两条死亡路径。本条目按此重构(用户批准的结构级改动)。

**新架构**。
- `bsp_usart`:`DMA_CIRCULAR`(运行时把 CubeMX 生成的 NORMAL/FIFO 改为 CIRCULAR/DIRECT,只配一次),硬件连续写 256B 环形缓冲、永不停止;正常数据路径**没有任何重启动作**;
- 消费由 `USARTProcessAll()` 在 200Hz RobotTask 首行轮询 NDTR 驱动,写指针取硬件真值、读指针只在任务侧推进(幂等,不依赖 IDLE/TC 中断);
- **跨拍装配**:新字节追加到 512B 装配缓冲,回调通过改小 `recv_len` 上报实际消费字节 —— 5ms 轮询会把 1.56ms 的 DBUS 帧切成两半,不装配则定长帧永远拼不齐(实现中发现并修正的真 bug);
- 错误处理退化为"清标志 + err_cnt++,DMA 不动";丢字节由解析器重同步兜底(DBUS 定长帧由 daemon 100ms 重启对齐);
- 模块侧:`remote_control.c` 按 18B 逐帧切分并上报消费量,坏帧整块丢弃;裁判/视觉等流式解析器零改动(默认全消费);
- 删除:USART_RearmRx、last_err_ms 风暴判定、ErrorCallback 两级处理、rearm_fail_cnt、memset(recv_buff);
- `chassis/gimbal` 的 VOFA 口显式 `tx_only=1` 不启动接收;四处未清零的 config 补 `{0}`;`ax650_uart1.c` 长度护栏改用装配缓冲上限。

**IWDG(独立看门狗,最后防线)**。工程 HAL 树裁掉了 iwdg 驱动 → 寄存器级实现(`bsp/iwdg/`):LSI 32kHz / PR=64 / RLR=1000 ≈ **2s**,RobotInit 末尾启动、RobotTask 每拍喂;置 `DBGMCU_APB1FZ` 的 IWDG_STOP 冻结位,调试暂停不复位。IWDG 不依赖中断/调度,任何僵死都会在 2s 内复位 —— 这是"不可能僵死到关不掉"的唯一硬件保证。

**代价与取舍**。①串口实例从 ~272B 增到 ~788B(环形 256 + 装配 512),3 实例占 FreeRTOS 堆约 2.4KB(共 20KB);②解析延迟由"中断即时"变为"≤5ms",DBUS 控制链总延迟仍 <10ms 帧周期;③DBUS 丢字节后需 daemon 100ms 恢复(定长帧物理限制)。

**验证**。主机回归 **ALL PASS**(适配了两处旧测试:坏帧判据由"长度≠18"改为注入非法档位的完整帧 —— 流式装配下长度不符不再等于坏帧);ARM 编译 0 error、第一方 0 warning;FLASH 138588B/13.22%,RAM 107744B/82.20%;ELF SHA-256 `e58ee77e…`。**未烧录**。判据:①连发/拔插裁判串口/开关遥控器,`error storm` 应永不出现,失控不再发生;②若仍僵死,IWDG 会在 2s 内自动复位(观察是否出现"整车重启");③`[rc]/[rm_ref]` lost 日志频率不高于改造前。

**未决**。①IWDG 复位会中断当前动作,比赛前需确认复位后自动行为可接受(复位后为急停态,需重新解锁);②USART1/VOFA 只发口不启动接收,若将来需要下行须改 `tx_only`;③FreeRTOS 堆剩余量建议下次用 `xPortGetFreeHeapSize` 打一条日志确认。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-07｜2026-11-21|零接触诊断:开机鸣叫码播报复位原因 + 射速护栏(待实车验证)

**背景**。串口架构重构(DEV-06)后首测:第一次发射时云台仍可控(架构修复有效),但出现两个新现象:①松手后仍射 1~2s;②第二次发射数秒后拨盘飞转,随后疑似重启(无法区分 IWDG 复位/手柄断电)。探针在运行中读取会暂停/干扰核心(用户多次实测),**改为不给用户车做任何外部连接**,诊断信息由固件自己播报。

**改动**。
- `robot.c`:开机读 `RCC->CSR` 复位标志,LOGINFO 打一条,并转成**蜂鸣器鸣叫码**:1 长声=上电/掉电(POR|BOR)、2 短=软件复位(SW,烧录后正常)、3 短=**看门狗复位**;优先级 IWDG > POR/BOR > SW(DEV-20261121-07)。播报 900ms 后自动停止。
- `robot.c`:连发射速护栏 —— `fire_rate` 非有限或 >`SHOOT_RATE_MAX(20)` 归零并 LOGERROR;单发角度目标 `loader_step_target` 非有限值时丢弃请求。"拨盘飞转"在合法路径不可能(守卫钳 6 发/s),护栏用于拦截异常值并留痕。
- `buzzer.c/h`:修复 `BuzzerTask` 对未注册槽位解引用 NULL 的隐患(此前靠"F4 地址 0 映射 Flash 可读"侥幸不崩);BuzzerInit 内注册一个默认实例并新增 `BuzzerDefault()`。

**验证**。主机回归 ALL PASS;ARM 0 error、第一方 0 warning;ELF SHA-256 `659d8134…`。**未烧录**。判据:①开机听鸣叫码 —— 若为三短声即 IWDG 复位,一长声即掉电;②复现"飞转"时看是否出现 `[shoot] illegal burst rate` 日志(需要 RTT,由用户自行读取或事后查看);③松手延迟若确认是拨盘继续转(非摩擦轮余响),再增加"迟滞带内强制停"判据。

**关于"松手后 1~2s"**。代码上拨盘在松手沿(拨轮 > -220)当拍即 `LOAD_STOP`,1~2s 与 `DIAL_FRICTION_KEEP_MS=2500`(摩擦轮松手后保持 2.5s)高度吻合,疑为用户听到的是摩擦轮余转;待实车确认。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-08｜2026-11-21|僵死根因二:解析递归栈溢出 + HardFault 死循环(待实车验证)

**现场**。DEV-07 固件(``659d8134``)实测:启动射击瞬间失控、遥控无效,下电后蜂鸣**三响 = IWDG 复位**。说明 RobotTask 被阻塞 ≥2s(看门狗没能被喂),而非串口错误风暴(风暴已由循环 DMA 架构消除)。

**代码级定因(两项叠加)**。
1. `JudgeReadData` 的错位自愈用**递归**实现(DEV-04 引入),裁判缓冲被噪声填成"频繁出现 0xA5"的形状时递归深度可达数百层;ROBOT 任务栈仅 **1024 word = 4KB**,层均数十字节 → **栈溢出 → HardFault**;
2. `HardFault_Handler` 原实现 `asm("bx lr")` **返回故障指令 → 再次 fault → 死循环**,不记录、不复位 → 表现正是"整车僵死、遥控无效",直到 IWDG 2s 超时复位。

**改动**。
- `rm_referee.c`:`JudgeReadData` 改**纯迭代**(对齐扫描 + 逐帧推进,CRC16 通过跳整帧/失败跳 1 字节),栈占用恒定,对垃圾数据只做线性扫描;
- `shoot.c`:槽沿记账 while 循环加**单拍上限(20 发)+ 反馈跳变清零**,彻底消除"巨大累计值导致超长循环"的可能;
- `stm32f4xx_it.c`:`HardFault_Handler` 改为**记录死因后 `NVIC_SystemReset()` 立即复位**(几百 ms 恢复,不再死等 IWDG);
- 新增 `bsp/fault/`:死因写入 **RTC 备份寄存器**(复位/掉电不丢),开机读取并转成蜂鸣码;避开 Flash 写入(故障现场不安全);
- `FreeRTOSConfig.h`:`configCHECK_FOR_STACK_OVERFLOW=2`,钩子 `vApplicationStackOverflowHook` 记录后复位。

**开机鸣叫码(扩展)**。1 长=上电/掉电;2 短=软件复位(烧录);3 短=看门狗;4 短=HardFault;5 短=**任务栈溢出**。

**验证**。主机回归 ALL PASS;ARM 0 error、第一方 0 warning;ELF SHA-256 `b86bf2bd…`。**未烧录**。判据:重复"启动射击"压力测试;若再次僵死,开机鸣叫码将直接指认死因(4=硬件异常,5=栈溢出);若出现 5 短,说明还有别的栈大户,需查各任务栈水位。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-09｜2026-11-21|堆再扩容 + 故障详情可读(USART1 ASCII)+ 静态守护池

**现场**。DEV-08 版(`e0cf8895`)冷启动 1 长(无故障记录,启动正常),但**射击时仍响四短=HardFault**,伴随拨盘异常转速。

**改动**。
- 堆 **24KB → 28KB**(RAM 90.9%):任务栈 11.8KB + 9 个电机实例 + 消息中心队列同吃堆,两次扩容后仍不够,改以 RAM 换稳定;
- `daemon.c`:实例改**静态池**(10+ 个守护不再 malloc),`DaemonRegister` 增加池满判空,`DaemonIsOnline/DaemonReload/DaemonTask` 全部判空;
- `dji_motor.c`:`malloc` 失败显式返回 NULL + LOGERROR(原实现直接 `memset(NULL)` 崩溃);
- `shoot.c`:`ShootTask` 与 `FrictionReady` 对三个电机指针判空,初始化失败时静默不发弹而不是崩;
- `bsp/fault` + `stm32f4xx_it.c`:**HardFault 现在记录 CFSR/BFAR/PC/HFSR** 到 RTC 备份寄存器,开机由 `FaultDumpToUart()` 以 ASCII 打到 **USART1**(921600,普通 USB 串口可读,不碰 SWD)。

**判据**。烧录后复现射击问题,用串口助手连 USART1(921600)看开机首行:
`[FAULT] code=.. CFSR=.. BFAR=.. PC=.. HFSR=..`
其中 PC 可直接 `arm-none-eabi-addr2line -e build/basic_framework.elf <PC>` 定位到源码行;
CFSR 位含义:IACCVIOL=取指违例、DACCVIOL=数据访问违例、MSTKERR=压栈失败(栈溢出)、UNDEFINSTR=未定义指令。

**验证**。主机回归 ALL PASS;ARM 0 error、第一方 0 warning;RAM 119152B/90.91%、FLASH 140552B/13.40%;ELF `ad30bfcd…`。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261121-10｜2026-11-21|最终定因:日志用 %f 导致参数错位 → HardFault

**现场**。DEV-09 版连测四次,故障块完全一致:PC/LR 均在 `SEGGER_RTT_vprintf`,`CFSR=0x8200`(PRECISERR)、`BFAR=0x40240000`、SP 恒定。关闭全部日志后复现消失。

**根因(可精确复现解释)**。
1. SEGGER 的 `SEGGER_RTT_vprintf` 只实现 `%c %d %u %x %s %p %%`,**不支持 `%f`**;
2. `heat_guard.c` 的状态迁移日志使用了 `%.0f`(三处)。`%f` 落入 `default: break`,**既不打印也不消费 `va_arg`**;
3. 于是 `LOG_PROTO` 自动追加的结尾 `%s`(本应打印颜色复位串)消费到的是第一个未消费参数 —— **`heat_q` 这个 float**,被当作 `char*` 去解引用;
4. `heat_q` 恰为 2.5625 时位模式即 `0x40240000` → 读未映射地址 → PRECISERR → HardFault。四次 BFAR 恒定正是 heat_q 在触发点上的固定取值;
5. 只在射击时崩:`[heat]` 迁移日志仅在守卫状态变化(NORMAL→SINGLE→HOLD)时打印,不打弹时状态恒 NORMAL,代码走不到。

**为什么表现为"失控+遥控失灵"**:HardFault 后旧固件 `bx lr` 死循环,新固件立即复位;复位/死循环期间 CAN 停发,电机丢失指令,操作手看到的就是"卡死/失控",随后看门狗或故障复位重启。

**修复**。`[heat]` 两处日志改为整型打印(`(int)` + `%d`);恢复日志输出(`BSP_LOG_OFF=0`、`s_log_enabled=1`);保留 RTT 控制块三项健全性校验与运行期总开关(`LogSetEnabled`)作为兜底。

**规则沉淀**。**LOG 宏内禁止 `%f`**;浮点一律 `Float2Str` 转字符串或 `(int)` 截断后 `%d`。变参函数的占位符与参数必须逐个数对齐 —— 少一个不一定报错,但会把下一个参数当别的类型读。

**验证**。主机回归 ALL PASS;ARM 0 error、第一方 0 warning;ELF `de63c1e0…`。判据:射击时不再出现 HardFault,且 RTT 能看到 `[heat] NORMAL -> SINGLE (q=200/230 c=14/s)` 这类整数格式日志。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

**防护升级(同日 20:36)**。除注释外增加两道强制拦截:①`bsp_log.c` 的 `LogPrintf` 运行期扫描格式串,**命中浮点占位符(%f/%e/%g/%a)直接拒绝打印**并提示一次 —— 以后即使有人再写错,最坏是丢一条日志,不会崩车;②新增 `tools/check_log_format.ps1`,扫描 application/modules/bsp 下所有 `LOGxxx` 调用,发现浮点占位符返回非零(已验证能识别 `%.0f`、不会误报 `%d`/`%s`),作为提交前检查。ELF `6f1b3ff7…`,主机回归 ALL PASS。

## DEV-20261121-11｜2026-11-21|射速律可选恒定摊平 + 战术档位 + 本车 UI(待实车验证)

**背景**。调研开源强队热量管理(同济 RM2024 公开策略、rm-controls 的 LOW/HIGH/BURST/MINIMAL、UT Austin 的热量缓冲)后,为本车补两项能力;反转解卡因发射机构硬件不支持反转作废(堵转仍为"锁存停盘 + 松扳手重置")。

**1. 射速律二选一**。`HEAT_GUARD_RATE_MODE`:
- `0` 指数衰减(原行为):`r = r_sustain + 余热/(10×SPEND_TIME_S)`,前快后慢;
- `1` 恒定摊平(默认):`r = r_sustain + 余热/(10×FLAT_WINDOW_S)`,窗口 4s,手感可预期。
两式稳态均为散热平衡 `r_sustain = cool/10`;230/14 档下约 7 发/s 恒定,约 4s 花完余热后落到 1.4 发/s。

**2. 战术档位**(`fire_tier_e`:ECO/NORMAL/BURST)。键盘 **X** 循环切换(遥控器无空闲输入,暂只支持键鼠);档位只决定"请求射速"(1.4 / 6 / 12 发/s),能否真打这么快仍由守卫裁决,爆发档不会越热量。状态在 `robot_cmd.c`,`cmd_remote/cmd_kbm` 发布时经 `FireTierRate()/Get()` 填入 `Shoot_Ctrl_Cmd_s`。

**3. 本车 UI**。删除上游遗留的 `RobotModeTest`(会自动改模式、干扰真实显示),在左下区域新增热量面板:
- 热量条框 (150,455)-(650,495) + 绿色填充线(按 q/limit 比例);
- 热量数字(整数)、`TIER:ECO/NORMAL/BURST`、`HEAT:NORMAL/SINGLE/HOLD/OFF`;
- 刷新 10Hz 节流且仅在数值变化时发送(RefereeSend 内部有 ~10Hz 延时,不能每拍调)。
数据源:`HeatGuardGetSnapshot()`(新增快照接口)、`HeatGuardGetState()`、`RobotCMDGetFireTier()`(新增对外接口,返回 uint8_t 避免 UI 反向依赖 APP 头文件)。`UICharDraw` 的 fmt 参数改 `const char *`,消除 const 限定符告警。

**验证**。主机回归 ALL PASS;`tools/check_log_format.ps1` 通过;ARM 0 error、第一方 0 warning;ELF `aa831e7a…`。待实车:①FLAT 模式射速手感与"打完即停"观感;②X 键切档时 UI 与日志同步;③热量条是否与裁判自绘血条/热量区域冲突。

**AI**:DeepSeek-V4.1-Flash,2026-11-21。

## DEV-20261122-01｜2026-11-22|README 同步当前工程状态

**目标与触发**。README 内容停留在 2026-11-20 前后,与当前树不符:config 已拆成伞头 + 分轴子头、Makefile 产物目录改为 `build_make/`、发射链路已接枪口热量守卫、键鼠新增 X 战术档位、裁判 UI 新增热量面板、提交前检查扩到日志格式与源清单。

**内容**。更新接手顺序(本文件 → `RM_DEVLOG.md` §0 → `.Doc/README.md`)、目录结构、接线表(补 USART6 裁判 / USART1 二选一)、编译烧录(两套产物目录、工具链版本、新增 `.c` 登记)、主机测试与提交前检查、新增"枪口热量守卫"章节(两级闸门 / 射速律 / 战术档位 / UI / 允许发弹量开关)、遥控与键鼠表(左/右上挡急停、X 键切档)、文档索引(计划/重构/热量设计/DEVLOG/AGENTS/code-map)。

**验证**。`python tools/refactor_gate.py check-links` PASS(所有 `.Doc/` 引用存在);基线审计 exit 0(Unchanged 297 / Changed 102 / Missing 61 / Added 61 / RedFlags 4,红旗均为既有项:VOFA 的 huart1/huart6 句柄与两处启动期 `while(1)`)。仅文档改动,未跑编译、主机测试与烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-22。

## DEV-20261122-02｜2026-11-22|RAM 占用 91%→49%:FreeRTOS 堆与遥测环搬入 CCMRAM(零行为变化,待实车)

**目标与触发**。用户要求降低 RAM 占用。ELF/map 静态分析(arm-none-eabi-nm + readelf):报告占用 119,304B/128KB=91.02%,其中实际静态 .data 27,784 + .bss 50,560 = 78,344B,另有链接器保留 40,960B(`_Min_Heap_Size`/`_Min_Stack_Size` 各 0x5000,纯链接期检查);**CCMRAM 64KB 完全未用(0%)**。静态大头:ucHeap 28,000(6 任务栈 12.3KB+TCB+队列)、5 条遥测环 26,930(且因 tag 静态初始化落在 .data,白占 26.9KB Flash 加载映像)、USB CDC 组 ~6.6KB(`VisionInit` 无调用者)、`dm_motor_instance` 2,208(DMMotorInit 无调用者)、`daemon_pool` 1,024(64 槽实际注册 ~13)。用户裁决:只做零行为变化的搬运,**排除**会改变可观察行为的选项(C1 遥测环 256→128 减调试窗口、C4 削 FreeRTOS 堆等)。结构级变更已获用户批准(2026-11-22 会话确认,排除影响性能项后按 A1+A2 实施)。

**功能变化(布局)**。①链接脚本新增 `.ccmbss (NOLOAD) >CCMRAM` 段(启动不拷贝不清零、不占 Flash 映像);②`configAPPLICATION_ALLOCATED_HEAP=1`,ucHeap 改由应用侧定义在 `Src/freertos.c` USER CODE 区,放 `.ccmbss` 段 —— 堆内只有任务栈/TCB/队列(纯 CPU 访问,CCM 不通 DMA,安全),heap_4 首次分配自建空闲链无需清零;③5 条遥测环去静态初始化、加 `TELEMETRY_CCM`(x86 主机宏置空)放 `.ccmbss`,新增 `TelemetryInit()` 在 RobotInit(任务创建前)整环 memset 并赋 tag —— CCM 上电是随机值,不初始化则首次 TelemetryPush 按垃圾 write_idx 越界写。

**踩坑(重要)**。段名不能带 `.ccmram_` 前缀:上面既有 `.ccmram` 输出段的 `*(.ccmram*)` 通配会**先**把 `.ccmram_bss` 输入段吞进可加载段,表现为 FLASH +28KB 全零映像(data 55,792);改名 `.ccmbss` 后归位。链接脚本与 telemetry.h 均留了注释警示。

**文件与符号**。STM32F407IGHx_FLASH.ld(.ccmbss 段)、Inc/FreeRTOSConfig.h(configAPPLICATION_ALLOCATED_HEAP=1)、Src/freertos.c(USER CODE Variables 定义 `uint8_t ucHeap[configTOTAL_HEAP_SIZE]` section(".ccmbss") aligned(8))、modules/telemetry/telemetry.h(TELEMETRY_CCM 宏 + TelemetryInit 声明 + 注释更新)、modules/telemetry/telemetry.c(5 环零初始化 + TELEMETRY_CCM + TelemetryInit)、application/robot.c(include + Calib_Init 后调用 TelemetryInit)。无新增 .c,Makefile/CMake 无需登记。

**配置/API/协议影响**。协议零改动;新增对外接口 `TelemetryInit(void)`(modules 层,RobotInit 单点调用);CCM 使用纪律写入链接脚本注释:DMA 目标缓冲(usart_pool.ring、CAN FIFO)禁止放 `.ccmbss`。

**验证**。ARM 编译 0 error、第一方 0 warning;**RAM 119,304→64,368B(91.02%→49.11%),CCMRAM 0→54,944B(83.84%),FLASH 141,616→114,848B(-26.8KB)**;nm 核对 ucHeap@0x10000000、telemetry 环@0x1000xxxx、`.ccmbss` NOBITS 0xd6a0;主机测试 chassis ALL PASS;`tools/check_log_format.ps1` PASS;审计 exit 0(295/104/61/61/4,红旗均为既有项);check-links PASS。**run_ax650_tests 编译失败为既有问题**(cbb90e3 改 bsp_usart.c 未同步测试桩,已入账 RM-012,与本批无关——本批未改该测试编译的任何文件)。未烧录实车,由用户执行:①上电看 [boot]/鸣叫正常、无 HardFault;②RTT `[tel][yaw|pit|chs|chf|sho]` tag 正常;③跟随/小陀螺/射击全流程跑一遍(任务栈/队列已在 CCM);④Ozone 确认遥测环地址在 0x1000xxxx。

**遗留(后续候选,均待批准)**。.ioc 关 USB_DEVICE+USB_OTG_FS(-6.6KB RAM,-text;用户已倾向,批 .ioc 后做);DM 电机池摘构建(-2.2KB);`daemon_pool` 64→24(-640B);链接保留 40KB→12KB(把链接余量 11.7KB 放大,需先实测 libc malloc 峰值与 MSP 栈);FreeRTOS 堆 28K→20K(先加 `xPortGetMinimumEverFreeHeapSize()` RTT 日志实测再定)。

**AI**:DeepSeek-V4.1-Flash,2026-11-22。

## DEV-20261122-03｜2026-11-22|自研框架升级总方案 v2.6.1 批准签署(A→B)与启动

**批准范围(用户会话裁决,AskUserQuestion 记录)**。①范围 = **先批 B0a**(§9 清单式批准,收工全绿后再批下一批);②档位 = 档二·硬日期暂无,按 §7.3 未填 T_season 只授档一授权(B5/B6a-b/B8/B11/B12 锁定);③B0a 四项结构级操作随批授予:RM-012 测试桩修复(tests/)、Makefile 补 bsp_fault.c/bsp_iwdg.c 两条登记、新增 tools G4 自基线执行器、新增 `.Doc/gate/redflags`。

**动作单执行(§9 ①~⑦)**。①§0.1 签署(状态 A→B,批准记录入档)✅;②AGENTS.md Map 两轴三行式(结构轴=自主研发框架升级总方案,功能轴=总计划书冻结)+ skill 同步(evolution-roadmap.md 整体替换为指针/SKILL.md 三处/coding-contract.md §2 批次豁免)✅;③批准文档提交 `da2db9b`(方案 v2.6.1 + CHANGELOG + 参考文献 + 审查七份 + AGENTS + skill + refactor_gate.py)✅;④tag `arch-before-upgrade`(代码树=84d51d7)✅;⑤本条目✅;⑥⑦待执行(seed 采集 + B0a 开工)。

**过程注**。`.Doc/review/` 七份审查文档曾被用户删除(判过期),批准流程中发现后由用户找回,随批准文档一并入库(check-links 跳过该目录的规则保留)。方案正文同时落「批准记录」「两轴」与 v2.6.1 版本注,引用路径零失效(check-links PASS)。

**下一步**。⑥ 从 `arch-before-upgrade` 采种子快照 → `.Doc/gate/seed-arch-before-upgrade/`;⑦ B0a 开工(⓪种子 → ②红旗落档 → ③RM-012 → ④Makefile → ⑤G4 执行器+自检;**①9 项未验证改动实车回填 = 用户执行**,⑥收工重锚 `arch-before-B0` 在实车回填之后)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-04｜2026-11-22|B0a AI 侧五步完成:种子快照/G4 执行器/红旗落档/RM-012/Makefile(待实车回填后收工重锚)

**范围与授权**。B0a 四项结构级操作已随批准授予(DEV-20261122-18);本条执行 ⓪②③④⑤,①实车回填与⑥收工重锚待用户。

**⓪ 种子快照**。`python tools/g4_baseline.py snapshot --ref arch-before-upgrade --out .Doc/gate/seed-arch-before-upgrade`:1069 文件 + sha256(`files.sha256`)+ `git_rev`(commit da2db9b / tag object 7be34b2c 分开记录)+ `env.txt`(gcc_build=15.3.Rel2 15.3.1 / gcc_path=14.2.1 分开记录,G1a 版本门禁以 gcc_build 为准;cmake_cube=4.3.1;clang-format=not-found,B1 交付前需安装)+ `redflags_scan.json`(85 个原始红旗站点,作"新增站点"比对的基线)。排除集=G4 扫描集(build/.Doc/gate/.workbuddy/.zcode/.Doc/review)。**踩坑**:附注 tag 的 `rev-parse` 返回 tag 对象 sha 而非 commit,快照脚本已修(`^{commit}`);PATH 上 gcc 14.2.1 与构建工具链 15.3.1 并存,env.txt 两者分键防 G1a 误杀。

**⑤ G4 自基线执行器**。新增 `tools/g4_baseline.py`(snapshot/check 两模式,纯 git 对象遍历,不扫工作树):snapshot=清单+sha256+env+红旗扫描;check=moved/added/deleted/changed 四计数 + 红旗"新增站点"(站点键=(文件,token),计数增加才算新增,行号漂移不算)+ `.Doc/gate/redflags` 豁免/待裁决记账 + 可选 `--approve` 批准清单逐项核对。**踩坑**:首版把当前树的 git blob sha1 与基线 sha256 直接比较导致 1069 全报 changed,已改为双侧统一 sha256(cat-file blob 内容)。自检:`check --ref HEAD` → changed=1(仅 RM_DEVLOG 的 DEV-20261122-03 条目)/新增红旗 0/PASS。

**② 红旗落档**。新增 `.Doc/gate/redflags`(行格式 path|token|verdict|reason):豁免 2 = chassis.c huart1(Init 句柄注入,锚注 chassis.c:142)+ bsp_fault.c while(1)(NVIC_SystemReset 后不可达,锚注 bsp_fault.c:61,不进 T02 台账);待裁决 2 = gimbal.c huart6(疑同合规口径,B6b 收编)+ ins_task.c while(1)(DEV-20261116-06 已裁决启动期 fail-fast,B6a 收编)。G4 口径 4→2 达成(与方案 §5-G4 一致)。audit_baseline 仍显示 4(文本正则不解析锚注,预期行为,豁免以 G4 redflags 文件为准)。

**③ RM-012 修复(ax650 测试桩对齐循环 DMA 架构)**。cbb90e3 把 bsp_usart.c 重构为循环 DMA(DEV-20261121-06)后测试桩仍是旧 ReceiveToIdle 语义。修复全部在 tests/ 侧,bsp 零改动:`tests/ax650_stubs/main.h` 补 DMA_HandleTypeDef(Init.Mode/FIFOMode + 伪 NDTR 计数)、`__HAL_DMA_GET_COUNTER`、`__HAL_UART_CLEAR_OREFLAG`(no-op)、HAL_DMA_Init/HAL_UART_Receive_DMA/HAL_UART_AbortReceive 声明,`hdmarx` 从 void* 改类型指针;`tests/ax650_uart1_test.c` 删 ReceiveToIdle/RxEventCallback 路径,改为 **FeedBytes(按 写位置=容量-NDTR 写环 + 回绕)+ USARTProcessAll() 消费泵**,末尾断言 `rx_calls==1`(循环 DMA 永不 re-arm,旧断言 2 来自 NORMAL 模式软件重启)。验证:`run_ax650_tests.ps1` **ALL PASS(O2/Ofast 双档)**。

**④ Makefile 补登记**。bsp_fault.c/bsp_iwdg.c 两条入 C_SOURCES;`check_source_lists.py` PASS(138 个)。

**G 闸门证据**。G2:cmake(bundle 4.3.1)+ninja 0 error、第一方 0 warning,text 113,988/data 852/bss 118,464、CCMRAM 83.84% —— 与 §0.2 基线逐字节一致(注释改动不进代码段);G3:chassis ALL PASS + ax650 ALL PASS;审计 295/104/61/61/4 与 §0.2 一致;check-links PASS。

**遗留(B0a 收口条件)**。①9 项未验证改动实车回填(8 行为 DEV-20261121-04~11 + 1 零行为 RAM DEV-20261122-02)= **用户执行**;⑥实车回填后 `save-baseline` 重锚 `arch-before-B0` + G4 manifest 由 tag 生成入库 → B0a 状态 C。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-05｜2026-11-22|B0a-① 9 项未验证改动实车回填完成(用户实车确认)→ B0a 收口

**验证结论(用户实车口头确认,2026-11-22,采信入档)**。9 项全部实测通过、无问题:
- **8 项行为**:DEV-20261121-04(半死态修复+0x0208 发弹额度)、05(错误风暴→急停失效根因)、06(循环 DMA+任务消费+IWDG)、07(开机鸣叫码+射速护栏)、08(解析迭代化+HardFault 死循环)、10(日志 %f 参数错位→HardFault)、11(射速律恒定摊平+战术档位+本车 UI)——解锁后全流程(底盘跟随/小陀螺、射击、接裁判系统)实测正常;
- **1 项零行为 RAM**:DEV-20261122-02(CCMRAM 搬迁)——上电无 HardFault、遥测五环在 CCM(0x1000xxxx,nm+运行时索引推进)、`[tel]` 出流。

**探针旁证(非采信依据,仅记录)**。RTT 会话跨测试前后累计 ~470s 运行:`[FAULT]`/HardFault/E: 风暴/IWDG=1/二次重启全部 0 命中;uptime 759s 时各环正常出流;ELF↔固件 sha256 一致。

**状态迁移**。B0a ①~⑥ 全部完成 → 方案 §0.1 状态 **B→C(升级期)**;收工动作:save-baseline 重锚 → tag `arch-before-B0` → G4 manifest 由 tag 生成入库 `.Doc/gate/manifest-arch-before-B0/` → `refactor_gate check --no-tests` 输出随本批提交。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-06｜2026-11-22|B0 清理锚定批完成:台账/删除 2133 行/T13 契约/G8 对拍器/整链 harness/过滤器金标准

**交付物(全部入库,提交链 0e1…→495d245)**。
- **⓪ 逐站点台账**:`tools/site_ledger.py` + `.Doc/gate/site-ledger.csv`(口径 84d51d7,先剥注释;94 有效站点 = while40/for7/**malloc47 与 T07 同源**;T02 白名单 10 点行号精确命中;ist8310.c:93 注释自动剥离、bsp_fault.c:61 归"复位后不可达不进台账";登记 B3/B0/B7/豁免分类)。**禁止手数**。
- **①② 删除 2 提交**(abf74ae/8a06159):modules 死件(oled/ist8310/蓝牙/TFmini/LK)1393 行 + bsp_gpio/iic/spi/bsp_tools 740 行 = **2133 行;text −100B/bss −48B**(modules 部分本就被 --gc-sections 回收故 text+0)。G4 对 `arch-before-B0` manifest 记账:deleted=19/changed=3/added=1 全在批准清单。
- **③ 附D.4 死项**(87503f9):zmalloc/abs_limit/sign/float_deadband/int16_constrain/loop_float_constrain/theta_format/NormOf3d/MatInit/VAL_LIMIT/rad_format 11 项,外部引用逐一 grep 验零;`sign` 的 9 处命中核实为 calib 形参名。
- **④ 乱码修复**(4b1bb7a):bsp_flash.c 21 处 GBK 双编码残片(地址/开始/获取/下一页/指针/写入/写数据),user_lib.h 残留乱码随死项删除、存留项注释按语义重建。
- **⑤ T13 工具符号契约**(fc6a714):`tools/check_symbol_contract.py`(--update/校验两模式)+ `.Doc/gate/tool-symbols.json`(**35 路径/10 根**,9 工具脚本;排除 EmberProbe API 名/RCC SVD 路径/注释散文);nm 根符号命中 **10/10**。B5/B6 改名前先跑 --update 对比漂移。
- **⑥a G8 对拍器**(22da4e0):`tools/check_golden.py` —— rtol/atol ≤1e-6 + 模式字/枚举精确相等 + tick 逐拍对齐,内置 7 例自检全过。
- **⑥b host 整链 harness**(17a4a88):`tests/fullchain_harness.c` + runner。真实编译 robot_cmd 五件/gimbal+recenter/shoot+heat_guard/chassis+kinematics/**dji_motor 级联 PID**/remote/message_center/daemon/telemetry/calib(无标定回退)/controller/user_lib;桩=usart 直注入/can 记录帧+反馈注入/referee/UI/fault/flash/supercap/DWT。反馈按 **C620 报文格式**造字节走真实 DecodeDJIMotor;电流→转速模型为 harness 固定常量。确定性脚本 700 拍(1kHz 级联+200Hz 链):解锁→跟随→vx→vy+wz→拨轮扳机→单发→急停;金标准 `.Doc/gate/arch-baseline/g8b-fullchain.csv`(140 行×36 列,轮组/摩擦轮/yaw/**重力前馈**电流闭环全活)。**两次运行逐字节一致**。
- **⑥c CAN 过滤器金标准**(495d245):`tests/can_filter_golden.c` 用真实 bsp_can.c 注册路径按 robot.c init 序注册 9 电机 → `.Doc/gate/arch-baseline/can-filter-golden.csv`:bank 按 bus 分段(c1 0-4/c2 14-17)、**FIFO 归属=注册序奇偶**逐字段入册;两次运行逐字节一致。B4a 重放后 diff 必须为空。

**记录在案的方案偏差**。①删除两提交**顺序对调**:ist8310.h/tfminiplus.h include bsp_iic.h/bsp_gpio.h,按方案原序先删 bsp 会断编译(§6-B0 行下次评审回写);②枚举值 `LK9025` 保留于 Motor_Type_e(motor_def.h,删值会使后续枚举移位——遥测/CAN 语义风险),仅删 motor_task.c:9 include;③金标准 harness 范围注记:bsp_usart 消费泵语义不在整链金标准内(ax650 测试+B4b 对拍承载),remote 采用与既有测试同构的回调直注入;④yaw GM6020 走 0x2FE 电流帧(dji_motor.c group2),过滤器/整链 CSV 均已含 c1_02fe 列。

**闸门证据**。G2:cmake+ninja 0E、第一方 0 warning(newlib 桩警告为基线既有);text 113,888/data 852/bss 118,416;G3:chassis ALL PASS+ax650 ALL PASS(O2/Ofast)+整链 harness PASS+对拍器自检 PASS;G4:两次收口记账 PASS;G10:check_source_lists PASS(129 条);审计 295/104/61/61/4(红旗=已收编项,G4 redflags 豁免2+待裁决2)。check-links PASS。

**收工**。save-baseline 重锚 → tag `arch-before-B1` → G4 manifest `.Doc/gate/manifest-arch-before-B1/` → refactor_gate check 全绿输出随批提交。下一批 = **B1 框架地基+闸门工具包**(工具先行,7~10 会话),开工需用户清单式批准。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-07｜2026-11-22|B1 框架地基+闸门工具包完成:工具先行/骨架落地/契约+宏账本

**① 工具先行(扩目录前交付,7 条 fixture 全演示)**(cee5331)。
- refactor_gate.py 扩:**G1a 版本门禁**(gcc_build=C:/Toolchain/bin 15.3.1 vs 最新 manifest env.txt,失配 FAIL)+ **G2 三断言**(CMAKE_BUILD_TYPE=Debug / 编译单元数==磁盘 .c 数(第一方边界 85) / 编译 FLAGS 无 -flto;LINK_FLAGS 的 -flto 对非 LTO 目标惰性,不在判据)+ **G5b --budget**(text 增长 % 上限,B4 起强制)+ **G6 --mode move/rewrite + --paths**(move=行多重集+include diff 单列;rewrite=逐文件增删行)+ **--expect-audit 部分声明语义**(未声明键沿用基线)+ **--expect-new-symbols**(新框架符号声明式豁免)。
- 独立工具:`check_sections.py`(G5c:readelf -S 解析,.ccmbss VMA∈[0x10000000,0x10010000)+CCM≤90%/主RAM≤70%,当前 83.8%/49.7-50.1%)、`check_tags.py`(G7:DEV 条目+标签链)、`check_plan.py`(方案机器自检:ARCH-DEC 行首 6 条/G.2 唯一存留/M3′ 统一/§13 关键词)、`tests/run_fwk_tests.ps1`(G3 聚合:对拍器自检+符号契约+can golden+整链确定性回归+kernel 用例)。
- **fixture 记录**:G1a 失配 FAIL(临时 seed-fixture env 注入)✓;G2 单元数不等 FAIL(开发期实测 0 vs 81)✓;check_sections 阈值 1% FAIL ✓;--budget 负值 FAIL ✓;--mode 无 --paths FAIL ✓;check_tags 非法批次/缺 DEV FAIL ✓;check_plan 抓 §13 关键词缺(修复:已交付项标 ✅ 落真实路径,方案自维护规则)✓。

**② 框架骨架落地**(e864c85)。
- `framework/kernel/kernel.h`:平台无关 API,**零 RTOS/CMSIS/HAL include**;优先级钉死 NORMAL=3/HIGH=4/RT=5 + 文件尾 _Static_assert;15+3 原语含 is_in_isr/current_task/start(T05 要求)。
- `framework/err/`:四档分投递(LOG_ONLY 无锁环 32 槽满丢最旧计溢出/SAFE_STATE 原子标志/RESTART/FAILFAST_BOOT 注册钩子);**计数器踩坑**:按低 8 位索引时 0x0101(KERNEL_ASSERT)与 0x0201(MEM_OOM)撞槽 → 改 64 槽关联表。临界区禁 err_report 的保证由 port 断言(B3 接线收口)。
- `framework/mem/`:唯一入口,bump 池(调用方注入缓冲)+OOM 计 err;链接期 --wrap 在迁移批落。
- `framework/kernel/kernel_version.c`:`fw_abi_anchor=0xB10C0DE` 放 **.fw_anchor 段 + 链接脚本 KEEP**(零引用符号被 --gc-sections 回收的坑);G2 断言 b 的 nm 锚点。
- `ports/freertos/kernel_freertos.c`:**RTOS include 唯一合法落点**;xTaskCreate 直用;BASEPRI 临界区(FROM_ISR 变体);is_in_isr=__get_IPSR;time_us=DWT_CYCCNT;**configASSERT 改造出口** = err FAILFAST + NVIC_SystemReset(行为变化点:断言路径挂死→受控复位,随 B1 批准;旧实现 arch-before-B1)。
- FreeRTOSConfig.h USER CODE 区:三处手改的静态断言守卫(堆≥28000/APPLICATION_ALLOCATED_HEAP==1,防 CubeMX 再生成回退)+ configASSERT 宏改造。
- CMake:framework/ports 入 GLOB+include 递归;**framework_core 独立静态库 target**;G2 单元数 81→85。
- 主机桩 `tests/fwk_posix/`:`fw_kernel_test.c` 全绿(双线程临界区互斥 2×20000 无丢失/任务登记/err 四档/mem 对齐 OOM/锚点)。**偏差:pthread → Win32 CreateThread + stdatomic 自旋锁**(本机主机编译器 MinGW.org 6.3 无 pthread.h;双线程语义不变)。
- `check_source_lists.py`:默认树扩 framework/ports + `--trees` 临时扩扫(G10);Makefile 补框架 4 文件登记(133 条一致)。

**③ 宏账本**(87406b7):`tools/check_macros.py`(--update/校验)+ `.Doc/gate/macro-ledger.csv`:**220 宏**(与 §0.2 同源)五类 A177/B9/C6/D17/E11;跨子文件 #if 违例 2 处(已知 ax650→chassis 反例)入账本登记,只减不增,治理目标=消除。工具踩坑:DEFINE_RE 漏 re.M 导致全 0(修);账本完整性判断反了(伞头无宏属正常,只 FAIL 文件消失)。

**④ G4 红旗扫描口径统一**(d5f04b4):g4_baseline 剥注释(err.h 文档里的 while(1) 字样不再计站),strip_comments 上移 g4_baseline 消除与 site_ledger 的双份实现;G4 new_sites=0。

**闸门证据(B1 收口)**。refactor_gate check --no-tests --expect-audit "added=72" --expect-new-symbols(16 个框架符号) --allow-linkage:**ALL PASS**——G1 宏逐字节/G1a 版本/G2 三断言+0E/G4 291/89/**72 增**(68 基线+4 框架文件)/尺寸 text 114,524(+636=框架骨架,「+0.558%」)/bss 119,200(+784=err 环+计数表)。**G5 --allow-linkage 声明**:FreeRTOS 内核 TU(pvPortMalloc/vTaskDelay/xPortStartScheduler 等 6-7 符号)尺寸变化 = configASSERT 改造波及,预期内。**G5 不适用说明**:G5a/G5b 属移动批闸门,方案 B1 闸门列无 G5;本次以声明式通过作为迁移批的口径预演。fw_version 函数 ARM 侧被 GC(GC 正常),nm 锚点=fw_abi_anchor 恒存活(KEEP);fw_kernel_test 主机侧覆盖 fw_version。check_sections PASS;G3(run_fwk_tests)7 项全 PASS(含整链确定性回归)。

**范围注记**。§4 契约定稿 = kernel.h/err.h/mem.h 即定稿载体(方案 §4 文本已定,头文件为实现);§13 check_plan 机器断言纳入日常 G3。方案行"CMake 接线锚点 xr_version"→ 落地为 `fw_abi_anchor`(v1.5 去品牌残留,回写方案时更正)。

**收工**。save-baseline 重锚 → tag `arch-before-B1b` → G4 manifest `.Doc/gate/manifest-arch-before-B1b/`。下一批 = **B1b 工程风格批**(clang-format 全仓单次提交,仅批间隙,队友停写,单日完成,G11 等价闸门)——需要 clang-format 可执行(当前 not-found,需用户安装 LLVM 15+ 或指定路径)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-08｜2026-11-22|B2 真-发布订阅完成:framework/msg + 适配头 + 对照表 12 行

**裁决记录**。B1b 工程风格批**后移**(用户 2026-11-22 裁决:clang-format 未安装;按方案 B1b 行"单日完不成则后移 M3 前"),关键路径直进 B2。B1b 前置不变:LLVM 装 + 批间隙 + 队友停写。

**交付**(18fdcdb)。`framework/msg/`(msg.h/msg.c):深度 1 只保最新(覆盖未取帧)、多订阅链表(message_center 机制保留)、话题上限 **8 实施检查**(替换死宏 MAX_TOPIC_COUNT=12;现用 6 话题余量 2)、data_len ≤255 超长拒绝、**msg_publish 非 ISR**(ISR 调用丢弃 + drop 计数,方案 §4)、**CheckLen/CheckName 挂死改返 NULL**(T04 缺陷修正,对照表 #5/#9)、msg_publish 返回真实投递数(旧实现恒返 1 的修正)。**适配头** `modules/message_center/msg_adapter.h`:PubRegister/SubRegister/SubGetMessage/PubPushMessage 四旧名转调 msg_*,**PubPushMessage 保留恒返 1**(调用方零行为;B7 切换时整体替换)。**对照表** `.Doc/gate/msg-semantic-map.md`:12 行逐项(6 一致 + 6 修正已声明,零稳态行为变化)。**fwk_msg_test**:roundtrip/覆盖/多订阅真计数/上限与守卫不挂死/适配头/**Win32 双线程 1000 帧并发后必取末帧**(深度 1 确定性),全绿并入 run_fwk_tests。

**闸门**。G1a PASS/G2 三断言 PASS(单元数 86 = 85+msg.c)/G4 --expect-audit added=74(72+msg.c+msg_adapter.h)PASS/G5 新符号声明 6 个 PASS/尺寸 text 114,524(+0,框架 msg 未被业务引用,期望:B7 切换后其调用链才进)/check_sections PASS(CCM 83.8%)。run_fwk_tests 12 项全 PASS(含整链确定性回归)。

**范围纪律**。msg_publish 的深度>1 为新能力**未实现**(T04:有调用者再议);适配头不切源码(application/ 25 处调用点 B7 才切换);对照表 B7 切换时逐行复核。

**收工**。save-baseline 重锚 → tag `arch-before-B2` → G4 manifest `.Doc/gate/manifest-arch-before-B2/`。下一批 = **B7 消息切换**(25 处生产调用点+主机测试 8 处切 msg,删适配头与 message_center,BASEPRI 临界区 ≤20µs 实测,2~3 会话)——B7 与 B2 相邻执行(方案 §4:生产调用点只改一遍)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-09｜2026-11-22|B1b 工程风格批完成:全仓格式化 + 证据迁移 + 署名规则

**前置**。clang-format **23.1.6** 安装(C:\LLVM\bin,用户下载 LLVM 23.1.6 x86_64-msvc tar.xz 解压;版本入 env 钉死口径)。范围裁决:全做(格式化+证据迁移+署名规则,AskUserQuestion 记录)。

**① 全仓格式化**(0182ebd):`.clang-format`(LLVM 基底定制:4 空格/列宽 100/Allman 函数大括号/尾注释对齐/**SortIncludes=false**——include 顺序是语义敏感区永不重排);120 文件 **+3689/−2892**;范围=application/bsp/modules/framework/ports/tests,**排除 Src/Inc/Drivers/Middlewares**(USER CODE 纪律)。**踩坑**:4 文件非幂等(注释对齐震荡),二次 -i 到不动点后重放 0 diff。

**G11 四项等价全过**:①重放 0 diff(全量 dry-run -Werror)②ARM 0E0W ③size **114,524/852/119,200 逐字节同**+nm 符号集合等价 ④**include 序列 137 文件全同**(比"集合 diff 空"更强,顺序也未动)。G2 断言全 PASS。

**② 证据迁移**(febda5c,N13/F3):`tools/…` 迁移脚本(临时目录,未入库——一次性脚本)执行:**134 卡瘦身**(config 7 子头 +134/−576,卡片=「[等级] 一句话+指针」,细节块/失效/标法/风险原样迁 `.Doc/spec/参数证据表.md`「B1b 迁移增补」按宏名键控);**G1 宏集合逐字节一致**(只动注释)、size 逐字节同;证据表发现即已有体系(11-09 建/11-20 复核,六等级制),故为增补而非重建。**注:发现参数证据表既成体系后,迁移形态从"重建"改"增补"——卡片细节保真搬运,未做二次加工。**

**③ 署名规则落地**(同提交):注释规范 §2 修订——`@author 小猫大王` 署名规则(本队新建/重写;上游衍生保留原行+追加 modified by;版权行不动)替换原"不要写 @author"条款;M3 验收口径不变。

**闸门**。refactor_gate ALL PASS(--expect-audit 278/102/74 声明式:13 注释文件 unchanged→changed 为预期;G5 新符号沿用 B1 声明+msg 6 个;--allow-linkage 沿用 configASSERT 波及);G2 三断言 PASS(单元 86);run_fwk_tests **24 项 PASS**(kernel/msg/golden/契约/整链确定性);check_sections PASS(CCM 83.8%)。

**范围注记**。Src/Inc/Drivers/Middlewares 永久排除(AGENTS 禁改+第三方);`.agents` 上游快照不格式化;G11 的 include 检查升级为**序列级**(顺序敏感,#if 依赖)。clang-tidy naming 配置(B1 交付的 README 部分)随 B5/B6 迁移文件启用,本批仅交付 clang-format 面。

**收工**。save-baseline 重锚 → tag `arch-before-B7`(B1b 落在 B2 后,下一批 B7)→ G4 manifest 入库。下一批 = **B7 消息切换**。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-10｜2026-11-22|B7 消息切换完成:25 调用点切 msg,删除 message_center 与适配头

**裁决记录**。用户批准 B7 开工(会话确认);B1b 落在 B2 之后(DEV-20261122-09),本批为关键路径第 5 批。

**切换内容**(13 调用点实测,非方案预估 25:robot_cmd 6/gimbal 2/shoot 3/chassis 2 = 13 处 API 调用 + 4 文件类型句柄改写):robot_cmd/gimbal/shoot/chassis 四真用户的 `#include "message_center.h"`→`"msg.h"`,PubRegister→msg_pub、SubRegister→msg_sub、SubGetMessage→msg_recv、PubPushMessage→msg_publish(机械替换脚本,未入库);句柄从 `Publisher_t *` 改 `msg_pub_t`(值句柄)。**7 个零使用文件的 include 残留直接删除**(cmd_authority/remote/autoaim/kbm/gimbal_recenter/chassis_kinematics/referee_vt_task,grep 实测 0 API 使用)。删除 message_center.c/h + msg_adapter.h(B2 过渡件)。

**语义补齐(msg.c 重写)**。旧 SubRegister 内调 PubRegister → **订阅先于发布合法**;robot_cmd.c 的 feed_sub 注册早于 GimbalInit 的 pub,这是切换零行为的前提 —— msg_sub 改为 topic_get_or_create(自动建话题,长度冲突仍 NULL)。对照表 #4 更新:13 处调用点 grep 实证**零消费发布返回值** → 真实计数零行为,适配头随删。fwk_msg_test 适配用例随删(使命完成)。

**踩坑(一次批内三次)**。①heredoc 里写 `'\0'` 被 JSON 层解成真 NUL 写进 C 文件(第 2 次);②补丁锚文本跨 msg_pub/msg_sub 两处相似结构,插入点错位致括号失衡(第 3 次);③stub 插进函数签名与函数体之间。msg.c 最终**全文重写**收敛;教训:补丁式改小文件连续失败 3 次即应全文重写。测试脚本 include 面:framework/msg 需配 framework/kernel+err(kernel.h/err.h 被 msg.c include),framework_is_in_isr 主机桩补进 chassis_remote_test/fullchain_harness。

**闸门**。G1a PASS/G2 三断言 PASS(单元 86→85:−message_center.c+msg.c 已在)/G4 --expect-audit 278/100/**82 missing**(message_center.c/h 删除 vs 上游)/73 added(适配头删出)/G5 --expect-gone-symbols(旧 mc 符号 10 个含 strcpy/strnlen)+ --expect-new-symbols(msg 16 个)PASS;G3:chassis ALL PASS(**msg 后端**)/ax650 4 PASS/fwk 聚合全 PASS/整链 700 拍确定性 PASS。尺寸 text 114,568(+44)/bss 123,792(+4,592=msg 静态池替代 message_center 的 malloc 订阅者,堆占用反降);check_sections PASS。

**BASEPRI 白名单注记**。msg 路径**零临界区**:单写者同拍纪律继承 message_center(无锁),kernel_enter_critical 尚无调用者 —— 方案 §5 白名单"B7 BASEPRI ≤20µs 实测"项**转 B3**(diag/err 接线为首个 BASEPRI 使用者,届时 DWT 实测)。§5 白名单行下次评审同步。

**收工**。save-baseline 重锚 → tag `arch-before-B3` → G4 manifest 入库。下一批 = **B3 diag/监督+err 接线**(串行,先于 B4a;独占 dji_motor/bsp_can/ins_task 的 err/diag 改动;G9 自动化钩子)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-11｜2026-11-22|B3 diag/监督+err 接线完成(行为批):心跳位掩码/IWDG 门控/G9 钩子/白名单锚注

**交付**。`framework/diag/`(diag.h/c):任务级心跳监督——必活 {INS,MOTOR,ROBOT,DAEMON} / 可选(注册即豁免 + unregister,修"UI 合法离线被当死"毁车缺陷);**超时单位=拍数不变量**(拍源=DaemonTask 100Hz,与 daemon 同源);喂狗只看必活。接线:robot_task.c OSTaskInit 注册 4 必活(INS 30 拍/MOTOR 30/ROBOT 20/DAEMON 10)+四循环 kick+DaemonTask 推拍;robot.c IWDGFeed 门控(失活限流留痕 1Hz)。

**err 四档消费接线**(robot.c RobotTask):SAFE_STATE 首拍消费 → robot_state=ROBOT_STOP(B1 err 模块首次产生行为效果);RESTART → 受控复位;LOG_ONLY → 排水 LOGERROR(环容量 32 天然限流)。UI 不注册=可选豁免。G9 钩子 `bsp/diag_test/dg_test_hang.c`(-DDG_TEST_HANG):启动延时后自动挂死指定必活任务,验收=复位 ≤3.0s+IWDG=1+复位后需解锁;Makefile 已登记(G10)。

**T02 白名单锚注**(B0 台账口径):dji_motor 5/ins_task 1/bsp_can 2 = 8 站点 `T02-WHITELIST` 注释;台账 10 点中 message_center 2 点已随 B7 删除,余 8。**台账一致性**:site_ledger 重跑白名单 10/10 命中不变。

**闸门**。G1a/G1(宏逐字节)/G2 三断言 PASS(单元 86)/G4 声明式 PASS(278/100/82/74)/G5 --expect-new-symbols(诊断+err 消费 19 符号)+--allow-linkage PASS/G5c PASS(CCM 83.8%,主 RAM 50.4%)。尺寸 text 115,720(+1,196=diag+接线)/bss 124,160(+368=监督表)。G3 四套件全绿。

**行为变化登记(§5 白名单)**。①IWDG 喂狗从无条件改必活门控——任一必活任务失活即复位(修真实安全差距,P0 类);②configASSERT 挂死→复位(B1);③err SAFE_STATE 使 robot_state 可被框架层置 ROBOT_STOP(框架→应用方向的新控制面)。**G9 实测回填**:用户口头确认通过(2026-11-22,口头采信规则);RTT 旁证 = 常规构建 116s 实车行驶零误复位/零 [diag] dead(正常路径成立)。**B3 完全关闭**。

**收工**。tag `arch-before-B4a`(G9 实测回填后 B3 完全关闭;B4a/B5/B6a 从本批 tag 重锚——本批独占的 dji_motor/bsp_can/ins_task err/diag 改动已就位)。下一批 = **B4a CAN 总线**(bus 接口+ports 逐函数搬运+FIFO 等价表 can_filter_golden diff 空)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-12｜2026-11-22|B4a CAN 总线迁移完成:ports 化 + §4 增补 + 等价表实战首胜

**交付**。`git mv bsp/can/bsp_can.c|h → ports/stm32f4/can_port.c|h`(历史保留,R 标记),`bsp/can/bsp_can.h` 转**转发 shim**(调用方零改动;B8 随 bsp/ 删)。**§4 增补**:①TX FIFO 满丢最老**计数**(`CAN_GetTxFifoDropCount`)+ 日志**限流 1/100ms**(旧实现每次满都打,风暴占满 RTT);②filter bank **预算断言**(can1<14/can2<28,越界 FAILFAST);③**ConfigFilter 返回值必查**(失败=过滤器未生效=收不到帧,FAILFAST);④**伪实例 sender_assignment → bus_tx_frame_t**(仅 can_handle/tx_id/tx_mailbox/tx_buff,246→120B;CANInstance 前 4 字段前缀兼容,7 处调用点显式转换:can_comm 1/dmmotor 3/HT04 3/super_cap 1——can_comm/DM/HT 为留置桩,允许指针转换)。err 新码 ERR_CODE_CAN_FILTER=0x0301。

**等价表实战首胜**:`can_filter_golden` 换编 can_port.c 重放 → **diff 空**(9 行逐字节);golden 脚本补 DWT 假钟桩(can_port 限流引用)。

**闸门**。**G5b --budget 首次强制 PASS**:text +236B ≤ 1% 上限 1,157B;G5 --allow-linkage 声明(CANAddFilter/CANSendFIFOPut 增补膨胀;sender_assignment 246→120B=伪实例瘦身;DJIMotorControl/MotorSenderGrouping 随换型微变);G4 声明式 PASS(278/99/**83 missing**(bsp_can.c 删)/74 added(can_port.c));G2 三断言 PASS(单元 87)/G1a/G1 逐字节;G10 PASS(135 条;**发现并补齐 B2/B3 漏登记的 msg.c/diag.c——教训:G10 必须显式跑,refactor_gate check 不含它**)。G3 四套件全绿(chassis 适配 msg+can_port 后端/ax650/框架聚合/整链确定性)。check_sections PASS。

**范围注记**。Bus-Off 检测未入本批(B4a 行无此项,T06 目标后续);CANTransmit 双类型兼容用 **C 前缀结构模式**(bus_tx_frame_t = CANInstance 前 4 字段,文档化);dji_motor.c DJIMotorLostCallback 里的 `motor_can_instace` 拼写错误为 HT04 自有字段(留置桩,附E.2 不动项)。

**收工**。save-baseline 重锚 → tag `arch-before-B4b` → G4 manifest 入库。下一批 = **B4b USART 总线**(循环 DMA 语义搬运;RING_SIZE 256→≥1024 行为白名单;错误风暴回归;台架)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-13｜2026-11-22｜B4b USART 总线迁移完成:ports 化 + RING 扩容白名单 + 追平计数

**交付**。`git mv bsp/usart/bsp_usart.c|h → ports/stm32f4/usart_port.c|h`(历史保留),`bsp/usart/bsp_usart.h` 转发 shim。**RING_SIZE 256→1024**(§5 行为白名单 B4b,方案 v2.6 预登记:921600bps 下 256B≈2.8ms<5ms 消费周期=丢帧行为变化;1024B≈11ms 覆盖抖动)。**消费追平覆盖计数**(方案 §4):avail>RING-1 时序 overrun_cnt++ + 限流留痕 100ms(时序源 HAL_GetTick——ax650 桩无 bsp_dwt,踩坑记)+ `USARTGetOverrunCount` 接口。循环 DMA 语义零改动:先填后推/覆盖未读/回调零消费(不变量)。

**闸门**。G5b --budget PASS(text +188B ≤ 1,160B);G4 声明式 PASS(278/98/**84 missing**(bsp_usart.c 删)/74 added(usart_port.c));G2 三断言 PASS(单元 85:−bsp_usart.c+usart_port.c,纯搬移)/G1a/G1 逐字节;G5c PASS(主 RAM 55.1% ≤70%,RING 扩容 +2.3KB 入预算;CCM 83.8%)。G3:ax650 **4 PASS(RING 1024 自动适配,断言 USART_RX_RING_SIZE 同源零改)**/chassis ALL PASS/fwk 聚合 9 PASS/整链 700 拍确定性 PASS。

**踩坑**。usart_port 限流初版用 DWT_GetTimeline_ms → ax650 桩无 bsp_dwt 隐式声明失败 → 改 **HAL_GetTick**(三平台皆有);CMake 重跑时机落后于 git mv(缓存仍引用旧路径)——git mv 后必须立即重跑 cmake。

**待实测(用户台架)**。错误风暴回归(关遥控/拔线,ORE 风暴不复发的 B0a 修复回归);RING 扩容后的实际消费余量(overrun_cnt 应恒 0)。

**收工**。tag `arch-before-B4c` → manifest 入库。下一批 = **B4c 其余总线**(pwm/flash/usb/dwt/kernel_time_us 化;bsp/ 实现全部迁完 + 原头转 shim,目录 B8 删;bsp_init.h 职责并入)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-14｜2026-11-22|B4c 其余总线迁移完成:bsp/ 实现层全部 ports 化(pwm/flash/usb/dwt/fault/iwdg/log 七模块)

**交付**。`git mv` 七模块 → `ports/stm32f4/{pwm,flash,usb,dwt,fault,iwdg,log}_port.c|h`(历史保留)+ 7 转发 shim。**text 逐字节同(116,144,+0)**——纯搬移零语义。dwt_port cmsis_os include 零使用零残留(检查实证)。Makefile C_SOURCES 同步(G10 PASS,135)。

**闸门**。G5b --budget PASS(**text +0B**)/G1a/G1 逐字节/G2 三断言 PASS/G5c PASS。G3 四套件全绿(ax650 4/chassis 1/fwk 9/整链 700 拍确定性)。G4 声明式 PASS(277/94/**89 missing**/72 added/**3 红旗**)。

**口径注记**。audit_baseline 的红旗扫描面**不含 ports/**(搬运后 fault_port.c 的 while(1) 退出其计数,4→3)——红旗的权威机制 = **g4_baseline**(80 站点全覆盖,剥注释),audit 仅作上游对比历史证据。方案 §5-G4 行"audit 扫描集"措辞回写时同步。dwt_port 的 cmsis_os include 零使用(检查实证),§3.2"后端唯一落点 ports/freertos"措辞不受影响。

**状态**。bsp/ 实现层**全部 ports 化**:目录内仅剩 8 个转发 shim + bsp_init.h(B8 并入)+ bsp_adc 0 字节占位(ARCH-DEC-006)+ diag_test(G9 钩子)。下一批 = **B5 组件 I**(DJI/GM6020/PID/蜂鸣迁 components;`can_handle==&hcan1`→bus_id 白名单等价表;G5b+G8a)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-15｜2026-11-22｜B5 组件 I 迁移完成:components/{motor,algorithm,alarm} 落地 + bus_id 白名单

**交付**。11 文件 `git mv` → **components/**(历史保留):motor/{dji_motor.c·h,motor_task.c·h,motor_def.h} + algorithm/{controller.c·h,user_lib.c·h} + alarm/buzzer.c·h。modules/ 剩余:HT/DM/servo/step 留置与 B6a/b 项 + kalman/QuaternionEKF/crc(B6a/b)。CMake(GLOB+include 递归)/Makefile(C_SOURCES+include 面)/三个测试脚本同步。**零 shim**：名字不变+路径更新即可(B4a/b 的 bsp shim 因 bsp 目录保留其他头而必需,components 直接换 -I)。

**bus_id 白名单(方案 §5-B5)**。`can_port.h` 增 `can_port_bus_of(handle)`(0=can1/1=can2,抽象层不触 &hcan1 符号);dji_motor 六处 `== &hcan1` 换 bus_of 三元式(M3508 组 1/4、GM6020 id1-4 组 0/3、id5-7 组 2/5、Lost 日志 1/2)。**等价表**:bus_of(h)==0 ⇔ h==&hcan1 恒等(主实现+fullchain 桩同语义)→ 分组/日志/发送序不变;fullchain 金标准 700 拍重放逐字节一致即等价背书。制作组行数据提示:首版换型把组号表写错(bus_of 直加 2 破坏 0/3、2/5 映射),fullchain 未拦(分组只影响静态初始化不影响 CSV)——**人工梳理分组表后修正**，足见 G8b 金标准对“不影响 CSV 的错”仍需人工审查。

**闸门**。G5b PASS(text +0B ≤ 1%)/G4 声明式 PASS(277/83/**100 missing**(11 文件搬家 vs 上游)/72 added(components 11 新路径))/G5 符号集合不变(can_port_bus_of 声明)/G2 三断言 PASS(单元 87)/G10 PASS(135、components 入默认树)/G5c PASS。G3 四套件全绿。踩坑:CMake 需 components GLOB+include(两处都要);check_source_lists 默认树补 components。

**收工**。tag `arch-before-B6a` → manifest 入库。下一批 = **B6a 组件 II-敏感**(IMU/BMI088+INS+EKF 迁 components/imu、algorithm/kalman+QuaternionEKF 入 components/algorithm;kalman 启动期 malloc 豁免按调用点登记;gimba_IMU_data 路径冻结;G5b+G8a)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-16｜2026-11-22|B6a 组件 II-敏感迁移完成:components/imu 落地 + gimbal_imu_data 改名(E.2/T13)

**交付**。`modules/imu` 全部(BMI088driver/BMI088Middleware/ins_task + BMI088reg)+ `modules/BMI088/bmi088_regNdef.h` → **components/imu**;`kalman_filter/QuaternionEKF` → **components/algorithm**。**gimba_IMU_data → gimbal_imu_data 搬家即改名**(E.2 首实战):11 文件 56 处(gimbal.c 18/工具 7 文件 33/harness 1)——T13 冻结清单刷新(`check_symbol_contract --update`,35 路径)后 **nm 契约 10/10 背书改名**。kalman 启动期 malloc 豁免登记:27 处留 components/algorithm/kalman_filter.c(调用点 = Kalman_Filter_Init 链;T07 口径「稳态零 malloc」不变)。

**运维教训(重大)**。本批 git mv 与改名完成后**未提交就跑了 g4 检查**——HEAD 仍是旧树,检查假 PASS;提交后再查 → 10 个白名单站点在新路径下变"新增"。修正:redflags 白名单路径随搬家(ins_task 条目 → components/imu/)+ 标准收口顺序 = **先提交 → 打 tag → 采新 manifest → 再验**(B0a 以来一直是这个顺序,本批违反后已回归)。g4 对新 manifest:0 新增站点 PASS。

**闸门**。G1a/G1 逐字节/G2 三断言 PASS(单元 89 = 87+imu 3+kalman/Quaternion 2 −0 删)/G4 声明式 PASS(277/71/**112 missing**(imu 8+kalman 4 搬家)/**72 added**/**2 红旗**(ins 白名单站点随搬家退出 audit 扫描面,g4 权威机制仍记))/G5 声明(gimba 消/gimbal 增)/G5b PASS(text +0 ≤ 1%)/G5c PASS。G3 四套件全绿(ax650 4/chassis 1/fwk 9/整链 700 拍确定性——**改名后的金标准确定性 = 改名零语义的硬背书**)。T13 契约 nm 10/10。

**收工**。tag `arch-before-B6b` → manifest 入库。下一批 = **B6b 组件 II-其余**(remote/referee/ax650/遥测/标定/crc 迁 components;app_types 下沉 types.h 消 AGENTS 例外;can_comm 去留裁决;DM/HT 留置桩不迁)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-17｜2026-11-22|B6b 组件 II-其余迁移完成:components/ 齐化 + app_types 下沉 + 遥测 arena 拆分

**交付**。remote/referee(+UI/vt/custom/protocol)/ax650(master_process+seasky+pose+uart1)/calib/crc8·16 → **components/**(git mv 保历史);`modules/app_types.h` → **framework/types.h**(§3.1 依赖方向:framework 拥有共享枚举,AGENTS "referee 需 APP 枚举"例外正式消除;rm_referee.h+robot_def.h 同步换 include)。**遥测 arena 注入拆分**(方案 §4 diag):逻辑 → framework/diag/telemetry.c(**零段宏**,仅 extern 消费);五环对象+tag 赋值 → ports/stm32f4/telemetry_arena.c(.ccmbss 段属性=平台细节)——T13 nm 符号/段不变、工具无感。can_comm 去留裁决 = **默认留置至 B12**(休眠双板支,modules/can_comm 不动)。

**踩坑(重大)**。Makefile 多行续行符被脚本替换丢失(telemetry 行无 `\`),C_SOURCES 从该行断腰→31 条漂移——**批量脚本替换 Makefile 后必须立即跑 G10**。脚本补丁连锁(include dirs 一处补另一处地址错)→ 整块重写 include dirs 收敛。daemon 仍在 modules/(B3 已消化其职能,文件本体 B8 并入)。telemetry_arena.c 曾漏 add(ports 路径不在 git add 列表)——已补录 fea085f。

**闸门**。G5b PASS(text +8 ≤ 1%)/G1a/G1 逐字节/G2 三断言 PASS(单元 87)/G4 声明式 PASS(277/46/**137 missing**(B6b 14 文件搬家+imu/kalman 12)/**63 added**/2 红旗)/G5 声明(diag_arena_assign_tags 增,gimba 消)/G5c PASS(主 RAM 55.4%)。G3 四套件全绿(ax650 4/chassis 1/fwk 9/整链 700 拍确定性)。T13 契约 PASS。

**收工**。tag `arch-before-B8` → manifest 入库。下一批 = **B8 应用装配**(方案最大批:robot_task→kernel接线+profile 两层装配;application/→apps/infantry_skeleton;sim 整链+G8b 重放;**删 forwarding shim 与 bsp/ 目录**;RCC->CSR 复位原因回归;8~12 会话)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-20｜2026-11-22｜A队友自瞄链路合入(main 7a4f105):分支×迁移冲突清单+基线重锚实录

**合入执行(批间隙=main 合入窗口,§7.1)**。队友两条基于 84d51d7 的提交合入:`85269f8`(AX650 UART1 接收开关:`AX650_UART1_ENABLE 0→1` + 新增 `AX650_UART1_RX_ENABLE` + 两条 #error 防呆)/ `958097b`(自瞄时底盘与云台解耦:自瞄右中=CHASSIS_NO_FOLLOW，清 yaw_recentering)。属性=§7.1 B 类开关变化+自瞄联调修复，批间隙合入合法。

**冲突解决(7 文件,方向已核)**。①`config/ax650.h` 手工合:AX650=1/RX=1,保留 B1b 注释等级+Pointer;②`config/chassis.h`:VOFA=0(交接正式执行,与队友一致);③`ports/stm32f4/usart_port.c`(bsp_usart.c 已搬家):队友的 include 微调已含在 ours;④`components/ax650/ax650_uart1.c`:+行 `.tx_only = AX650_UART1_RX_ENABLE ? 0u : 1u` 已自动合并;⑤`tests/ax650_uart1_test.c`+桩 main.h:ours 取 B4b 1024 环架构(回放 FeedBytes/泵),**保留队友 #if RX_ENABLE 分支与自适应断言**(末尾一处 `rx_calls==1` 补改为 RX 自适应——rx0 变体曾挂);⑥`tests/run_ax650_tests.ps1`:**队友 RX=1/0 双变体 × O2/Ofast × 新路径**,6 跑全 PASS;⑦`RM_DEVLOG.md`:DEVLOG 头部表以队友最新描述为准;**队友条目重编号 DEV-20261122-18/19**(03/04 已被迁移瓶领占),AGENTS 引用同步更正;DEC-008(代码改动必同步 Markdown+推送)已入台账表。

**基线重锚实录(合入批标准动作,开诚公开)**。重锚前实测差异:**G1 宏 3 增 2 消**(AX650_UART1_ENABLE 0→1 / AX650_UART1_RX_ENABLE 1 / CHASSIS_VOFA_UART1_ENABLE 1→0——B 类开关切换);**G5 符号 29 增 7 消**(AX650 链从裁剪态激活:Ax650Parser*/PoseCrc*/Uart1* €校验链回归;VOFA 链关闭:SendChassisVofa/chassis_vofa_usart/双正弦缓存消失;FaultHoldActive 无调用者被 GC)。save-baseline 重锚吸收为新基线(tag `arch-before-B8` 重打到 7a4f105,manifest 重采,旧 f27c27e 基线作为迁移区间历史留存)。text 117,384(+1,284，AX650 链激活属功能启用不计迁移预算)/主 RAM 55.5%/CCM 83.8%。

**验证**。ARM 0E0W;chassis ALL PASS(含自瞄解耦四断言分支)/ax650 **6 跑全 PASS**(RX=1/0×O2/Ofast)/fwk 9 PASS/整链 700 拍确定性 PASS(合入无行为漂移于金标准管道)/T13 契约 PASS/宏账本 221(220+新开关)。

**待实车(用户台架,队友四条判据+迁移首次实车)**。①左中+右中时转云台,底盘不自行回中/跟随;②右摇杆四向平移;③左中+右下小陀螺+右摇杆平移;④左上急停锁存;另核 0x0503 符号/坐标系/100ms 超时与帧计数。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-21｜2026-11-22｜B8 应用装配完成:application/→apps/infantry_skeleton + 5 任务 kernel 接线 + forwarding shim 全删

**交付(29 个 git mv + 10 删除 + 装配层)**。①`application/` 整体 28 文件 `git mv` → **apps/infantry_skeleton/**(git 历史保留),`dg_test_hang.c` → ports/stm32f4/(第 29 个);②新增 `apps/infantry_skeleton/profile.h`:ONE_BOARD 编译期断言 + `PROFILE_ROBOT_INFANTRY`(B11 三骨架 profile 化的唯一插点);③`robot_task.c` 5 任务 `osThreadDef/osThreadCreate` → **kernel_task_create**(ins/motor/daemon/robot/ui;栈 1024/256/128/1024/512 逐项不变),`osDelay` → kernel_task_delay,任务函数签名 `void *argument`;④`bsp_init.h` 职责并入 **ports/stm32f4/board_port.h**:`BoardInit()` = `DWT_Init(SystemCoreClock/1000000u)`(原写死 168,168MHz 下数值等价,方案 §3.2)+ BSPLogInit;⑤删除 9 个 forwarding shim + bsp_init.h,**bsp/ 仅剩 adc 0 字节占位**(ARCH-DEC-006)。

**优先级契约(零行为硬证据)**。旧 CMSIS:`osPriorityAboveNormal`(ins)/`osPriorityNormal`(其余 4 个);`cmsis_os.c:makeFreeRtosPriority` = tskIDLE(0)+(prio−osPriorityIdle(−3)) ⇒ Normal→**3**、AboveNormal→**4**,与 `KERNEL_PRIO_NORMAL=3/HIGH=4` 逐项相等(T05 静态断言同时锁死)。UI 任务资源/栈不变。

**include 面与构建收口**。全库 `#include "bsp_*"` 残留 **0**(57 个源码/harness 改用 `*_port.h`);Src USER CODE 区 3 文件 include 同步(it→fault_port.h/main.c→log_port.h/usbd_conf→dwt_port.h);**CMake apps 的 GLOB+include 递归置于 config 之前**(否则 `gimbal.h` 被 config 参数头遮蔽,隐式声明编译错);Makefile apps 路径与 `-Iapps/*` 同步,顺手清掉**HEAD 遗留的重复 can_port.c 登记**、已删目录的 `-Iapplication/-Ibsp/*` 与 CMake 的 `application/*.c` glob;dg_test_hang 在 `-DDG_TEST_HANG` 下引用的 bsp_log.h 改 log_port.h(@layer 同步 PORTS)。RCC->CSR 复位原因回归:robot.c 读 CSR + RMVF 清零路径**原样保留**,未改动。

**测试面修复(两处早前批次潜伏红)**。①`can_filter_golden` 编译链补 SEGGER_RTT 桩(tests/native_stubs 新增 h)与 LogPrintf 空实现——can_port.c 经 log_port.h 引用日志;②`fwk_msg_test.c` 话题数注释/循环脱节(只建 t1/t2/thr2 三话题却按 4 扣减,`msg_topic_count()==8` 断言必挂)——此前批次只 grep "RESULT: PASS" 行数、未核脚本 exit code,红被掩盖,**教训:聚合脚本一律以退出码为判据**;③`msg.c` 删除 `data_len > MSG_DATA_MAX` 死比较(uint8_t 天然 ≤255,主机 -Wextra 告警),零行为。

**闸门(全绿)**。text **117,368**(−16 B / −0.014% vs 批前 117,384)data 676(+0)bss 126,696(+0);**ARM 0E0W**;G1 宏逐字节/G1a 工具链/G2 三断言(Debug、无 -flto、单元 88=磁盘 88)/G4 声明式 PASS(**272 未变/28 改/54 增/160 缺/0 红旗**;缺失分解=.Doc 9+application 21+bsp 35+modules 95,全部为 B0–B8 已声明移动/删除;增量为 tests 桩与 config/根文档)/G5 声明(**BSPInit 消失;BoardInit、kernel_task_create、kernel_task_delay 新增**——B8 首次真实链接 kernel 任务原语)/G5b −16 B ≤2%/G5c PASS(主 RAM 55.26%、CCM 83.84%)/G8 整链 700 拍逐字节确定性 + CAN 过滤器 9 行金标准/G10 136/G7 `--batch B8` PASS。**G4 红旗白名单结案**:chassis/fault/gimbal 路径随搬家更新(application/、bsp/ 旧路径已不存在),ins_task 启动期 fail-fast 与 gimbal UITaskInit 句柄注入按既定口径转**豁免**——4 豁免/0 待裁决,manifest 重采后 `new_sites=0`。

**收工与状态**。tag `arch-before-M3` → manifest 入库(标准顺序:提交→tag→采 manifest→复核)。迁移主线 **B0a~B8 全部完成**;`application/` 目录已不存在,apps/ 成为唯一应用层。下一里程碑 = **M3 实车回归**(用户台架:队友自瞄 4 判据 + 迁移首次整车;判据见 DEV-20),之后 B11(三骨架 profile)/B12(脱离上游收官)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-22｜2026-11-22｜B11 三骨架 profile 完成:heavy/sentry 参数锚点 + 编译矩阵 + CMake profile 选择

**交付(方案 §6-B11 / G-B"三骨架编译,sim 目标即可")**。①`apps/heavy_skeleton/profile.h`:PROFILE_ROBOT_HEAVY + **PROFILE_HAS_42MM_SHOOT**(42mm 能力锚点)+ 板型守卫(三值互斥;双板分支 #error"未实装");②`apps/sentry_skeleton/profile.h`:PROFILE_ROBOT_SENTRY + **PROFILE_AUTO_OPERATION** + 同守卫(多机通信按 2027 手册不预留);③两份 profile 均声明"骨架代码/参数伞头待 M5 实装",参数仍只允许来自 config 伞头(零散数值不入 profile);④`tests/profile_compile_{heavy,sentry}.c` + `tests/run_profile_tests.ps1`(sim TU:-DONE_BOARD,验证宏/守卫/锚点),已接入 run_fwk_tests(G3 聚合)。

**CMake 骨架选择(防同名头遮蔽)**。新增 `ROBOT_PROFILE` 缓存变量(默认 infantry);include 面从"递归全部 apps/"改为**只递归选中 `apps/<profile>_skeleton`**(三份含 `profile.h`/`gimbal.h`,全递归必然互相遮蔽——B8 已踩过 gimbal.h 遮蔽);SOURCES glob 同步为 `apps/${ROBOT_PROFILE}_skeleton/*.c`。**heavy/sentry 现状 = header-only 锚点**:configure 显式 FATAL_ERROR("骨架代码待 M5;验收走 sim 编译"),避免落到晦涩链接错。默认 infantry 文件集不变。

**E.2 坏名清理**。`chasiss_can_comm` → **chassis_can_comm** 4 处(apps chassis.c,均在 `#ifdef CHASSIS_BOARD` 休眠支);ONE_BOARD 固件不编译该支 → ELF 符号/尺寸零变化。

**范围裁决(DEC-009)**。板型运行期化("若做"子项)不做:无双板硬件、双板代码未实装,运行期字段无消费者;保持编译期三值守卫,`assembly_t` 运行期表随硬件窗口引入。M3′ 按 G-B 只要求三骨架编译。

**闸门**。**text 117,368 逐字节 +0**(data/bss 同),ARM 0E0W,编译单元 88;G1 宏逐字节/G1a/G2 三断言/G5 符号集合不变/G5b +0B ≤2%/G5c PASS/G4 声明式(**272 未变/28 改/57 增**=54+3 新测试文件/160 缺/0 红旗)/G10 136。G3:chassis/fwk/ax650 **exit 0**(fwk 内新增 profile 矩阵 2 行 PASS;整链 700 拍确定性保持)。G7 `--batch B11` PASS:check_tags 补**非顺序批**映射(B9/B10 不执行,B11 前序收工点=arch-before-M3;B11 收工标签=arch-before-B12),B8 回归仍 PASS。

**踩坑(工具脚本编码)**。PowerShell 5.1 对**无 BOM 的 UTF-8 .ps1 按 ANSI(GBK)解析**;本次新脚本中文注释的 UTF-8 字节在 GBK 下恰好形成续行反引号,导致 try 块解析错。修复=**.ps1 注释一律 ASCII**(中文理由写 DEVLOG),以后新增 ps1 遵此;既有脚本未发作属侥幸,不改动。

**收工**。tag `arch-before-B12` → manifest 入库。下一批 = **B12 脱离上游收官**(上游资产/manifest/skill 引用收口,G-E)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-23｜2026-11-22｜B12 脱离上游收官:modules/ 与 bsp/ 目录删除 + AGENTS/Skill/code-map v2 + 版权审查

**交付(代码收口)**。①modules/ 残余全部处置:`daemon → components/daemon`(**DEC-010**:依赖 log/dwt/buzzer,framework 零平台 include 放不下)、`super_cap → components/super_cap`、`can_comm → components/can_comm`(休眠双板支保留,profile 守卫仍 #error)、`motor/{HTmotor,DMmotor,servo_motor} → components/motor/`、`general_def.h → framework/general_def.h`;②删除零引用遗留:`standard_cmd/`、`unicomm/`(仅 Makefile 有登记)、0 字节 `step_motor.c/h`(唯一 include 同步移除)、0 字节 `apps/.../chassis/{balance,mecanum,steering}.h`;③**ARCH-DEC-006 结案**:0 字节 `bsp/adc/bsp_adc.c/h` 删除;④**`modules/`、`bsp/` 目录从树中消失**(残留文件数 0)。CMake 移除 bsp/modules 递归与 glob;Makefile 全部路径/-I 同步;测试脚本 include/source 路径同步;`check_source_lists` 132。

**文档与技能(脱离上游的"软"部分)**。①**AGENTS.md 重写**:Map 改自研分层与现行基线 tag,Before editing 保留三步,Boundaries 更新依赖方向(framework 零 RTOS include、ports 唯一后端、任务只走 kernel)/句柄/inline profile 规则,Checks 补全套命令与 ps1 ASCII 约束;②**README**:分层/目录树/文档索引改新架构,标注 application|modules|bsp 仅历史;③`.Doc/spec/架构与开发指南.md` **全文重写**(新分层图/纪律/启动任务表/数据流/加组件与骨架/构建);④`注释规范.md`:适用目录、`@layer` 词表归一(APP/COMPONENT/FRAMEWORK/PORTS/CONFIG/TESTS)、§12/§14 计数更新;**61 个文件 @layer 归一**(MODULE→COMPONENT、framework→FRAMEWORK、BSP→PORTS(stm32f4)),ports 头里过期 shim 说明清理;⑤`必须做&禁止做.md` 路径与测试命令更新;⑥`.Doc/README.md` 索引补升级方案与版权页;⑦**Skill**:SKILL.md v1.0.0(描述/分层/授权词表)、ai-constraints 层级词表、**code-map.md 重写为 v2**(当前树症状定位/调用链/分层索引/当前陷阱),evolution-roadmap 指针已在位。

**版权审查(新增 `.Doc/spec/版权与署名.md`)**。`LICENSE` 为上游 MIT(© 2022 NeoZng)原样保留;派生文件保留原署名/散文归属,新文件 `@author 小猫大王`;第三方(CMSIS/HAL/USB/FreeRTOS/SEGGER/DSP)许可文件齐全,未发现 copyleft 混入;135→**130** 个第一方文件 `@file` 覆盖 130/130(删除 5 个 0 字节文件后)。

**闸门**。**text 117,368 逐字节 +0**(data 676 +0;bss 126,704 **+8**——文件搬迁后链接顺序变化引起的段对齐,`G5` 逐符号尺寸多重集完全一致,非行为变化);ARM 0E0W;单元 **84**(88−3 删除 −1 空 TU);G1 宏逐字节/G1a/G2 三断言/G5 符号/G5b +0B ≤2%/G5c PASS/G4 声明式(**264 未变/15 改/57 增/181 缺/0 红旗**;缺失分解=.Doc 9+application 21+bsp 37+modules 114)/G10 132。G3:chassis/fwk/ax650 **exit 0**,整链 700 拍金标准确定性保持。

**收工与状态**。tag `arch-before-M3p` → manifest 入库。**升级主线 B0a~B12 全部收口**,工程不再依赖 `application/`、`modules/`、`bsp/` 任何路径;剩余 = **M3 实车回归(用户)** + **M3′ 结项验收**(G-A~G-F,判据见方案 §6.0)。B9/B10/M5 按裁决不执行(能力缺口登记)。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-24｜2026-11-22｜B12 补漏:附表 E.2 改名与 N10 单字母宏全部落地 + PI 双定义统一

**触发**。用户复查"宏、变量都改好了吗"→ 对照方案附表 E.2/N10 逐项审计:**5 组改名未落地**(B3/B6/B8 排期项被迁移批跳过):`BuzzzerInstance`(14)、`*_cmd_send`(29+24+31=**84**)、`*_fetch_data`(12)、`*_feedback_data`(10)、`dins`(8);N10 单字母宏 X/Y/Z(ins_task.h + 53 处索引,含 chassis.c 5 处)、TEMP/LAST(remote_control.h + 62 处索引);另附D 遗留 PI 双定义值不一致(3.1415926535f vs 3.14159265354f)。

**执行(机械改名,零行为)**。①`BuzzzerInstance→BuzzerInstance`、`buzzer_temp→buzzer_new`;②`chassis/gimbal/shoot_cmd_send → *_cmd`(N6 构造方向);③`*_fetch_data → *_fb_recv`(消费方向)、`*_feedback_data → *_fb`;④`dins→daemon_instance`;⑤`X/Y/Z → IMU_AXIS_X/Y/Z`、`TEMP/LAST → RC_SLOT_CURRENT/RC_SLOT_PREVIOUS`;⑥`user_lib.h` PI 改为与 `framework/general_def.h` 同值(3.1415926535f,浮点表示不变)。测试桩同步:`tests/native_stubs/ins_task.h` 轴宏;`chassis_remote_test.c` 的静态命令变量改名 `*_ut` 避免与新全局符号冲突。

**踩坑**。脚本按单词边界全量替换时**误改了消息话题字符串** `"chassis_cmd"`→`"chassis_cmd_ut"`,chassis 主机套件订阅错名、断言 `msg_recv==1` 失败——已回改字符串。教训:**改名脚本必须排除字符串字面量**(或先替换后逐条 diff 字符串行);本轮其余替换均先查词边界与既有符号冲突。

**闸门**。text **117,368 逐字节 +0**(data/bss 同),ARM 0E0W,单元 84;G1 宏逐字节/G2 三断言/G4(264/15/57/181/0)/G5 **声明式**(消失 7:`chassis/gimbal/shoot_cmd_send`、`chassis/gimbal_feedback_data`、`chassis/gimbal_fetch_data`;新增 7:`chassis/gimbal/shoot_cmd`、`chassis/gimbal_fb`、`chassis/gimbal_fb_recv`)/G5b +0B/G5c/G10 132 全 PASS;G3 chassis/fwk/ax650 **exit 0**(整链 700 拍金标准确定性保持)。

**剩余命名口径(M3′ 登记,未做)**。N2 函数"模块前缀+动词"全仓重命名、N4/N5/N9 的 `ins/buf/tmp` 缩写与单位后缀全量清理、TRUE 三处同值重复(无害)——这些属**全仓风格大扫**,未在迁移批排期且会动公共 API,留待用户决定是否单独开批;config 宏名/遥测环名/话题名/DM-HT 桩按 E.3 不动。`clang-tidy readability-identifier-naming` 尚未配置(无 `.clang-tidy`),T15 的"迁移文件 naming 0 违例"暂无机器证据,同样列入 M3′ 口径缺口。

**收工**。tag `arch-before-M3p` 重打到本提交 → manifest 重采入库。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261122-25｜2026-11-22｜宏重复/冲突治理收口 + `check_macro_dups` 机器化;变量命名补漏复核

**触发**。用户追问"宏管理、宏重复/冲突、变量命名不统一是否解决"→ 对全仓第一方 `.c/.h` 做宏定义全量扫描(同名跨文件/同文件多值/单字母宏),并复核 E.2/N10 改名残留。

**宏治理(附D 收口)**。①删除**孤儿寄存器头** `components/imu/bmi088_regNdef.h`:零 include、与 `BMI088reg.h` 约 140 个同名同值宏重复;②模块级同名宏拆分:`CURRENT_SMOOTH_COEF` → `DJI_CURRENT_SMOOTH_COEF`/`HT_CURRENT_SMOOTH_COEF`,`KP/KD_MIN/MAX` → `DM_*`/`HT_*`(此前 dji/HT/DM 头互相覆盖,包含顺序决定语义);③`crc_ref.h` 的 TRUE/FALSE 补 `#ifndef` 守卫;④**PI 单源**:`user_lib.h` 改 include `framework/general_def.h`,删除本地 3.14159265354f;⑤`user_malloc` 保留条件宏重复并登记豁免——**试过去重(让 kalman include user_lib.h),结果 `_CMSIS_OS_H` 变为已定义,分配器从 `malloc` 静默切到 `pvPortMalloc`,text +68B**;已回退,text 复原 **117,368 +0**。此为"去重也会改行为"的实证,记入教训。

**机器化(T14 补强)**。新增 `tools/check_macro_dups.py`:同文件多定义=条件分支忽略;跨文件同名不同值=**冲突 FAIL**;同名同值=重复须登记;单字母宏(N10)一律 FAIL。豁免清单 `.Doc/gate/macro-dups-allow.txt`(TRUE/FALSE 布尔约定 + user_malloc 平台选择,共 3 项)。接入 `run_fwk_tests.ps1`(G3)与 AGENTS Checks。

**变量命名复核**。DEV-24 的 E.2 五组 + N10 单字母宏已归零(`Buzzzer`/`cmd_send`/`fetch_data`/`feedback_data`/`dins`/`X,Y,Z`/`TEMP,LAST` 残留均为 0);本轮不新增变量改名。**未做(维持 M3′ 口径)**:N2 函数前缀全仓重命名、N4/N5/N9 缩写与单位后缀清理、`.clang-tidy` naming 配置——动公共 API,待用户决定是否单独开批。

**闸门**。宏扫描:跨文件冲突 **0**/未登记重复 **0**/单字母宏 **0**(豁免 3);text **117,368 逐字节 +0**,ARM 0E0W,单元 84;G1 宏集合逐字节/G1a/G2 三断言/G4(264/15/57/181/0)/G5(声明式)/G5b/G5c/G10 全 PASS;G3 chassis/fwk/ax650 **exit 0**(fwk 已含宏重复检查;整链 700 拍金标准确定性保持)。

**收工**。tag `arch-before-M3p` 重打到本提交 → manifest 重采入库。

**AI**:GLM-5.3-Flash,2026-11-22。

## DEV-20261123-01｜2026-11-23｜工程统一化收官:目录语义化 + 全量命名/宏/注释治理 + 机器闸门

**触发**。用户给出三张目录截图(骨架根文件与子目录混放、config 子头与伞头分离、motor 厂商目录与通用文件混放),要求"非常优秀的目录结构、最标准的命名、全工程(含上游衍生)统一、功能缺口预留但做好架构管理",并**全权批准、一口气完成**。前一轮分析确认:B0a~B12 已完成 70%,但 N2/N4/N5/N9 命名大扫、`.clang-tidy`、`check_comment_style.py`、裸宏治理、RM-011 旧视觉栈均在 M3′ 口径挂起(DEV-20261122-24/25 原文)。

**功能变化**。无。全部为结构/命名/注释/文档变更;控制逻辑、参数数值、协议布局、消息话题字符串零改动。唯一可观测差异是删除旧视觉栈死调用后的 text −40 B、bss −24 B。

**目录与文件(结构级,已获批准)**。
1. `apps/{infantry,heavy,sentry}_skeleton` → `apps/{infantry,heavy,sentry}`;`profile.h`/`robot.c`/`robot_def.h`/`robot_task.*` 收进 `app/`,骨架根层只留目录。
2. `config/robot_infantry_2026.h` + 同名目录 → `config/infantry_2026/config.h` + `config_<域>.h`(board/chassis/gimbal/ax650/shoot/heat/kbm/remote/protect);子头加 `config_` 前缀消除与 `apps/*/gimbal.h` 等同名头冲突(实测主机测试曾拿错头)。
3. `components/algorithm/` 拆为 `control/`(controller/kalman/quaternion_ekf)、`crc/`(crc8/16)、`math/`(user_lib);`motor/` 平铺为 `dji_motor.c` / `dm_motor.c` / `ht_motor.c` / `servo_motor.c` + `motor_def.h`/`motor_task.*`;`imu/BMI088*` → `bmi088_*`;`referee/rm_referee.*` → `referee.*`、`crc_ref.*` → `referee_crc.*`、`referee_UI.*` → `referee_ui.*`。
4. 删除:3 个 0 字节占位头(`chassis/{balance,mecanum,steering}.h`,此前 DEV-20261122-23 误记"已删除",实际仍在且被 git 跟踪);旧视觉栈 4 文件 + host 桩(`VisionInit/Send` 零调用者,`VisionSetAltitude` 只写无人读结构体,RM-011 结案)。
5. 协议枚举 `Enemy_Color_e`/`Bullet_Speed_e` 从旧视觉头收敛到 `apps/infantry/app/robot_def.h`;`COLOR_*`/`BIG_AMU_*` 同步改 `ENEMY_COLOR_*`/`BULLET_SPEED_*`。

**命名(机械改名,零行为)**。
1. 函数 **320 项**:`DJIMotorSetRef→dji_motor_set_ref`、`ChassisTask→chassis_task`、`RobotCMDInit→cmd_init`、`RemoteControlSet→cmd_remote_set`;修正历史拼写 `Eular→euler`、`Intergral→integral`、`heade→header`、`ger_sector→get_sector`。
2. 类型 **109 项**:`Chassis_Ctrl_Cmd_s→chassis_ctrl_cmd_t`、`DJIMotorInstance→dji_motor_t`、`Buzzer_config_s→buzzer_config_t`、`AlarmLevel_e→buzzer_alarm_level_e`;`rc`/`mouse` 是匿名结构体成员名,`mat` 是宏,三者排除避免误改字段。
3. 文件级变量/枚举 **38+20 项**:`INS→ins_state`、`QEKF_INS→ekf_ins`、`UI_*→referee_ui_*`、`SysTime/CYCCNT_*→dwt_*`、`ROBOT_STOP→ROBOT_STATUS_STOP`、`LOAD_*→LOADER_*`。
4. 宏 **100+ 项**:`PI→FW_PI`、`DEGREE_2_RAD→FW_DEG_TO_RAD`、`VAL_MIN→MATH_MIN`、`msin/mcos→MATH_SIN/MATH_COS`、`MatAdd→MATH_MAT_ADD`、`WHEEL_BASE→CHASSIS_WHEEL_BASE`、`FRICTION_*→SHOOT_FRICTION_*`、`KEY_*→REMOTE_KEY_*`、`LOGINFO→LOG_INFO`、`SERVO_*→SERVO_MOTOR_*`、`BMI088` 零偏/音名等;删除 `TRUE/FALSE` 改 `<stdbool.h>`,删除 `mat`/`Matrix_*` 重复矩阵别名统一 `math_matrix_t`;`MATH_ABS` 提升到 `framework/general_def.h` 并补参数括号。
5. ABI 例外保留:FreeRTOS `vApplicationStackOverflowHook`/`pcTaskGetName`、`configASSERT` 出口 `fw_assert_fail`、STM32 HAL/CMSIS/CubeMX 名称。替换器**跳过字符串字面量**,话题名/日志串零改动(DEV-20261122-24 教训)。

**注释与文档**。
1. 第一方 `apps/components/framework/ports/config` 共 **135 个 .c/.h**:`@file` 0 错配、`@layer` 0 异常、`@author` 0 缺失(自研 `小猫大王`,上游衍生 `modified by 小猫大王`);含 tests 桩共 171 个文件头全覆盖。
2. 新增 `.Doc/spec/命名规范.md`(五条铁律 + 动词表 + 缩写表 + 例外清单 + 机器闸门);新增 `components/README.md`(ACTIVE/OPTIONAL/DORMANT/VENDOR 登记 + 功能缺口落点 + 新组件最小动作)。
3. `README.md`/`AGENTS.md`/`.Doc/spec/架构与开发指南.md`/`.Doc/spec/注释规范.md`/`.Doc/README.md`/code-map/`guides` 同步新目录与符号;历史 DEV/plan/review/上游快照**不改写**(路径按当时语义)。
4. `tools/check_macros.py` 的 `SUB_DIR` 改 `config/infantry_2026`;`check_macro_dups` 豁免收敛为 `MATH_MALLOC` 1 项;`tools/pitch_trace.js` 与 `.Doc/gate/tool-symbols.json` 的 `INS.*` 改 `ins_state.*`。

**新增机器闸门**。
1. `tools/check_naming.py`:文件/`@file`/`@layer`/`@author`/函数/类型/宏/文件级变量,白名单与 `.clang-tidy` 一致;当前 **PASS**(135 文件 / 693 函数 / 165 类型 / 648 宏 / 213 变量)。
2. `.clang-tidy`:`readability-identifier-naming` 规则与规范一致(本机未装 clang-tidy,以脚本为准)。
3. `.github/workflows/c-cpp.yml`:四套主机测试 + naming/macros/dups/symbol/sections/source-lists/links 全进 CI。

**闸门与证据**。
- ARM(arm-none-eabi-gcc 15.3.1 @ C:/Toolchain/bin):**0 error,第一方 0 warning**。
- 尺寸:text **117,336**(基线 117,368,−32 = −40 RM-011 死调用删除 +8 日志串 `Float2Str→log_float_to_str`;命名阶段逐字节 +0),data 676(+0),bss 126,680(−24);FLASH 118,020 B(11.26%),RAM 72,408 B(55.24%),CCMRAM 54,944 B(83.84%)。
- 主机四套件 **exit 0**:chassis(仲裁/看门狗)、fwk(700 拍金标准确定性 + kernel/msg)、ax650(O2/Ofast × RX 1/0)、profile(heavy/sentry)。
- `check_naming` PASS;`check_macros` PASS(跨子文件 `#if` 违例 **0**);`check_macro_dups` PASS(冲突 0/未登记重复 0/单字母 0,豁免 1);`check_symbol_contract` PASS(10/10);`check_source_lists`/`check_sections` 见收工复核。
- 回退点:每阶段独立 commit(结构 `c6fa5cb`、函数 `1f664b4`、类型 `f98ef17`、宏 `c67e21e`、变量 `a27cb37`、注释 `5f9d1ca`),可单批 revert。

**遗留(不在本轮)**。
- RM-007(gimbal 手动 yaw PID 双来源)、RM-008(shoot 堵转判据两份)、RM-009(双 ms 时基)仍待处理,属逻辑重构,需单独批准 + 实车。
- 功能缺口:功率控制 `chassis_estimate_speed()` 空实现、超电未注册、IST8310 未接、发射硬件挂起(RM-002)、自瞄待实车;落点与判据见 `components/README.md`。
- M3 实车回归(用户执行):重点核对改名后无行为变化的 4 条判据(DEV-20261122-19)。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-07｜2026-11-23｜RM-007/008/009 三条逻辑债清偿:深审修正台账口径后逐条落地

**触发与授权**。用户要求"再深度思考一下修法"→ 逐条读码深审(发现台账两处口径失真),给出修正版修法后用户"全部批准"。

**深审修正(先于动码,两条台账口径被证伪/收敛)**。
1. RM-007 非"两份 PID":运行时每拍无条件覆盖 yaw 六字段(角度 Kp/Kd/DeadBand、速度 Kp/Ki/IL),Init 同名字段是死初值(仅存在于 ROBOT_STOP 窗口,PID 不参与输出)。pitch Init 是真唯一写者,明示不动(防扩大化)。
2. RM-008 非"单发缺清零隐患":两处判据与 else 清零逐字相同,差异仅在锁存后果(连发保持锁存/单发丢队列,系有意语义)。原记的误锁机理经读码证伪,该条降级为纯防漂移抽取。
3. RM-009 范围收敛:ist8310 已随 B0 删除,残留=components 层 ax650 ×3;ports 层 iwdg/usart 的 HAL_GetTick 属合法平台资源。**判据从"全工程字面清零"修正为"逻辑层(apps/components/framework)HAL_GetTick=0,ports 豁免登记"**——比原判据更准,已固化进 AGENTS Boundaries。

**落地(三独立提交)**。
1. **RM-009**(4ca00e7):ax650 三处 HAL_GetTick → `kernel_tick_ms()`(framework 时间原语);第一版改 dwt 浮点 ms 被 ax650 套件 0xFFFFFFF8 wrap 断言当场拦下(float 精度毁 32 位回绕),改判 kernel 原语后整数回绕语义与原完全一致;ax650 桩补 kernel.h 最小面,套件注入源同步。
2. **RM-007**(ac71046):删除 yaw Init 六死字段,注释声明运行时唯一写者;**整链 700 拍 golden 逐字节一致**;text −44。
3. **RM-008**(本条):抽取 `shoot_loader_jam_tick()`(判据+累计+清零内聚,锁存后果留调用点);golden 逐字节一致;text −84。

**验证**。每步 ARM 0E0W;RM-007/008 各自整链 golden 逐字节一致(零行为实证);ax650 套件(freshness/wrap)全绿;chassis 套件全绿。三步 text 合计 −128。

**遗留**。RM-008 的台架人为卡弹验证归 RM-002 硬件修复窗口;本条不关闭该验证项,仅关闭"判据单点化"。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-08｜2026-11-23｜UART1/UART6 所有权收口:自瞄/调试宏切换,裁判固定 UART6

**触发与硬件事实**。用户确认本车 USART1 因硬件原因只能承担“AX650 自瞄或调试”二选一,
切换由编译期宏完成;裁判系统的热量、冷却、弹速、功率和 UI 均走 USART6。此前
`referee_task.c` 在等待 USART6 `robot_id` 时直接向 `huart1` 发送 ASCII,绕过了
UART1 宏所有权,会在 AX650 模式下污染 8E1 视觉流。

**修复内容**。①删除裁判组件中的 `extern UART_HandleTypeDef huart1` 和
`HAL_UART_Transmit(&huart1,...)`,等待裁判上线只保留 RTT `LOG_INFO`,延时改走
`kernel_task_delay(1000)`;②在 `config/infantry_2026/config.h` 固化 UART1 二选一
规则(两个使能宏不得同时为 1),并禁止 `YAW_AUTO_VOFA_USART6_ENABLE=1`;
③`chassis.c` 的裁判初始化始终使用 `&huart6`,不再保留把 USART6 让给 VOFA 的 app 分支;
④`tests/run_ax650_tests.ps1` 新增 UART routing contract:裁判组件不得出现
`huart1` 直发,且 `referee_ui_task_init` 必须绑定 `huart6`。

**行为与接口**。无协议结构体、消息话题或任务频率变化。USART1 当前仍为
`AX650_UART1_ENABLE=1 / CHASSIS_VOFA_UART1_ENABLE=0`;调试模式仍为
`AX650=0 / VOFA=1`,需重新编译。USART6 固定裁判系统,USB/RTT 日志不占用串口。

**验证**。`run_ax650_tests.ps1` PASS(新增 UART routing contract);`run_chassis_tests.ps1`
PASS;`check_macros.py` PASS;`check_macro_golden.py` PASS;ARM CMake/Ninja 编译 0 error,
第一方 0 warning,text 117,396 B。未执行烧录、AX650 实机、裁判台架、VOFA 波形及 M3。
基线审计:Unchanged 264/460、Changed 15、Missing 181、Added 60、RedFlags 0。
`g4_baseline.py check --baseline .Doc/gate/manifest-arch-before-M3p` 仍因目录改名后
manifest 未重锚而 FAIL(2 个 while(1) 路径漂移),已登记到升级方案 U4,不冒充本轮 PASS。

**文档与后续**。同步 `README.md`、`.Doc/spec/架构与开发指南.md`、`.Doc/guide/RTT日志与调试通道.md`、
`.Doc/guide/常见故障排查.md`、`.Doc/spec/参数证据表.md` 和 code-map。
后续升级排程见 `.Doc/plan/工程升级方案-20261123.md` U0~U5;U1 起需逐批批准。

**AI**:Codex,2026-11-23。

**更正**。本条将 USART6 固定给裁判系统的口径已被 DEV-20261123-09 取代:
最终设计保留 USART6 的裁判/自瞄 yaw 调试二选一。本条保留为当时的修复记录。

### DEV-20261123-09｜2026-11-23｜恢复 USART6 自瞄 yaw 调试链:与裁判系统宏二选一

**触发与修正**。用户进一步明确:USART1 被 AX650 自瞄占用时,仍需要独立调试通道;
因此 USART6 不能永久固定给裁判系统,而应像 USART1 一样支持“业务/调试”二选一。
UART6 调试内容只放自瞄相关量,UART1 调试内容只放系统/底盘相关量。

**恢复内容**。恢复 `YAW_AUTO_VOFA_USART6_ENABLE` 与 `YAW_AUTO_VOFA_SEND_DIVIDER`;
恢复 `gimbal.c` 的 USART6 VOFA 实例、自瞄 yaw 13 通道发送函数和调用点;
恢复 `ROBOT_TASK_UI_ENABLE` 与裁判注册的二选一条件。

**最终路由**。
- USART1:`AX650_UART1_ENABLE` / `CHASSIS_VOFA_UART1_ENABLE` 二选一;
  前者 AX650 自瞄,后者系统/底盘 VOFA 调试。
- USART6:`YAW_AUTO_VOFA_USART6_ENABLE=0` 为裁判系统,提供热量/冷却/弹速/功率/UI;
  `=1` 为自瞄 yaw VOFA 调试,暂停裁判 UI/数据。
- 两个调试口独立选择,但同一物理口不能混发两套协议;需要裁判数据时 USART6 必须回 0。

**基线同步**。本次宏集合恢复 420→422;`.Doc/gate/macro-ledger.csv` 恢复两个相关宏条目;
`tools/check_macro_golden.py --update` 重建宏探针基线。历史 DEV/review/history 中关于
旧方案的错误描述保留为历史,不作为当前实现依据。

**验证**。四套主机测试 exit 0;UART routing contract PASS;ARM CMake/Ninja 0 error,
第一方 0 warning,text 117,396 B;静态宏、ABI、命名、注释、源清单、段占用和构建 parity
检查纳入本轮复核。未执行烧录、AX650 实机、裁判台架、UART6 VOFA 波形及 M3。

**AI**:Codex,2026-11-23。

### DEV-20261123-25｜2026-11-23｜AX650 姿态帧增加 MCU 采样时间戳

**触发与目标**。AX650 原先把串口姿态的到达时刻直接当作相机曝光时刻，串口排队、图像取流与
颜色转换延迟会形成动态角度误差。为支持上位机按图像真实采集时间插值 IMU 姿态，在 UART1
`0x1021` 帧中增加 MCU 采样时间，同时兼顾 AX650 逐步升级。

**实现**。
- `components/ax650/ax650_pose.c/.h` 将姿态负载由 17 字节扩展为 21 字节、整帧由 26 字节扩展为
  30 字节；在原有 `mode`、`sequence` 后追加 little-endian `sample_time_ms`，CRC16 覆盖前 28 字节。
- `components/ax650/ax650_uart1.c` 在组帧时写入当前 FreeRTOS 内核毫秒计数，时间戳与本帧
  yaw/pitch/roll 属于同一发送周期。
- `tests/ax650_pose_test.c`、`tests/ax650_uart1_test.c` 更新黄金帧、CRC、序号回绕和时间戳断言；
  README 同步协议表及上下位机兼容说明。AX650 端可同时解析旧 17 字节和新 21 字节负载，
  因而固件与上位机可分步部署。

**验证**。
- `tests/run_ax650_tests.ps1`：`-O2/-Ofast × RX=0/1` 全部 PASS，UART1/USART6 所有权与路由检查 PASS。
- `build.ps1 infantry -Current`：arm-none-eabi-gcc 14.3.1 编译链接成功，生成
  `firmware/current.elf`；FLASH 117,308 B，RAM 72,416 B，CCMRAM 54,944 B。仅保留 newlib
  `_close/_lseek/_read/_write` 既有桩警告。
- 源清单、命名、注释、宏账本、宏金标准、宏重复、ABI、段占用与 `git diff --check` 均 PASS。
- 开工基线审计仍被仓库既有 `LICENSE` 哈希不一致阻断，工具报告 `snapshot corrupted`；该问题
  在本次修改前已存在。本次未烧录，也未连接 AX650 做实机串口时序验证。

**决策台账**。协议选择“尾部追加时间戳并由 AX650 双格式兼容”，不复用现有字段，避免旧固件
误解释角度、模式或序号。本次不新增 DEC 条目。

**AI**:Codex,2026-11-23。

### DEV-20261123-10｜2026-11-23｜重写工程升级方案:按当前文件和安全证据排序

**目标与范围**。原版方案偏概念,未把当前 M3、四组串口宏、监督真实性和闸门债排成
可执行顺序。本轮重写 `.Doc/plan/工程升级方案-20261123.md` 并更新 `.Doc/README.md`
索引,不改控制逻辑、协议、任务、消息或构建文件。

**新方案结构**。v2.0 按“先保证 M3,再修运行时安全,再清组件边界,再修闸门和发布,
最后做竞技能力”排序;每个工作包绑定现有文件、失败现象和可执行验收:

- U0:UART1/UART6 所有权已收口;
- U1:M3 实车矩阵,含裁判、AX650、系统 VOFA、自瞄 VOFA 的宏组合;
- U2:Daemon 监督 tick 和启动期 HAL 超时;
- U3:components 直连 RTOS/HAL/具体句柄、OPTIONAL 组件和 G4 扫描范围;
- U4:四套测试纳入一键闸门、redflags 判定、manifest 重锚和 Release 构建;
- U5:功率闭环、超电、IST8310、发射和 AX650 实车能力。

**事实边界**。方案明确写入:四套测试通过不等于整车通过;`g4_baseline` 当前仍被旧
manifest 路径漂移卡住;`refactor_gate` 的 G3 只跑 chassis/ax650;CMake 默认 Debug;
RTT 只作低频诊断,不替代 UART VOFA 或裁判数据。

**验证**。本轮不改源码,执行文档链接检查;不执行烧录、台架和实车动作。U1 起逐批
批准后再实施。

**后续更正**。U3/U4 已由 DEV-20261123-11 完成;其“G3 只跑两套/旧 manifest 卡住”的
描述是当时状态,不再是当前状态。

**AI**:Codex,2026-11-23。

### DEV-20261123-11｜2026-11-23｜U3 组件边界清理 + U4 闸门/Release 收口

**授权与范围**。用户明确要求执行 U3、U4。U3 清理 apps/components/framework 的
直接 RTOS/HAL/具体句柄依赖;U4 把四套测试纳入一键闸门、修正 G4 扫描和豁免语义、
增加 RelWithDebInfo 构建,并重锚基线。

**U3 落地**。
- 新增 `ports/stm32f4/imu_port.c/.h`:BMI088 片选、SPI 传输、加热器 PWM 全部下沉到 ports;
  `ins_task/bmi088_*` 不再直接引用 `hspi1/htim10`。
- `ht_motor` 的 CMSIS `osThreadDef/osThreadCreate/osDelay` 改为 `kernel_task_create/delay`;
  裁判/VT 延时改走 `kernel_task_delay`。
- `user_lib.h/kalman_filter.h` 删除 CMSIS 分配器探测,固定 math 分配路径。
- `dji_motor` 删除静态 `hcan1/hcan2` 发送伪实例绑定,句柄由注册实例注入;
  `buzzer/ax650` 的硬件句柄改为应用层 Init 注入。
- 当前 `components/` 中 RTOS 头和具体句柄直连为 0;G4 扫描扩展到
  `apps/components/framework` 并增加 RTOS include/API token。

**U4 落地**。
- `refactor_gate.py` G3 改为四套测试;负向测试输出中的 `[FAIL]` 不再误判;
  G2 编译单元数按 `ROBOT_PROFILE` 过滤未编译骨架。
- `g4_baseline.py` 的豁免 verdict 参与最终 PASS/FAIL,不再只打印;
  redflags 更新为当前 U3 后站点。
- CMake 默认仍为 Debug,但允许显式 `RelWithDebInfo`;编译器路径可由命令行覆盖。
- CI 增加 RelWithDebInfo 构建、sha256 和 size。
- 新 tag/manifest:`arch-after-U3U4` / `.Doc/gate/manifest-arch-after-U3U4/`;
  `g4_baseline check` 为 `new_sites=0`,RESULT PASS。

**验证**。
- 四套主机测试 exit 0;
- Debug CMake/Ninja 0 error、第一方 0 warning;`refactor_gate.py check` ALL PASS;
- RelWithDebInfo CMake/Ninja 0 error、第一方 0 warning;
- Debug text **117,232 B**;RelWithDebInfo text **122,732 B**;
- 基线审计 Unchanged 264/460、Changed 15、Missing 181、Added 60、RedFlags 0;
- `check_naming/check_macros/check_macro_golden/check_comment_style/check_source_lists/
  check_abi_golden/check_macro_dups/check_build_parity/check_symbol_contract/check_sections/
  check-links` 均 PASS。

**边界与未执行项**。本轮未烧录、未做台架/实车。U1 M3 仍未执行;U2 的监督/启动安全
和 U5 竞技能力未在本轮启动。`arch-before-M3p` 保留为历史基线。

**AI**:Codex,2026-11-23。

### DEV-20261123-12｜2026-11-23｜通读疑点集中修复(13 文件):递归解析/PWM 时钟/Flash 越界/diag 句柄漂移(另一设备产出,收编合一)

> **合并说明(收编方 DeepSeek-V4.1-Flash)**:本条原为两份记录——另一设备(GLM(WorkBuddy))
> 未提交的 DEV(其设备编号 12,含完整"不修留档"与双工具链验证)与本机收编时另写的条目
> (原编号 11,与其 U3/U4 条目撞号,现让出并改 12)。文中"git 状态"段为原设备时点快照,
> 实际该设备已恢复 git 并推送 eecdb85;本批修复在收编后于本仓库提交 a10a821。
> 其验证注记 check_macro_golden FAIL(环境差异)已按建议修复:比对收敛到项目自有宏,
> 环境无关(见同批 tools/check_macro_golden.py 变更与重建后的 244 宏基线)。

**目标与触发**:全工程逐文件通读(144 文件/16,656 行)发现的可疑点,用户批准集中修复。除第 1/2/3/4 条外均为注释/命名/登记级零行为修正。

**功能变化**。
1. `referee_vt.c` `JudgeVtData` 递归改迭代 + 帧长边界检查:与 `referee.c` 已修的栈溢出同款缺陷(DEV-20261121-08),图传链路 DORMANT 但启用即有 HardFault 风险;lost 回调文案 `"referee vt is jj"` 调试残留改为正经告警。
2. `pwm_port.c` `pwm_select_tclk` APB2 分支误用 `RCC_CFGR_PPRE1`(上游遗留真 bug)改为 PPRE2,本板 APB1/APB2 分频相同故从未暴露;`pwm_register` 超限由裸 `while(1)` 补 `err_report`+锚注(新增错误码 `ERR_CODE_PWM_POOL=0x0401`)。
3. `flash_port.c` 擦/写三入口加地址越界防御(新增 `ERR_CODE_FLASH_ADDR=0x0402`):原先 `flash_get_sector` 对超界地址兜底返回 SECTOR_11,非法地址会被静默擦掉;`flash_port.h` "Sector 23" 注释修正为"F407IG 1MB 单 bank,sector12~23 宏为本板死码"。
4. `diag.c` `diag_watch_unregister` 由 memmove 紧缩改为标记清除,`reg()` 先复用空槽:紧缩会让其他在用 `diag_watch_t` 句柄整体悬空(当前唯一注销者 UI 暂未触雷,属潜伏缺陷);删除重复 `#include <string.h>`。
5. 注释/命名/登记卫生:`dwt_port.c` 删未用 `cmsis_os.h`;`can_port.c` 三处"溢出中断"注释修正为报文挂起(MSG_PENDING);`referee_task.h` include guard `REFEREE_H`→`REFEREE_TASK_H`(占用通用名);`dji_motor.h` `FORCE_DISABLE` 行内注释与值 0 矛盾修正;`ht_motor.c` static 函数 `dm_motor_float_to_uint/dm_motor_uint_to_float` 改名 `ht_motor_*`(上游复制残留);`components/README.md` `kalman_filter` 登记 ACTIVE→VENDOR(grep 证实仅 `quaternion_ekf` 内部使用,自瞄 yaw 整形是 `cmd_autoaim` 自带实现,原登记失实)。另:`config_gimbal.h` `YAW_SPEED_KP_HIGH` 裁决注释 + **DEC-015**(见台账)。

**不修并留档**:`super_cap/can_comm` init malloc 无判空(OPTIONAL/DORMANT 未接线,接入前必须补);`servo_motor.h` 直用 `UART_HandleTypeDef`(OPTIONAL);`crc16_modbus` 与 `crc16_calc` 逐行重复(需先查清调用者归属);`motor_task.c` 无条件调 `dm_motor_task()`(空实例提前 return,无害)。通读子代理曾报 `referee.h` 文件头 0x0202 注释误导——经核实该注释本来就写明"曾未解析→2026-11-20 已修正",无误,不改。

**文件与符号**:`ports/stm32f4/{pwm_port.c, flash_port.c/.h, can_port.c, dwt_port.c}`、`framework/diag/diag.c`、`framework/err/err.h`(错误码 +2)、`components/referee/link/referee_vt.c`、`components/referee/ui/referee_task.h`、`components/motor/{dji/dji_motor.h, ht/ht_motor.c}`、`components/README.md`。另:`config_gimbal.h` `YAW_SPEED_KP_HIGH` 裁决注释 + DEC-015(见台账)。

**验证(双工具链)**。
- 原设备(Administrator,GCC 14.2.1):ARM 143 目标 0 error(4 条 newlib 桩 warning,第一方 0);text 116,700(与 15.3.1 基线 Δ≈600B 为工具链差异,AGENTS 已记载);主机 chassis/ax650/profile exit 0,kernel/msg 用例 ALL PASS;静态闸门全 PASS。
- 收编方(本机,15.3.1):ARM 三骨架 0 error,text 117,304;十项静态闸门 PASS;四套主机测试 exit 0。
- check_macro_golden 在跨 gcc 环境曾 FAIL(MinGW 系统头多 96 个 DUMMY* 宏)——已按建议将比对收敛到项目自有宏(244 条),环境无关,CI(ubuntu gcc)同样适用。

**git 状态(原设备时点快照,现已演进)**:其工作当时未提交未推送;该设备随后恢复 git 并推送 eecdb85(含 DEV-08/09/10 与 U3/U4 批次),本批 13 文件修复由本仓库收编提交 a10a821。

**AI**:GLM(WorkBuddy) 产出;DeepSeek-V4.1-Flash 收编合并,2026-11-23。

### DEV-20261123-13｜2026-11-23｜哨兵兵种手动整机实装:双 yaw 分配 + 三轴力矩环 + 哨兵热量守卫

**触发与授权**。用户提供参考工程 `F:\Project\robomaster\RM_shaobin`(问题较多),明确"只参考所有电机的 CAN 通信,其他逻辑全部重写",并要求开始本工程哨兵开发。经确认:①电机/总线完全按参考映射;②大 yaw 世界系瞄准 + 小 yaw 相对精瞄;③首批交付手动整机;④接 AX650 自瞄。结构级变更已获用户批准。

**CAN 映射(与参考一致)**。
- CAN1 1 Mbit/s:4×M3508 底盘 ESC1-4(0x200/反馈 0x201-0x204)+ DM-J4310 大 yaw(控制 ID5/Master0,MIT 0x005)。
- CAN2 1 Mbit/s:DM-J4310 pitch(ID5/Master0)+ GM6020 小 yaw(ESC1,0x1FF 电压帧)+ 2×M3508 摩擦轮(ESC1/2)+ M2006 拨盘(ESC3)。

**新增/修改**。
1. `config/sentry_2026/`:伞头 + board/chassis/gimbal/shoot/heat/remote/kbm/protect/ax650/autonomy 子头;哨兵 260 热/30 冷却/每发 10 热;三轴参数全部 `[待实测]`,移除骨架 `pragma message`。
2. `apps/sentry/app/`:assembly 接线(`cmd→gimbal→shoot→chassis`,200Hz)、`sentry_def.h` 消息合同(6 个话题)、`sentry_hooks.c` 接 AX650 USART1。
3. `apps/sentry/cmd/`:遥控/键鼠/仲裁/0x0503 目标整形;左中挡自瞄、左横杆世界 yaw、左纵杆 pitch、右杆平移;解锁沿重锚定世界 yaw/pitch。
4. `apps/sentry/gimbal/`:大 yaw/pitch 达妙 MIT 力矩环(Kp=Kd=0,应用层角度→速度→力矩 + 重力前馈),小 yaw GM6020 相对角/速度串级;**双 yaw 分配纯函数** `gimbal_alloc.h`:小 yaw 先吃误差,饱和后大 yaw 承担剩余;IMU 安装位置由 `SENTRY_IMU_AFTER_SMALL_YAW` 选择。
5. `apps/sentry/chassis/`:4×M3508 45° 麦轮、跟随/回中/小陀螺/零力、断流看门狗、轮速逆解;`referee_ui_task_init(&huart6)` 只注册接收,不创建 UI 任务。
6. `apps/sentry/shoot/`:摩擦轮/拨盘、单发/三发/连发、堵转锁存、摩擦轮互锁;`heat_guard.c` 重写为哨兵 260/30/10 规则,支持裁判断流降级与 0x0208 允许弹量闸门。
7. `components/motor/dm/dm_motor.h`:DM 映射范围改为 J4310 驱动器参数(PMAX 12.5 rad / VMAX 30 rad/s / TMAX 10 N·m);此前 ±π/±45/±54 与本车驱动器不一致,会静默错位。
8. `tools/check_naming.py`:宏前缀白名单增加 `SENTRY_`(新 profile 域前缀)。
9. `tests/run_profile_tests.ps1`:sentry 子目录 include + 新增双 yaw 分配主机测试;新增 `tests/sentry_gimbal_alloc_test.c`。

**验证(本机 arm-none-eabi-gcc 14.2.1)**。
- `cmake -G Ninja -B build_sentry -DROBOT_PROFILE=sentry && ninja -C build_sentry`:0 error,第一方 0 warning;text **112,592**,data 676,bss 127,216。
- 默认步兵 `build`:0 error,第一方 0 warning;text 116,668(工具链 14.2.1,与 AGENTS 记录的 15.3.1 基线存在工具链差异)。
- `run_profile_tests.ps1`:heavy/sentry anchor PASS;sentry 双 yaw 分配 10 项 PASS。
- `run_chassis_tests.ps1` ALL PASS;`run_ax650_tests.ps1` PASS。
- `check_naming` / `check_comment_style` / `check_macro_dups --profile sentry_2026` / `check_source_lists` / `check_sections` / `check_symbol_contract` / `check_macros` / `check_macro_golden` / `check_abi_golden`:PASS。
- **未通过**:`run_fwk_tests.ps1` 的 fullchain 700 拍 golden 在同机重跑时字节不一致(差异集中在 GM6020 电流帧,疑为既有 harness 时间/浮点非确定性);已将 harness 重写的 `.Doc/gate/arch-baseline/g8b-fullchain.csv` 恢复为原基线,未冒充 PASS。该失败与哨兵代码无调用关系(harness 只编 infantry + 常用组件)。
- 未执行:烧录、架空/台架、整车、AX650 实机、裁判系统实机。三轴零位/方向/限位必须由用户首车确认。

**已知边界**。
- 大 yaw/pitch 的 DM 反馈 `rx_refresh_dt` 按 2 ms 周期计数,超时阈值 `SENTRY_GIMBAL_DM_LOST_MS=100 ms`;失能/故障时输出 0,驱动组件自动重使能。
- 小 yaw 相对角以 `SENTRY_SMALL_YAW_ZERO_ECD` 为零位;未标定时以 0 代替,首车必须重标。
- 功率控制仍未实现(哨兵 100 W 规则);本批只保留裁判功率数据接收,下一批按 WDR/Origin 的功率限制方案落地。
- 自主决策仅接口桩(`autonomy`),`SENTRY_AUTONOMY_ENABLE=0`;启用前必须先定定位/决策输入协议与降级策略。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-14｜2026-11-23｜兵种切换便捷化:CMake Presets + build.ps1 一键构建

**触发**。用户反馈命令行切换兵种太麻烦,要求更方便的方式。按用户选择采用 “CMake Presets + 一键脚本”,不做运行期多兵种固件。

**新增**。
1. `CMakePresets.json`:三套 configure/build preset(`infantry`/`sentry`/`heavy`),各自固定 `ROBOT_PROFILE` + `CONFIG_PROFILE` 与独立 build 目录(`build/`、`build_sentry/`、`build_heavy/`),避免 `CONFIG_PROFILE` 缓存残留。
2. `build.ps1`:一条命令完成 configure + build,并打印 ELF 路径;默认 infantry,参数限定 infantry/sentry/heavy。脚本只构建,不自动烧录。
3. `README.md` 新增“切换兵种(编译期)”小节;`AGENTS.md` Checks 增补一键构建命令。

**行为**。无 C 源码、协议、参数或任务变化;仅构建入口与文档。烧录后仍固定为编译时兵种。

**验证**。`cmake --list-presets` 列出 3 套;`build.ps1 sentry` 成功(增量 no work,ELF `build_sentry/basic_framework.elf`);`build.ps1 infantry` 成功(ELF `build/basic_framework.elf`);`build.ps1 heavy` 成功(text 88,816,ELF `build_heavy/basic_framework.elf`)。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-15｜2026-11-23｜EmberProbe 固定烧录副本:firmware/current.elf + VSCode 兵种构建任务

**触发**。用户确认 EmberProbe 烧录只读 `workspaceState.elfPath`;其自动检测按“项目根内 + 带 DWARF + mtime 最新”选 ELF,而工程有 `build/`、`build_sentry/`、`build_heavy/` 三个产物,构建顺序不同会导致插件选中错误兵种(实测面板选到 heavy 87.39 KiB)。用户批准“项目侧固定 current.elf”方案,不改插件源码。

**新增/修改**。
1. `build.ps1` 增加 `-Current`:构建成功后复制到 `firmware/current.elf`,强制刷新 mtime,并写 `firmware/current.txt`(PROFILE/SOURCE/UPDATED/SHA256,UTF-8 无 BOM)。
2. `.vscode/tasks.json` 增加 `Build: Sentry/Infantry/Heavy (current.elf)` 三个构建任务,统一调用 `build.ps1 <profile> -Current`;原 `build task` 仍为默认任务。
3. `.gitignore` 忽略 `firmware/current.elf`(虽已被 `*.elf` 覆盖,显式登记)与 `firmware/current.txt`。
4. `README.md`/`AGENTS.md` 同步 EmberProbe 固定路径用法:首次手动选 `firmware/current.elf`,之后切兵种 = 构建任务 + 点烧录,不再点“自动检测配置”。

**验证**。`build.ps1 sentry -Current` → current.elf text 112,592、current.txt `PROFILE=sentry`;`build.ps1 infantry -Current` → 116,668、`PROFILE=infantry`;再切回 sentry 成功。`.vscode/tasks.json` 经 json5 解析共 7 个任务。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-16｜2026-11-23｜VS Code CMake/EmberProbe 工具链路径修正

**触发**。VS Code CMake Tools 报 `spawn cube-cmake ENOENT`、无法配置项目；EmberProbe 面板同时报 OpenOCD 找不到。根因是 `.vscode/settings.json` 把 `cmake.cmakePath` 和 `CMAKE_COMMAND` 指到本机不存在的 `cube-cmake`,且 `emberprobe.openocdPath` 指向另一台机器的用户目录。

**修改**。
1. `cmake.cmakePath`: `cube-cmake` → `cmake`(本机 PATH 为 CMake 4.3.1);删除 `cmake.configureArgs` 里的 `-DCMAKE_COMMAND=cube-cmake`。
2. 新增 `cmake.useCMakePresets: "always"`,让 CMake Tools 直接使用 `CMakePresets.json` 的 infantry/sentry/heavy preset。
3. `emberprobe.openocdPath`: 另一台机器绝对路径 → `openocd`(本机 PATH 为 OpenOCD 0.12.0 @ `E:\OpenOCD0.12.0\bin`)。

**验证**。`cmake --version` 4.3.1、`openocd --version` 0.12.0 均可用;`.vscode/settings.json` 经 json5 解析通过。未改 C 代码、未烧录。若 VS Code 的 PATH 与终端不同,把 `cmake.cmakePath` 换成 `C:/Program Files/CMake/bin/cmake.exe`、`emberprobe.openocdPath` 换成 `E:/OpenOCD0.12.0/bin/openocd.exe` 即可。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-17｜2026-11-23｜Makefile framework include 修复 + Ctrl+Shift+B 兵种选择

**触发**。用户按 `Ctrl+Shift+B` 走默认 Makefile 任务,`ports/stm32f4/{can,pwm,flash}_port.c` 报 `fatal error: err.h: No such file or directory`。根因是框架升级后 Makefile 的 `C_INCLUDES` 只加了 `framework` 与 `framework/diag`,漏了 `framework/err`、`kernel`、`mem`、`msg`;CMake 递归加 include 所以不受影响。

**修改**。
1. `Makefile` `C_INCLUDES` 补 `-Iframework/err`、`-Iframework/kernel`、`-Iframework/mem`、`-Iframework/msg`。
2. `.vscode/tasks.json` 默认构建任务改为 `Build: Select Profile (current.elf)`:`Ctrl+Shift+B` 弹出 sentry/infantry/heavy 选择,构建并更新 `firmware/current.elf`;原 Makefile `build task` 保留但不再默认,避免“默认编步兵、EmberProbe 却烧 current.elf”的错配。

**验证**。`mingw32-make -j24` 成功,text 116,564、data 672、bss 126,672,产物 `build_make/basic_framework.elf`;`check_source_lists` PASS(132 源);`check_build_parity` PASS;`.vscode/tasks.json` 经 json5 解析共 8 个任务,默认任务唯一,选择项为 sentry/infantry/heavy。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-23。

### DEV-20261123-18｜2026-11-23｜构建入口合并:build.ps1 自动定位工具链 + download dap 兵种一致化

**背景**:另一设备 3fa8933(DEV-14/15/17)已用 CMake Presets + `build.ps1` 接管 Ctrl+Shift+B,并在 Makefile 补了 `framework/{err,kernel,mem,msg}` 四个 `-I`(与本机并行排障得出的修复内容一字不差)。但 `build.ps1` 要求 `cmake` 在 PATH,本机实测直接报 `cmake not found in PATH` —— Ctrl+Shift+B 只是换了个报错,仍未可用。按用户裁决"合并进 build.ps1",只保留一个构建入口;本机先写的 `tools/build_arm.ps1` 与 tasks 改造在合并后作废(已 stash 丢弃),避免双入口漂移。

**改动**。
1. `build.ps1`:新增 cmake/ninja/arm-none-eabi-gcc 定位(PATH → `CUBE_BUNDLE_PATH`/`%LOCALAPPDATA%\stm32cube\bundles` → `C:\Toolchain`);gcc 以 `C:\Toolchain` 优先、PATH 兜底(避免新目录落到 PATH 上旧版 14.2.1,与基线差 ~600 B);configure 时显式传 `CMAKE_C_COMPILER/ASM_COMPILER/MAKE_PROGRAM`。
2. `tools/flash_manual.ps1`:新增 `-Profile`(缺省读 `firmware/current.txt`,再退 `infantry`)与 `-DryRun`;openocd 改 PATH 优先、xpack 兜底;修字面 bug——**烧录失败时脚本仍 exit 0**,现为 `exit $flashExit`;并给文件加 **UTF-8 BOM**——该脚本原本是无 BOM 中文 UTF-8,PS 5.1 按 GBK 解码后**一直解析失败(HEAD 版实测 3 个 parse error),此前从未真正可运行**,加 BOM 后 0 error。
3. `.vscode/tasks.json`:新增 CMake 链路 `download dap`(`dependsOn` 默认构建任务、`dependsOrder=sequence`,编译失败不烧旧 bin);原 `mingw32-make` 三任务改名 `Legacy:` 并写明需 MinGW;构建任务补 `$gcc` problemMatcher。
4. `Makefile`:`BUILD_DIR` 注释更新(tasks.json 主流程已不读它;Legacy 任务仍读)。
5. README 编译与烧录段同步。

**事故记录(如实)**:为验证 `-Profile` 映射,曾用 `-Profile heavy` 做"负向测试"(以为 `build_heavy/` 不存在会被挡下)。实际该目录已有旧 bin,脚本直接进入 openocd 流程,对本机 CMSIS-DAP 发起两次烧录尝试,均止于 `CMSIS-DAP CMD_INFO failed`(未进入擦除/编程阶段,片上固件未变)。违反"未授权不烧录"。为此新增 `-DryRun`(解析后即退出,不碰硬件),此后烧录路径的验证只用 dry-run。

**验证**。
- `build.ps1 infantry`:自动选 bundle cmake 4.3.1 + `C:\Toolchain` gcc 15.3.1 + bundle ninja;exit 0,text 117,272。
- `build.ps1 sentry`:全新 `build_sentry/` 冷配置,exit 0,text 113,288(证明 preset + 工具定位在新目录可用)。
- `build.ps1 infantry -Current`:写入 `firmware/current.elf` + `current.txt`(PROFILE/SOURCE/UPDATED/SHA256)。
- `flash_manual.ps1 -DryRun`:缺省解析=infantry/`build/`;`-Profile sentry -DryRun`=sentry/`build_sentry/`;openocd 取自 PATH;解析 0 error。
- `tasks.json` JSONC 解析通过(9 任务,默认 = Build: Select Profile)。
- 闸门:`check_source_lists` PASS(132)、`check_build_parity` PASS、`refactor_gate check-links` PASS。
- 未执行:`download dap` 真实烧录(未授权);`Legacy:` make 任务本机无 `mingw32-make`,未测。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

### DEV-20261123-19｜2026-11-23｜EmberProbe 烧录 ELF 跟随兵种 + launch.json 收口

**问题**:用户报告 Ctrl+Shift+B 无论选哪个兵种,EmberProbe 装载的 ELF flash 恒定 115 KiB。`config.js --get` 显示工作区 `elf` 写死 `build/basic_framework.elf`(步兵目录),换兵种自然不生效;`.vscode/launch.json` 4 个 cortex-debug 配置也全部硬编码该路径,且 `preLaunchTask` 指向本轮已改名的旧任务 `build task`(F5 会找不到任务)。

**改动**。
1. EmberProbe 工作区 `elf` → `firmware/current.elf`(经 mcu-config Bridge `config.js --set`,未手改扩展存储)。
2. `launch.json`:4 处 `executable` → `firmware/current.elf`;2 处 `preLaunchTask` → `Build: Select Profile (current.elf)`。
3. `flash_manual.ps1`:更正过期的风险描述——10 MHz CMSIS-DAP 失步已解决(用户 2026-11-23 确认),脚本仍固定 1000 kHz 只为保守烧录。

**验证**。
- `build.ps1 sentry -Current` → `current.elf` 哈希 == `build_sentry/basic_framework.elf`;EmberProbe `analyze-elf` 报 flash 111.3 KiB。
- 切回 `build.ps1 infantry -Current` → 哈希 == `build/basic_framework.elf`;flash 115.2 KiB(117,948 B)。兵种切换端到端生效。
- `launch.json` 解析通过:4 配置 executable 均为 `current.elf`,两个 launch 配置 preLaunchTask 已更新。

**未执行**:真实烧录(未授权)。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

### DEV-20261123-20｜2026-11-23｜USART6 裁判实测弹速转发至 AX650

**触发与目标**。用户要求把 USART6 裁判系统 `0x0207` 的实测弹速通过 USART1
`0x1021` 姿态帧转发给 AX650；首帧尚未收到时默认 12 m/s，任何低于 12 m/s 的值也按
12 m/s 上报。协议字段和帧长保持不变。

**实现**。
- `apps/infantry/app/assembly.c` 在 200 Hz 应用任务中读取裁判缓存
  `ShootData.bullet_speed`，经新增 AX650 组件接口更新上报弹速；裁判任务未初始化时传入 0，
  由组件执行回退。
- `components/ax650/ax650_uart1.c/.h` 新增弹速更新接口，保存最近实测值；未收到、NaN/Inf
  或低于下限时使用 `AX650_TX_SPEED_MPS`。有限性检测使用 IEEE754 指数位，避免 `-Ofast`
  快速数学优化破坏边界判断。
- `config/infantry_2026/config_ax650.h` 将 `AX650_TX_SPEED_MPS` 从 0 改为 12.0 m/s，语义调整为
  “初始值兼最小值”；宏账本、宏金标准和参数证据表同步更新。
- `tests/ax650_uart1_test.c` 覆盖上电默认 12、8 m/s 钳位为 12、16.5 m/s 原值转发、Inf
  回退 12，并继续覆盖 UART1 RX 开/关与序号回绕。

**验证**。
- `tests/run_ax650_tests.ps1`：`-O2/-Ofast × RX=0/1` 全部 PASS，UART1/USART6 所有权及
  “USART6 裁判弹速 → UART1 AX650”装配契约检查 PASS。
- `build.ps1 infantry`：arm-none-eabi-gcc 14.3.1，编译链接成功；第一方源码 0 error、0 warning，
  仅保留 newlib `_close/_lseek/_read/_write` 既有桩警告；FLASH 117,280 B。
- 开工基线审计未形成有效结果：随仓库上游快照的 `LICENSE` 哈希不一致，工具报告
  `snapshot corrupted`；本次以 Git 定向差异、主机测试、ARM 构建和后续现行闸门为证据。
- 命名、注释、宏账本、宏金标准、宏重复、ABI、源清单、工具符号、段占用与文档链接检查
  均 PASS。G4 对 `manifest-arch-after-U3U4` 检查 FAIL：该历史基线早于仓库既有哨兵代码，
  报出的 8 个红旗均位于既有 `apps/sentry`/`referee_vt.c`，本次文件无新增红旗；未擅自重锚。
- 未烧录，未连接裁判系统或 AX650 做实机串口核对；因此“真实弹速端到端正确”仍待台架验证。

**决策台账**。本次未产生被否方案或新增长期架构决策，不新增 DEC 条目。

**AI**:Codex,2026-11-23。

### DEV-20261123-22｜2026-11-23｜哨兵阶段0标定观测:零力态三轴原始反馈日志

**目标与触发**:用户批准"先开始阶段 0"(双 yaw 标定)。同步带驱动的 `SENTRY_BIG_YAW_TRANSMISSION_RATIO` 仍是占位 1.0,零位 `-1.254962` 抄自 RM_shaobin(其用户确认 1:1 同步带),两者都必须在实车手转标定后才有意义。

**改动**。`apps/sentry/gimbal/gimbal.c` 新增第 8 段"标定观测":零力状态下以 5 Hz 通过 `LOG_INFO` 输出 `[cal] bigTot/bigRaw/bigVel/sEcd/sTot/pitchTot/imuY/imuP`,单位统一整数缩放(rad→mrad、deg→cdeg),不使用浮点占位符。仅在 `zero_force` 成立时输出(DM `MOTOR_STOP` 是零增益、电机仍使能且 `dm_auto_recover_comm` 会拉回,反馈不中断),不影响正常控制与比赛期日志量。

**验证**。
- `build.ps1 sentry -Current`:exit 0,第一方 0 warning,FLASH 114,352 B(10.91%),RAM 55.66%,CCMRAM 83.84%。
- `check_log_format.ps1` PASS(无浮点占位符)、`check_comment_style.py` PASS(178 文件 0 违例)、`check_source_lists.py` PASS(132)。
- 未执行:烧录与实测标定(用户执行)。

**标定注意(已知风险,写入方案)**。
1. `SENTRY_BIG_YAW_ZERO_RAD` 是**电机轴累计位置**,`joint_rad = (total_position - ZERO)/RATIO`;若同步带比 ≠1,上电后 `total_position` 相差整圈会在关节域产生 `360/RATIO` 度歧义,`wrap180` 无法吸收——届时零位只能"每次上电人工摆正/加 homing",配置常量只是同一上电周期内的基准。
2. IMU 是否在枪体可用"手转小 yaw 看 imuY 是否变化"判定,决定 `SENTRY_IMU_AFTER_SMALL_YAW`。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

### DEV-20261123-23｜2026-11-23｜哨兵实车反馈初修:摩擦轮方向 + 小 yaw 中位/限位 + 大 yaw 零位(RM_shaobin 实测值)

**目标与触发**:哨兵首次上电反馈三症状——摩擦轮反转、解锁后摩擦轮常转、大 yaw 不回中。前两项中"反转"按 RM_shaobin(`config/robot_infantry_2026.h` 与 `application/shoot/shoot.c`)的 2026-11-22 实测结论修正;"常转"属自瞄挡设计,待另行裁决(见下),本条不涉及代码。

**改动(仅参数)**。
1. `config/sentry_2026/config_shoot.h`:`SHOOT_FRICTION_REVERSE_L 1→0`、`_R 0→1`。依据 shaobin `shoot.c` 注释:"2026-11-22 用户实测摩擦轮反向,故把 REVERSE 从左轮换到右轮";我方到速互锁由 `shoot_friction_target_error()` 按宏自动取符号,无需像 shaobin 那样手工同步互锁符号。
2. `config/sentry_2026/config_gimbal.h`:
   - `SENTRY_SMALL_YAW_ZERO_ECD 0→27`(shaobin 2026-11-22 21:13 实测;离 ECD 回绕点 8192 仅 27 计数,解缠是硬要求)
   - `SENTRY_SMALL_YAW_LIMIT_DEG 60→90`、`MARGIN 5→10`(机构半圆 180°,后半圆为禁区)
   - `SENTRY_BIG_YAW_ZERO_RAD 0→-1.254962`(shaobin 2026-11-22 21:35 实测机械正前)

**验证**。
- `build.ps1 sentry -Current` exit 0;FLASH 113,976 B;`check_macros` PASS(244 宏守恒)、`check_source_lists` PASS。
- 未执行:实车复测(用户执行)。**注意**:`-1.254962` 建立在 shaobin"同步带 1:1"的前提上;若本车同步带比 ≠1,该零位作废,必须按 DEV-20261123-22 的 `[cal]` 日志重标(阶段 0)。

**未裁决(已记录)**:自瞄挡(左中)`SENTRY_SHOOT_AUTO_AIM_REMOTE_ENABLE=1` 下摩擦轮常开——用户要求改为拨轮触发,属代码变更,待实施。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

### DEV-20261123-24｜2026-11-23｜哨兵阶段0:台架慢扫标定模式 + 静态采集结论(IMU位置/小yaw行程/打滑嫌疑)

**目标与触发**:用户批准阶段 0,并要求由 AI 直接读 RTT 采集(不再手工誊抄)。为此新增台架慢扫模式,并由 EmberProbe 变量写入触发,完成首轮静态标定采集。

**改动**。
1. `apps/sentry/gimbal/gimbal.c`:新增 `volatile uint8_t gimbal_cal_sweep`(0=关,1=标准档 ±20°/5°/s/1.5 N·m,2=强档 ±14°/3°/s/3.5 N·m)。零力态下按力矩闭环驱动大 yaw 三角波来回两趟,50 ms 打印 `[sweep] pos/tgt/imu/vel`;超 ±90° IMU 或 1.5 倍幅度自动中止,扫完置 0。不参与正常运行。
2. `config/sentry_2026/config_gimbal.h`(阶段0采集结论):
   - `SENTRY_IMU_AFTER_SMALL_YAW 1→0`(**用户确认 IMU 装在大 yaw 座上**;手转小 yaw ±136° 期间 imuY 不变,实测佐证)
   - `SENTRY_PITCH_ZERO_RAD 0→-1.935`(枪管水平时 pitch 电机 total_position)
   - `SENTRY_SMALL_YAW_LIMIT_DEG 90→135`(电机端实测行程 −135.0°/+136.4°,对称;余量 10°)
   - `SENTRY_BIG_YAW_ZERO_RAD -1.254962→0`(外部参考值作废,待比例与正前重标)

**阶段0采集结论**。
- 整机原地转 ±90°(上体与车体一起):`bigTot` 全程恒定 −421 mrad、`imuY` +90.4°/−91.3° → 空载下同步带无相对滑移。
- 扶上体、只转车体(±90°):电机 +58.4°/−59.8°,上体 −24.4°/+26.0° → **电机/关节 ≈ 0.51(两个方向一致)**;该值依赖"正好 90°"的假设,且提示关节约为电机 1.95 倍、上电后大 yaw 绝对零位存在多圈歧义,需重标或 homing。
- 台架慢扫(标准档):电机呈"顶死→跳 5.7°→顶死"循环(vel 峰值 2.16 rad/s),`imuY` 全程仅变 0.2~0.4° → **负载下带子打滑或机械卡滞嫌疑**,待用户检查带子张紧/啮合后重扫;强档(3.5 N·m)用于区分"力矩不足"与"打滑"。

**验证**。
- `build.ps1 sentry`:exit 0,FLASH 115,876 B(11.05%);`check_naming`/`check_comment_style`/`check_source_lists` PASS。
- EmberProbe 变量写入 `gimbal_cal_sweep 0→1`:readback 一致、`firmwareConsistency=match`。
- 采集:`[sweep]` 50 ms 采样 282 条 + `[cal]` 5 Hz 基线,均已落盘分析。
- **未完成**:真实传动比与虚位(待机械确认)、大 yaw 正前零位、pitch 反馈改电机位置(IMU 不在俯仰体上,pitch 环现状无效,列入阶段 1)。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

 HEAD
### DEV-20261123-29｜2026-11-23｜哨兵阶段1(第一批):pitch 反馈改合成角 + 分配律粗精分离 + 比例/零位落地

**目标与触发**:阶段 0 收口后的首批控制修正。传动比按手转上部斜率法实测写 0.96,正前零位 `bigTot=-470 mrad` 已读;IMU 在大 yaw 座上,原 pitch 环反馈源错误。

**改动**。
1. `config/sentry_2026/config_gimbal.h`:
   - `SENTRY_BIG_YAW_TRANSMISSION_RATIO 1.0→0.96`(实测 0.974/0.945 取中;齿数未数)
   - `SENTRY_BIG_YAW_ZERO_RAD 0→-0.470`(机械正前实测)
   - 新增 `SENTRY_SMALL_YAW_FINE_DEG 12.0`(粗精分离阈值)
   - `SENTRY_BIG_YAW_ANGLE_KP 20→8`、`DEADBAND 0.5→2.0`(同步带虚位 5~7°,先防极限环)
2. `apps/sentry/gimbal/gimbal.c`:
   - 新增 `gimbal_pitch_joint_deg()`;**pitch 反馈改为"底座 IMU pitch + pitch 关节角"合成世界俯仰**,俯仰率同理合成(`imu gyro + 电机速度`);重力前馈改用合成角。IMU 在大 yaw 座上,原实现用 imuP 控枪管是错的。
   - 分配律改为**粗精分离**:小 yaw 只吃 ±12°,其余归大 yaw(旧实现小 yaw 吃满 ±125°,大 yaw 几乎不动)。
   - `gimbal_fb.pitch_deg` 改为世界合成俯仰,cmd 解锁重锚定语义不变。

**验证**。
- `build.ps1 sentry -Current`:exit 0,第一方 0 warning,FLASH 116,032 B(11.07%)。
- `check_naming`/`check_comment_style`/`check_source_lists`/`check_macros`/`check_macro_golden` 全 PASS(244 宏守恒)。
- 未执行:实车验证(需烧录);虚位换向补偿、小 yaw 软回中、大 yaw 回正前尚未实现(下一批)。

**风险**。带子仍松:大 yaw 闭环在 5~7° 虚位下可能仍抖/迟滞;先把 Kp 压到 8、死区 2° 试,若仍抖需机械张紧或加换向补偿。比值 0.96 ≠1 使断电整圈歧义约 15°,零位跨上电可能偏 15°——需在下一批加"上电摆正重锚定"(变量触发)。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。

### DEV-20261123-30｜2026-11-23｜哨兵阶段1(第二批):自瞄挡拨轮触发 + 大 yaw 回正前 + pitch 符号实测

**目标与触发**:用户确认"自瞄挡摩擦轮不常开、改拨轮触发"与"加回正前逻辑";同时按台架实测补 pitch 方向符号。

**改动**。
1. `apps/sentry/cmd/cmd_remote.c`:自瞄挡不再无条件 `FRICTION_ON`。改为 `else if (auto_aim && !SENTRY_SHOOT_AUTO_AIM_REMOTE_ENABLE)` 的禁射分支;宏=1 时自瞄挡与手动挡共用拨轮触发(下压启动并保持 2.5 s,短拨单发、按住连发)。删除已无用的 `auto_dial_armed`。
2. 大 yaw 回正前:`robot_cmd.c` 新增 `big_yaw_recentering` 状态(2° 死区/600 拍超时/自瞄挡或摇杆接管即退出),目标取 `world − joint`(即底盘朝向),把关节角收到 0;`cmd_authority.c` 解锁沿与 `cmd_kbm.c` F 键同时置位。小 yaw 不参与。
3. pitch 方向实测:零力下抬枪,`pitchTot` −1629→−1015(增大)→ "+向上" 与 `SENTRY_PITCH_DIRECTION=+1` 一致,合成角方向可用。

**验证**。
- `build.ps1 sentry -Current`:exit 0,第一方 0 warning,FLASH 116,104 B(11.07%)。
- `check_naming`/`check_comment_style`/`check_source_lists`/`check_macros` 全 PASS。
- 未执行:实车行为验证(需烧录;用户执行)。已知带子虚位 5–7°,大 yaw 可能有死区/顿挫。

**风险**。pitch 力矩符号沿用 `SENTRY_PITCH_DIRECTION`,若实际力矩方向与关节方向不一致,首次解锁会出现正反馈抢枪——已要求用户手扶枪管、异常立即上挡。

**AI**:DeepSeek-V4.1-Flash(ZCode),2026-11-23。


### DEV-20261124-04｜2026-11-24｜步兵单发/三发多发(2~3/4~5连)根因修复

**目标与触发**。用户实车反馈:单发有概率 2~3 连发、三发有概率 4~5 连发。要求先查代码定位,再修。用户选定方案 = 启用真角度环 + 修逐发停车。

**根因(代码事实,非触发重复)**。
1. `apps/infantry/shoot/shoot.c` 的 loader 配置 `close_loop_type=SPEED_LOOP`,不含 `ANGLE_LOOP` 位(`motor_def.h`:ANGLE=0b0100 / SPEED=0b0010);`dji_motor_control` 的角度环分支因 `(close_loop_type & ANGLE_LOOP)` 恒为 0 而整段短路,`angle_PID` 全零。**所谓"角度环推进"实际从未执行**。
2. 起步时 `set_ref(loader_step_target)` 把绝对角度(当前 total_angle + 1851.4deg 电机轴)塞给速度环,误差达几万 deg → `Kp=6` 输出饱和 `MAXOUT=8000` → **拨盘满速正转**。
3. 到位判据 `|total_angle−target| <= 216deg` 在 200Hz shoot 任务里轮询;到位只 `pending--`,队列未空时不断电,下一发基准又取已冲过的当前角 → 逐发累积。
4. 每拍无条件 `dji_motor_outer_loop(loader, ANGLE_LOOP)` 使到位后 ref=0 在角度环里变成"0deg 目标 vs 几万度当前角"的巨大反向误差——回退旧档仍有拨盘反转隐患。

**改动**。
1. `config/infantry_2026/config_shoot.h`:新增 `LOADER_ANGLE_LOOP_EN=1`、`LOADER_ANGLE_PID_KP=2.5f`/`KI=0.1f`/`MAXOUT=6000.0f`(`[待实测]`,起点参考哨兵未标定值)、`LOADER_STOP_AFTER_STEP_EN=1`。两个开关可回退旧行为做 A/B。
2. `components/motor/dji/dji_motor.{c,h}`:新增 `dji_motor_clear_pid()`(清三环积分与历史,不改使能/闭环/参考值)。
3. `apps/infantry/shoot/shoot.c`:loader `close_loop_type=ANGLE_LOOP|SPEED_LOOP` + 填角度 PID;切角度环从"每拍无条件"收窄到"本发起步那一刻";每发到位立即切回速度环 + 清 PID + ref=0;队列清空兜底也清 PID;起步前清 PID 防残余积分。
4. `.Doc/gate/macro-ledger.csv` + `.Doc/gate/arch-baseline/macros-probe.txt` 同步登记 5 个新宏;`.Doc/spec/参数证据表.md` 补 4 组参数卡(标证据等级)。

**验证**。
- ARM 全量编译(cmake+ninja,`/c/Toolchain/bin` 15.3.1):exit 0,第一方 0 warning;FLASH 118,220 B(11.27%),CCM 83.84%,RAM 55.25%;text 117,532(基线 117,336,+196 B)。
- 主机测试:chassis / ax650 / profile 三套实质全绿;`run_fwk_tests.ps1` 因 G1b 宏守恒探针失败被拦在启动阶段,冲突项是工作区**既有的** 3 个键鼠参数改动(`KBM_YAW_SENS 0.7→0.06`、`KBM_PITCH_SENS 0.43→0.04`、`KBM_MOUSE_DEADZONE 2→4`,无 DEV 记录、非本批改动),未替其归档。profile 脚本因 PowerShell 5.1 把 gcc 的 pragma note 当异常而中断,已手动跑完矩阵:heavy/sentry 锚点 + sentry 分配律全 PASS。
- 静态闸门:check_naming / check_macro_dups / check_comment_style / check_source_lists / check_sections / check_abi_golden / check_build_parity / check_macros 全 PASS。
- 未执行:实车出弹计数验证(需烧录;用户执行)。判据:单发 10 次全 1 发、三发 5 组全 3 发,无 2~3/4~5 连。

**风险**。`LOADER_ANGLE_PID_*` 未标定,首次上电必须先空拨盘(无弹)短拨单发观察:到位慢 → 加 Kp;目标角附近振荡 → 减 Ki;超时 → 查 `LOADER_SINGLE_TIMEOUT_MS`。`LOADER_ANGLE_TOL_DEG=6°` 未做单变量实验,若仍偶发多打优先收紧容差。两个回退宏任一置 0 即回旧行为,便于实车 A/B 隔离。

**AI**:Tencent Hunyuan Hy,2026-11-24。

### DEV-20261124-05｜2026-11-24｜复审 DEV-04 单发修复:发现 LOADER_STOP 覆盖角度环并补三处安全缺口

**目标与触发**。用户对 DEV-20261124-04(混元实现)不放心,要求独立复审单发/三发修复、Shift 自瞄、Shift 慢速删除三件事。复审不采信原结论,逐行回到 `shoot.c` / `dji_motor.c` / `cmd_*.c` 核对。

**复审结论:DEV-04 的修复实际未生效(代码事实)**。
1. 单发/三发指令都是**单拍脉冲**:键鼠 R/E 沿(`cmd_kbm.c` `if 沿 ... else load_mode=STOP`)、自瞄拨轮 `auto_dial_armed` 一次性、手动拨轮松手沿,下一拍 `load_mode` 必回 `LOADER_STOP`。
2. `shoot_task()` 的 `case LOADER_STOP` **每拍无条件** `outer_loop(SPEED_LOOP) + set_ref(0)`;同一拍后面的状态机 `else`(已起步)只写 `set_ref(loader_step_target)` 不重设外环 → 速度环把**绝对角**(约 1851°电机轴)当 deg/s 目标 → 满速冲,与修复前同形。角度环只在起步那一拍存在。
3. 因此 DEV-04 的"真角度环"对键鼠单发/三发、自瞄拨轮单发均无效;只有"持续压住拨轮"的手动路径能维持 ANGLE。

**本次改动**。
1. `shoot.c` `case LOADER_STOP`:仅在 `!loader_step_active` 时切速度环/ref=0;在途发弹交给下面状态机。
2. `shoot.c` 已起步分支每拍幂等 `outer_loop(ANGLE_LOOP)`,兜住 STOP/BURST 等路径的覆盖。
3. `shoot.c` 起步顺序改为 `set_ref(target) → clear_pid → outer_loop(ANGLE)`:原顺序若在两条语句间被 1kHz 电机任务抢占,角度环会拿旧 ref=0 追几万度当前角 → 拨盘反向满速。
4. `shoot.c` 堵转分支补 `dji_motor_clear_pid()`:原路径 `loader_jammed=1` 会跳过末尾兜底,残余积分继续压弹。
5. `robot_cmd.c` `cmd_auto_aim_set()`:`vision_valid` 增加 yaw/pitch `isfinite`;pitch 写入后过 `PITCH_MIN/MAX_ANGLE`。**键鼠自瞄路径没有 `cmd_remote_set` 末尾那道统一限幅**,原实现会把 AX650 的超范围 pitch 直接下发,可能打到机械死点。
6. `robot_cmd.c` 统一自瞄状态:Shift 锁存仅在 `input_source == INPUT_KBM` 时参与或运算。键鼠超时回落遥控后若仍保留自瞄,操作手唯一退出方式是右上挡急停。`cmd_kbm_auto_aim_toggle()` 仍每拍调用以维护沿基线。
7. DEVLOG 编号去重:DEV-04 原写入时占用 `DEV-20261124-02`,与远程 `DEV-20261124-02｜首发前以 0 标记弹速` 冲突;单发条目重编号为 04,`shoot.c`/`config_shoot.h`/参数证据表引用同步(提交 335fd14 的 message 仍为 01,历史不改)。

**验证**。
- ARM 全量编译(cmake+ninja,`/c/Toolchain/bin` 15.3.1):exit 0,0 error;FLASH 118,700 B(11.32%),RAM 72,424 B(55.26%),CCM 54,944 B(83.84%);text 118,016 / bss 126,696。
- 静态闸门:check_naming / check_comment_style / check_macros / check_macro_dups / check_source_lists / check_symbol_contract 全 PASS。
- `check_macro_golden.py` 仍 FAIL,仅 3 个**工作区既有、非本批**键鼠参数(`KBM_YAW_SENS 0.7→0.06`、`KBM_PITCH_SENS 0.43→0.04`、`KBM_MOUSE_DEADZONE 2→4`);宏数量 248/248 一致,本批未新增/删除宏。
- 未执行:实车出弹计数、Shift 进出自瞄、自瞄期间 WASD/鼠标开火(需烧录;用户执行)。

**风险(实车首烧前必看)**。
- `LOADER_ANGLE_PID_KP=2.5` 是纯 P 一阶收敛:从 1851° 收到 216° 容差需 `ln(1851/216)/2.5 ≈ 0.86 s`,而 `LOADER_SINGLE_TIMEOUT_MS=1000` 只剩 14% 余量;实车有摩擦与速度环滞后,很可能超时。建议首烧把超时临时放宽到 1500 ms,或把 Kp 提到 4.0 再观察。
- `LOADER_ANGLE_PID_KI=0.1` 但 `angle_PID.Improve` 未置 `PID_Integral_Limit`、`IntegralLimit=0`,积分**无界**。长行程下 I 项会累积到与 P 同量级,增大过冲;首版建议 Ki=0(纯 P)或补 `LOADER_ANGLE_PID_IL` 后再启用。
- `LOADER_ANGLE_TOL_DEG=6°`(输出轴)= 216° 电机轴 = 11.7% 槽距,偏大;若实车出现"少打/哑弹"优先收紧容差,若"多打"优先查刹车距离。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-06｜2026-11-24｜变速小陀螺抗自瞄轨迹落地(P0-P3)

**目标与触发**。用户确认小陀螺改造计划的三个决定:高速 400 deg/s、允许反转但先温柔、多尺度正弦保留测试。按计划一次落地 P0-P3 代码骨架。

**实现**。
1. 新增 `apps/infantry/chassis/chassis_spin.{c,h}`:状态机 CRUISE/BOOST/GLIDE/BRAKE/REVERSE/RECOVER;多尺度调制 0.31/0.83 Hz;xorshift32 随机段长/转移/反转符号/相位;α 限幅 800 deg/s²;输出仍走 `chassis_cmd_recv.wz`。
2. `chassis.c`:`CHASSIS_ROTATE` 接入轨迹;非 ROTATE 模式复位进入沿;随机种子混入进入时刻,避免轨迹可复现。
3. `config_chassis.h`:新增 25 个 `CHASSIS_SPIN_*` 宏,`VARY_EN=0` 可回退恒速。
4. `Makefile` C_SOURCES 同步;宏账本/探针基线/参数证据表同步。
5. 反转温柔版:-200 deg/s、0.6~0.9 s、最短间隔 1.5 s、概率 25%。时长按 `(300+200)/800=0.63 s` 取,保证真正过零;短于 0.6 s 只会减速,卡尔曼不会重初始化。

**验证**。
- ARM 全量编译 0 error;`text 119,496` / `bss 126,744`。
- 6 项静态闸门(check_naming/comment_style/macros/macro_dups/source_lists/symbol_contract)PASS。
- 宏守恒 273/273,仅剩工作区既有的 3 个键鼠参数改动(`KBM_YAW_SENS`/`KBM_PITCH_SENS`/`KBM_MOUSE_DEADZONE`)。
- 参数级仿真(120 s × 60 组):反转段 100% 过零,ω 范围 -200~400 deg/s,平均 6.5 s 一次反转。
- 未执行:台架电流/机械冲击、实车对抗命中率。

**风险**。
- 本车无功率控制,反转电流峰值只能靠 α 限幅;首台架必须录四轮电流,峰值应 <恒速 2 倍。
- 反转时轮地打滑会让实际 ω 偏离指令;若 `est_wz` 与输出偏差持续 >30%,先降反转概率,不要先加 α。
- 变速会通过底盘 roll/pitch 影响自身射击;验收门槛为打靶命中率 ≥恒速 85%。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-07｜2026-11-24｜小陀螺轨迹复审:修 RECOVER 误判、补连续 BOOST 抑制、修回退档未使用变量

**目标与触发**。用户要求对 DEV-20261124-06 的小陀螺改动做二次逻辑审查。逐行复核 `chassis_spin.c` 与 `chassis.c` 接入点,发现三处需要修。

**复审发现与修正**。
1. **RECOVER 到速判定会被反转残余欺骗**:原用 `fabsf(wz_feedback) >= CRUISE*0.95`,反转段若因超调/打滑到 -300,`fabsf(-300)=300` 会误判"已回到巡航",提前结束回稳段。改为带符号 `wz_feedback >= CHASSIS_SPIN_CRUISE_DPS*0.95f`;负速度只能靠 800ms 超时兜底。
2. **连续 BOOST 未抑制**:计划写明"不允许连续两次同向 BOOST",代码只在概率上分开,`40%×40%=16%` 仍会出现"加速-滑行-加速"固定节拍。新增 `s_last_boost`,上一次为 BOOST 时本轮强制回落 BRAKE/REVERSE,连续 BOOST 概率降为 0。
3. **回退档未使用变量**:`CHASSIS_SPIN_VARY_EN=0` 时 `chassis.c` 的 `spin_active/spin_last_ms` 只定义不读取,可能触发 `-Wunused-variable`。用 `#if CHASSIS_SPIN_VARY_EN` 包住定义与进入沿复位,回退档编译零残留。

**验证**。
- ARM 编译 0 error;`text 119,520` / `bss 126,744`(较 DEV-06 +24 B,来自 `s_last_boost` 与判定改动)。
- 6 项静态闸门 PASS;宏守恒 273/273,仅剩工作区既有 3 个键鼠参数。
- 参数级仿真(120 s × 80 组):反转过零 `1970/1970`;连续 BOOST `0` 次;平均 12.3 次反转/分钟;反转段最低 ω 中位数 -200 deg/s。
- 未执行:台架电流/机械冲击、实车对抗。

**新发现的手感风险(待实车确认)**。
- 进入 ROTATE 时 `s_wz_out=0`,按 α=800 爬升到 300 deg/s 需 **375 ms**;原恒速模式是阶跃,速度环约 50~100 ms 到位。操作手可能感觉"小陀螺起步变肉"。
- 若不可接受,加 `CHASSIS_SPIN_ALPHA_ENTRY`(建议 2000 deg/s²)只在首次到巡航前使用,到速后切回 800;不要直接把 800 全局加大,否则反转冲击同步变大。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-08｜2026-11-24｜裁判UI状态栏重做:小陀螺/自瞄/摩擦轮/发射档/射速档

**目标与触发**。用户反馈裁判客户端 UI 显示不对,要求显示:小陀螺模式(是否开启、匀速/变速)、摩擦轮是否开启、自瞄是否开启、射速档位、发射档位。

**根因(代码事实)**。
- `referee_ui_state_dynamic[0..4]` 的模式来自 `referee_interactive_info_t ui_data`,该结构在 `chassis.c` 定义为 static,`referee_ui_task_init(&huart6, &ui_data)` 传入后再无任何赋值。
- 因此 `referee_ui_change_check()` 永远比较 0==0,`chassis/gimbal/shoot/friction/lid_flag` 从不置位,UI 永远停在初始化文本 `zeroforce/off`。不是绘制错,是数据源从未填充。

**改动**。
1. `referee_app_status.h`:新增 `referee_spin_mode_e`(OFF/CONST/VARY)与 `referee_load_mode_e`(OFF/SINGLE/BURST3/AUTO);`referee_app_status_t` 增加 `auto_aim`/`spin_mode`/`friction_on`/`load_mode` 四个中立字段。
2. `robot_cmd.h/.c`:新增 `cmd_get_auto_aim_active()`/`cmd_get_spin_mode()`/`cmd_get_friction_on()`/`cmd_get_load_mode_latched()`。单发/三发是单拍脉冲,UI 10Hz 必漏,故在 cmd 200Hz 侧锁存最近非 STOP 模式 500ms。
3. `assembly.c` 的 `infantry_referee_status()` 填新字段。
4. `referee_task.c`:状态栏 5 行改为 `SPIN/AIM/FRICT/LOAD/TIER`,坐标 150/270 × 750/700/650/600/550;所有状态文本统一 12 字符(Change 覆盖不会残留);10Hz 且仅变化时重绘;删除 420 的旧 TIER 行,TIER 并入第 5 行。

**显示口径**。
- `SPIN:OFF/CONST/VARY`:只在 `chassis_mode == CHASSIS_ROTATE` 时非 OFF;CONST/VARY 由编译宏 `CHASSIS_SPIN_VARY_EN` 决定,当前为 VARY。
- `AIM:ON/OFF`:`auto_aim_active`(左中挡或键鼠 Shift 锁存)。
- `FRICT:ON/OFF`:`shoot_cmd.friction_mode`,请求侧。
- `LOAD:OFF/SINGLE/BURST3/AUTO`:最近一次发射档位,单发/三发保持 500ms。
- `TIER:ECO/NORMAL/BURST`:`cmd_get_fire_tier()`。

**验证**。
- ARM 编译 0 error;`text 120,484` / `bss 126,768`。
- 6 项静态闸门 PASS;所有状态文本静态检查均为 12 字符。
- `firmware/current.elf` 已同步,SHA-256 `057d04b2...454b9c`。
- 未执行:实车客户端实际显示(需烧录;用户执行)。

**遗留**。
- 匀速/变速目前是编译期选择,UI 只反映当前构建;运行期切换未做,若要切换需在 cmd 增加状态位与入口。
- 旧 `ui_data` 通道保留但未填充,不要重新启用;底盘/云台细分模式如需上屏,另加中立字段。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-09｜2026-11-24｜Shift 改按住自瞄;单发/三发改模式切换;UI 带按键与射速数值

**目标与触发**。用户要求:Shift 自瞄改为按住;自瞄期间平移/发射/模式切换等其余功能正常;单发、三发改成切换,左键按当前模式发射;射速档位显示具体数值;UI 每个模式带专属按键。

**改动**。
1. `cmd_kbm.c`:
   - 删除 `last_shift`/`kbm_auto_aim` 的 toggle 锁存,改为 `cmd_kbm_auto_aim_held()`:Shift 电平 + 键鼠输入源 + READY,松手立即退出。
   - 新增 `kbm_fire_mode_e`(AUTO/SINGLE/BURST3)与静态 `kbm_fire_mode`。R 键在 SINGLE↔AUTO 间切,E 键在 BURST3↔AUTO 间切;按住左键时忽略切换,防走火。
   - 左键按当前模式发:`AUTO→LOADER_BURST`、`SINGLE→LOADER_SINGLE`、`BURST3→LOADER_BURST3`;摩擦轮未开或左键松开时 `LOADER_STOP`。单发/三发按住只登记一次(shoot 端沿检测),松手复位。
   - 新增 `cmd_get_fire_mode()`(1=SINGLE 2=BURST3 3=AUTO),供 UI。
2. `robot_cmd.c/h`:调用改 `cmd_kbm_auto_aim_held()`;删除 DEV-08 的 500ms 发射锁存;新增 `cmd_get_fire_rate()` 转发 `cmd_fire_tier_rate()`。
3. `referee_app_status.h`:新增 `fire_rate`(发/s)。
4. `assembly.c`:`load_mode = cmd_get_fire_mode()`、`fire_rate = cmd_get_fire_rate()`。
5. `referee_task.c`:状态文本统一 23 字符,新增 `referee_ui_status_text()` 补齐;5 行改为
   `SPIN:VARY [Q]` / `AIM:ON [SHIFT]` / `FRICT:ON [RMB]` / `LOAD:AUTO [R/E][LMB]` / `TIER:NORMAL 6.0/s [X]`。

**行为口径**。
- Shift 按住=自瞄,松开=手动;自瞄期间 WASD 平移、左键发射、R/E 切模式、右键摩擦轮、Q 小陀螺全部照常,只有鼠标云台增量被 AX650 目标替代。
- R:单发↔连发;E:三发↔连发。单发/三发模式按住左键只出一发/一组,松手再按才重新触发。
- 射速档位仍由 X 循环 ECO/NORMAL/BURST,UI 显示对应 1.4/6.0/12.0 发/s。

**验证**。
- ARM 编译 0 error;`text 120,936` / `data 680` / `bss 126,752`。
- 6 项静态闸门 PASS;UI 状态文本最长 22 字符(<23 上限),`show_Data[30]` 不溢出。
- `firmware/current.elf` 已同步,SHA-256 `3fdbd774...032217`。
- 未执行:实车 Shift 按住/松开、左键按模式发射、UI 客户端实际显示(需烧录;用户执行)。

**风险**。
- 模式切换要求左键已松开,按住左键按 R/E 会被忽略,这是防走火的刻意行为。
- 单发/三发依赖摩擦轮已开(右键按住),否则 `LOADER_STOP` 不出弹;与旧逻辑一致。
- `AUTO` 是默认模式;若用户希望上电默认单发,改 `kbm_fire_mode` 初值即可。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-10｜2026-11-24｜自瞄指示改为准星中心圆环(绿=开/白=关)

**目标与触发**。用户要求把状态栏的 `AIM` 文字行去掉,改为准星正中心的圆环:绿色=自瞄开启,白色=未开启。

**改动**。
1. `referee_task.c` 新增 `referee_ui_aim_circle`(graphname `ac0`)。
2. 初始化:准星线之后 `UI_Graph_ADD` 一个圆,圆心 `(960,540)`、半径 22、线宽 3、初始白色、layer 8。
   半径按 sl3/sl4 到圆心 25px 选取,保证圆环不与两条短准星线相交。
3. 10Hz 刷新:`app.auto_aim` 变化时 `UI_Graph_Change` 改颜色,绿=`UI_Color_Green`、白=`UI_Color_White`。
4. 删除 `AIM:` 标签与文字行;状态栏由 5 行缩为 4 行:
   `SPIN:` 750 / `FRICT:` 700 / `LOAD:` 650 / `TIER:` 600,动态数组索引同步为 0~3。

**验证**。
- ARM 编译 0 error;`text 120,988` / `data 680` / `bss 126,768`。
- 6 项静态闸门 PASS。
- `firmware/current.elf` 已同步,SHA-256 `f8b66e5f...fa39ce`。
- 未执行:实车确认客户端是否支持 `UI_Graph_Change` 改圆环颜色(需烧录;用户执行)。

**风险/回退**。
- 裁判客户端若对 Change 的 color 字段不刷新,圆环会停在初始白色;此时改为 `referee_ui_delete("ac0")` + 重新 ADD 的换色方式(会闪一帧)。
- 圆环与水平准星线 sl0 必然相交,属于几何必然;若视觉上干扰瞄准,可把半径降到 18 或改到准星上方。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-11｜2026-11-24｜小陀螺加 jerk 限幅;yaw Kp 调度加一阶低通

**目标与触发**。用户反馈变速小陀螺可能让 yaw 轴旋转变差,要求在轨迹层加 jerk 限幅、在 yaw 速度环 Kp 调度上加低通。先做参数级仿真确认两者不会互相破坏,再改代码。

**设计**。
1. `chassis_spin.c`:`spin_ramp_to()` 换成 `spin_track()`,引入加速度状态 `s_alpha`:
   - `a_des` 由"当前 α 卸到 0 还会走多远"`a^2/(2·jerk)` 预测刹车点决定,不会到目标才反向;
   - `α` 以 `CHASSIS_SPIN_JERK_MAX` 爬升/卸掉,消除起步/到位/反转的加速度阶跃;
   - 夹到目标时不清 α,让 jerk 自然卸掉,避免二次阶跃;`jerk=0` 退回旧恒 α 行为。
2. `config_chassis.h`:`CHASSIS_SPIN_JERK_MAX=6000 deg/s³`;反转时长随动改为 0.9~1.2s
   (jerk 后 +300→-200 约 740ms、+390→-200 约 840ms)。
3. `gimbal.c`:速度环 Kp 双路调度的输出加一阶低通 `yaw_kp_dps_lp += YAW_SPEED_KP_LP·(kp_dps−lp)`,
   初值 `YAW_SPEED_KP_LOW`;只平滑过渡,不改变稳态调度值。
4. `config_gimbal.h`:`YAW_SPEED_KP_LP=0.30`(每拍,200Hz;τ≈12ms、截止≈13.6Hz)。

**仿真(120s×80 组)**。
- 起步 0→300 中位 522ms(恒 α 375ms,+147ms);300→400 约 230ms、300→180 约 265ms。
- 反转过零 `1832/1832`,到 -200 `1832/1832`;最低 ω 中位 -201 deg/s;11.4 次/分钟。
- 连续 BOOST 0 次;巡航稳态跟踪误差中位 2.0 deg/s。

**验证**。
- ARM 编译 0 error;`text 121,280` / `data 684` / `bss 126,776`。
- 6 项静态闸门 PASS;宏守恒 275/275,新增 2 宏已入库,仅剩工作区既有 3 个键鼠参数。
- `firmware/current.elf` 已同步,SHA-256 `6f368eac...c74216`。
- 未执行:台架 yaw 电流/roll-pitch 对比、实车对抗(需烧录;用户执行)。

**风险/后续**。
- 起步变慢 147ms,是 jerk 平滑的代价;若觉得肉,优先把 `CHASSIS_SPIN_JERK_MAX` 提到 8000
  (起步约 490ms),不要直接删掉限幅。
- Kp 低通只抑制快速抖动,不改变"巡航 300 正好在 `YAW_SPEED_KP_BLEND_LO=300` 拐点"这一事实;
  若实车某些转速段仍抖,下一步把 `BLEND_LO` 降到 220~250,让 180~400 全落在插值区。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-12｜2026-11-24｜键鼠斜向归一化;小陀螺平移偏置按 wz 比例调度

**目标与触发**。用户问变速小陀螺是否影响平移/横移的方向偏移,并要求补斜向合速度归一化。

**方向偏移的代码事实**。
- 指令方向不受变速影响:`chassis_mecanum_calculate()` 是线性叠加,`vx/vy` 斜坡独立于 `wz`。
- 但 `CHASSIS_TRANSLATE_BIAS_ANGLE=10°` 是**按恒速 300 标定的固定值**,只在 ROTATE 模式施加。
- 变速到 400 时打滑漂移更大,10° 不够;180 时 10° 过头;**反转 -200 时打滑方向反转,仍加 +10° 会加剧反向漂移**。

**改动**。
1. `cmd_kbm.c` 平移合成后按合速度归一化:`norm = sqrt(vx²+vy²)`,超过 `CHASSIS_MAX_SPEED=1500`
   则等比缩放。W+D 从 `1500/1500`(合 2121)变为 `1060/1060`(合 1500),方向保持 45°。
   与 `cmd_remote.c` 右杆的归一化口径一致。
2. `chassis.c` 平移偏置角改为按 `wz/CHASSIS_ROTATE_SPEED` 比例调度:
   300→10°、400→13.3°、180→6°、-200→-6.7°;限幅 ±20° 防异常 wz。反转符号自动反向。

**验证**。
- ARM 编译 0 error;`text 121,420` / `data 684` / `bss 126,776`。
- 6 项静态闸门 PASS;无新增宏。
- `firmware/current.elf` 已同步,SHA-256 `ebb27b76...7903c05`。
- 未执行:实车直线/横移轨迹对比(需烧录;用户执行)。判据:满平移+300 与恒速原行为一致;
  400/180/-200 时横向漂移小于固定 10° 版本。

**风险**。
- 偏置按 `wz` 线性调度是一阶近似;打滑可能非线性,幅值 10° 仍要实车复调。
- 归一化让键鼠斜向从 2121mm/s 降到 1500mm/s,斜向移动会变慢 29%,这是**纠正超速**,
  不是性能损失;若实车觉得斜向太慢,应调 `CHASSIS_MAX_SPEED` 而不是取消归一化。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-13｜2026-11-24｜按实车截图重排裁判UI状态栏与准星圆环

**目标与触发**。用户烧录后截图反馈:UI 已在线,但状态栏标签与值重复(`SPIN` + `SPIN:OFF`)、红/粉配色对比低、`TIER` 的 `[X]` 被截、准星圆环看不见。

**改动**。
1. 标签去冒号、改白色:`SPIN`/`FRICT`/`LOAD`/`TIER`,x=150,y=750/700/650/600(裁判 UI 原点左下,y 向上)。
2. 数值列去重复前缀,统一 18 字符,x=240:
   `VARY [Q]`、`OFF [RMB]`、`AUTO [R/E][LMB]`、`NORMAL 6.0/s [X]`。
3. 数值颜色按状态:小陀螺 VARY/CONST 绿、OFF 白;摩擦轮 ON 绿/OFF 白;
   发射 AUTO 青、SINGLE/BURST3 绿;射速 ECO 绿/NORMAL 白/BURST 黄。
4. 准星圆环半径 22→80、线宽 3→4、layer 8→9:原半径落在客户端默认准星大圆内,
   22px 被完全淹没。

**验证**。
- ARM 编译 0 error;`text 121,160` / `data 684` / `bss 126,776`。
- 4 项关键静态闸门 PASS。
- `firmware/current.elf` 已同步,SHA-256 `5b49bbd8...f9edbc8`。
- 未执行:实车确认客户端是否按 `UI_Graph_Change` 刷新文本/圆环颜色(需烧录;用户执行)。

**风险/回退**。
- 若客户端只覆盖文本不刷新 color,数值颜色会停在初始色;此时把变色图形改为
  `Delete + ADD`(会闪一帧),或退回固定白色。
- 圆环 80px 仍可能被客户端默认准星压住;若实车看不见,继续加到 110~140 或改 layer 9 确认生效。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-14｜2026-11-24｜自瞄圆环不可见:加大+整层重画+文字兜底

**目标与触发**。用户反馈"自瞄没有显示出来"。DEV-13 的圆环半径 80/线宽 4/白色,与客户端默认准星大圆同色重叠;且 `UI_Graph_Change` 对 color 字段不敏感时,状态切换不会变色。

**改动**。
1. 圆环半径 80→120、线宽 4→6,仍用 layer 9。
2. 自瞄状态变化时不再用 `Change`:先 `referee_ui_delete(UI_Data_Del_Layer, 9)` 删除整层,
   再 `UI_Graph_ADD` 重画绿/白圆环。layer 9 只放圆环,删除影响可控。
3. 准星下方新增兜底文字 `AIM ON`/`AIM OFF`(`sd4`,x=910,y=460,字号 18,绿/白),
   文本内容用 `Change` 更新——截图中 TIER 能从 NORMAL 变 ECO,证明文本 Change 可靠。

**验证**。
- ARM 编译 0 error;`text 121,420` / `data 684` / `bss 126,776`。
- 4 项关键静态闸门 PASS。
- `firmware/current.elf` 已同步,SHA-256 `1c942209...da4807b`。
- 未执行:实车确认圆环可见性与文字切换(需烧录;用户执行)。

**风险/回退**。
- 整层删除+重画会闪一帧,只发生在自瞄开关瞬间,可接受。
- 若圆环仍不可见但 `AIM` 文字能变:说明客户端圆绘制有问题,改用准星旁矩形或只保留文字。
- 若文字也不变:先确认 Shift 时处于键鼠输入源(动一下鼠标再按),再查 `auto_aim_active`。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-15｜2026-11-24｜UI:删AIM文字/加拨弹盘/删功率条

**目标与触发**。用户确认圆环显示正常,要求:删掉 `AIM ON/OFF` 文字只留圆环;在摩擦轮下方加"拨弹盘是否开启";删除上游遗留的功率条。

**改动**。
1. 删除 `AIM ON/OFF` 文字(`sd4` 不再画 AIM);圆环保留,换色仍用 `Delete layer 9 + ADD`,已实车验证可行。
2. 新增拨弹盘状态:
   - `referee_app_status_t` 加 `loader_on`;`cmd_get_loader_on()` 返回 `shoot_cmd.load_mode != LOADER_STOP`;
   - `assembly.c` 填字段;UI 新增 `LOADER` 行,ON 绿/OFF 白。
3. 状态栏改为 5 行:`SPIN` 750 / `FRICT` 700 / `LOADER` 650 / `LOAD` 600 / `TIER` 550,
   数值列 x=240,统一 18 字符。
4. 删除上游功率条:`Power:` 标签、能量框、浮点、粉色条全部不再 ADD;
   启动时 `referee_ui_delete(ALL)` 会清掉旧固件残留。

**验证**。
- ARM 编译 0 error,无新增 warning;`text 121,236` / `data 684` / `bss 126,784`。
- 4 项关键静态闸门 PASS。
- `firmware/current.elf` 已同步,SHA-256 `97916bd0...968ce583`。
- 未执行:实车确认 `LOADER` 随左键/发射请求切换(需烧录;用户执行)。

**口径说明**。
- `LOADER:ON` 表示当前存在非 STOP 的发射请求(单发/三发按住、连发按住、或队列未清),
  不是"拨弹盘电机使能"。后者由 `shoot_mode` 决定,正常运行时恒 ON,显示无意义。
- 若用户要的是电机总使能,改 `cmd_get_loader_on()` 读 `shoot_cmd.shoot_mode` 即可。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-16｜2026-11-24｜底盘功率控制首版:裁判功率闭环 + 缓冲能量降额

**目标与触发**。用户观察到跟随转向瞬时功率可到 300W、小陀螺变速反向 110~150W,要求做功率控制。此前工程无功率模块,`PowerControl()` 从未调用。

**现状事实**。
- 裁判 0x0202 已解析 `chassis_voltage/current/power/buffer_energy`,0x0201 有 `chassis_power_limit`;
  `chassis.c` 的 `referee_data` 可用,`PowerHeatTick` 每帧递增。
- 本车无超电,只能靠规则功率上限 + 60J 缓冲吸收瞬态。
- 不做电机模型前馈:KT/铜损未标定,`V×I`/`Kt·I·ω` 在正常巡航段就可能高估,容易把 45W 工况误限死。

**实现**。
1. 新增 `apps/infantry/chassis/chassis_power.{c,h}`:
   - `cap = limit × (1 + 0.5×buffer/60)`,45~67.5W;`buffer<10J` 时 `cap=limit×0.8`;
   - 实际功率 > cap 时 `scale *= cap/p_meas`;低于 `0.8×cap` 时每帧 +0.02 恢复;
   - 只在 `PowerHeatTick` 变化(10Hz 新帧)时更新,避免 200Hz 重复乘性降额。
2. `chassis_kinematics.c`:`chassis_limit_output(float power_scale)` 把 scale 乘到四轮
   `speed_PID.MaxOut`(基准 10000 码),不动速度目标;速度环 `PID_Integral_Limit` 处理降额后的积分饱和。
3. `chassis.c`:每拍取 `chassis_power`/`buffer_energy`/`chassis_power_limit`,裁判未接时 scale=1。
4. `config_chassis.h` 新增 9 个 `CHASSIS_POWER_*` 宏;`CHASSIS_POWER_CTRL_EN=0` 可整体回退。

**验证**。
- ARM 编译 0 error;`text 121,700` / `data 688` / `bss 126,784`。
- 6 项静态闸门 PASS;宏守恒 284/284(新增 9 宏已入账本/探针),仅剩工作区既有 3 个键鼠参数。
- `firmware/current.elf` 已同步,SHA-256 `f84d58d5...96f77da7`。
- 未执行:台架/实车功率曲线验证(需烧录;用户执行)。

**风险/标定**。
- `MaxOut` 与功率非线性,乘性降额需 2~3 帧(200~300ms)收敛;300W 峰值前 100~200ms 靠缓冲吸收。
- `scale` 下限 0.05 → MaxOut 500 码,四轮约 58W@24V;再低会掉到 45W 以下但转向/平移明显发顿。
- 调参顺序:先看 300W 是否被压到 60~80W;压不住降 `BUFFER_GAIN`(0.5→0.2)或 `SCALE_MIN`;
  正常转向太顿则反向调。不要同时动两个参数。
- 若后续要更快压峰值,加电机模型前馈;本版刻意不做,避免未标定参数把正常工况限死。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261124-17｜2026-11-24｜裁判接收修半帧丢失:回调回写 recv_len

**现象**。采样固件 VOFA 里 `chassis_power=0`、`buffer_energy=0`,RTT 一直 `[referee] waiting for robot_id`,但 USART6 硬件层其实有数据。

**证据**。
- `USART6 SR=0x000000D0`(IDLE=1)、`CR1=0x0000208C`(UE=1/RE=1/RXNEIE=1)、`BRR=0x2D9`(115200 正确)。
- `DMA2_Stream2 CR=0x0A020517`(EN=1/CIRC=1)、`NDTR=381`,已搬约 640 字节。
- 说明不是接线/波特率,是消费/解析层。

**根因**。
- `usart_process_all()` 的契约:回调入口 `recv_len=assemble_len`,回调把 `recv_len` 改小表示已消费字节数,余量 `memmove` 保留。
- `referee_judge_read()` 是 `void`,只改局部 `buff_len`,从不回写 `referee_usart_instance->recv_len`。
- 于是每次消费都把整块缓冲(包括半帧)当作已消费丢弃,0x0201/0x0202 可能长期拼不齐。

**修复**。
- `referee_judge_read()` 返回 `total - buff_len`(已消费字节数),所有提前 `return` 同样返回已消费量。
- `referee_rx_callback()` 把返回值写回 `referee_usart_instance->recv_len`,半帧由 `usart_process_all` 保留拼接。

**验证**。
- 正常固件与采集固件均编译 0 error;`check_naming/check_comment_style/check_source_lists` PASS。
- `firmware/current_ax650.elf`(正常) SHA `690bbbd5...f344d0`;
  `firmware/current.elf`(采集,VOFA) SHA `11f2083e...cfc98`。
- 未执行:实车确认 `robot_id`/`power` 是否恢复(需烧录;用户执行)。

**风险**。
- 若修复后仍为 0,说明 DMA 里的字节不是合法 0xA5 裁判帧,需要换逻辑分析仪/探针 dump 内容再查;
  本次探针出现 CMSIS-DAP command mismatch,无法直接 dump。
- 该修复对正常固件同样生效,属于公共组件修正。

**AI**:DeepSeek-V4.1-Flash,2026-11-24。

### DEV-20261123-26｜2026-11-23｜步兵摩擦轮目标按 24 m/s 候选值提升至 8000 RPM

**触发与目标**。用户实测当前 `4000 RPM` 时弹速约 `12 m/s`，要求把摩擦轮目标改为约
`24 m/s`。现有控制接口使用电机转子 RPM 而不是枪口 m/s，因此按近似线性关系把候选目标
换算为 `8000 RPM`；该换算尚未经过本车 24 m/s 实弹标定。

**修改**。
- `config/infantry_2026/config_shoot.h`：`SHOOT_FRICTION_TARGET_RPM 4000.0f → 8000.0f`；
  双轮到速互锁仍使用原 `±500 RPM`，控制链和发射状态机不变。
- `.Doc/spec/参数证据表.md`、`.Doc/spec/本车调参结论.md`：把参数证据改为
  `[换算 待实测]`，记录 4000 RPM≈12 m/s 的用户实测来源、线性外推限制和高速复测风险。
- `.Doc/gate/arch-baseline/macros-probe.txt`：同步宏金标准。

**范围边界**。本次没有修改 `AX650_TX_SPEED_MPS=12.0f`：它仍是裁判弹速首帧到达前的安全
回退值和上报下限；收到 USART6 裁判 `0x0207` 后，AX650 使用实时实测弹速。目标 RPM 不等于
实际弹速，摩擦轮压缩、打滑、电池电压和负载都会使比例关系偏离。

**验证**。
- `tests/run_profile_tests.ps1` PASS；宏账本、宏金标准、注释规范和 `git diff --check` PASS。
- `build.ps1 infantry -Current`：arm-none-eabi-gcc 14.3.1 编译链接成功，生成
  `firmware/current.elf`；FLASH 117,308 B，RAM 72,416 B，CCMRAM 54,944 B，仅有 newlib
  `_close/_lseek/_read/_write` 既有桩警告。
- 开工基线审计仍被仓库既有 `LICENSE` 哈希不一致阻断，报告 `snapshot corrupted`。
- 未烧录、未进行空载高速检查或实弹测速；首次测试必须用裁判弹速逐发核对，若双轮达不到
  8000 RPM、弹速超过目标或离散明显，应降低目标并按实测重新标定，不能继续线性外推。

**后续更正**。用户随后确认 `6900 RPM` 才是本车控制约 `24 m/s` 的实际目标值；8000 RPM
候选值已由 DEV-20261123-27 取代，本条保留为当时的换算记录。

**AI**:Codex,2026-11-23。

### DEV-20261123-27｜2026-11-23｜启用自瞄挡遥控拨轮单发并保留 6900 RPM

**触发与目标**。用户要求增加一个置 `1` 时允许 AX650 自瞄挡通过遥控拨轮发射一发的标志位，
并明确保留其已修改的 `6900 RPM` 摩擦轮目标，用于把本车实际弹速控制在约 `24 m/s`。

**实现**。
- 复用已有 `SHOOT_AUTO_AIM_REMOTE_ENABLE`，在 `apps/common/app/robot_def.h` 中由 `0→1`。
  自瞄挡持续开启摩擦轮；拨轮必须先回中，随后压入触发区才发布一个周期的
  `LOADER_SINGLE`；保持拨轮不重复登记，重新回中后才能触发下一发。
- 自瞄发射仍只接受遥控拨轮，不使用 AX650 `0x0503` 的 `shoot` 字段；失联、停机和热量守卫
  的既有优先级不变。宏置 `0` 可恢复自瞄挡发射链完全关闭。
- `SHOOT_FRICTION_TARGET_RPM` 正式记录为用户确认的 `6900 RPM`；参数证据表、调参结论、
  宏金标准和 README 同步。DEV-20261123-26 的 8000 RPM 线性候选值已被本条取代。

**验证**。
- `tests/run_chassis_tests.ps1` 全部 PASS，覆盖自瞄挡初始摩擦轮开启、拨轮单发脉冲、保持不
  重复和回中重装；注释检查与 `git diff --check` PASS。
- 先在仅包含本次暂存修改的隔离工作树构建，再清理主工作区全部 143 个对象并执行
  `build.ps1 infantry -Current`；arm-none-eabi-gcc 14.3.1 编译链接成功，最终 FLASH
  117,368 B，RAM 72,416 B，CCMRAM 54,944 B，仅有 newlib
  `_close/_lseek/_read/_write` 既有桩警告。
- 未烧录、未装弹实测；首次测试应先卸弹确认每次拨轮只产生一个拨盘单发动作，再装弹验证。

**AI**:Codex,2026-11-23。

### DEV-20261123-28｜2026-11-23｜自瞄拨轮触发后才开启摩擦轮

**触发与目标**。用户更正 DEV-20261123-27 的“自瞄挡摩擦轮持续开启”行为，要求只有拨轮
转动进入发射触发区后才开启摩擦轮。保留 `SHOOT_AUTO_AIM_REMOTE_ENABLE=1`、本车已确认的
`6900 RPM` 目标和遥控拨轮单发语义。

**实现**。
- `apps/infantry/cmd/cmd_remote.c`：进入 AX650 自瞄挡时摩擦轮默认关闭；拨轮从回中位置压入
  触发区后开启摩擦轮并发布一个周期的 `LOADER_SINGLE`。保持拨轮不会重复登记，回中后才
  能再次触发；松开后沿用 `SHOOT_DIAL_FRICTION_KEEP_MS=2500 ms` 续转，再自动关闭摩擦轮，
  使已登记的单发有时间等待双轮达到 6900 RPM。AX650 `0x0503` 的 `shoot` 字段仍不参与发射。
- `apps/common/app/robot_def.h`、`README.md`：同步宏行为说明；宏值仍为 `1`。
- `tests/chassis_remote_test.c`：覆盖自瞄空闲关闭、拨轮压入开启并单发、保持不重复、回中重装
  和续转超时关闭。

**验证**。
- `tests/run_chassis_tests.ps1` 两种宏配置均 PASS；看门狗边界测试 ALL PASS。
- `check_comment_style.py`、`check_source_lists.py`、`git diff --check` 全部 PASS。
- 清理 143 个构建对象后执行 `build.ps1 infantry -Current`，arm-none-eabi-gcc 14.3.1 编译
  链接成功；FLASH 117,416 B，RAM 72,416 B，CCMRAM 54,944 B，仅有 newlib
  `_close/_lseek/_read/_write` 既有桩警告。`firmware/current.elf` SHA-256 为
  `44EB4E52B0A8B506B5F996FFDB8BAD9EB9E36CE500A3B7C0B11AD84D8449A511`。
- 基线审计实际执行但被仓库既有 `LICENSE` 快照哈希不一致阻断，报告
  `snapshot corrupted (1 files) - HASH LICENSE`，未将该项声称为通过。
- 未烧录、未进行空载机构或装弹实测；首次台架验证应先卸弹，确认拨轮压入才启动摩擦轮且
  每次回中后只登记一发。

**AI**:Codex,2026-11-23。

### DEV-20261124-01｜2026-11-24｜裁判 robot_id 随 UART1 姿态包转发给 AX650

**触发与根因**。用户将裁判系统设为蓝方 3 号步兵后，AX650 日志仍显示
`lock:blue`。排查确认 AX650 的 `enemy_color=auto` 需要裁判 `robot_id`，而当前
USART1 `0x1021` 只携带姿态、弹速、模式和时间戳，导致上位机一直保留默认的
蓝色敌方。

**实现**。
- `0x1021` payload 从 21 字节扩展为 22 字节，在 `sample_time_ms` 后追加
  `uint8_t robot_id`；整帧从 30 字节变为 31 字节，CRC16 覆盖范围同步更新。
- `apps/infantry/app/assembly.c` 每周期将裁判 `0x0201 GameRobotState.robot_id`
  写入 AX650 组件缓存；尚未收到裁判数据时发送 `0`。
- 这个字段表示本方 ID：红方 ID 使 AX650 锁定蓝色，蓝方 ID 使 AX650 锁定
  红色。AX650 端的 17/21 字节旧包兼容逻辑在对应仓库中实现。

**验证**。
- `tests/run_ax650_tests.ps1` 的 `O2/Ofast × RX=0/1` 全部 PASS，包含 31 字节金标准帧、
  `robot_id=103`、CRC、分片/粘包、序号回绕和裁判字段转发合同。
- `build.ps1 infantry -Current` 编译链接成功；FLASH 117,464 B，RAM 72,416 B，
  CCMRAM 54,944 B。`firmware/current.elf` SHA-256 为
  `7C4F735114A2D750DFC917230AD573B76D9BAC8F83BE975B557D59D53835FC3E`。
- `check_comment_style.py`、`check_source_lists.py` 和 `git diff --check` PASS。开工基线审计仍被
  仓库既有 `LICENSE` 快照哈希不一致阻断，未将该项声称为通过。
- 未烧录 STM32；用户烧录新 `firmware/current.elf` 后，才会向已部署的 AX650
  发送新版 `robot_id` 字段。

**AI**:Codex,2026-11-24。

### DEV-20261124-02｜2026-11-24｜首发前以 0 标记弹速未就绪

**触发与目标**。用户要求上电后、裁判系统尚未给出首个实测弹速时，AX650
使用 `24 m/s` 作为首发前默认弹速，并在日志中标记 `first`；首发后收到裁判
`0x0207` 实测值后，改为转发实测弹速。

**实现**。
- `AX650_TX_SPEED_MPS` 从 `12.0f` 改为 `0.0f`，该值仅表示“尚无裁判实测弹速”，
  不再是弹道计算的默认值或下限。
- `ax650_uart1_set_projectile_speed()` 对非有限值和非正值写入 `0`；裁判系统给出
  的有效正值按原值转发，不再强制夹到 `12 m/s`。
- 同步更新 UART1 单元测试、宏账本、宏金标准、参数证据表和 README。AX650 端
  将该 `0` 替换为 `24 m/s`，并在首个实测值到达前打印 `speed:24.00m/s first`。

**验证**。
- `tests/run_ax650_tests.ps1` 的 `O2/Ofast × RX=0/1`、姿态协议和路由合同全部 PASS；
  覆盖初始 `shoot_speed=0`、`8 m/s` 原值转发以及非有限值回到 `0`。
- 同步远端并发提交 `fc303bd` 后重新执行 `build.ps1 infantry -Current`，编译链接成功；
  FLASH 117,792 B，RAM 72,424 B，
  CCMRAM 54,944 B。`firmware/current.elf` SHA-256 为
  `215954AC0AC66D3C41FB430D04EC7C9B3968D8D61BFB2F81886418DB89E840F5`。
- `check_comment_style.py`、`check_source_lists.py`、`check_macros.py`、
  `check_macro_golden.py`、`check_abi_golden.py`、`check_naming.py` 和 `git diff --check` PASS。
- 开工基线审计仍被仓库既有 `LICENSE` 快照哈希不一致阻断，未将该项声称为通过。
- 未烧录 STM32，未进行实弹验证；烧录本次 `firmware/current.elf` 后，才能在实车上
  验证 AX650 从 `24 m/s first` 切换到首个裁判实测弹速的整链路行为。

**AI**:Codex,2026-11-24。

### DEV-20261124-03｜2026-11-24｜删除 Shift 慢速，Shift 改自瞄 toggle 并与左中挡统一状态

**目标与触发**。用户要求删除 Shift 慢速逻辑与按键、增加自瞄启动按键;选定方案 =
Shift 改成自瞄 toggle(按一下开/再按关),且与遥控器左中挡统一成一个自瞄状态。

**根因/背景(代码事实)**。原自瞄状态散落两处:`cmd_remote.c` 读左中挡、`robot_cmd.c`
分派与回中判据也各读一次;`cmd_kbm.c` 的 Shift 只用于平移乘 `KBM_SLOW_RATIO=0.4`。
键鼠路径因 `robot_cmd.c` 左中挡强制走遥控而无法开自瞄。

**改动**。
1. `robot_cmd.c`:新增全局 `auto_aim_active`,每周期按 `左中挡 || cmd_kbm_auto_aim_toggle()`
   刷新;AX650 模式与回中判据统一改读该状态。抽出 `cmd_auto_aim_set()`(0x0503 →
   yaw/pitch 目标 + KF/平滑),从 `cmd_remote_set()` 原样搬迁,两路共用。
2. `cmd_kbm.c`:删 Shift 慢速(平移恒 `CHASSIS_MAX_SPEED`);新增
   `cmd_kbm_auto_aim_toggle()`(Shift 位域上升沿翻转锁存,只在键鼠+已解锁响应,
   切入键鼠时同步 last_shift 防残留沿);自瞄时清鼠标滤波、不叠加云台增量,
   改由 `cmd_auto_aim_set()` 给定目标 + `GIMBAL_AUTO_AIM_MODE`;底盘非小陀螺时改
   `CHASSIS_NO_FOLLOW`;平移与发射仍走键鼠原逻辑。
3. `cmd_remote.c`:`auto_aim` 改读统一状态,右上挡仍可手动接管;目标解算替换为
   `cmd_auto_aim_set()`。
4. `config_kbm.h` 删 `KBM_SLOW_RATIO`;宏账本/宏探针基线同步删除;
   参数证据表补"Shift 改自瞄"说明节(历史 KBM_SLOW_RATIO 档案条目保留不改写)。

**设计要点**。两入口取"或"且互不清除:Shift 锁存自瞄后拨左中挡再拨回,自瞄仍保持
(toggle 语义);自瞄期间键鼠保留 WASD 平移与鼠标开火,避免牺牲操作手感。

**验证**。
- ARM 全量编译(cmake+ninja):exit 0,第一方 0 warning(仅 newlib 桩既有警告)。
- 静态闸门:check_naming / check_comment_style / check_macro_dups / check_source_lists /
  check_macros 全 PASS;宏探针 248/248 平衡。
- `check_macro_golden` 仍 FAIL:3 个键鼠宏值变化(`KBM_YAW_SENS 0.7→0.06`、
  `KBM_PITCH_SENS 0.43→0.04`、`KBM_MOUSE_DEADZONE 2→4`)是工作区**既有未归档**改动,
  非本批引入,未替其确认。
- 未执行:实车功能验证(需烧录;用户执行)。判据:Shift 按一下进自瞄(云台跟目标)、
  再按退出(鼠标恢复控云台);左中挡进出自瞄正常;自瞄期间 WASD 可平移、鼠标可开火。

**风险**。Shift 为位域,沿检测靠 self-maintained `last_shift`;若 DBUS 帧抖动可能出现
连续 toggle —— 已用"只在键鼠输入源 + 已解锁 + 上升沿"三重门控。未烧录前不建议实战使用。

**AI**:Tencent Hunyuan Hy,2026-11-24。

### DEV-20261125-01｜2026-11-25｜底盘功率控制改为电机模型预测(2026 协议删除功率反馈)

**触发与根因**。用户报"裁判串口信息又读不到"。逐层实测后定位为**协议层删除**,不是接线或解析故障:

1. **接收链路正常**:探针读 DMA2_Stream2 `CR=0x0a020517`(EN/CIRC/CHSEL5)、NDTR 持续递减、`read_pos` 同步、`err_cnt=0`;ring 内 9 帧 0x0202 与 9 帧 0x0201 全部通过 CRC8+CRC16。
2. **0x0202 的 data 在总线上就是全 0**:9 帧 CRC 全过的 0x0202,14 字节 payload 无一非零;同批 0x0201 的 robot_id/血量/热量上限/弹速上限全部正常。
3. **协议依据**:《RoboMaster 2026 通信协议 V1.2.0》表 1-12 把 0x0202 前三项改成 `Reserved`(16 位/16 位/float),只保留 `buffer_energy` 与两个热量;0x0202 名称也从"功率热量"改为"实时底盘缓冲能量和射击热量数据"。
4. **官方是有意删除**:2025 物资说明原文"学生串口协议将逐步减少和删除可自行检测的数据;**在2025赛季,重点会删除功率数据的反馈**";协议 V1.7.0(2025-01-06)已把 0x0202 前 4 字节标 Reserved,长度 16;2026 进一步缩到 14。
5. **降级不可行**:RM Tool 2 主控只可选 12.0.0.10 / 12.0.0.3 / **9.0.0.5(2025-04-28,V1.7.0 同样无功率)**;电源管理模块只有 12.0.0.3/12.0.0.4。2024 的 V10(有功率)拿不到,且单降一侧不配套。
6. **离线模式 buffer 恒 0**:社区同类求助帖(2022)确认"开了离线模式,缓冲能量一直是 0";本车实测 `buffer_energy=0`。2025 起缓冲耗尽改为**底盘断电 5 秒**,用 buffer 做闭环容错率低。

**改动**。
1. `apps/infantry/chassis/chassis_power.c/.h`:改为电机模型预测。模型 `P = K0 + K1*i + K2*w + K3*i*w + K4*i² + K5*w²`,`i = 电流码/1000`,`w = 转子 deg/s`;新增 `chassis_power_predict_total()` 四轮求和,`chassis_power_update()` 改为 `(p_pred, limit_w)` 两参数,超限立即降额、恢复限速。
2. `config/infantry_2026/config_chassis.h`:新增 `CHASSIS_POWER_CURRENT_DIV` 与 `CHASSIS_POWER_MODEL_K0..K5`(天工大 304 战队 2026 开源实测值,MIT);删除已失效的 `CHASSIS_POWER_BUFFER_*` / `CHASSIS_POWER_RECOVER_RATIO`。
3. `apps/infantry/chassis/chassis.c`:功率段改取四轮 `measure.real_current` / `measure.speed_aps` 送入模型;上限仍取 0x0201 的 `chassis_power_limit`(该字段有效,实测 120W);补 `chassis_power_reset()`——此前从未被调用,静态系数跨运行残留。
4. `tests/run_fullchain_harness.ps1`:源列表补 `chassis_power.c` 与 `chassis_spin.c`(两者为新增文件,此前整链测试链接失败)。

**验证**。
- ARM 全量编译(cmake+ninja):exit 0,第一方 0 warning;text 121,924(+4,692),RAM 55.33%,CCM 83.84%,FLASH 11.69%。
- 主机测试:chassis / fwk / ax650 / profile **四套全 PASS**;fullchain golden determinism PASS。
- `refactor_gate.py check`:G1 宏/G1a 版本/G2 编译/G3 测试/G4 审计/G5 符号 **ALL PASS**(本批重锚 `build/refactor_baseline`,原因:2026 协议变更导致功率控制重构 + 前批遗留改动一并入库)。
- `check_naming` / `check_comment_style` / `check_macros` / `check_source_lists` / `check_sections` 全 PASS;`check_macro_golden --update` 后 286/286 平衡。

**风险与待办**。
- 模型系数取自 304 的 M3508 实测值,其转速单位未写明;本工程按 deg/s 转子代入,若实测预测功率随转速明显偏离,K2/K3/K5 需用本车功率计重拟合。静态校验:四轮 K0 之和 2.6W,与裁判 OLED 的 2.3W 同量级。
- 限幅仍作用在速度环 `MaxOut` 而非发送电流(APP 层拿不到"即将发送值"),饱和工况等效、稳态不饱和时功率本就低于上限,但非线性会导致轻度过度限制。
- 未烧录、未实车验证;**必须实测**:大功率工况下 scale 是否及时下压、恢复是否过冲、小陀螺+平移叠加是否发顿。
- 待办:按 V1.2.0 修正协议长度常量 —— `LEN_game_robot_state` 17→13、`LEN_game_robot_HP` 20→16、`LEN_projectile_allowance_t` 8→6(当前多拷会读到 CRC/下一帧,尾部字段为垃圾)。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-02｜2026-11-25｜裁判 UI 静态文字消失:客户端重连后无重发机制

**现象与根因**。用户报"客户端 UI 又不见了,自瞄圆环还在,字不见了"。排查确认**不是绘图失败,是缺少重发机制**:

- `referee_my_ui_init()` 只在 UI 任务启动时执行一次,19 个图元(静态标签 5 + 动态值 5 + 热量条/数字 + 准星线 + 圆环)一次性发出。
- 之后 `referee_my_ui_refresh()` 只在**数值变化**时发 Change,没有任何周期性全量重发。
- 裁决客户端重启、切换视角或重新进入比赛会清空本地 UI;机器人不会感知,于是静态标签永久消失。
- 圆环"还在"是因为它与自瞄状态绑定:状态切换时走 `UI_Data_Del_Layer,9` + 重画,等于被"顺带重发"了一次;文字没有这条路径。

**临时恢复**。重启机器人(UI 任务重新初始化);或对圆环那样触发一次状态切换(只恢复圆环,文字仍缺)。

**修复(已实施)**。`components/referee/ui/referee_task.c` 新增 `referee_ui_replay_static()`:每 300ms 重发 1 个静态图元,8 步(5 个标签 / 5 条准星线 / 热量条框 / 热量档位文字)约 2.4s 轮完一圈。只重发静态图元——动态值本来就在数值变化时走 Change,圆环随自瞄状态重画;整批重发会让 `referee_send()` 每帧 34ms 的延时累加到约 646ms,阻塞 UI 任务。

**验证**。ARM 0 error / 第一方 0 warning,text 122,408(+484);chassis/fwk/ax650/profile 四套主机测试 PASS;`refactor_gate` ALL PASS(重锚基线);naming/comment/macros/source_lists PASS。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-03｜2026-11-25｜修功率模型转速单位:deg/s 误当 rpm 导致过度限力

**现象**。用户报"总是会自己断电";实车表现是底盘频繁失去动力,并非裁判真的断电。

**根因**。304 的 M3508 系数按 **rpm** 拟合,DEV-20261125-01 直接把 `speed_aps`(deg/s 转子)代入。`K5*w^2` 对转速是平方关系:54000 deg/s 与 9000 rpm 差 6 倍,该项放大 36 倍,预测功率虚高到数百瓦,`scale` 随即掉到 `CHASSIS_POWER_SCALE_MIN=0.05`(MaxOut 819 码),四轮几乎没有扭矩。

**修复**。`chassis_power.c` 的 `motor_power_predict()` 内先 `w = w_dps / 6.0f` 转 rpm 再代入;config 系数保持 304 原值不动。

**量级复核(四轮合计)**。

| 工况 | 修正前(deg/s 代入) | 修正后(rpm 代入) | 判读 |
|---|---|---|---|
| 静止 | 2.6W | 2.6W | 与裁判 OLED 的 2.3W 吻合 |
| 巡航 3000rpm 小电流 | ~120W | 39.9W | 不再误限 |
| 巡航 5000rpm | ~300W | 93.2W | 接近 120W 上限 |
| 加速 2000rpm 大电流 | ~900W | 204.6W | 该降额 |
| 堵转满电流 | 197.6W | 197.6W | 与转速无关项 |

**验证**。ARM 0 error / 第一方 0 warning,text 122,416;量级由脚本复核;未烧录、未实车。

**教训**。引用外部模型参数时必须核对**自变量的量纲与单位**,不能只看系数值;平方项对单位误差的放大是平方级。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-04｜2026-11-25｜功率上限按 90W 钳制(裁判下发 120W 偏宽)

**背景**。用户确认本队底盘功率规则上限是 **90W**,但实机读到的 `chassis_power_limit`
是 **120W**(裁判按机器人等级下发,同时读到 robot_id=3 / max_HP=300 / cooling=100 /
heat_limit=320)。若直接信裁判值,允许功率会比规则宽 30W。

**改动**。
- `config/infantry_2026/config_chassis.h`:`CHASSIS_POWER_LIMIT_DEFAULT` 45→90
  (裁判未连时兜底也按规则上限,避免过度限制);新增 `CHASSIS_POWER_LIMIT_MAX = 90`。
- `apps/infantry/chassis/chassis.c`:裁判值超过 `CHASSIS_POWER_LIMIT_MAX` 时按上限算。

**验证**。ARM 0 error / 第一方 0 warning,text 122,432;chassis/fwk/ax650/profile
四套主机测试 PASS;`refactor_gate` ALL PASS(重锚基线);naming/comment PASS。
未烧录、未实车。

**待办**。实机 `robot_id` 读回 3(此前曾为 103 蓝方 3 号),裁判系统可能被重置;
蓝方队伍需重设 ID,否则 UI 仍画到红方客户端。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-05｜2026-11-25｜变速小陀螺:加卡死保护 + 压加速段功率

**触发**。用户要求先修变速小陀螺:①本身容易超功率 ②会出现停止转动。

**排查结论**。
1. 状态机无死锁路径:CRUISE/BOOST/GLIDE/BRAKE/REVERSE/RECOVER 都有时间超时兜底,
   `spin_target()` 最低段是 BRAKE(180deg/s),`mag` 又被 `LOW_DPS` 钳住,正常不会输出 0。
2. **"停转"的两个真实来源**:
   - CRUISE 可能带着 +800deg/s² 残余加速度直接进 REVERSE,`s_alpha` 翻负需要约 267ms
     (jerk 限幅),期间转速继续冲到 400+(按 α=800/jerk=6000 估算约 407),过零点被推后,
     低速段被拉长;
   - 完全没有卡死保护:一旦输出掉进低速段(功率限幅、数值异常),没有自愈路径。
3. **超功率来源**:BOOST(400deg/s)概率 40%、超时 350ms,"加速-滑行"节拍占比高;
   上面的换向超调又额外抬高瞬时功率。

**改动**。
- `config/infantry_2026/config_chassis.h`:
  - `CHASSIS_SPIN_BOOST_TIMEOUT_MS` 350→250(缩短高功率段停留)
  - `CHASSIS_SPIN_BOOST_PROB` 40→25(降低加速节拍占比)
  - 新增 `CHASSIS_SPIN_STALL_DPS=60` / `CHASSIS_SPIN_STALL_TIMEOUT_MS=600`
- `apps/infantry/chassis/chassis_spin.c`:
  - `spin_enter(REVERSE)` 清 `s_alpha`:反转从零加速度起步,消除 400+ 超调
  - `chassis_spin_update()` 加卡死保护:输出 <60deg/s 持续 600ms → 清 α + 强制回 CRUISE

**验证**。ARM 0 error / 第一方 0 warning,text 122,544(+112);chassis/fwk/ax650/profile
四套主机测试 PASS;`refactor_gate` ALL PASS(重锚基线);naming/comment/macros PASS。
未烧录、未台架。

**风险**。清 α 会在进入反转瞬间产生一次加速度阶跃(+800→0),扭矩冲击交给速度环吸收;
若台架电流尖峰明显,改为"先 BRAKE 再 REVERSE"两段式。卡死保护的 60deg/s 阈值低于
BRAKE 目标(180)与换向过零段(约 200ms),不会误触发。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-06｜2026-11-25｜UI 改为全量 ADD 重绘:修动态值与圆环不显示

**现象**。DEV-20261125-02 的静态重发生效后,用户反馈"只显示模式名字,切换后的值不显示,
自瞄圆环也没了"—— 与上一次正好相反。

**根因**。上次只重发**静态**图元,而动态值(`sd0..sd4`)走的是 `UI_Graph_Change`:
裁判客户端重连会清空本地 UI,此时 Change 找不到目标图元,被服务端直接丢弃。
圆环同理——它在重发列表里被刻意跳过(怕覆盖颜色),所以不会自己回来。

**改动**。`referee_task.c` 的 `referee_ui_replay_static()` 重写为
`referee_ui_redraw_item()` + `referee_ui_redraw_tick()`:
- 16 步覆盖全部图元:5 个静态标签 / 5 条准星线 / 圆环 / 热量条框 / 热量档位文字 /
  5 个动态值 / 热量数字 / 热量条填充
- **全部改成 `UI_Graph_ADD`**:客户端对同名图元 ADD 会覆盖,Change 类图元因此能被
  重新"创建"出来
- 圆环按当前 `auto_aim` 选绿/白,不再依赖状态变化沿
- 每 150ms 一步,16 步约 2.4s 一轮;不能整批:`referee_send` 每帧固定 34ms,
  16 帧会阻塞 UI 任务约 540ms

**验证**。ARM 0 error / 第一方 0 warning,text 123,832(+1,288);chassis/fwk/ax650/profile
四套主机测试 PASS;`refactor_gate` ALL PASS(重锚基线);naming/comment/macros PASS。未烧录。

**权衡**。UI 串口占用从"只在变化时发"变成"每 150ms 一帧",约 34/150 ≈ 23%;
`referee_send` 用 `kernel_task_delay` 让出 CPU,不影响 200Hz 控制任务。
若实车觉得占用高,把周期放宽到 250ms(约 4s 一轮)即可。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-07｜2026-11-25｜拨盘单发角度环提速:Kp 2.5→8、Ki 0.1→0.2、MaxOut 6000→15000

**说明**。改动先于本条落在工作区(代码注释已引用 DEV-20261125-07),本次补录并随
2026-11-25 提交入库。

**动机**。单发/三发每发延迟 0.5~1s。单发目标角 = 360/7 × 36 ≈ 1851 电机轴 deg;
原 Kp=2.5 在大误差段只给 2.5×1851 ≈ 4627 deg/s,低于 MaxOut 6000,角度环从未
用满转速上限。即使按 6000 deg/s 匀速,走完 1851 deg 也要 0.31s,再叠加加速与
PID 收敛,与现场观察的 0.5s+ 吻合。

**改动**(`config/infantry_2026/config_shoot.h`,参数级):
- `LOADER_ANGLE_PID_KP` 2.5→8.0:误差 1851deg 时输出 14808deg/s,大误差段直接顶到
  新 MaxOut。
- `LOADER_ANGLE_PID_KI` 0.1→0.2:到位静差收敛更快,仍只在小误差段起作用。
- `LOADER_ANGLE_PID_MAXOUT` 6000→15000(≈2500 RPM 转子):去掉人为限速;
  单发理论最短 1851/15000 ≈ 0.12s。

**验证**。参数级改动,ARM 0 error / 第一方 0 warning,四套主机测试 PASS。
**未上台架/实车**:新增益与 15000 MaxOut 的实际节拍、电流冲击、堵转判据是否误触发
均待实测;到位容差 `LOADER_ANGLE_TOL_DEG` 未改。

**风险**。拨盘冲击与电流峰值随 MaxOut 同比放大;若首测顶到 `LOADER_JAM_CURRENT`
(1800) 或出现弹丸跳槽,先把 MaxOut 降到 10000 再试;高增益下的过冲可看
`[shootcal]` 的 ldr 波形。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-08｜2026-11-25｜小陀螺匀速/变速运行期切换(键鼠 C)

**需求**。操作手希望小陀螺能在匀速(`CHASSIS_ROTATE_SPEED`)与变速轨迹之间现场切换,
原计划用 Tab 键。

**约束**。图传键鼠走 DJI DBUS 18 字节帧,键盘只有 16 个位
(W/S/A/D/Shift/Ctrl/Q/E/F/G/Z/X/C/V/B,见 `components/remote/remote_control.h`),
**没有 Tab 位**,机器人侧收不到 Tab;客户端侧把 Tab 映射为 C 的方案实际也不可用。
经用户确认:键位直接定为空闲的 **C** 键。

**改动**。
- `apps/common/app/robot_def.h`:`chassis_ctrl_cmd_t` 新增 `uint8_t spin_vary`
  (1=变速轨迹 0=恒速);布局守卫 ARM 21→22、主机 24→25。单板工程无对端固件;
  双板 CAN 协议需同步对端后才能混用。
- `apps/infantry/cmd/robot_cmd.c`:新增运行期档位 `spin_vary` 与
  `cmd_spin_vary_toggle()`;`cmd_task` 每周期写入 `chassis_cmd.spin_vary`;
  `cmd_get_spin_mode()` 改读运行期档位。
- `apps/infantry/cmd/cmd_kbm.c`:C 沿切换档位;切入键鼠时同步 C 的沿计数基线。
- `apps/infantry/chassis/chassis.c`:`CHASSIS_ROTATE` 分支按 `spin_vary` 选恒速或轨迹;
  降到恒速时清进入沿,切回变速从实测转速重新起步。
- `apps/infantry/chassis/chassis_spin.c/.h`:`chassis_spin_reset()` 增加 `wz_start` 参数,
  初值取 `est_wz`,避免匀速→变速瞬间掉到 0 再爬升;初值夹到 ±`CHASSIS_SPIN_HIGH_DPS`
  并过滤 NaN。
- `components/referee/ui/referee_task.c`:状态行改为 `VARY [Q/C]` / `CONST [Q/C]`。
- `tests/chassis_remote_test.c`:新增 C 沿 1→0→1 断言。
- `.Doc/gate/arch-baseline/abi-host.txt`:T11 快照重锚(仅 `chassis_ctrl_cmd_t` 24→25)。

**验证(与 -09 合并提交)**。ARM 0 error / 第一方 0 warning,text 123,144;
四套主机测试 PASS(chassis 新增 spin-vary-toggle 用例);fullchain 700 拍金标准
逐字节不变(脚本不跑 ROTATE);`refactor_gate` 重锚基线后 ALL PASS
(宏删除与符号尺寸变化属 -09 的预期);naming/comment/macros/ABI/source-list/sections
全 PASS。未烧录、未台架。

**风险/待实测**。
- 变速→匀速是阶跃(正弦当前转速→300deg/s,幅值最大约 90deg/s),实车若电流尖峰明显,
  再加一段 slew。
- 匀速档的平移偏置补偿仍按 `wz/CHASSIS_ROTATE_SPEED` 比例缩放,变速档沿用现逻辑。
- C 键需实车确认手感(位于 X/V 旁,不与现有键冲突)。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-09｜2026-11-25｜小陀螺去掉反转,改为恒正双尺度正弦调制

**现象**。用户实车反馈:变速小陀螺在反向段仍会超功率(DEV-20261125-05 已压过
BOOST 概率/时长,未解决)。反向要从 +300~390 刹到 -200 再回正,换向期间四轮
同时输出大扭矩;功率模型限幅用的是上一拍电流/转速,对这类阶跃滞后,裁判功率仍会
短时越 90W。

**决策**。用户给出两条路:解决反向功率,或去掉反向只做正弦。反转带来的抗自瞄收益
抵不上反复超功率的风险,选后者:移除 REVERSE/BOOST/GLIDE/BRAKE/RECOVER 状态机,
轨迹改为 `CRUISE × (1 + 双尺度正弦)`,输出恒正(210~390deg/s),功率峰值只随正弦
幅值缓慢变化,给模型限幅留出跟随时间(待实车功率波形确认)。

**改动**。
- `apps/infantry/chassis/chassis_spin.c/.h`:删除状态机、段长/概率、反转符号、
  卡死保护与 `chassis_spin_out_t`;保留 xorshift32(每次进入重掷两路相位)、
  双尺度正弦、α/jerk 限幅;`chassis_spin_update()` 改为只收 dt、返回 float。
- `apps/infantry/chassis/chassis.c`:调用改为 `chassis_spin_update(spin_dt_ms)`。
- `config/infantry_2026/config_chassis.h`:删除 16 个反转/分段/卡死宏;
  `LOW/HIGH_DPS` 改为正弦输出的上下限钳位,`ALPHA/JERK` 注释去掉反转描述。
- `.Doc/gate/macro-ledger.csv`、`macros-probe.txt`:同步删除 16 个宏。
- `.Doc/plan/小陀螺抗自瞄改造计划.md`:参数表/接口/阶段表更新,P3 标为已移除,
  反转章节保留为历史。

**验证**。ARM 0 error / 第一方 0 warning,text 123,144(较 DEV-20261125-06 的
123,832 **−688 B**:状态机删除省下的代码大于 C 键切换新增);
四套主机测试 PASS;fullchain 700 拍金标准逐字节不变(不跑 ROTATE);
`refactor_gate` 重锚基线后 ALL PASS;naming/comment/macros/ABI 全 PASS。未烧录、未台架。

**风险/待实测**。
- 纯正弦是准周期轨迹,长时间观测后可被拟合;换来的是功率可控。若对抗验证发现
  命中率下降不足,优先加"随机幅值/相位",不要恢复反转。
- 正弦本身最大角加速度约 273deg/s²、jerk 约 1044deg/s³,均在
  `ALPHA_MAX=800` / `JERK_MAX=6000` 内,波形不会被削顶。
- 变速→匀速阶跃最大约 90deg/s(210~390→300),比原反转切换温和。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261125-10｜2026-11-25｜功率控制真正生效(压轮速目标)+ 小陀螺空闲不再自停

**现象 1**。用户实测小陀螺高速段功率约 100W(超 90W 约 10W),怀疑功率控制没工作。
**排查**。控制确实在跑(`CHASSIS_POWER_CTRL_EN=1`,`chassis_power_update` 每拍返回
scale),但 scale 只乘到速度环 `MaxOut`。小陀螺稳态不饱和,稳态电流远低于 MaxOut,
把 MaxOut 从 10000 压到 9000 不改变实际电流,所以功率不变。

**现象 2**。小陀螺在无操作时自己停转。
**根因**。键鼠 3s 无输入触发 `KBM_TIMEOUT_MS` 回落遥控;`cmd_remote_set` 按右拨杆
重设底盘模式,右中挡=跟随/无跟随,`spin_enabled` 锁存被绕过 → 退出 ROTATE。

**改动**。
- `config/infantry_2026/config_chassis.h`:
  - 新增 `CHASSIS_POWER_MARGIN_W=5`:控制目标 = 规则上限 90W − 5W = 85W。
  - `CHASSIS_POWER_RECOVER_STEP` 0.02→0.005:压轮速目标后 0.02 会在
    "降速-功率回落-恢复"之间形成约 10W 极限环。
- `apps/infantry/chassis/chassis.c`:算 `limit_w` 后减 `CHASSIS_POWER_MARGIN_W`,
  下限 20W。
- `apps/infantry/chassis/chassis_kinematics.c`:`chassis_limit_output()` 把
  `power_scale` 同时乘到四轮速度目标,再按 MaxOut/最高转速兜底。
- `apps/infantry/cmd/cmd_kbm.c`:键鼠超时回落条件加 `!spin_enabled` —— 小陀螺锁存
  期间不回落遥控;遥控急停/失联仍由仲裁独立处理,按 Q 关闭后可正常回落。
- 宏账本/探针同步(`CHASSIS_POWER_MARGIN_W` 新增,`RECOVER_STEP` 改值)。

**验证**。ARM 0 error / 第一方 0 warning,text 123,212(+68);
四套主机测试 PASS;fullchain 700 拍金标准逐字节不变(脚本功率未越限);
`refactor_gate` 重锚后 ALL PASS;naming/comment/macros/ABI 全 PASS。未烧录、未台架。

**预期/待实测**。
- 小陀螺高速段目标 85W;按模型线性近似,转速峰值会从 390 降到约 330deg/s。
  实际功率是否守在 85W 取决于模型精度,需实车看裁判功率。
- 若仍超:先把 `CHASSIS_POWER_MARGIN_W` 加到 10,或 `CHASSIS_POWER_LIMIT_MAX`
  降到 85;不要恢复反转。
- 若出现 2~3Hz 周期性抽动,把 `CHASSIS_POWER_RECOVER_STEP` 再降到 0.002。
  (该参数已在 DEV-20261126-12 中由 `CHASSIS_POWER_SCALE_LP` 取代。)
- 小陀螺锁存期间遥控摇杆不生效(输入源仍是键鼠),这是刻意取舍;急停拨杆始终有效。

**AI**:DeepSeek-V4.1-Flash,2026-11-25。

### DEV-20261126-01｜2026-11-26｜[SENTRY] 明确哨兵工程身份与 Git 强制同步规范

**目标**。用户指定远端 `https://github.com/youyucat/RM-JIT-CR`,要求先同步最新代码,并把“当前项目是哨兵 Sentry、每次 STM32 工程修改必须配套 Markdown 说明并同步 Git”固化为仓库规则。

**同步基线**。当前分支 `main`,同步前提交 `234c78b`;执行 `git fetch --prune origin` 后 `HEAD...origin/main = 0 0`,再执行 `git pull --ff-only origin main`,结果 `Already up to date`。工作区原有 EmberProbe skill、`.clangd`、`.vscode/*` 与 `.settings/` 本机改动未覆盖、未纳入本次任务。

**哨兵影响**。本次只修改文档与协作契约,不改变 STM32 控制逻辑、参数、协议、构建默认值或固件产物。仓库当前交付目标显式固定为哨兵 Sentry;有效应用/参数入口为 `apps/sentry/` 与 `config/sentry_2026/`;CMake 无参数默认 infantry 仅保留历史兼容,哨兵交付必须执行 `build.ps1 sentry -Current` 或 sentry preset。

**变更位置**。

1. `README.md`:标题和首屏增加哨兵项目身份,哨兵目录/参数/构建入口前置,步兵接线降级为历史参考并加禁止误烧提示。
2. `AGENTS.md`:工程地图切换到哨兵,规定 DEV/提交必须含 `[SENTRY]` 或 `sentry(<scope>)`,每次 STM32 改动执行 `pull --ff-only → Markdown → commit → push → 0/0 核对`。
3. `.Doc/guide/团队协作指南.md`:命令、示例、调参路径和提交格式全部改为哨兵工作流;`.Doc/README.md` 增加索引入口。
4. `RM_DEVLOG.md` §0:增加哨兵工程目标并把有效配置改为 sentry 装配入口。

**验证与证据**。文档改动执行 `git diff --check` 与 `python tools/refactor_gate.py check-links`,均通过;未修改 C/C++、config 或构建文件,因此 ARM 编译、主机测试、烧录、台架和整车均不适用/未执行。基线审计执行但未取得可靠计数:`.agents/skills/rm-electrical-dev/assets/yuelu-basic-framework/LICENSE` 实际 SHA256 `ed75fb030bf39c6d5bc91bb47d3d3bfbf9282517fc5b9364b98e4c85bef9517f`,manifest 期望 `47c4b2a98a02ecbf399033453bb8333531a3c2698d1c88f04e3e32abcef022c2`,脚本报告 `snapshot corrupted (1 files)`;两文件均不在工作区改动列表,本条不擅自修复历史快照。

**提交与推送**。本条所在提交使用 `sentry(docs): ... (DEV-20261126-01)`;推送后以 `git rev-list --left-right --count HEAD...origin/main = 0 0` 作为同步完成判据,实际 SHA 与推送结果见本次交付。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-02｜2026-11-26｜[SENTRY] USART1 输出三轴编码器 VOFA，关闭 AX650 并独立 VCP 通信

**目标与边界**。用户要求哨兵通过 UART1 向 VOFA 输出大 yaw、小 yaw 和 pitch 电机编码器位置，并明确哨兵不再使用 AX650，自瞄/上位机通信改走独立 USB CDC 虚拟串口。本条不新增任务、函数、消息字段或构建文件，也不臆造尚未给出的 VCP 应用帧协议；现有 USB CDC 负责枚举和端口能力，具体下行帧、坐标系、超时与安全降级留待单独定义。

**行为变化**。

1. `AX650_UART1_ENABLE=0`、`AX650_UART1_RX_ENABLE=0`，旧 USART1 `0x0503/0x1021` 链路不再初始化或收发；左中挡的自瞄和回中判据同时受该总开关约束，关闭后按普通遥控处理。
2. `GIMBAL_VOFA_UART1_ENABLE=1`。`gimbal_init()` 在初始化期注入 `&huart1` 并注册 TX-only 端口，`gimbal_task()` 以 200 Hz 发送 VOFA JustFloat。运行期只经 `usart_is_ready()` / `usart_try_send_it()` 访问端口，不直接操作 HAL 句柄或寄存器。
3. `VISION_USE_VCP` 明确为上位机独立物理链路。当前没有注册哨兵应用层 RX 回调，因此文档明确标为“应用协议待定义”，不得把 USB CDC 收到的任意字节直接作为控制命令。
4. USART1 所有权守卫扩展为云台 VOFA、底盘 VOFA、旧 AX650 三者互斥；当前组合为云台 VOFA=1、底盘 VOFA=0、AX650=0。

**VOFA 协议与接线**。USART1 为 PA9 TX/PB7 RX、921600 8N1；USB-TTL RX 接 PA9、GND 共地、不接 5 V。每帧 3 个小端 `float` 后跟 `00 00 80 7F`，共 16 B：通道 0=大 yaw DM `measure.total_position`，rad；通道 1=小 yaw GM6020 `measure.ecd`，原始码 0~8191；通道 2=pitch DM `measure.total_position`，rad。`GIMBAL_VOFA_SEND_DIVIDER=1` 对应 200 Hz，置 2 为 100 Hz。

**文件**。`apps/sentry/gimbal/gimbal.c`、`apps/sentry/cmd/{cmd_remote.c,robot_cmd.c}`、`apps/sentry/app/sentry_hooks.{c,h}`、`config/sentry_2026/config{,_gimbal,_ax650,_board,_chassis,_autonomy}.h`、`README.md`、`RM_DEVLOG.md`。

**验证证据**。

- `build.ps1 sentry -Current` 成功，0 error；FLASH 116228 B，RAM 72712 B，CCMRAM 54944 B，text/data/bss=115532/688/126976。`firmware/current.elf` SHA-256=`3BB3097987D030EDA84F039B266E80CD3983C018541AB522275EFD1ACA6E6F7D`，profile=sentry。
- 四套主机测试中 chassis、AX650、profile 通过；fwk 聚合在与本次无关的 infantry fullchain harness 链接阶段失败：`gimbal_imu_data` 在 `apps/infantry/gimbal/gimbal.c` 与 `tests/fullchain_harness.c` 重复定义。本条哨兵文件不参与该失败链接，未把 fwk 套件记为通过。
- `check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`check_comment_style.py`、`check_source_lists.py`、`check_symbol_contract.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过；为满足固定读取 `build/basic_framework.elf` 的符号契约工具，另执行一次不带 `-Current` 的 infantry 兼容构建，成功且未覆盖哨兵固定烧录副本。
- 基线审计仍因仓库既有 skill 快照 `LICENSE` 哈希不匹配退出，未取得可靠计数；该文件不在本次改动中。编译保留既有 CMake `COMMENT/CMP0175` 开发警告、sentry `gimbal.c` 两条未使用变量警告和 newlib syscall 链接警告。

**未执行与待实测**。未烧录、未接 VOFA、未做台架或整车动作。用户烧录后需架空核对三通道方向、单位、零位、0~8191 跳变和 200 Hz 刷新；VCP 控制下行必须先明确协议与安全边界后另开修改。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-03｜2026-11-26｜[SENTRY] 哨兵固件固定生成 sentry.elf

**目标**。用户要求编译后的哨兵固件单独命名为 `sentry.elf`，避免与步兵或通用名称产物混淆。本条只修改构建产物命名与文档，不改变 STM32 源码、参数、协议或运行行为。

**构建行为**。`build.ps1 sentry` 在 CMake 内部产物 `build_sentry/basic_framework.elf` 链接成功后，生成明确交付名 `build_sentry/sentry.elf`；使用 `-Current` 时再生成 `firmware/sentry.elf`。既有 `firmware/current.elf` 和 `current.txt` 继续保留，兼容 EmberProbe 与 VSCode 已固定的路径；`current.txt` 新增 `NAMED=.../firmware/sentry.elf`，且 `SOURCE` 指向 `build_sentry/sentry.elf`。人工选择哨兵 ELF 时应优先使用 `firmware/sentry.elf`，不得使用步兵目录的 `build/basic_framework.elf`。

**文件**。`build.ps1`、`README.md`、`RM_DEVLOG.md`。没有修改 CMake target、VSCode 任务或烧录脚本，原有构建入口保持兼容。

**验证**。执行 `powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 sentry -Current` 成功，脚本明确打印 `ELF=build_sentry/sentry.elf`、`SENTRY=firmware/sentry.elf`、`CURRENT=firmware/current.elf`。四份 ELF 长度均为 1428972 B，SHA-256 均为 `3BB3097987D030EDA84F039B266E80CD3983C018541AB522275EFD1ACA6E6F7D`。`git diff --check` 与 `refactor_gate.py check-links` 通过。编译沿用既有 ELF，Ninja 报 `no work to do`；未烧录、未操作实车。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断，与本次文件无关。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-04｜2026-11-26｜[SENTRY] 右摇杆控制底盘自由平移，左摇杆控制大 yaw

**目标**。用户要求哨兵遥控器右摇杆控制四个底盘 M3508 自由移动，左摇杆控制大 yaw 旋转；右中挡下底盘与大 yaw 相互独立，不再执行底盘跟随云台。

**行为变化**。右拨杆中挡把底盘模式固定为 `CHASSIS_NO_FOLLOW`：右摇杆纵向生成前后速度 `vx`，横向生成左右速度 `vy`，底盘角速度为 0，不再根据大 yaw 角差自动旋转。左摇杆横向沿用世界系大 yaw 目标增量控制，左摇杆纵向仍控制 pitch。右拨杆下挡的小陀螺和右摇杆叠加平移保持不变；由小陀螺切回右中挡时不会触发“回正后跟随”，避免重新耦合底盘与大 yaw。

**边界**。本条没有新增任务、函数、消息字段、配置项或构建文件，也没有改变大 yaw 速率、底盘最大速度、死区、急停、解锁、发射、AX650/VCP 或 USART1 VOFA 行为。

**文件**。`apps/sentry/cmd/cmd_remote.c`、`README.md`、`RM_DEVLOG.md`。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 116224 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115528/688/126976。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `A2843E107F06EBDA1BFAA5131445E26FF70B4057BC25EB8E8435F91C5A27D257`。`tests/run_profile_tests.ps1` 通过（heavy/sentry profile anchor 与 sentry yaw allocation 均 PASS）；`check_comment_style.py`、`check_naming.py`、`refactor_gate.py check-links`、`git diff --check` 通过。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断，与本次三个文件无关。未烧录、未操作实车，摇杆方向和手感仍需用户架空确认。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-05｜2026-11-26｜[SENTRY] 取消解锁时底盘自动回中

**现象与根因**。用户实车反馈右拨杆从解锁上挡切到中挡后，底盘会自行旋转，表现类似小陀螺。右中挡本身已正确生成 `CHASSIS_NO_FOLLOW`，但解锁沿仍把 `yaw_recentering` 置 1；右上挡期间该请求被急停零力遮蔽，切到右中挡后 `robot_cmd.c` 又把模式覆盖为 `CHASSIS_ALIGN`，以回中速度旋转，形成非预期动作。

**修复**。删除解锁路径对 `yaw_recentering` / `recenter_cnt` 的写入。解锁现在只把世界大 yaw 与 pitch 目标重锚到当前反馈，随后右拨杆切到中挡直接保持 `CHASSIS_NO_FOLLOW`，不会触发底盘自动回中。键鼠 `F` 主动回中、小陀螺切换逻辑、右下挡 `CHASSIS_ROTATE`、急停与失联保护均保持不变。

**文件**。`apps/sentry/cmd/cmd_authority.c`、`apps/sentry/cmd/robot_cmd.c`、`README.md`、`RM_DEVLOG.md`。没有新增任务、函数、消息字段、配置项或构建文件。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 116204 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115508/688/126976，较 DEV-20261126-04 删除解锁回中路径后 text −20 B。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `7F9CECB4ABC59BA56BB475F0A4DAEE21935E22B36E4247C12F642EECC8C67E23`。`tests/run_profile_tests.ps1` 通过；静态反查确认 `yaw_recentering = 1` 仅余键鼠 `F` 和小陀螺切回跟随两处，解锁仲裁文件已无写入。`check_comment_style.py`、`check_naming.py`、`refactor_gate.py check-links`、`git diff --check` 通过。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断，与本次四个文件无关。未烧录，修复后的实车行为仍需用户架空确认。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-06｜2026-11-26｜[SENTRY] 左摇杆只驱动大 yaw，小 yaw GM6020 失能

**现象与根因**。用户实车反馈左右拨杆处于中挡、左摇杆横向操作时，动作主要由小云台 GM6020 完成。现有双 yaw 分配把 `SENTRY_SMALL_YAW_FINE_DEG=10°` 以内的世界 yaw 误差全部交给小 yaw，大 yaw 只承担超出部分，因此小幅摇杆输入只动 GM6020 是原控制律的预期行为，但不符合当前单独使用大 yaw 的需求。

**修改**。新增哨兵配置开关 `SENTRY_SMALL_YAW_ENABLE=0`。正常控制下小 yaw 可用角强制为 0°，`sentry_gimbal_allocate()` 因而把完整世界 yaw 目标交给大 yaw DM；GM6020 实例继续注册并接收 CAN 反馈，但每拍保持 `dji_motor_stop()`，发送槽位为零输出。USART1 VOFA 通道 1 继续输出 GM6020 编码器原始值，便于观察接线。小 yaw 台架扫掠模式在失能配置下直接拒绝启动，避免误把 ±125° 测试目标转交给大 yaw。

**保留行为**。左摇杆纵向仍控制 pitch；右摇杆仍控制四个底盘 M3508；右中挡底盘不跟随大 yaw；右下挡小陀螺、急停、失联保护、解锁不回中和大 yaw/pitch 控制参数均不变。重新将 `SENTRY_SMALL_YAW_ENABLE` 置 1 可恢复原双 yaw 粗精分配。

**文件**。`config/sentry_2026/config{,_gimbal}.h`、`apps/sentry/gimbal/gimbal.c`、`tests/sentry_gimbal_alloc_test.c`、`README.md`、`RM_DEVLOG.md`。没有新增任务、函数、消息字段或构建文件。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 116000 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115304/688/126976，较 DEV-20261126-05 的 text −204 B（当前配置裁剪小 yaw 正常控制与扫掠分支）。`tests/run_profile_tests.ps1` 通过，新增用例确认 `small_target=0°`、`big_target=30°`，即大 yaw 获取完整世界目标。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `79AF02DBF228DA263BCC9D2AF11A8BEB0F3870A81B7BA19A17012AB1EFF3EDE9`。`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`check_macro_golden.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过；基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断。编译保留 `gimbal.c` 既有两条未使用变量警告和 newlib syscall 链接警告。未烧录，大小 yaw 方向和大 yaw 手感仍需用户架空确认。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-07｜2026-11-26｜[SENTRY] 提高大 yaw 速度环出力

**触发与判断**。用户烧录大 yaw 独占目标固件后反馈左摇杆仍拨不动大 yaw，并明确要求通过 PID 增加扭矩。现有最大力矩与速度环输出上限都已是 DM-J4310 参考上限 10 N·m，继续抬上限不安全；历史阶段 0 还记录过同步带 5~7° 虚位，直接提高角度环 Kp 会加重换向冲击。因此本轮保持角度环 `Kp=12`、死区 1°、输出滤波 0.03 s 和最大力矩 10 N·m，只提高速度环产生实际力矩的能力。

**参数变化**。`SENTRY_BIG_YAW_SPEED_KP 0.063→0.10 N·m/(deg/s)`，同一速度误差下比例力矩提高约 59%；`SENTRY_BIG_YAW_SPEED_KI 0.0105→0.015`，增强克服静摩擦的累积出力；`SENTRY_BIG_YAW_SPEED_IL 1.2→2.0 N·m`，允许更大的稳态补偿但仍远低于 10 N·m 总限幅。小 yaw 继续失能，左摇杆世界 yaw 目标仍全部交给大 yaw。

**安全边界**。新增 sentry profile 测试约束：最大力矩不得超过 10 N·m、速度环输出不得超过最大力矩、积分限幅不得超过速度环输出，且 Kp/Ki 非负。如果新固件仍完全无动作，应停止继续加 PID，检查 `gimbal_fb_recv.big_yaw_online`、DM CAN1 控制 ID 5/Master ID 0、驱动器 ENABLE 状态和 `big_yaw_torque_nm`；离线分支会强制输出 0，任何 PID 值都无法生效。

**文件**。`config/sentry_2026/config_gimbal.h`、`tests/profile_compile_sentry.c`、`RM_DEVLOG.md`。没有修改电机协议、控制链、任务、消息字段、最大力矩或其他轴参数。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 116000 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115304/688/126976。`tests/run_profile_tests.ps1` 通过，其中 sentry profile 的大 yaw 力矩边界检查 PASS。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `B4FB1069DA0466044A8EC9F3319F0826A7904CCC1175560DF1BFE1428125B25A`。`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过；基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断。编译保留 `gimbal.c` 既有两条未使用变量警告和 newlib syscall 链接警告。未烧录，实际起步力矩、方向、振荡和同步带状态仍需用户架空确认。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-08｜2026-11-26｜[SENTRY] 临时关闭大 yaw 输出供同步带自由移动检查

**触发与现场证据**。用户反馈大 yaw 能带动同步带但转动不流畅；EmberProbe 连续采样确认遥控横杆、目标角和工作模式正常，但 DM-J4310 一受力便返回 `0xC5`，`big_yaw_online` 变 0，应用力矩被安全分支清零。随后用户要求先失能大 yaw，使机构可以自由移动检查。当前不绕过驱动器故障，也不继续提高 PID。

**修改**。新增 `SENTRY_BIG_YAW_OUTPUT_ENABLE=0`。`gimbal_task()` 无论遥控目标和工作模式为何，都对大 yaw 实例保持 `dm_motor_stop()`，每拍清零应用层角度/速度 PID 与 `big_yaw_torque_nm`；DM MIT 发送帧保持 Kp=0、Kd=0、力矩=0，机构可手动移动。电机实例、CAN 接收、状态/温度反馈和 USART1 VOFA 通道 0 保留，便于继续检查同步带、轴承和驱动器。小 yaw 的既有失能状态、pitch、底盘和发射不变。

**恢复方式与边界**。机械与驱动器故障处理完成后，把 `SENTRY_BIG_YAW_OUTPUT_ENABLE` 改回 1，重新编译并烧录即可恢复大 yaw 闭环。本次是软件零输出，不发送 DM `RESET_MODE` 硬失能命令；这样既允许低阻手动检查，也保留反馈诊断能力。若需要完全切断电机电气使能，应断开动力或使用达妙调试助手，不在运行控制链内自动清除 `0xC` 保护。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 115884 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115188/688/126976。`tests/run_profile_tests.ps1` 通过，新增 profile 锚点确认 `SENTRY_BIG_YAW_OUTPUT_ENABLE` 必须为 0；`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `8F30B7D1CADD966909425EB57AA7C2B76FD36B8C96F6558F4EE9F37D455541C7`。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断；编译保留 `gimbal.c` 两条既有未使用变量警告、newlib syscall 链接警告和 CMake CMP0175 开发警告。未烧录、未操作实车。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-09｜2026-11-26｜[SENTRY] 记录当前大 yaw 人工中心编码值

**目标与边界**。用户把当前手动摆放位置定义为云台机械中心，要求获取大 yaw 编码器值并保存，但明确不改成云台归中模式。本条只更新大 yaw 零位参数；`SENTRY_BIG_YAW_OUTPUT_ENABLE=0` 保持不变，解锁不触发自动回中，遥控模式与 `yaw_recentering` 调用链均不修改。

**实测证据**。EmberProbe 使用已烧录且哈希匹配的 `firmware/sentry.elf` 连续读取 10 次：`big_yaw_motor->measure.total_position=1.4368276596 rad`、单圈 `position=1.4368276596 rad`，速度约 `-0.007326 rad/s`，DM 状态 `0x1`、`rx_refresh_dt=0~1`、`big_yaw_online=1`、应用力矩 0。按六位小数写入 `SENTRY_BIG_YAW_ZERO_RAD=1.436828f`，该位置计算出的相对关节角约为 0°。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 115884 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115188/688/126976。`tests/run_profile_tests.ps1` 通过，其中大 yaw 固定失能锚点继续 PASS；`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`check_macro_golden.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `938D87BE06D995DCCA1E455006D4597A3BBD1FF53F2700BEDBDABCCD1C387BAE`。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断；编译保留既有警告。未烧录、未操作实车。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-10｜2026-11-26｜[SENTRY] 恢复大 yaw 闭环输出

**目标与修改**。按用户要求使能大 yaw，把 `SENTRY_BIG_YAW_OUTPUT_ENABLE` 从 0 改为 1，并把 sentry profile 锚点同步改为必须使能。大 yaw 恢复角度环→速度环→MIT 力矩输出；左摇杆横向目标继续全部交给大 yaw，小 yaw GM6020 仍固定零输出。

**保留行为与安全边界**。人工中心零位 `SENTRY_BIG_YAW_ZERO_RAD=1.436828f` 保持不变；解锁仍只重锚当前云台目标，不设置 `yaw_recentering`，因此不会自动回中。最大力矩仍限制为 10 N·m，速度环参数与 DEV-20261126-07 一致。DEV-20261126-08 曾观测到 DM-J4310 受力返回 `0xC5`；若恢复后再次报错或动作不流畅，应立即急停并检查同步带、轴承、驱动器保护和 CAN 状态，不继续盲目增加 PID。

**文件**。`config/sentry_2026/config_gimbal.h`、`tests/profile_compile_sentry.c`、`README.md`、`RM_DEVLOG.md`。没有修改控制链、任务、消息字段、零位、归中模式或其他轴参数。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 116000 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115304/688/126976。`tests/run_profile_tests.ps1` 通过，sentry profile 输出 `big-yaw enabled` 且力矩边界继续 PASS；`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`check_macro_golden.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `090B741C76BC067AA8BB5CC00EE81D555F2AFBA18B66502CA48A453747E299BA`。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断；编译保留 `gimbal.c` 两条既有未使用变量警告、newlib syscall 链接警告和 CMake CMP0175 开发警告。未烧录、未操作实车。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-11｜2026-11-26｜[SENTRY] 大 yaw 带载失败后重新失能

**触发与实测证据**。用户确认大 yaw 仍带不动机构并要求重新失能。EmberProbe 使用与板上固件匹配的 `firmware/sentry.elf` 采样：推左摇杆时角度误差约 80°、角度环速度目标达到 240 deg/s，速度环输出和最终命令均顶到 10 N·m，但电机速度仍接近 0；随后 DM 状态持续为 `0xC`（线圈过温），转子温度回传 59~67°C，安全分支把应用力矩清零。该现象不能通过继续增大 PID 解决。

**修改与边界**。把 `SENTRY_BIG_YAW_OUTPUT_ENABLE` 从 1 改回 0，并把 sentry profile 锚点同步改为必须失能。运行时对大 yaw 固定发送 Kp=0、Kd=0、力矩=0，保留电机实例、CAN 状态、温度、编码器反馈和 USART1 VOFA 通道 0。人工中心零位 1.436828 rad、自动归中状态、遥控映射、小 yaw、pitch、底盘和发射均不修改。

**恢复条件**。只有在机械阻力、同步带张力、轴承状态、驱动器参数和电机选型确认后，才能再次把开关改为 1；不得通过提高 10 N·m 上限或放宽线圈温度保护绕过故障。

**文件**。`config/sentry_2026/config_gimbal.h`、`tests/profile_compile_sentry.c`、`README.md`、`RM_DEVLOG.md`。

**验证**。`build.ps1 sentry -Current` 成功，0 error；FLASH 115884 B、RAM 72712 B、CCMRAM 54944 B，text/data/bss=115188/688/126976。`tests/run_profile_tests.ps1` 通过，sentry profile 输出 `big-yaw disabled` 且力矩边界继续 PASS；`check_comment_style.py`、`check_naming.py`、`check_macros.py`、`check_macro_dups.py`、`check_macro_golden.py`、`refactor_gate.py check-links` 与 `git diff --check` 通过。`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf` 四份 SHA-256 均为 `938D87BE06D995DCCA1E455006D4597A3BBD1FF53F2700BEDBDABCCD1C387BAE`，与 DEV-20261126-09 的失能固件一致。基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断；编译保留既有警告。未烧录、未操作实车。

**AI**:OpenAI Codex (GPT-5),2026-11-26。

### DEV-20261126-12｜2026-11-26｜功率控制对齐 304:指令电流 + 绝对值 + 二次方程 + 标定工具

**动机**。用户要求把功率控制做准。先查开源:天工大 304 `mc3545dada/PowerManager`(MIT)
的 `limiter()` 与本工程差异明确:

1. 它用**PID 输出电流(指令)**预测,本工程用 `real_current`(还带 `DJI_CURRENT_SMOOTH_COEF=0.9`
   的一阶低通,约 10ms 滞后);
2. 它对电流/转速**取绝对值**,本工程带符号代入,制动段 K3*i*w 会抵消功率;
3. 它超限时**解二次方程**求电流衰减系数(功率对电流是二次的),本工程用线性
   `limit/p_pred`,对 K4*i² 项会过度/不足限制;
4. 它直接乘**输出电流**,本工程只压 MaxOut/轮速目标,稳态不饱和时压不住。

**改动**。
- `components/motor/dji/dji_motor.h/.c`:新增 `output_scale`(默认 1.0)与
  `current_cmd`(乘 scale 前的电流指令快照);`dji_motor_control` 在电流环前记录指令、
  乘 `output_scale`。默认 1.0,云台/发射行为不变。
- `apps/infantry/chassis/chassis_power.c/.h`:重写为
  `chassis_power_scale(current_cmd[4], w_dps[4], limit_w, *p_pred)`:
  - 取绝对值;按 `S2*s^2 + S1*s + (S0-limit)=0` 取 [0,1] 较大根;
  - `CHASSIS_POWER_MODEL_SCALE/BIAS` 标定修正;
  - `CHASSIS_POWER_SCALE_LP` 一阶低通(0.6/拍),替代原来的恢复步长。
- `apps/infantry/chassis/chassis.c`:预测输入改 `motor->current_cmd`;上限减 5W 余量后
  再乘缓冲衰减 `0.5+0.5*buffer/60`(参考 304 超电 SOC 衰减);新增 `[pwri]`/`[pwr]`
  标定日志(四轮指令电流/转速、预测功率、衰减系数、上限、缓冲)。
- `apps/infantry/chassis/chassis_kinematics.c`:`chassis_limit_output()` 改为写
  `output_scale` 并按比例压低 `speed_PID.IntegralLimit`(防 windup);不再改轮速目标/MaxOut。
- `config/infantry_2026/config_chassis.h`:新增 `SCALE_LP`、`BUFFER_MIN_SCALE`、
  `BUFFER_MAX_J`、`MODEL_SCALE`、`MODEL_BIAS`;删除 `RECOVER_STEP`。
- `tools/fit_power_model.py`:改为双模式——稳态点 CSV(`i,w,p` / `i0..w3,p`,可选 `s`)
  为主路径,保留 VOFA 25 通道旧解析;单位统一到固件口径(`raw/1000`、`deg/s→rpm`)。
- `.Doc/guide/功率模型标定.md`:实测流程(真实功率来源、采点、CSV、拟合、回填、验证)。
- `.Doc/README.md`、`.Doc/spec/参数证据表.md`:索引与参数卡同步。

**验证**。ARM 0 error / 第一方 0 warning,text 123,892(+680);
四套主机测试 PASS;fullchain 700 拍金标准**已重锚**:旧模型用低通反馈电流,在 harness
里电流指令饱和到 10000 时预测不到,输出一直是 10000;新模型用指令电流后开始限幅
(10000→约 6355,平滑下降),属预期行为变化。`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI/source-list/sections 全 PASS。拟合工具用合成数据自测:
R²=1.0000,系数原样恢复。未烧录、未台架。

**风险/待实测**。
- `K0..K5` 仍是 304 车的值,本车未标定;先验误差约 ±10W。按
  `.Doc/guide/功率模型标定.md` 采 10~20 个稳态点重拟合后,目标 ±2~3W。
- 二次解假设"电流按 s 线性缩放";速度环指令电流与转速的映射不是严格线性,
  但闭环每拍重算,稳态会收敛。
- 缓冲衰减假设 `buffer_energy` 有效;若裁判未接/未收到 0x0202,`be=0` 会按 0.5 限,
  属保守行为。
- `output_scale` 乘在电流环前;本工程底盘只用速度环,等价于直接乘发送电流。
  若以后给底盘加电流环,该位置仍然正确(压的是电流给定)。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-13｜2026-11-26｜小陀螺匀速/平移速度改为 ELF 可写变量

**需求**。操作手希望用调试器(EmberProbe/DWARF)直接写小陀螺匀速转速和平移最大速度,
不重新编译。

**约束**。宏是编译期常量,ELF 里没有可写实体;必须在 RAM 里放运行期全局变量,
并让读取点每拍从内存取值(`volatile`)。

**改动**。
- `apps/infantry/chassis/chassis.c`:新增 `volatile float chassis_rotate_speed_deg_s`
  (初值 `CHASSIS_ROTATE_SPEED`);匀速档读取并夹到 `[0, CHASSIS_SPIN_HIGH_DPS]`。
  变速档正弦中心仍用 `CHASSIS_SPIN_CRUISE_DPS`;平移偏置标定的分母仍用宏 300
  (标定基准,不随变量变)。
- `apps/infantry/cmd/robot_cmd.c`:新增 `volatile float chassis_max_speed_mm_s`
  (初值 `CHASSIS_MAX_SPEED`),经 `robot_cmd_internal.h` 暴露。
- `cmd_kbm.c`/`cmd_remote.c`:平移与斜向归一化改读该变量,每拍只读一次。
- 两个变量都是非 static 全局,符号名即调试器写入名;掉电恢复宏值。

**用法**。
- `chassis_rotate_speed_deg_s`:小陀螺匀速档转速,deg/s,建议 0~400;
  超出会被夹到 `CHASSIS_SPIN_HIGH_DPS`。
- `chassis_max_speed_mm_s`:平移最大速度,mm/s;轮速仍受 `CHASSIS_MAX_MOTOR_RPM` 限幅,
  写大值不会直接越过电机上限。
- 运行中写单个对齐 float 在 Cortex-M4 上是原子的;EmberProbe 用 mcu-variables 按符号写。
- 掉电丢失;要持久化需走 flash 标定流程(当前未做)。

**验证**。ARM 0 error / 第一方 0 warning,text 123,936(+44)、data 696(+8);
四套主机测试 PASS;fullchain 金标准不变(默认值=宏);`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI/source-list/sections 全 PASS。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-14｜2026-11-26｜小陀螺匀速/变速档改为 ELF 可写(手柄标定用)

**需求**。操作手用**手柄**(遥控路径),没有键鼠的 C 键;功率标定需要把小陀螺切到
匀速档才能采稳态点。

**改动**。
- `apps/infantry/cmd/robot_cmd.c`:`static uint8_t spin_vary` 改为
  `volatile uint8_t chassis_spin_vary`(非 static,符号可被调试器写)。
  键鼠 C 沿仍调用 `cmd_spin_vary_toggle()`;`cmd_get_spin_mode()` 与
  `chassis_cmd.spin_vary` 改读该变量。
- 默认 1(变速);写 0=匀速,写 1=变速;掉电恢复默认 1。
- 手柄操作:右拨杆向下=小陀螺,右摇杆=平移,左拨杆向下=手动;
  标定前写 `chassis_spin_vary=0`,再用 `chassis_rotate_speed_deg_s` 调转速。

**验证**。ARM 0 error / 第一方 0 warning,text 123,936、data 696;
四套主机测试 PASS;fullchain 金标准不变(默认值=1);`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI/source-list/sections 全 PASS。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-15｜2026-11-26｜缓冲衰减门控:未收到 0x0202 时不生效

**现象**。实车/台架静止采集时 `[pwr]` 显示 `be=0`、`lim=42.5`;日志同时有
`[referee] waiting for robot_id`,说明裁判离线。`referee_data` 指针非空但
`buffer_energy` 恒 0,缓冲衰减 `0.5+0.5*be/60` 把 85W 上限误砍到 42.5W。

**改动**。
- `apps/infantry/chassis/chassis.c`:衰减条件从 `referee_data != 0` 改为
  `referee_data != 0 && PowerHeatTick > 0`(0x0202 帧计数);`[chcal]` 的 `be`
  在未收到 0x0202 时输出 -1,便于区分"离线"与"缓冲耗尽"。
- `tools/rtt_pwr_to_csv.py`:只在 `0 <= be < 10` 时警告,`be=-1` 不再误报。

**验证**。静止 RTT 采集 12s(26 帧):`i≈[119,-115,333,-495]`、`w=0`、
`p_pred=2.52W`、`s=1`;旧固件 `lim=42.5 be=0`。ARM 0 error / 第一方 0 warning,
text 123,956(+20);四套主机测试 PASS;fullchain 金标准重锚(上限 42.5→85,
限幅变轻:10000→9619 而非 8312→6760);`refactor_gate` ALL PASS。未烧录。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-16｜2026-11-26｜功率开环降额:目标 80W + 小陀螺峰值 350

**背景**。按用户要求采了 12 个工况的 i/w(日志 `log/`,组装 `steady_points.csv`),但裁判
离线、无真实功率读数,无法重拟合 K0..K5。用采集数据反推 304 系数物理量级:
`R≈0.134Ω`、`Ke≈0.0184V·s/rad`,量级合理;问题不在系数,在绝对余量。
用户要求"下降 15W"。

**改动**(参数级)。
- `CHASSIS_POWER_MARGIN_W` 5→10:控制目标 90−10=**80W**。
- `CHASSIS_SPIN_HIGH_DPS` 400→**350**:正弦峰值 390 被钳到 350,匀速档上限同步 350。
- 依据:地面 390 约 100W(VS 采集时空载 i 只 ~0.4A/轮);350 预计降 10~15W。

**新增/修正工具**。
- `tools/rtt_capture_point.py`:一点 20s RTT 采集 + 解析 `[pwri]/[pwr]` + 写 staging CSV,
  支持 `--no-append`(平移按 vx 分段时只存日志)。
- `tools/rtt_pwr_to_csv.py`、`rtt_capture_point.py`:CSV 表头从 `i0,w0,i1,w1...` 修正为
  `i0,i1,i2,i3,w0..w3`(原表头与数据行分组不一致,拟合会读错列)。

**验证**。ARM 0 error / 第一方 0 warning,text 123,956;四套主机测试 PASS;
fullchain 金标准重锚(上限 85→80,限幅更早);`refactor_gate` ALL PASS;
宏账本/探针同步。未烧录。

**风险/待实测**。350 会降低抗自瞄轨迹峰值;若实测功率已够,可回调 360~380。
**真值缺失**:绝对精度仍是 304 系数;有钳流表后补 3~5 个点即可拟合
`MODEL_SCALE` 或完整 K0..K5。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-17｜2026-11-26｜变速小陀螺随机化:幅度/频率每次重掷 + 旋内相位游走

**动机**。用户要求在不抬峰值、不增功率的前提下提升抗自瞄。固定 0.31/0.83Hz 双正弦
在 1~2s 窗口内仍可被拟合;脉冲方案峰值功率更高,已否决。

**改动**(参数+轨迹层,峰值不变)。
- `apps/infantry/chassis/chassis_spin.c`:
  - 每次进入随机缩放两路幅度(0.7~1.0)与频率(±15%),相位仍随机;
  - 旋内每 500ms 对两路相位做一次随机游走(0.3rad/次),瞬时频率持续抖动;
  - 相位做 `fmodf` 周期折叠,避免长时间运行精度下降。
- `config/infantry_2026/config_chassis.h`:新增
  `CHASSIS_SPIN_MOD_A_SCALE_MIN/MAX`、`F_SCALE_MIN/MAX`、`PHASE_WALK_STEP/MS`。
- 峰值仍受 `MOD_LIMIT`/`HIGH=350` 约束;相位游走只改相位,不改峰值/平均转速。

**验证**。ARM 0 error / 第一方 0 warning,text 124,812(+856)、bss 126,792(+24);
四套主机测试 PASS;fullchain 金标准不变(不跑 ROTATE);`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI/source-list/sections 全 PASS。未烧录。

**风险/待实测**。相位游走会带来小幅速度抖动,由 α/jerk 限幅吸收;若台架电流尖峰明显,
把 `PHASE_WALK_STEP` 降到 0.15 或 `PHASE_WALK_MS` 加到 800;幅度缩放下限 0.7 保证
最低调制仍在。实际抗自瞄收益需对抗验证。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-18｜2026-11-26｜小陀螺中心降到 265,正弦完整覆盖 180~350

**依据**。查 `rm_auto_aim` 的 `armor_tracker`:EKF 状态
`[xc,vxc,yc,vyc,za,vza,yaw,vyaw,r]`,过程模型是**匀速中心 + 恒定 yaw 速率**,无角加速度项;
`sigma2_q_yaw=100`,开火点按弹丸到达时刻预测。抗自瞄指标 = **到达时刻的相位误差**,
峰谷差越大越好。300 中心会把 390 削到 350,形成平顶(恒速段),且平均转速高、功率大。

**改动**(参数级)。
- `CHASSIS_SPIN_CRUISE_DPS` 300→**265**((350+180)/2);
- `CHASSIS_SPIN_MOD_A1/A2` 0.20/0.10→**0.213/0.107**;
- `CHASSIS_SPIN_MOD_LIMIT` 0.30→**0.32**。
- `265×1.32≈350`、`265×0.68≈180`:正弦全程不削顶/削底,峰谷差 140→**170**,
  平均转速降 12%(功率更低),α≈444deg/s² 与原来 469 同级,对自家云台扰动不增。

**验证**。ARM 0 error / 第一方 0 warning,text 124,812;四套主机测试 PASS;
fullchain 金标准不变(不跑 ROTATE);`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI 全 PASS。未烧录。

**风险/待实测**。中心降低使平均转速慢 12%,对方单板跟踪时间变长,靠更大峰谷差补偿;
实测看抗自瞄命中率与自家云台跟踪误差。若云台晃动增大,先把 `MOD_LIMIT` 降到 0.28。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-19｜2026-11-26｜平移左偏诊断:偏置角/平均模式可写 + off 日志

**现象**。用户反馈平移严重左漂:不开小陀螺也有少量左偏,开变速小陀螺明显变大。

**分析**。
- 不开小陀螺的左偏与 ROTATE 偏置无关,是**基线问题**:云台-底盘夹角零点(off≠0)、
  轮系/几何不对称或打滑;
- 开小陀螺变大,说明 `10°×(wz/300)` 的偏置**方向/幅度不对**(可能反而加剧左偏),
  且变速正弦/相位游走让偏置以 0.3~0.8Hz 摆动,实际打滑跟不上。

**改动**(诊断,不改默认行为)。
- `chassis_translate_bias_deg`(volatile float,默认宏值):在线改写偏置角,找符号/幅度;
- `chassis_bias_mean_mode`(volatile u8,默认 0):1=用档位平均转速(变速 265 / 匀速 300)
  算偏置,不跟随瞬时 wz,用于验证"偏置摆动"假设;
- `[chcal]` 增加 `off=`(云台-底盘夹角):云台回正时应≈0,不为 0 是基线左偏首要排查项。

**验证**。ARM 0 error / 第一方 0 warning,text 124,884、data 700;四套主机测试 PASS;
fullchain 金标准不变(不跑 ROTATE);`refactor_gate` 重锚后 ALL PASS;
naming/comment/macros/ABI 全 PASS。未烧录。

**待用户实测**。
1. 云台回正、不开小陀螺前进:看 `[chcal] off=`;若 |off|>2°,先查 yaw 对齐零位/几何;
2. 匀速 300 + 前进,`chassis_translate_bias_deg` 依次写 0 / +10 / -10 / +20 / -20,
   看哪个走直;符号错就取反,幅度不够就加大;
3. 变速档重复,再把 `chassis_bias_mean_mode` 写 1 对比:若 1 明显更直,说明是偏置摆动;
4. 若 off≈0 且不开小陀螺仍左偏,查轮系(单轮方向/减速比/几何)与地面打滑。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-20｜2026-11-26｜be=0 不再触发缓冲衰减(修 80W→40W)

**现象**。用户平移+小陀螺,推杆前进时自转明显变慢;日志
`[pwr] p=154~212W s=0.16~0.32 lim=40 be=0`。

**根因**。裁判在线(`PowerHeatTick>0`)但 `buffer_energy=0`(未进比赛/字段无效),
缓冲衰减 `0.5+0.5*be/60` 把 80W 砍到 **40W**;模型 p 150~200W → `s≈0.2`,
四轮电流压到 20%,所以自转和前进都变慢。

**改动**。`apps/infantry/chassis/chassis.c`:`be > 0` 才做缓冲衰减;
`be=0` 视为无效/未进比赛,上限保持 80W。真·缓冲耗尽由裁判自身 5s 断电保护兜底。

**验证**。ARM 0 error / 第一方 0 warning,text 124,888;四套主机测试 PASS;
fullchain 金标准不变;`refactor_gate` ALL PASS。未烧录。

**说明**。1500mm/s + 300 自转本身模型 p≈150~200W,超过 90W 规则,限幅仍会降速;
漂移测试用 **500mm/s**(限幅内,`s=1`)才能看到真实偏置效果,而不是被限幅掩盖。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-21｜2026-11-26｜平移漂移补偿增加"横向前馈"模式

**现象**。用户前进/后退漂移角不同(约 45°),旧旋转补偿在后退时方向翻转,
抵消不了固定方向的横向漂移。

**分析**。漂移是**底盘系横向速度 ∝ wz**,与平移方向无关;旋转指令补偿只对前进有效,
后退时补偿方向反了,所以前进/后退漂移角不同。

**改动**。
- `apps/infantry/chassis/chassis.c`:新增
  `chassis_translate_bias_mode`(0=旋转旧模式,1=底盘系横向前馈)与
  `chassis_translate_bias_mm_s_per_dps`(k,mm/s per deg/s);模式 1 在 `chassis_vy`
  上叠加 `k*wz`,与前进/后退方向无关。
- 旋转模式保留,默认 0 保持旧行为;`chassis_bias_mean_mode` 对两种模式都生效。

**验证**。ARM 0 error / 第一方 0 warning,text 124,988、bss 126,800;
四套主机测试 PASS;fullchain 金标准不变;`refactor_gate` ALL PASS。未烧录。

**用法**。写 `chassis_translate_bias_mode=1`,`chassis_translate_bias_mm_s_per_dps`
从 ±0.5 起试;500mm/s + 300 自转,看前进/后退是否都变直。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-22｜2026-11-26｜平移斜坡加速度改为可在线调(起步/刹车打滑)

**现象**。横向补偿后移动过程正常,但起步/刹车有轻微打滑,停车后还会滑一点。

**分析**。平移斜坡 5000mm/s²,自转占用摩擦圆,起步牵引到摩擦边界会滑;
停车是底盘+自转的动量,轮子停了车还会滑一点。横向补偿是**稳态速度前馈**,不管瞬态。

**改动**。`apps/infantry/chassis/chassis.c`:
- 新增 `chassis_vx_ramp_accel`/`chassis_vy_ramp_accel`(volatile,默认宏值,
  0/NaN 回退宏),平移斜坡改读变量;
- 其余行为不变。

**验证**。ARM 0 error / 第一方 0 warning,text 125,072、data 708;
四套主机测试 PASS;fullchain 金标准不变;`refactor_gate` ALL PASS。未烧录。

**用法**。写 3000/2000/1500 试:越小起步越软、刹车滑得越少,但响应变慢。
停车后的滑移是底盘+自转动量,斜坡只能减轻,不能完全消除。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-23｜2026-11-26｜平移斜坡升级为 S 曲线(jerk 限幅),减轻起步/刹车打滑

**背景**。横向补偿后稳态正常,起步/刹车仍有瞬态打滑。查开源:
- COD 哨兵自转时把平移压到接近 0(`_Vx=5,_Vy=5`),自转/平移基本互斥;
- rm-controls 底盘用加速度斜坡 + 功率限幅,没有 S 曲线;
- 没有"高速自转 + 大平移完全不打滑"的通用解法。

**改动**。
- `config/infantry_2026/config_chassis.h`:新增 `CHASSIS_VX_JERK`/`CHASSIS_VY_JERK`
  =20000 mm/s³。
- `apps/infantry/chassis/chassis_kinematics.c`:新增 `chassis_ramp_to_jerk()`
  (jerk 限幅 + 刹车点预测;`jerk<=0` 退回线性 `chassis_ramp_to`)。
- `apps/infantry/chassis/chassis.c`:平移斜坡改读 jerk;新增
  `chassis_vx_jerk`/`chassis_vy_jerk` 可写变量;加速度状态静态保存。

**验证**。ARM 0 error / 第一方 0 warning,text 125,368、data 716、bss 126,808;
四套主机测试 PASS;fullchain 金标准重锚(S 曲线改变斜坡轨迹);
`refactor_gate` ALL PASS。未烧录。

**用法**。jerk=20000 时加速度约 0.25s 爬到 5000;想更软降到 10000;
0=回退线性斜坡。停车后的滑移是底盘+自转动量,S 曲线只能减轻。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-24｜2026-11-26｜小陀螺模式功率只压自转,平移保持与跟随同速

**需求**。小陀螺模式下功率限幅不能压平移,只压自转;平移要和跟随一样快。

**改动**。
- `config/infantry_2026/config_chassis.h`:新增 `CHASSIS_SPIN_POWER_LP=0.1`。
- `apps/infantry/chassis/chassis.c`:
  - `CHASSIS_ROTATE` 分支:`chassis_cmd_recv.wz *= chassis_spin_power_scale`;
  - 功率块:ROTATE 时用总功率 scale 经 `CHASSIS_SPIN_POWER_LP` 慢低通更新
    `chassis_spin_power_scale`,并把 `power_scale` 置 **1**——`output_scale` 不再压
    四轮电流,平移分量与速度环电流不受影响;非 ROTATE 重置 spin scale=1;
  - `[pwr]` 增加 `ss=`(自转衰减系数);`s=` 仍为总功率 scale。
- 自转衰减 50ms 低通,避免自转随功率抖动;总功率超限时自转先降、平移保持。

**验证**。ARM 0 error / 第一方 0 warning,text 125,524、data 720、bss 126,808;
四套主机测试 PASS;fullchain 金标准不变(不跑 ROTATE);`refactor_gate` ALL PASS。
未烧录。

**行为/边界**。若平移单独就超功率,自转降到 5% 下限(`CHASSIS_POWER_SCALE_MIN`),
平移仍保持——可能超 90W 规则,由操作手控制;这是"平移优先"的明确取舍。

**AI**:DeepSeek-V4.1-Flash,2026-11-26。

### DEV-20261126-25｜2026-11-26｜[SENTRY] 移植 RM_shaobin2 本车控制、PID 与遥控映射

**目标与授权**。用户指定本地 `D:\BaiduNetdiskDownload\Nav\RM_shaobin2` 为本车哨兵代码，要求
最快移植到 GitHub `youyucat/RM-JIT-CR` 的 `sentry` 分支；保留目标工程三条构建路径和 USB CDC
虚拟串口，PID 与遥控器以本地代码为准。用户随后再次明确“本套代码是 sentry 构建”，因此
infantry/heavy 只做兼容回归，不承接本地哨兵业务行为。该请求明确授权了本次新增 Sentry 控制文件、
命令字段语义和调用链调整。

**来源事实**。实际来源仓库根为
`D:\BaiduNetdiskDownload\Nav\RM_shaobin2\RM_shaobin2\RM_shaobin`，HEAD `593d6af`，工作树非干净。
本次按用户指定以工作树为准，采用其中未提交的 `SENTRY_BIG_YAW_INTEGRAL_NM 2→3 N·m`；其他
文档、调试导出和无关改动没有复制。迁移清单见 `.Doc/detail/SENTRY_RM_shaobin2移植说明.md`。

**哨兵影响**。

1. 新增 `apps/sentry/gimbal/gimbal_control.{c,h}`，以 SI 单位实现角度→速度→力矩串级、积分限幅、
   输出滤波和速度阻尼；`gimbal.c` 按来源工程重组为大 yaw 机械正前保持/小陀螺世界角速度阻尼、
   小 yaw 相对中位积分目标、pitch 解锁位置相对 ±20°。用户确认 IMU 在大 yaw 平台。
2. 遥控映射改为右杆底盘平移、左纵杆 pitch；左下挡左横杆控制小 yaw，其余左挡左横杆控制
   底盘转向；右中挡人工自由移动，右下挡小陀螺。大 yaw 不再由左横杆直接给目标。
3. 大 yaw 正前 `-1.52914 rad`、角度 `Kp=20`、速度 `Kp=5.2/Ki=1.2`、积分 `3 N·m`、
   总输出 `10 N·m`；小 yaw ECD 27/±90°；pitch 角度 `Kp=8`、速度 `Kp=0.6/Ki=0.2`、
   输出 `1 N·m`。底盘几何改为 600×560 mm，小陀螺 150°/s。
4. 发射保持目标工程状态机，但拨轮行为与来源对齐：短拨单发、按住 250 ms 连发、松开后摩擦轮
   延时 1 s 停止。键鼠鼠标横移改为底盘转向、纵移控制 pitch，不控制小 yaw。
5. `framework/types.h` 只在现有底盘模式枚举末尾追加 `CHASSIS_RC_CONTROL`，原枚举数值和 ABI
   大小不变。新增 Sentry 串级与遥控映射主机测试；测试脚本显式使用 `-fcommon`，恢复 GCC 10+
   对 fullchain harness 已初始化覆盖符号的历史合并语义。
6. 更新 README、Sentry 移植说明、参数证据表、调参结论和 skill `code-map.md`，使现行代码地图、
   操作表与参数证据一致。

**保留边界**。三条构建均保留；`VISION_USE_VCP`、USART1 三轴 VOFA、USART6 裁判链路保持不变，
AX650 继续关闭。未修改 `.ioc`、Drivers、Middlewares，未烧录、未操作实车。USB CDC 仍未定义
Sentry 应用帧，不能直接接管控制。

**验证**。`build.ps1 sentry -Current`、`build.ps1 infantry`、`build.ps1 heavy` 均成功；
`run_chassis_tests.ps1`、`run_fwk_tests.ps1`、`run_ax650_tests.ps1`、`run_profile_tests.ps1` 全部
PASS，其中 fullchain 700 拍确定性检查通过且历史金标准未重锚。Sentry 构建 RAM 72960 B、CCMRAM
54944 B、FLASH 115460 B，text/data/bss=114772/680/127232。链接、源清单、段占用、符号合同、
命名、宏、宏金标准、ABI、注释、双构建一致性、宏重复检查与 `git diff --check` 全部通过。
`build_sentry/{basic_framework,sentry}.elf`、`firmware/{sentry,current}.elf`
四份 SHA-256 均为 `8BD680199EABEF5B4B911F951F9C111CCAE173CE1E8C97A94B5BACF50E467100`。
基线审计仍被仓库既有 skill 快照 `LICENSE` 哈希不匹配阻断，未据此宣称审计通过；
`refactor_gate.py check` 因本轮未执行升级批次的 `save-baseline` 而拒绝运行，已改用逐项闸门。

**安全提示**。此前本车已记录大 yaw 10 N·m 饱和仍带不动、DM `0xC` 和线圈过温。本次恢复
大小 yaw 输出是按来源工程迁移，不构成历史故障关闭。首烧必须架空、短时、小扰动并监测状态码和
温度；复现时先查机械/供电/驱动器，禁止继续抬力矩上限。

**AI**：OpenAI Codex（GPT-5），2026-11-26。

### DEV-20261126-26｜2026-11-26｜基于 `youyucat/RM-JIT-CR@sentry`（`a24eeb6`）合并本地 `NV v1` 导航与共享 USB CDC 自瞄

- **触发与基线**：用户指定再次同步 `https://github.com/youyucat/RM-JIT-CR/tree/sentry`（HEAD=`a24eeb6e2984f9fae4286b71ed04d11bb2e1897d`，已包含 `DEV-20261126-25` 的 `RM_shaobin2@593d6af` 完整移植：600×560 mm 底盘、小陀螺 150°/s、大 yaw 正前 `-1.52914 rad` / `Kp=5.2, Ki=1.2` / 积分 `3 N·m` / 总输出 `10 N·m`、小 yaw ECD 27 / `±90°`、pitch `±20°`、CAN2 发射、USART1 三轴 VOFA 与遥控映射）。本轮完整保留 `a24eeb6` 的所有实车控制与参数，将本地已验证的 `NV v1` 32B USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503` 叠加合入。
- **叠加改动**：
  - 恢复并合入 `components/navigation/nav_protocol.{h,c}`、`components/ax650/ax650_usb.{h,c}`、`apps/sentry/autonomy/autonomy.{h,c}`、`ports/stm32f4/usb_port.{h,c}`、`ports/freertos/kernel_freertos.c`、`Src/usbd_cdc_if.c`、`components/motor/dm/dm_motor.{h,c}`（CAN Rx ISR 记录 `rx_sample_us`）、`components/imu/ins_task.c`、`config/sentry_2026/config_{autonomy,ax650}.h`、`apps/sentry/app/{assembly.c,sentry_hooks.c,sentry_def.h}`、`tests/navigation/`、`NAVIGATION_USB.md`、`AUTOAIM_USB_PROTOCOL.md`、`COLCON_IGNORE`。
  - 最小化补丁：`framework/types.h` 在 `CHASSIS_RC_CONTROL` 后追加 `CHASSIS_NAVIGATION`；`config/sentry_2026/config.h` 增加 `AX650_USB_ENABLE && AX650_UART1_ENABLE` 互斥检查（保留 USART1 给云台 VOFA）；`apps/sentry/chassis/chassis.c` 增加 `CHASSIS_NAVIGATION` 物理系直通与导航专用斜坡；`apps/sentry/cmd/robot_cmd.c` 保留 `cmd_remote.c` 原始 `RM_shaobin2` 遥控映射，在安全仲裁前挂载 `SENTRY_AUTONOMY_ENABLE` 门控（默认 `0`，且 `autonomy_get_command` 硬清零）；`apps/sentry/gimbal/gimbal.c` 临界区原子快照 `big_yaw_rel_deg` 与 `big_yaw_sample_us`；`build.ps1` 自动将找到的 `$gccDir` 前置到 `$env:PATH`。
- **本地验证（严禁小电脑编译）**：
  - 本地 Windows ARM GCC 14.2.1（`C:\sentry_fw\build.ps1 sentry -Current`）：147/147 清洁构建与链接通过，第一方 0 error / 0 warning；`FLASH 119,184 B (11.37%)`、`RAM 74,120 B (56.55%)`、`CCMRAM 54,944 B (83.84%)`（`text=118496, data=680, bss=128392`）。
  - 产物：`firmware/sentry.elf` 与 `firmware/current.elf`（`1,384,504 B`，`SHA256=152DEC03A4308FF92533CCC1612C69F1B0FE9A0D65874DF8EBE3259DD4138B1D`）。
  - 本地 WSL 主机测试：`profile_compile_{heavy,sentry}`、`sentry_gimbal_alloc_test`、`sentry_gimbal_control_test`、`sentry_remote_mapping_test` 以及 `tests/navigation/run_tests.py` 4 组矩阵（`feedback-only`/`drive-enabled` × `-O0`/`-Ofast -flto`，含 `NV`/`0x0503`/`0x1021` 黄金字节核对）全部通过。
- **未验证边界**：本次未烧录 MCU，未做实体 USB 联调与台架验证；同时遵循 `DEV-20261126-25` 安全提示，首次烧录必须架空四轮、短时测试并监测 DM 状态码与温度。
