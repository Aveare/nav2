# RM 工程开发记录

<!-- rm-ec-dev:handoff schema=1 -->
<!-- 使用说明:本文件是 JIT-RM 1029 的实例化版本,直接复制到工程根改名 RM_DEVLOG.md 即可接手。
新工程使用时填写实际内容,移除未使用的示例条目;已有记录保留原作者与编号。
配置、接口及构建已有独立文档时直接链接,避免重复维护。
当前摘要链接对应 DEV 条目/问题 ID,完整证据在历史或详情中维护。 -->

## 0. AI 冷启动摘要

2026-11-26 最新（DEV-20261126-07）：以 `hibhvkgvck/shaobin@b489e76` 为唯一固件底座（完整保留 600×560 mm 底盘、大 yaw `-1.52914 rad` 机械零位与小陀螺纯世界角速度 PI 稳像 `Kp=5.2/Ki=1.2`、小 yaw `±90°` 中心 `27`、pitch DM、CAN2 发射），在其上移植本地 `NV v1` 32B USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`（`AX650_USB_ENABLE=1`、`AX650_UART1_ENABLE=0`、`SENTRY_AUTONOMY_ENABLE=0` 默认硬清零）。本地 Windows ARM GCC 14.2.1 编译 0 err / 0 warn（FLASH 116324B 11.09%、RAM 97352B 74.27%、CCMRAM 0B，ELF SHA256=`A2B36454BC67EF9E59262B8C8877C09233BF08D774F268AE4E5FC7B635B17C8F`），WSL 4 组导航/RMCV 协议测试、13 文件语法检查及原工程 4 套底盘/看门狗/大 yaw 主机回归全部通过；未烧录、未实体联调。

2026-11-26 最新（DEV-20261126-05）：用户取消小陀螺世界角目标追踪，改为世界角速度目标0rad/s的纯速度PI反向补偿；退出仍回机械正前方-1.52914rad。保留本轮开始源码Kp5.2/Ki1.2，旁路角度环与角度前馈。ARM/主机验证通过，实体未验证、未代烧录，固件路径build/basic_framework.elf不变。

2026-11-26 最新（DEV-20261126-04）：用户反馈小陀螺时大yaw能向目标纠偏但快速旋转跟不上。按要求首轮速度PI：Kp3.6→4.5、Ki0.6→0.8；普通回正与世界稳像共用。其余限幅/滤波保持。ARM与主机回归通过，新PI待实测，未代烧录，仍为build/basic_framework.elf。

2026-11-26 最新（DEV-20261126-03）：用户确认C板只随大yaw转，小yaw不带动C板。小陀螺进入时锁定当前IMU世界航向，退出恢复-1.52914rad机械正前方；新增chassis_cmd独立订阅与看门狗、QEKF解算计数20ms停更保护。主机与ARM验证通过，IMU方向+1及实体稳像待台架，未代烧录；原路径build/basic_framework.elf不变。

2026-11-26 最新（DEV-20261126-02）：用户已实车确认DEV-01大yaw回正通过，-1.52914rad正式作为机械正前方。右杆纵向/横向平移命令相对上一版均反向，左杆转向不变。新方向主机测试与ARM构建通过，固件仍为build/basic_framework.elf；本版未代烧录，新方向实车效果待用户验证。

2026-11-26 最新（DEV-20261126-01）：RM_shaobin2 机械正前方标定为 -1.52914rad；用户选择上电零力、遥控解锁并切入运行挡后自动回正。已修复0.5～2deg区间原软死区导致目标反向的问题，复用现有串级PID。ARM构建和主机回归通过，未烧录、未实测；下文旧目标不作为当前配置。

最新方案及标定见DEV-20261122-13/16：C板随大yaw，大yaw编码器串级PID保持用户确认的云台正前方、相对底盘固定，电池方向不作为目标；底盘独立遥控转向，不做世界角保持或追随。pitch为连杆且无几何数据，当前只闭环4310输出轴角/速度，不换算实际俯仰角。共用PID输出Nm，轻载初值待实体整定。小yaw仍未接入。下表保留上游历史，不作为本车验证结论。

发射已按参考工程接入 CAN2：摩擦轮拨码 1/2、拨盘 3，`SENTRY_SHOOT_CONTROL_ENABLE=1` 让底盘调试固件也初始化发射（此前 `CHASSIS_TUNING_ONLY=1` 下发射完全不注册）。本车拨盘阻尼显著小于参考，拨盘速度环取参考值一半、角度环由 0 改为 2.5 起标（DEV-20261122-30），未烧录、未台架。

唯一固件路径为build/basic_framework.elf，默认CMake/Ninja；大yaw CAN1发送5/反馈0，pitch CAN2发送5/反馈0。yaw同步带1:1，正前方固定标定角已写入FRONT_ALIGN_RAD=-0.763523102rad、VALID=1（DEV-20261122-18最新指定值），用户已烧录，实时核对匹配；大yaw死区0.02deg已生效。DEV-20261122-19采样存在约1.16deg静差与速度积分限幅；DEV-20261122-20已将大yaw积分额度0.1提高至0.25Nm，用户已烧录并核对匹配；DEV-20261122-21仍有1.16deg静差，积分0.25Nm再次触顶、反馈力矩约-0.39Nm，尚未解决回正；DEV-20261122-22的Ki0.3、积分额度0.45Nm已由用户烧录；DEV-20261122-23停车16.91s仍1.639deg静差，积分触顶、指令及电机反馈约-0.65Nm。继续加Ki不能提高积分触顶后的稳态输出，需核查机械阻力/起动力矩；DEV-20261122-24按用户要求单独提高速度Kp0.6→0.9，待用户烧录复测。pitch按用户要求放宽为电机输出轴相对解锁点±20deg调试窗口（DEV-20261122-14）；真实机械限位与连杆映射待资料。

哨兵上部电机编号与达妙大 yaw 型号已登记到配置，接入条件见 DEV-20261121-01。

达妙大 yaw 应用逻辑已重写，CAN 回包与闭环有独立观察量，见 DEV-20261121-03；待台架验证。

CAN 实例增加提交/硬件完成/接收计数，过滤器四槽已填全，见 DEV-20261121-04。必须烧录本次 ELF 后再读取新增字段。

达妙协议已移植自用户下载的 COD H7 参考工程并接入经典 CAN1，见 DEV-20261121-05；主机达妙回归与 ARM 构建通过，实体回包待验证。

CAN联合主机测试执行真实BSP与达妙驱动，验证ID5发送和ID0接收，见DEV-20261121-06。CAN实例现保存启动与中断通知HAL结果，反馈超时边界已统一。

<!-- AI 会话开始时只读本节即可接手;只放结论与链接,证据留在对应条目。人工接手同样建议先读这里。 -->

| 项 | 内容 |
| --- | --- |
| 基线身份 | skill rm-ec-dev assets/jit-rm-1029.manifest.json(2026-10-29 初版开发完成,主机测试 PASS,ARM 编译第一方 0 error 0 warning) |
| 有效配置 | ONE_BOARD 单板;config/robot_infantry_2026.h;任务入口 application/robot_task.c |
| 功能状态 | 底盘/云台/发射/遥控/安全机制已实车验证;裁判UI/视觉/超电未启用(预留);yaw 电流帧与空载自检固件待实测(DEV-20261111-01) |
| 最近验证 | 实车回归 PASS(2026-10-30 用户确认);主机测试 PASS;ARM 编译 0 err 0 warn |
| 当前问题 | 无(RM-001 已关闭) |
| 下一步 | USART1 的 AX650 姿态上报已完成代码与主机/ARM 验证，待用户烧录核对实体链路（DEV-20261116-01）；继续手眼标定与真实模式/弹速接入 |
| 演进待办 | flash 标定流程(Yaw 零位等持久化,代码只读语义名),实车回归通过后评估 |

本次新增：USART1 921600 8E1 / 100 Hz / 0x1021，当前 mode=0、speed=0 用于手眼标定；未烧录，见 DEV-20261116-01。

## 1. 当前状态

| 项目 | 内容 |
| --- | --- |
| 当前目标与验收 | 下一阶段:按需接裁判系统/UI 与视觉,标定单发/三发与弹速分挡 |
| 机器人与任务范围 | 2026 步兵,RoboMaster RMUC,单板整车 |
| skill | rm-ec-dev v1.1 |
| 工程源码基线 | jit-rm-1029.manifest.json(见 §0);git 为字节历史事实源 |
| 有效配置 | ONE_BOARD;VISION_USE_VCP;CAN1=轮4+YAW5;CAN2=摩擦2+拨盘1+PITCH1(拨码见 README) |
| 功能状态 | 生效:底盘/云台/发射/仲裁/看门狗/遥测;预留:裁判/视觉/超电/电流模式/ist8310 |
| 最近验证 | 见 §0 |
| 板上固件身份 | 待确认(用户烧录后回填) |
| 当前问题 | 无 |
| 最近开发条目 | DEV-20261029-01 |
| 下一步 | 见 §0 |

## 2. 设计与配置

<!-- 详细接线/几何/标定值见 README.md 与 .Doc/本车调参结论.md,此处只记变更与待确认项。 -->

- 机械拓扑:麦克纳姆轮(45°全向),四轮 CAN1;云台 yaw GM6020(CAN1)/pitch M3508(CAN2)。
- 零位:YAW_CHASSIS_ALIGN_ECD=3564 / PITCH_HORIZON_ECD=3412(拆装传动须重标)。


<!-- 后续条目按此格式追加;同一主记录编号连续递增。 -->

## 3. 问题台账

| ID | 现象与范围 | 状态 | 下一步 | 关闭条件/证据 |
| --- | --- | --- | --- | --- |
| RM-002 | 发射机构硬件问题无法正常运行(单发/三发/射速代码已实装,DEV-20261030-01) | **挂起(硬件问题)** | 硬件修复后按原计划架空标定 | 单发逐发出弹无连击/无卡弹;连发射速与堵转保护实车确认(用户回填) |

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
| 调参否决记录(详见 .Doc/本车调参结论.md) | 摩擦前馈 B=18 否决(失稳);D 项限幅 12 否决;Kd-FAR 加大否决;0x2FE 电流帧暂不启用 | 实测证据,详见踩坑记录 | 2026-10 |
| DEC-004 | pitch手感三轮实验(ref前馈/阈值低通/重力前馈)全数回滚至初始Gyro[0]反馈方案,恢复丝滑接受偶发抖;保留解锁pitch重锚定与DEV-02分析结论 | 三轮盲改无遥测数据支撑,实车均无效;初始版无粘滑跳步。后续改动必须先取telemetry波形 | 2026-10-30 |

## 4. 构建与恢复

- 工具链:arm-none-eabi-gcc 14.3.1(STM32CubeCLT bundle)+ Ninja 1.13.2 + CMake;主机测试 gcc(mingw64)。
- 构建:`mingw32-make -j24` 或 `cmake -G Ninja -B build && ninja -C build`;-Werror(Makefile)。
- 烧录:tasks.json `download_dap`/`download_jlink`;日志:`JlinkRTTClient`。
- 恢复顺序:本记录 §0 → 问题台账 → DEV 条目 → 源码/config → 重跑审计与测试 → 板上固件核验。

## 5. 资料与设计依据

- 架构:README.md + .Doc/架构与开发指南.md(§1.5 为相对上游框架的架构改造清单);调参:.Doc/本车调参结论.md + .Doc/PID参数整定.md;纪律:.Doc/必须做&禁止做.md。
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

### DEV-20261116-01｜2026-11-16｜USART1 接入 AX650 的 0x1021 姿态上报

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

### DEV-20261117-01｜2026-11-17｜RM_shaobin 底盘电机调试

- 从本地 `RM-JIT-CR-clone` 的 `7ca9ea0` 导出。用同一提交中的文件核对移植差异。
- `CHASSIS_TUNING_ONLY=1` 时启用 CAN1 四轮的驱动权限，保留遥控急停、失联保护和断流看门狗。右中挡为车体坐标系平移，右下挡保留小陀螺。跳过云台、发射、INS 和裁判 UI 初始化，沿用原四轮速度环和运动学。用户给定的新接线以电池为中心，左上=1、右上=2、左下=3、右下=4，已更新电机注册顺序和 README。
- CMake/Ninja 的 STM32F407IG ARM 编译和主机仲裁测试均通过。整车配置也通过 ARM 编译与主机仲裁测试。两种配置均有工具链的 newlib 系统调用存根链接告警。
- 实车测试确认手柄控制正常：右杆前后与左右方向按新车修正后正确，左杆横向连续转向正常，小陀螺可用。用户反馈“没有任何问题”。
- 新机械几何参数：用户确认新底盘前后与左右外沿跨度为600mm，原车外沿跨度为500mm。沿用原工程测量方法，轴距取车长方向前后轮外沿跨度600mm；轮距取横向外沿跨度600mm减麦轮宽度40mm，配置为560mm。
- 用 ELF 符号及数据段确认底盘固件含四轮控制入口，CAN1 电机总闸初值为 `1`。代码、ARM 编译、主机仲裁测试和当前手柄实车测试均完成。
- **AI**：Codex（GPT-6）。

### DEV-20261119-01｜2026-11-19｜双 yaw 与雷达朝向接口

- 新增 `application/gimbal/sentry_yaw.c/.h`，Makefile 登记源文件，CMake 自动收集。模块在底盘电池方向为 +X 的约定下输出两轴机械角度参考，并独立按实测姿态和安装偏角计算带时间戳的雷达地图朝向。
- 电机反馈归一化、零位、限位及输出由后续硬件接入层负责。地图北向定义为建图起始时的底盘 +X，输入样本需与点云使用同一时基。完整建图还需位移、三维姿态和安装外参。
- CMake/Ninja ARM 构建通过，新增源文件已编译，仅见既有 newlib 系统调用链接告警。尚未接入运行任务，未执行主机测试或硬件验证。固定基线审计脚本路径在本机缺失，审计未执行。
- **AI**：Codex（GPT-6.2-sol）。

### DEV-20261121-04｜达妙首帧缺失的 CAN 诊断

- 用户实测 `rx_init_flag=0`、`rx_refresh_dt` 递增。只读独立复核确认原达妙任务仍向 CAN1 `0x005` 发送使能帧与 MIT 帧，零力逻辑不会阻止发送。当前源码未发现可确定解释首帧缺失的调用链断点，过滤器空槽也不能直接定为本次根因。
- `CANAddFilter` 的 IDLIST 四槽均设置为实例接收 ID，记录 `filter_status` 并对 HAL 配置失败打印错误。CAN 接收增加标准数据帧和 DLC 检查。
- `CANInstance` 增加 `tx_submit_count`（HAL 接受）、`tx_complete_count`（TXOK 中断确认）、`tx_error_count`、`tx_drop_count`、`rx_count`。每个硬件邮箱保存实例归属；独立审查指出硬件在 HAL 选择邮箱前可能释放旧邮箱，因此发送改为每条总线一帧在途，由完成中断立即续发队列。失败/撤销后的归属可清理，避免把提交当作发送成功。此策略减少硬件并发邮箱数，实际发送延迟/队列丢弃需台架确认。ACK 可来自任何节点，完成计数不能证明目标电机收到。
- 两次 EmberProbe Bridge 读取均返回 `OPENOCD_START_FAILED`，工具检测到 CMSIS-DAP 探针但没有建立目标采样连接。未取得本轮硬件数据，未烧录。原基线审计路径仍缺失，克隆基线审计自身哈希不符，无法出具可靠审计计数。
- `cmake --build build -j 8` 通过，四项已有 newlib 存根告警。新增字段改变 CAN 实例布局，烧录后必须绑定同一份 `build/basic_framework.elf`；电机回包、出力和 CAN 完成计数待板上验证。
- **AI**：Codex（GPT-6.1-sol）。

### DEV-20261121-05｜COD H7 达妙协议移植到 C 板

- 源文件为工作区 `cod-h7-template-main.zip` 的 `Components/Device/Src/Motor.c`、`Inc/Motor.h` 和 `Application/Task/Src/CAN_Task.c`。协议保留原作者 GrassFan Wang 署名。`DM_Motor_Command`、`DM_Motor_CAN_TxMessage`、`DM_Motor_Info_Update` 已原位接入 `modules/motor/DMmotor/dmmotor.c`，公共 DMMotor 调用接口与普通 PID 路径保留。发送统一通过经典 CAN 的 `CANTransmit`，控制 ID5、反馈 ID0 与量程 12.5/30/10 对应现有配置。
- 特殊命令修正为控制 ID，接收先验证 DLC8 和内嵌 ID，解析后保留 `position_raw/velocity_raw/torque_raw`。发送前限幅并处理非有限值；MIT 停止、无反馈、陈旧反馈与故障态均发零 Kp/Kd/力矩。
- 参考工程的30ms使能等待改为每实例非阻塞计数，另加100ms节流重试，覆盖首次无反馈、失能及陈旧 ENABLE 状态。两者按1kHz任务调用换算标称时间，任务延迟会延长实际等待。未移入原参考的越界数组、重复实例发送和 FD/BRS 配置。
- 新增 `tests/dmmotor_port_test.c`，模拟 CAN 并执行实际初始化与任务入口，命令帧、MIT零点与端点打包、原始反馈、错误ID/DLC拒绝、启动等待、停止、超时恢复、过流状态保持、限幅与NaN处理全部通过。MinGW gcc 使用 `-std=gnu11 -Wall -Wextra -Werror`，include 为 `tests/native_stubs,bsp/can,modules/motor,modules/motor/DMmotor,modules/algorithm,config`。
- `cmake --build build -j 8` 通过，已有四项newlib存根告警。ELF含三个移植协议函数。独立只读审查通过，实体回包与出力未验证，未烧录。固定基线审计入口缺失的限制沿用上一条。
- 原底盘测试脚本在 `chassis_remote_test.c:167` 失败，断言仍期待右纵杆+660产生正vx，当前 `robot_cmd.c:343` 已采用用户先前要求的反向映射。该脚本不编译达妙驱动，本轮记录此既有测试与当前映射不一致；命令看门狗测试单独编译执行全部通过。
- **AI**：Codex（GPT-6.1-sol）。

### DEV-20261121-01｜2026-11-21｜哨兵上部电机编号

- 用户提供摩擦轮编号 1/2、拨弹 3、大 yaw 4、小 yaw 5、pitch 6。大 yaw 型号为达妙 DM_J4310-2EC V1.2。已集中登记到 config/robot_infantry_2026.h 的 SENTRY_* 配置。
- 用户补充其余五台均为 DJI 型号，已将配置名标明为 DJI 电调 ID；具体电机型号尚未提供。
- 用户确认官方常见型号组合适用于本车：摩擦轮 Snail 2305+C615、拨弹 M2006 P36+C610、小 yaw 与 pitch 为 GM6020。配置已补充型号标注。当前步兵发射模块的摩擦轮仍按 M3508/C620 实例化，哨兵接入时需使用匹配的 ESC 控制接口。
- DJI 官方标准机器人手册说明 Snail 2305+C615 用于摩擦轮、M2006 P36+C610 用于供弹、GM6020 用于云台；来源链接 https://rm-static.djicdn.com/tem/17348/RoboMasterAI%20Robot%202020%20Standard%20Version%20User%20Manual%20v2.pdf 。该手册为标准机器人方案参考。
- 用户要求沿用工程预留 CAN 接口，并确认达妙大 yaw 设备 ID 为 4。按原工程接口记录 CAN1 大 yaw、小 yaw(ID5)，CAN2 摩擦轮(ID1/2)、拨弹(ID3)、pitch(ID6)。此分配是按接口迁移的代码侧映射，接线实物仍需对应核验。
- 用户确认达妙反馈 Master ID 使用出厂设置 0；已与控制 CAN ID 4 分开登记于 config/robot_infantry_2026.h。
- 新增哨兵大 yaw CAN 通信模块并接入 RobotInit。按 CAN1 控制 ID 4、反馈 Master ID 0 注册通道，缓存最近的 8 字节反馈和帧数，提供显式 8 字节发送接口。初始化只注册通道，不发使能或控制帧。发送接口尚无控制调用方，反馈暂未按模式和映射范围解码。
- `cmake -G Ninja -B build` 刷新 GLOB 后 ARM 构建通过，第一方无编译告警；保留 newlib 系统调用存根链接告警。尚未上板验证 CAN 收发。
- 修正 2026-11-21 用户按最新图纸确认的 yaw 电机归属：大 yaw 为 DJI GM6020 ID4，小 yaw 为达妙 DM-J4310-2EC V1.2 ID5、Master ID0。原先临时建立的达妙 CAN 通信模块已改名并调整为小 yaw 通道；先前 DEV 条目中称达妙为大 yaw 的描述由本修正覆盖。反向补偿算法仍针对大 yaw，尚未接入 DJI GM6020 实例。
- 大 yaw 开始闭环：底盘调试模式下启用 INS 与现有 GM6020 yaw 双环，yaw 注册 CAN1 ID4；收到有效云台控制模式时锁存当前 IMU 世界 yaw 作为保持目标，底盘旋转时由世界角闭环反向补偿。pitch 保持停止，发射仍未初始化。ARM 编译通过，第一方无告警，newlib 存根链接告警保留。尚未烧录/实车验证。
- 启动朝向说明：当前锁存的是使能瞬间的实际 IMU 朝向，不会自动搜索或判断电池方向。实际测试前需将大 yaw 机械摆到正对电池再解锁；电池方向标定角仍未实测。
- yaw 参考计算当前仅处理大 yaw 目标，随底盘相对建图起始朝向的变化量反向补偿。小 yaw 暂不纳入控制参考计算；雷达朝向数据结构留作后续建图接口。目标角尚未接入 IMU/达妙实例/周期任务，须待反馈和达妙控制参数明确后完成闭环。
- 用户要求上电时大 yaw 朝向正对电池。新增基准初始化接口：待 IMU 航向与大 yaw 反馈有效后记录底盘初始航向，并使用配置中的电池方向标定角作为启动目标；运行期按底盘累计转角反向更新大 yaw 目标。默认标定角为 0 度，仅在电机零位定义确实以电池方向为零时适用。此接口尚未接入启动流程或电机闭环。
- 本轮仅登记硬件接入参数。原步兵 GimbalInit/ShootInit 尚未引用此组，旧 pitch 初始化仍为 ID4。当前 CHASSIS_TUNING_ONLY=1，上部电机未注册。
- 未确认 CAN1/CAN2 归属、其余电机型号、摩擦轮左右对应、大 yaw 的 Master ID、控制模式及 P_MAX/V_MAX/T_MAX。现有达妙驱动为全局固定映射范围，且初始化会发送使能指令，接入前需核对并处理。
- 固定基线审计脚本在本机缺失。尚未验证实体通讯。
- CMake/Ninja ARM 构建通过，退出码 0；仅见既有 newlib 四项系统调用存根链接告警。日志为 build/sentry_ids_build.log。
- **AI**：Codex（GPT-6.1-sol）。

### DEV-20261121-02｜2026-11-21｜达妙大 yaw 闭环接入

- 按用户最新确认，大 yaw 为达妙、CAN1 控制 ID 5、反馈 Master ID 0；pitch 也为达妙、控制 ID 6，反馈 ID 配置为 1。小 yaw 暂不接入；当前预留 ID 仍为 5，后续启用前须调整为不冲突的 ID。
- 底盘调试模式的大 yaw 已从 DJI 实例切换为工程现有达妙 MIT 驱动。有效云台控制模式下锁存当前底盘航向，使用 IMU 航向变化计算 yaw 反向补偿，并叠加遥控/键鼠 yaw 目标增量；未标定时以首次使能时的电机位置为初始基准。实测电池对准角仍未写入。
- 达妙位置/速度/力矩映射按 DM 调试工具截图设置为 ±12.5 rad、±30 rad/s、±10 N·m。MIT 帧 KP 暂设 54、KD 暂设 0.5；调试工具显示的普通位置环 KP 不等同于 MIT 帧 KP，两项均需低速架空实测整定。官方 DM-J4310-2EC V1.1 说明书与工程 DMMotor 实现中的 MIT 8 字节字段顺序、MIT 帧 ID 规则一致；V1.2 的实际固件仍需联机核验。
- 新增 EmberProbe 观察量 `sentry_big_yaw_position_rad` 与 `sentry_big_yaw_target_rad`，单位 rad。ARM ELF 中已确认两者均保留为全局变量。
- `cmake --build build -j 8` 通过；保留既有 newlib `_close/_lseek/_read/_write` 存根链接告警。尚未烧录、实车联调或验证电机方向与 MIT 参数。
- **AI**：Codex（GPT-6）。

### DEV-20261121-03｜达妙大 yaw 反馈诊断与控制逻辑

- 比对同一远端的 `RM-JIT-CR-clone`（`bb9e78c`）。当前 `dmmotor.c` 的 SHA-256 与原件一致，头文件差异为 4310 上位机配置对应的六个量程常量。继续使用原驱动的 `MITMotorInit`、`DMMotorTask` 和接收回调，CAN1 控制 ID `0x05`，反馈 ID `0x00`。
- 应用任务直接导出 CAN 反馈角（rad）、反馈在线标志、驱动状态与闭环活动标志。收到近期反馈且驱动处于使能状态，并且控制命令有效时，才建立反向补偿参考并输出；退出闭环后清除该参考。底盘调试模式的遥控左横杆只指挥底盘旋转，键鼠的 yaw 增量通过原有命令链传入。
- `cmake --build build -j 8` 通过，仅见已有的四项 newlib 链接告警。`arm-none-eabi-nm` 确认 ELF 内含原达妙驱动和五个大 yaw 调试符号。原固定基线审计路径不存在，克隆工程的替代审计因自身基线 `LICENSE` 哈希不符退出。未烧录，电机是否回包、是否出力及反向补偿方向仍需台架验证。
- **AI**：Codex（GPT-6.2-sol）。

### DEV-20261121-06｜CAN 软件收发全链路复核

- 发现 CANServiceInit 忽略 HAL_CAN_Start 和通知开启的结果。现保存每总线 start_status/notification_status，复制到 CANInstance 并对失败打印日志。注册实例仍不等于启动成功，板上应同时核对这两项和 filter_status。
- 发现 Gimbal 在线门限采用 <=50，而驱动出力门限采用 <50，边界一周期内显示与出力状态不一致。现统一为 DM_ENABLE_RETRY_MS / DM_TASK_CYCLE 且使用严格小于。
- 新增 tests/dmmotor_can_path_test.c 和 tests/can_path_stubs。测试编译生产 bsp_can.c 和 dmmotor.c，仅替换HAL边界。正常用例覆盖FC经软件队列提交到标准ID5、ID0/FIFO1分发到真实解析函数、错误报文拒绝、停止仍发送、ID0与0x201四槽过滤、三邮箱回调、跨实例完成归属、提交错误重试、队列满丢弃及临界区恢复。独立进程注入Start/Notice/Filter失败，验证错误值保存。两组均PASS。
- 构建测试命令：MinGW gcc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Itests/can_path_stubs -Itests/native_stubs -Ibsp/can -Imodules/motor -Imodules/motor/DMmotor -Imodules/algorithm -Iconfig tests/dmmotor_can_path_test.c -lm -o tests/build/dmmotor_can_path_test.exe。运行可执行文件及追加参数fault各一次。
- 达妙协议回归再次PASS。cmake --build build -j 8通过，四项已有newlib告警。ARM RAM89832字节、FLASH100944字节。固定基线审计入口缺失的限制沿用此前记录。
- 验证范围是HAL操作符合模拟条件时的生产软件链路。实体HAL寄存器、NVIC运行时状态、实际固件与ELF一致性及总线收发尚未验证，因此未判断硬件故障。CAN2路径未运行联合测试。发送错误/撤销且无后续生产者调用时队列不主动恢复，当前周期电机任务依靠下一次发送恢复；初始化失败记录结果但未实现自动重启CAN。未烧录。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-01｜OpenOCD板上只读证据

- EmberProbe变量读取成功，source=active-sampling。当前ELF SHA256为a37b3fa720e39c1c0b2e3f975f6ef8404f44f201d8a644e32d6a83b4e8eef758，扩展记录firmwareConsistency=match，最近烧录记录为2026-11-22T04:01:10.498Z。
- 达妙实例CAN1 tx_id=5、rx_id=0、working_type=MIT_MODE；start_status、notification_status、filter_status、tx_error_count、hcan1.ErrorCode全部0。连续读取tx_complete_count从65567增加到73486，再到83595；rx_count和rx_init_flag持续0。运行中多字段读取非原子，不以同次读取的submit与complete大小关系推断错误。
- CAN1.ESR=0（TEC/REC/BOFF均0）、IER=0x13（TX和RX0/RX1通知开启）、FA1R=0x1F（bank0..4开启）、FFA1R=0x15（bank0/2/4到FIFO1）、FMR.FINIT=0、CAN2SB=14。F0R1/F0R2均0，FM1R=0x1F、FS1R=0，确认bank0为16bit列表接收标准ID0。BTR=0x00290002，正常模式且非静默/环回。
- 右前/右后实例rx_count分别读到167750/167660，证明同属FIFO1的底盘反馈正在被分发。达妙发送缓冲为7F FF 7F F0 00 00 07 FF，对应零Kp/Kd/前馈力矩的MIT帧。
- 当前证据证明对应固件启动了CAN、ID5帧有硬件发送完成、FIFO1收发中断/底盘分发在工作、接收ID0的硬件过滤正确。尚无ID0反馈到达达妙实例，不能据此断言目标达妙已收到命令，因为CAN ACK可由其他节点产生；其他反馈ID、实际电机配置与电机分支连接仍未验证。本轮仅只读及记录日志，未重编译、烧录或写寄存器。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-02｜遥控与大yaw反馈分离定位

- 用户确认底盘能动，仅大yaw不动。板上只读结果rc_frame_received=1、daemon.temp_count=9、robot_state=ROBOT_READY、authority_source=AUTH_RUN、input_source=INPUT_RC、g_dji_can1_enable=1。大yaw收到GIMBAL_GYRO_MODE，反馈online=0、driver_state=255、rx_init_flag=0、rx_count=0、stop_flag=MOTOR_STOP、control_active=0；达妙硬件发送完成数继续增到249603。确认遥控链路工作，大yaw因无反馈保持零力。
- 新增默认关闭的CAN1_DIAGNOSTIC_CAPTURE_ENABLE诊断开关，诊断变体把CAN1 bank0设为32bit掩码模式，接收全部标准数据帧。回调在原业务分发前记录16个ID的count/最后数据/长度至can1_rx_capture，未注册反馈ID也可观察。只增接收记录，出力门控沿用现有路径。超过16个ID时增加can1_rx_capture_overflow。
- 单独构建命令cmake -S . -B build/can_capture -G Ninja -DCMAKE_C_FLAGS=-DCAN1_DIAGNOSTIC_CAPTURE_ENABLE=1，以及cmake --build build/can_capture -j 8均通过。ELF包含can1_rx_capture符号。build/basic_framework.elf未覆盖，仍对应板上原固件。诊断固件为build/can_capture/basic_framework.elf。
- 主机联合测试分别在开关0和1下运行正常/初始化失败注入，共4组PASS。诊断模式还检查掩码IDE/RTR位、未知ID1的记录和ID0反馈的记录。实际抓包内容待用户烧录诊断ELF后读取，未执行烧录或目标写入。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-03｜诊断固件全ID接收实测

- 用户烧录can1_capture.elf成功，Bridge确认SHA256 b483c7f133efccbb522e1bec92c919b73214511548c2f2133e9ae06106aed789匹配，烧录记录2026-11-22T04:18:35.794Z。
- 连续两次读取can1_rx_capture，仅有0x201/0x202/0x203/0x204四种ID，后次累计分别82445/82340/82304/82344，均为8字节DJI反馈；其余槽count=0，overflow=0。达妙实例rx_init_flag/rx_count仍0，tx_complete_count由29134增加到41017。
- 寄存器F0R1=0、F0R2=6、FM1R=0x1E、FS1R=1，确认bank0实际为32bit掩码并接受所有标准数据帧。RF0R/RF1R/ESR均0，采样时无FIFO溢出或总线错误迹象。
- 当前抓包窗口没有发现其他标准反馈ID，因此没有证据支持“仅因反馈ID设置成别的标准ID而被过滤”的解释。正常CAN模式不自收自身ID5。尚不能区分电机实际配置不匹配、供电或支路连接问题；未确认电机本体是否收到ID5，也不能仅凭这些值认定器件损坏。已向用户询问本体指示灯状态。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-04｜CAN2 达妙 pitch 调试接入

- 用户最新确认pitch电机为DM-J4310-2EC V1.2，接CAN2，控制ID5，反馈ID0x01（反馈ID仍待回包实测）。用户不确定MIT上位机设置，编码量程暂按现有同型号12.5rad/30rad/s/10Nm，实际设置尚未读取核验。
- gimbal.c新增独立MIT pitch实例与SentryPitchTask，复用已有达妙驱动。首次有效反馈且解锁后锁存当前实测角，左纵杆/鼠标Y产生pitch增量。默认调试Kp5/Kd0.3、方向+1、相对行程±5°、目标限速10°/s均为待实测配置。急停、命令断流、反馈超时或故障时零力并清锚，下次解锁重新捕获起点。
- 原有大yaw宏不再独占Gimbal与INS接入条件，pitch可独立开启。输入端pitch范围和速度同步到调试窗口，解锁时清命令增量，避免长推后反向需要消耗窗口外累积命令。原整车模式继续使用原pitch命令范围和速度。
- 单独固件构建：cmake -S . -B build/pitch_can2 -G Ninja -DCMAKE_C_FLAGS=-DSENTRY_BIG_YAW_CONTROL_ENABLE=0；cmake --build build/pitch_can2 -j 8。两条通过，构建产物另存为build/pitch_can2/pitch_can2.elf。该固件启用pitch与原底盘调试，未注册大yaw。ELF符号核对pitch变量齐全且无sentry_big_yaw_motor。已有newlib四项存根链接告警，未烧录。
- tests/dmmotor_can_path_test.c增加CAN2 ID5/反馈1，真实BSP和达妙驱动联合测试验证与CAN1 ID5按总线隔离、bank14过滤、反馈解析、零力发送及允许出力，普通/抓包变体及初始化失败注入均PASS。
- 从当前gimbal.c提取SentryPitchTask实际函数体至tests/build/sentry_pitch_task_test.c并以MinGW gcc -Wall -Wextra -Werror编译，首次反馈、锚定、限速、行程、反向、急停、重新锚定、超时、故障、NaN测试PASS。该测试验证控制逻辑，不含实体HAL/重力负载；电机模式、量程、转向及机械行程均待实测。固定基线审计路径缺失的限制沿用此前。
- 后续独立审查发现通信恢复时非零命令可能产生新的命令窗口偏移，已增加sentry_pitch_rearm_required。控制中反馈断流/故障后锁存零力，须先急停、回中再解锁；不会直接按中断期间累积命令自动恢复。主机测试补充断流后即使反馈恢复也保持零力，以及经过安全态后重新解锁成功，两项PASS；最终ARM重建通过。
- 新监视量：sentry_pitch_position_raw、sentry_pitch_position_rad、sentry_pitch_target_rad、sentry_pitch_feedback_online、sentry_pitch_driver_state、sentry_pitch_control_active、sentry_pitch_rearm_required。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-05｜pitch 红灯失能的代码与在线状态复核

- 用户报告 pitch 红灯、driver_state=0。只读 Bridge 实际读取选中的 build/pitch_can2/basic_framework.elf，SHA256 990d6a982309f7ade760a0e14ec1c04811f68655a6ebcd27dcaea7739480ac46，与烧录记录匹配；当前 driver_state=255、feedback_online=0、rx_init_flag=0、rx_count=0，尚不能将 measure.state 默认0解释为已收到失能反馈。
- 发送实例 tx_id=5、rx_id=1，start/filter/notification 状态均 HAL_OK。tx_submit/tx_complete 连续增长（运行中多字段读取不原子，不能比较同批计数大小），tx_error/drop=0，hcan2.ErrorCode=0。CAN2 ESR=0、BTR=0x00290002、IER=0x13。ACK/完成不能证明目标达妙电机收到，其他总线节点也可 ACK。
- 源码核对 MITMotorInit 发送 FF FF FF FF FF FF FF FC，DMAutoRecoverComm 在首帧缺失或失能时标称100ms重试。DMMotorStop 只设置软件停止标志，MIT输出零增益，不发送 FD；工程未发现 DM_CMD_RESET_MODE 调用。零力发送不阻止FC重试。
- 重编并运行现有 dmmotor_port_test 和真实 BSP+驱动的 dmmotor_can_path_test，均PASS，覆盖FC发送ID5、CAN2反馈ID1分发、MIT零力/输出与总线隔离。无固件源码修改，无重建目标ELF、烧录或执行器写入。当前证据无法确定实体根因，pitch本体实际Master ID/模式及控制帧到达仍需核验。基线审计固定路径缺失的既有限制沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-06｜大 yaw 反馈 ID 改为 0x005

- 用户明确要求大 yaw 反馈也使用0x005。config/robot_infantry_2026.h 的 SENTRY_BIG_YAW_DM_MASTER_ID 从0改为0x005；GimbalInit已有rx_id引用该宏，CAN过滤及软件分发随实例配置自动更新。CAN1控制ID仍为5，电机本体Master ID须由用户同步写为5。
- 独立构建cmake -S . -B build/yaw_feedback5 -G Ninja与cmake --build build/yaw_feedback5 -j 8通过。输出另存build/yaw_feedback5/yaw_feedback5.elf，符号确认大yaw与pitch均启用，底盘仍按当前默认配置。CAN1大yaw收发5，CAN2 pitch收1发5；总线隔离，无新增CAN1注册ID冲突，小yaw接口保持禁用。
- ARM构建0错误，保留四项既有newlib系统调用存根链接告警。该次为单一配置变更，未新增或运行主机测试，未烧录、未改变板上状态，实体反馈待用户验证。原基线审计脚本在本机不存在，无法提供基线审计计数。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-07｜pitch 反馈 ID 同步为 0x005 与总线清单

- 用户要求pitch也配置发送/反馈均0x005。SENTRY_PITCH_DM_MASTER_ID从1改为0x005；CAN2发送仍5，大yaw CAN1收发均5。两实例总线不同，BSP按总线和rx_id共同分发。
- cmake --build build/yaw_feedback5 -j 8成功，既有四项newlib链接告警保留。当前双轴固件为build/yaw_feedback5/yaw_pitch_feedback5.elf，旧yaw_feedback5.elf别名同步为本次产物。未烧录，用户须将两电机各自Master ID设为5并写入。
- 核对RobotInit、ChassisInit、GimbalInit和DJI分组：当前CHASSIS_TUNING_ONLY=1，CAN1四轮M3508编号1/2/3/4，共用控制0x200，反馈0x201/202/203/204；CAN1大yaw DM4310控制5/反馈5；CAN2 pitch DM4310控制5/反馈5。ShootInit未执行，小yaw通信禁用，板间通信及超电分支在当前底盘调试模式未启用。预留摩擦/拨弹及小yaw不能列为当前已注册电机。
- 参数变更未新增或运行主机测试；未进行实体回包验证。基线审计入口缺失沿用既有限制。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-08｜按 pitch 上位机截图使用 Master ID 0

- 用户提供pitch参数界面并要求按图调整：CAN ID5、Master ID0、1M、MIT、P/V/T量程12.5/30/10。将SENTRY_PITCH_DM_MASTER_ID从5改为0，CAN2控制ID仍5。大yaw CAN1收发5配置沿用。截图为当前界面值，电机参数写入及读回由用户确认。
- cmake --build build/yaw_feedback5 -j 8通过，四项既有newlib链接告警；最新另存build/yaw_feedback5/pitch_feedback0.elf，包含大yaw与pitch。旧yaw_pitch_feedback5.elf属于此前反馈5版本，不能作为本次固件使用。未烧录，未进行目标写入、实体反馈验证或新增主机测试。固定基线审计入口缺失的限制沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-09｜绿灯但遥控无效：实际烧录旧 pitch 固件

- 用户报告绿灯、截图pitch状态255/在线0。Bridge只读确认已选/已烧录build/pitch_can2/basic_framework.elf SHA256 990d6a982309f7ade760a0e14ec1c04811f68655a6ebcd27dcaea7739480ac46，firmwareConsistency=match，烧录记录2026-11-22T05:59:54.873Z；实际pitch rx_id=1、rx_count=0、rx_len=0、rx_init_flag=0、driver_state=255、control_active=0。
- 上位机截图pitch Master ID0，与板上旧监听1不匹配，能解释反馈缺失及闭环保护拦截遥控；绿灯只说明使能进展，不能证明反馈已送达控制程序。此前另目录的pitch_feedback0.elf并未被本次烧录记录采用。
- 为消除目录混淆，重建用户当前选中的build/pitch_can2/basic_framework.elf，并同步pitch_can2.elf别名。新固件pitch CAN2控制5/反馈0、大yaw不注册，底盘按原调试配置。ARM构建成功，保留四项既有newlib链接告警。未烧录、未解除反馈保护、未写目标RAM。重建后磁盘ELF已不同于板上旧版，须用户重烧后再读取变量。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-10｜统一唯一默认固件路径与编译入口

- trace_id=RM-ONE-ELF-20261122；用户要求工程内只保留自己编译的一个默认路径。唯一固件路径确定为build/basic_framework.elf，参数仍为大yaw CAN1发送5/反馈5、pitch CAN2发送5/反馈0，底盘四轮保留。
- .vscode/tasks.json统一默认CMake/Ninja：configure task固定工程源目录与build目录，build task依赖配置成功；DAP/JLink下载任务依赖编译成功，替换原分号连接（编译失败仍可能继续烧旧产物）。不再由默认任务调用本机PATH不存在的mingw32-make。下载命令只配置，未执行。
- .vscode/settings.json固定cmake.buildDirectory与已验证的CubeCLT cmake.exe路径，移除CMAKE_COMMAND=cube-cmake覆盖；F5的既有launch.json本来就指向根build ELF，无需更改。AGENTS.md登记唯一默认路径与后端，禁止另建固件变体目录和别名ELF。
- Bridge config.set将EmberProbe选中ELF同步为build/basic_framework.elf，随后config.get读回一致。原选中pitch_can2路径不再使用。没有编程Flash或执行器写入；磁盘新ELF不同于此前板上pitch专用固件，须用户重烧后观察。
- 自动审批拒绝递归删除三个旧变体目录（blocked by policy）。采用保留旧文件的可恢复归档方式：先校验源绝对路径均直接位于工程build内、非reparse point，且归档目标为明确同级目录，再Move-Item将can_capture/pitch_can2/yaw_feedback5移到C:/Users/10821/Desktop/diankon/RM_shaobin-build-archive-20261122。当前工程递归仅剩build/basic_framework.elf。
- cmake -S . -B build -G Ninja、cmake --build build --parallel 8完成135项全编，0错误，四项既有newlib链接告警。静态配置检查PASS（任务依赖、固定路径、项目仅一ELF）；nm确认大yaw与pitch均启用，CMake flags为空，无pitch专用关闭yaw覆盖。独立只读审查指出两种构建后端交替写同一ELF会破坏增量可信度，已通过唯一默认CMake后端解决。原Makefile作为历史源码保留，默认入口不再调用。固定基线审计脚本仍缺失，无法提供审计计数。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-11｜大 yaw 按截图恢复 Master ID 0

- 用户最新大yaw截图明确CAN ID5、Master ID0、CAN Baud1M、MIT模式、P/V/T量程12.5/30/10。SENTRY_BIG_YAW_DM_MASTER_ID从5改为0，CAN1控制发送仍为5；pitch CAN2发送5/反馈0。两总线独立，同号反馈按CAN句柄隔离。
- 仅使用唯一默认目录执行cmake --build build --parallel 8，通过，四项既有newlib链接告警。产物build/basic_framework.elf，无另名复制或新增变体。未烧录、未执行实体测试；源码配置不证明上位机已写入，实体反馈仍需用户烧录后验证。固定基线审计入口缺失沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-12｜参考 Sentinel 的串级云台/底盘跟随草稿，用户撤回 yaw 前提待澄清

- trace_id=RM-SENTRY-CASCADE-20261122。参考工程为RM-JIT-CR-Sentinel/RM-JIT-CR-Infantry。yaw GM6020用世界角deg→速度rad/s→DJI码；pitch M3508用IMU Pitch/GyroX及旧19:1和标定重力电流。原数值不能直接用作4310 MIT增益或Nm。
- 用户第一轮回答IMU随大yaw且需要底盘跟随，基于此形成草稿：新增sentry_control.c/.h共用现有PID库，角rad→速rad/s→Nm；MIT Kp/Kd为0仅发力矩。yaw用IMU世界角/GyroZ，pitch暂用电机输出编码器连续角/速。轻载初值yaw角Kp12、速度Kp0.6/Ki0.1、扭矩0.8Nm，pitch角Kp4、速度Kp0.6/Ki0.2、扭矩1Nm；均待实测，不是由旧DJI码线性换算或实体整定结论。
- 草稿CMD接入左横yaw、右中FOLLOW、右下自转，底盘读取有效yaw相对夹角，线性P+软死区+世界角速度前馈+位置额度保留。feedback无效底盘零力；云台故障/超时/数值或周期异常Stop、清PID、锁存重解锁。首次更新零力建立微分历史。yaw对齐与offset使用DM total_position，避免25rad协议回绕造成假夹角。pitch仅保留相对解锁±5deg窗口，没有确认机械硬限位。IMU采样新鲜度未新增独立判据。
- 文件范围：config、gimbal.c/.h、新sentry_control.c/.h、robot_cmd.c、chassis.c、robot_def.h注释、Makefile源登记及测试。保留原CAN驱动与CAN1/2控制5/反馈0。唯一默认build/basic_framework.elf编译PASS，既有四项newlib链接告警，无烧录。
- 软件验证：run_chassis_tests.ps1的真实CMD/DBUS/仲裁/看门狗测试及新真实PID控制器测试PASS；同步更新原先与已确认反向摇杆映射不一致的vx断言、按当前pitch调试窗口断言。增加左横yaw/FOLLOW夹角/无反馈底盘零力检查。dmmotor_port_test与真实CAN BSP联合测试PASS，CAN2按当前config反馈0验证与CAN1同ID隔离、零增益力矩帧。tests/build/sentry_task_integration_test.c由当前gimbal.c提取真实globals/初始化/tasks/getter并链接真实PID，验证世界yaw/编码器pitch、跨25rad、Nm限幅、超时/故障恢复锁存、重解锁首拍零力和周期异常，PASS。测试不证明机械映射或实体稳定性。独立只读Verifier未发现上述假设下的软件阻断。固定基线审计入口仍缺失。
- 最新用户纠正：pitch是连杆结构，之前yaw方案有误，要求重新询问。已重新询问IMU实际安装部件、大yaw与底盘期望行为，以及yaw传动和pitch连杆几何/角度对应数据。在取得答案前不继续基于旧假设修改依赖逻辑；当前草稿不是最终实车固件，明确告知用户先不要烧录。config中pitch比例1仅占位，不能视作连杆等效固定比例。尚未获得任何实体PID/方向/几何验证。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-13｜按用户重新确认方案：大 yaw 锁电池方向，pitch 连杆先控电机轴

- 用户重新确认C板固定于大yaw平台，仅随大yaw转；大yaw应始终朝电池、与底盘保持相对固定；大yaw仍同步带1:1。pitch为连杆结构，暂时没有杆长、铰点或电机角/俯仰角对应数据。本条覆盖DEV-12的世界角保持及底盘追随草稿前提。
- 大yaw改为DM连续total_position/velocity反馈，角度环→速度PI→Nm力矩，目标为固定battery_align。IMU Yaw/Gyro与遥控world yaw不参与出力，world_rad仅观察。对齐标定有效时用编码器标定角；否则首次有效解锁捕获当前姿态，并跨停机/故障保持原参考，再解锁限速回到原参考。尚不能未摆正就自动识别电池方向，须用户先人工对齐后首次解锁。
- 底盘恢复独立遥控：右中RC_CONTROL、左横杆转底盘、右下自转；键鼠X控制底盘转向、Q切自转，平移使用车体系offset0。删除本轮草稿的主动底盘跟随接入、GetOffset接口与Follow算法；保留旧非调试整车代码及既有雷达参考接口。大yaw不会因底盘转动而做世界朝向反向补偿。
- pitch不再假设外部1:1或把电机角叫作俯仰角：直接用4310输出轴连续角rad和速度rad/s，方向仅映射输入增量，PID输出Nm直接给MIT力矩。±5deg和10deg/s均为电机输出轴调试窗口/限速，不构成实际俯仰行程或机械安全范围。等待连杆数据后再建非线性角度/力矩映射，未复制原M3508的19:1或旧负载重力电流。
- 保留参考串级结构、角速度前馈、近远阻尼调度、积分限幅和0.012s输出低通。新轻载初值yaw角Kp12、速度Kp0.6/Ki0.1、限矩0.8Nm；pitch角Kp4、速度Kp0.6/Ki0.2、限矩1Nm。数值集中config并标待实测，不能声称已完成实体整定。MIT电机内部Kp/Kd均0，只发送C板计算力矩，避免叠加两套位置环。CAN1大yaw、CAN2pitch均发送5/反馈0。
- 验证：run_chassis_tests.ps1全部PASS，真实CMD/DBUS仲裁及看门狗、pitch电机窗口、左横只转底盘而不累加yaw、鼠标X接管底盘、PID SI单位/限幅/前馈/复位/NaN/周期异常。重新从当前gimbal.c提取实际任务体到tests/build/sentry_task_integration_test.c，真实PID联测PASS，明确验证IMU转360deg和yaw世界命令变化不产生大yaw力矩、编码器0.02rad偏离产生反向纠正、跨25rad仍连续、pitch电机窗口、CAN超时/故障停机、反馈恢复不自动重启、急停重解锁清积分首拍零力及任务延迟锁存。DM驱动与真实BSP联合测试上一轮PASS且驱动未改；当前CAN测试按config反馈0和MIT零增益力矩帧验证。
- 默认cmake --build build --parallel 8通过，唯一build/basic_framework.elf，无新固件目录或另名ELF，既有四项newlib链接告警。未烧录、未操纵目标或电机。独立只读Verifier确认无最新方案的软件阻断；编码器对齐、实际方向、轻载增益、连杆映射和真实限位仍待实体/资料。固定基线审计路径缺失的限制沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-14｜按用户要求放宽 pitch 电机轴调试行程

- 用户反馈动作微小，要求先大幅放宽再测试。SENTRY_PITCH_TRAVEL_DEG由5改为20，4310输出轴相对解锁点±20deg（总窗口40deg），输入端命令夹限和实际任务目标窗口均引用同一宏。速度仍10deg/s，输出限矩仍1Nm，PID增益本轮未改。
- 连杆几何仍未知，该窗口是电机输出轴角度，不是实际俯仰范围或已验证机械行程。用户将自行测试，未进行目标操作或烧录。本轮此前实时读数尝试返回OPENOCD_START_FAILED，未取得可用实体数据，未将此判为PID故障。
- 原CMD限位测试推杆时间400周期延至600，保证20deg目标有足够时间触及夹限，避免浮点边界恰好未到位。run_chassis_tests.ps1全PASS；默认cmake --build build --parallel 8通过，唯一build/basic_framework.elf，四项既有newlib链接告警。固定基线审计入口缺失沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-15｜大 yaw 标定开始，目标改为云台正前方

- 用户最新说明正前方不以电池方向定义，已明确选择应固定于云台正前方。本定义覆盖DEV-13中电池方向的表述；后续标定参数应使用FRONT命名，不能再把battery方向当作物理目标。
- 用户上电后Bridge只读成功，当前build/basic_framework.elf SHA256 f9bc1ec96cf122e481f239e9883cb8244f9235125bcbc65e239b07c946d5efb0与烧录记录匹配（2026-11-22T07:40:24.182Z）。测得position与total_position均-2.406538963317871rad，反馈在线1、state1、control_active1。此为当前位置读数，不是已确认标定值。
- 截图中ELF筛选栏输入指针表达式没有结果，已告知搜索全局sentry_big_yaw_position_rad，或从指针对象展开measure/position。Agent Bridge读取支持指针路径不代表UI的变量名筛选框会求值该表达式。
- 已询问当前姿态是否准确对准用户所需正前方，等待确认后再采样、固化。用户若需手动调整，应先右上挡急停，当前闭环正在运行，未代用户改RAM或操纵电机。本轮尚未写入标定角、改源码/编译或烧录。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-16｜固定大 yaw 云台正前方标定角

- 用户确认当前摆正角度是正确云台正前方并明确要求写入。写入前Bridge复读position=total_position=-2.406538963317871rad，online1/state1；当前所选ELF与烧录SHA256 f9bc1ec96cf122e481f239e9883cb8244f9235125bcbc65e239b07c946d5efb0一致。机械定义按用户最新说明，电池方向不再作为目标。
- config改为SENTRY_BIG_YAW_FRONT_ALIGN_VALID=1、SENTRY_BIG_YAW_FRONT_ALIGN_RAD=(-2.406538963f)，替换旧BATTERY_ALIGN_VALID/DEG。gimbal直接读取rad常量，目标与反馈应用同样direction/ratio转换，无重复度转换；battery_target及当前注释改为front。CAN、PID与pitch行程配置不变。标定写入固件参数，不操作电机置零或RAM。
- 从当前gimbal.c更新实际任务提取测试并链接真实PID后PASS：正前方起始目标等于标定角；停止后电机偏离标定0.1rad，再解锁仍向固定标定角限速回正而非捕获偏离姿态，20拍后目标到标定角、力矩向负方向纠偏；独立跨圈场景测试25rad展开及0.02rad误差；超时/故障/重解锁清积分等继续PASS。
- cmake --build build --parallel 8通过，唯一build/basic_framework.elf，四项既有newlib存根链接告警。独立只读审查确认单位、坐标和限速接入无阻断。更新.Doc/本车调参结论.md及冷启动摘要。固定基线审计入口缺失的限制沿用。
- 未烧录，目标板仍运行旧版；用户重烧后需验证断电重启时同姿态角度与回正结果。此次不声称实体标定复现或PID整定已验收。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-17｜用户纠正摆正方向，重采正前方角0.673113823rad

- 用户明确要求重新读取当前位置作为目标，前次姿态理解有误。连续两次Bridge读取position=total_position=0.6731138229370117rad（38.566581deg），online1/state1、control_active0。旧运行固件SHA256 613c7db7c1844978d6c3e96d08102df485f21ba2124788851a01b194e721bdb3与所选ELF匹配，烧录记录2026-11-22T07:59:10.835Z。
- 将SENTRY_BIG_YAW_FRONT_ALIGN_RAD改为0.673113823f，VALID保持1；前次-2.406538963rad不再作为本车正确标定目标。同步冷启动与.Doc/本车调参结论.md，保留旧DEV作为历史。
- 重新编译并运行实际任务提取测试PASS，覆盖固定标定参考、偏离后重解锁限速回参考、故障锁存、跨圈及首拍零力；默认cmake --build build --parallel 8通过，四项既有newlib链接告警。唯一产物build/basic_framework.elf，未烧录。磁盘新ELF不同于板上旧版，需用户重烧后确认。基线审计入口缺失限制沿用。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-18｜按用户要求用当前实际角替换大 yaw 固定目标

- 用户明确要求将实际角当前读取值设为新目标。Bridge读取position=total_position=sentry_big_yaw_position_rad=-0.7635231018066406rad（约-43.746651deg），旧target=0.6731138229370117rad，online1、control_active1。所选ELF与板上烧录SHA256 a0c1f6946e36b3127270b6c54d871a5aca3b6c8837b1655d9eaca262a91ad646匹配。
- SENTRY_BIG_YAW_FRONT_ALIGN_RAD更新为-0.763523102f，VALID保持1；同步冷启动与调参结论。该值是用户指定时刻的实际位置快照，不据此声称机械正前方独立复测或PID整定完成。
- 更新参数后重新编译真实任务提取测试并运行PASS，默认ARM构建PASS，既有四项newlib链接告警。唯一build/basic_framework.elf。未写目标RAM、未烧录；新固定目标需用户重新烧录后解锁才应用。此前四轮RAM零输出计划尚未获具体确认，本轮未执行该写入。
- AI：Codex（GPT-6.1-sol）。

### DEV-20261122-19｜大 yaw 0.02deg 死区实机采样

- 用户要求大yaw死区改0.02deg，并明确自行烧录后通知采样。当前config为SENTRY_BIG_YAW_DEADBAND_DEG=0.02，pitch仍0.3deg；gimbal初始化分别引用。用户烧录后Bridge确认唯一build/basic_framework.elf与烧录记录SHA256 d50c80c86b1a6d1199813984b6c4a14024f8e6af8f68861e40815621f0033ef2匹配，实时angle.DeadBand=0.0003490658418741077rad。Agent未烧录、未写目标RAM。
- 初段采样control_active全0，不用于评价PID。用户解锁后补采400点、请求间隔50ms、实际跨度20.285s，保存exports/yaw_pid_002deg_capture.ndjson：active全1，固定target=-0.7635231018066406rad；target-actual误差范围-3.49711至+2.42614deg，RMS1.44679deg；指令力矩-0.8至+0.70824Nm，400个离散样本中2点达到负向限幅。仅为本次转动扰动记录，不代表完整频响或已整定。
- 再采160点、跨度9.31s，保存exports/yaw_pid_002deg_rest.ndjson：actual=-0.7433052062988281rad保持不变，残差-1.15840deg，力矩约-0.24117Nm，速度积分Iout=-0.1Nm达到限幅，active全1。随后独立读取online1/state1、rx_refresh_dt0，电机反馈力矩约-0.242至-0.247Nm变化，说明存在有效反馈和持续出力。请求的motor->rx_count字段不可解析，未将其当作有效读数。
- 解释：当前存在角度静差与速度积分限幅，缩小死区本身不能消除该静差；摩擦/负载可能参与，但未据此断言机械故障或电机力矩标定正确。建议后续分步增加积分额度、复测静态回正，再评估速度Kp和角度Kp；本轮只采样分析，未修改PID增益或限矩，未声称整定完成。此前固定基线审计入口缺失限制沿用。
- AI：Codex（GPT-6）。

### DEV-20261122-20｜提高大 yaw 静态回正的积分额度

- trace_id=RM-YAW-IRECOVERY-20261122，round=1。用户要求修改PID，反馈停车后不回正。本轮按DEV-19的实机记录仅把SENTRY_BIG_YAW_INTEGRAL_NM从0.1改为0.25Nm，标待实测；速度Kp0.6/Ki0.1、角Kp12、总限矩0.8Nm、死区0.02deg和固定目标-0.763523102rad保持。提高额度允许速度积分继续累积，不构成已验证消除静差的结论。
- gimbal的InitSentryController将该宏传入speed.IntegralLimit，现有PID_Integral_Limit限制Iout、MaxOut仍限制最终力矩；停机及异常仍调用SentryAxisReset清积分，未改控制逻辑或接口。
- cmake --build build --parallel 8通过，唯一build/basic_framework.elf；四项既有newlib链接告警。SHA256 affc5584b07d4416507f9c87a1981b1414c2dbb3e263a51dc1553241e15eb0ae。tests/run_chassis_tests.ps1通过真实CMD/遥控仲裁、断流看门狗及通用串级PID软件测试；通用PID测试使用独立夹具参数，不构成0.25Nm的实车稳定性验证。固定基线审计入口仍缺失，无法提供审计计数。
- 未烧录或写目标RAM。用户烧录默认ELF、解锁后需复测停车回正、目标与实际误差、积分累积及过冲；新旧ELF不一致期间不使用新地址评价旧固件。
- AI：Codex（GPT-6）。
- 独立只读Verifier确认积分额度接入与0.8Nm总限矩有效，无需改逻辑；Ki未提高，积分建立速度不变。新参数的回正时间、过冲和振荡仍待实体复测。

### DEV-20261122-21｜积分0.25Nm烧录后回正复测仍有静差

- 用户通知烧录完成并要求采集。Bridge只读核对root ELF与烧录SHA256 affc5584b07d4416507f9c87a1981b1414c2dbb3e263a51dc1553241e15eb0ae匹配，烧录记录2026-11-22T08:52:04.623Z，实时speed.IntegralLimit=0.25。初次读control_active0，提示按原流程解锁；随后600点请求50ms间隔、实际跨度31.968s的记录active/online/state均全1。
- exports/yaw_pid_I025_capture.ndjson保留原始数据。target始终-0.7635231018066406rad，实际角范围-0.8165483474731445至-0.7131681442260742rad，target-actual误差-2.88513至+3.03812deg。两次人工转动不保证相同激励，不用峰值直接宣称PID优于或劣于前轮。
- 末约10秒实际角固定-0.7433052062988281rad，残差-1.15840deg不变；积分随时间累积至-0.25Nm，指令力矩最终-0.391173Nm，仍未继续回正。补采4点真实驱动measure.torque范围-0.39316273至-0.39316273Nm，rx_refresh_dt0、online1/state1、active1，保存exports/yaw_pid_I025_feedback.ndjson。说明新积分额度及出力反馈有效，但不能据此声称力矩传感精度已独立标定。
- 本次改额度未消除静差。静摩擦、传动预紧/负载或其他机械阻力属于待验证假设，未仅凭软件读数认定硬件故障。后续宜控制扰动测起动力矩与回正，再决定速度Kp/积分额度；本轮只采样，未修改PID、写RAM、烧录或操纵电机。
- AI：Codex（GPT-6）。

### DEV-20261122-22｜继续提高大 yaw 积分建立速度及额度

- trace_id=RM-YAW-IRECOVERY-20261122，round=2。用户明确要求继续调整PID。本轮依据DEV-21停车1.1584deg静差、Iout=-0.25Nm触顶及约-0.393Nm反馈力矩，将SENTRY_BIG_YAW_SPEED_KI由0.1提高至0.3Nm/rad，SENTRY_BIG_YAW_INTEGRAL_NM由0.25提高至0.45Nm。均标待实测，目的是加快持续回正力矩建立并增加额度，尚未证明消除静差。
- 速度Kp0.6、角Kp12、近远Kd、总限矩0.8Nm、死区0.02deg、固定目标-0.763523102rad及pitch全部保持。只改config两项数值及其来源注释，未变接口、控制逻辑、CAN或停机复位。积分增大后的过冲/振荡及机械起动力矩需实体验证。
- 默认cmake --build build --parallel 8通过，四项既有newlib链接告警；tests/run_chassis_tests.ps1通过真实CMD/遥控仲裁、断流看门狗及通用串级PID夹具测试。通用测试非本车机械仿真，不能代替新参数的实测。唯一固件build/basic_framework.elf，SHA256 f2c0e08c7f779e1a59d0e7fbcf738d1e93b8126cef82b1abeeff8b6395f30b1a。固定基线审计入口仍缺失。
- 未烧录、未写RAM、未操纵目标；用户烧录解锁后复测回正、积分及过冲。磁盘ELF已变，采样前须重新核对烧录一致性。
- AI：Codex（GPT-6）。
- 独立只读Verifier确认Ki与积分额度进入yaw速度环、停机/故障清积分及滤波后±0.8Nm总限幅有效，无软件阻断；回正时间、过冲和振荡尚未实测。

### DEV-20261122-23｜Ki0.3及积分0.45Nm复测仍未回正

- 用户通知已烧录。Bridge只读确认唯一build/basic_framework.elf与烧录SHA256 f2c0e08c7f779e1a59d0e7fbcf738d1e93b8126cef82b1abeeff8b6395f30b1a匹配，记录2026-11-22T08:56:55.559Z。实时speed.Ki=0.3000000119、IntegralLimit=0.4499999881，active/online/state均1。
- 第一段600点、跨度29.921s，保存exports/yaw_pid_Ki03_I045_capture.ndjson。target固定-0.7635231018066406rad，转动期误差-3.54082至+3.95610deg，力矩指令触及正负0.8Nm，active/online/state全1、反馈年龄0..1。该段末尾仍有变化，不能在不确认外部扰动的情况下认定自激振荡，也不能与前轮非一致激励直接比较整定优劣。
- 提示用户完全停车/摇杆回中后补采300点、跨度16.91s，保存exports/yaw_pid_Ki03_I045_rest.ndjson：chassis_cmd_recv.wz全0，actual全程-0.7349128723144531rad，target全程-0.7635231018066406rad，误差-1.6392454deg。Iout持续-0.4499999881Nm触顶，指令约-0.651598Nm；驱动measure.torque回报-0.6617823至-0.6471310Nm，反馈在线且active全1。反馈速度-0.007326rad/s处于协议零速附近量化档，不据此声称实际持续转动。
- 结论仅限本段：新参数已生效并持续出力，但未消除静差。积分触顶后继续加Ki不会增加该状态的稳态输出。摩擦、带传动预紧、线束/限位或实际负载等仍需检查或测起动力矩；软件数据不能直接锁定某个机械原因。未修改PID、未写RAM、未烧录或代用户操纵执行器。
- AI：Codex（GPT-6）。

### DEV-20261122-24｜单独提高大 yaw 速度环比例增益

- trace_id=RM-YAW-IRECOVERY-20261122，round=3。用户明确要求再改一轮PID。本轮仅将SENTRY_BIG_YAW_SPEED_KP由0.6提高至0.9Nm/(rad/s)，按DEV-23的1.639deg停车静差及约-0.65Nm出力记录试增比例纠偏；新值标待实测。
- Ki0.3Nm/rad、积分限幅0.45Nm、总限矩0.8Nm、角Kp12、死区0.02deg、固定目标-0.763523102rad及pitch保持，控制逻辑、停机清积分和CAN配置未改。提高比例增益会改变速度误差到力矩的响应，近目标过冲、振荡与回正效果需实体复测；不据此认定机械阻力原因。
- cmake --build build --parallel 8通过，四项既有newlib链接告警；tests/run_chassis_tests.ps1通过真实CMD/遥控仲裁、断流看门狗及通用串级PID软件夹具测试。通用夹具不证明本车新Kp的闭环稳定性。唯一build/basic_framework.elf，SHA256 8e8eae9e09d12aed8a8e48a9b2a17a64ecf5ead67a870b2d495773425fdc6a77。固定基线审计入口缺失限制沿用。
- 未烧录、未写RAM、未代用户操纵执行器；用户烧录解锁后再采集，先核对ELF一致性。
- AI：Codex（GPT-6）。
- 独立只读Verifier确认新速度Kp0.9接入正确、最终±0.8Nm限矩与停机清积分保持；按旧停车误差估算指令约-0.752Nm，仅为计算，不是新参数实测。

### DEV-20261122-25｜提高大 yaw 可用力矩限幅

- trace_id=RM-YAW-IRECOVERY-20261122，round=4。用户反馈当前手推大 yaw 费力，并怀疑电机给力偏小。结合DEV-23在±0.8Nm总限矩下仍有1.639deg静差、控制和驱动反馈在线且约-0.65Nm出力的记录，将大yaw总力矩限幅从0.8提高到1.5Nm，积分限幅从0.45提高到0.8Nm；角Kp12、速度Kp0.9/Ki0.3及其他逻辑不变。
- `cmake --build build --parallel 8`成功（Ninja提示无需重编）；唯一固件`build/basic_framework.elf`，SHA256 `F52EE6C88438C1EFE713A6C0701B72C3C1FEE7CC24B752EE71C5723F49DA0946`。未烧录、未写RAM。仍需烧录后复测回正静差、过冲、振荡及电机温升。
- 手推阻力本身不证明电机力矩偏小：若遥控已解锁、闭环正在保持，阻力说明存在保持出力；若失能或断电仍难推，应先排查机构阻力/传动预紧。1.5Nm是新的软件指令上限，不等于已验证的关节实测力矩。
- AI：Codex（GPT-6）。

### DEV-20261122-26｜按用户要求将大 yaw 力矩限幅提高至 6 Nm

- 在DEV-25基础上，仅将`SENTRY_BIG_YAW_TORQUE_NM`由1.5改为6.0Nm；`SENTRY_BIG_YAW_INTEGRAL_NM`维持0.8Nm，其他PID参数与控制逻辑不变。
- `cmake --build build --parallel 8`通过；唯一固件`build/basic_framework.elf`，SHA256 `4DAC1F27FB600987182AD08675500C596AE7B03E3606FDCB7713BBC1EB71EDC5`。未烧录、未写RAM。6Nm为软件力矩指令上限；烧录后应先低速、悬空并在急停可及条件下验证，观察实际电机力矩、回正、过冲和温升，再决定是否继续调整。
- AI：Codex（GPT-6）。

### DEV-20261122-29｜大 yaw MIT 力矩映射扩展到电机峰值范围

- 用户要求大 yaw 力矩调到最大。按 DM-J4310-2EC V1.2 手册的峰值12.5Nm，将大yaw控制限幅及该电机实例的MIT发送/反馈扭矩映射设为±12.5Nm；pitch与普通DM电机保留默认±10Nm。`MIT_Init_Config_s`新增可选每实例映射上下限，未提供时回退默认范围。
- 独立只读审查确认发送、反馈解码及普通模式限幅使用实例范围；未发现当前配置阻断。实际大yaw驱动器TMAX必须先同步配置为12.5，pitch维持10，否则命令映射缩放且反馈力矩被高估25%。没有验证驱动器当前TMAX已写入。峰值仅供短时使用，额定3.5Nm，不应持续顶峰。
- `cmake --build build --parallel 8`通过；唯一固件`build/basic_framework.elf`，SHA256 `8D9D268DD51B5B49A93423793F458DF036690C31C2116BCC04B73A321149AC58`。未烧录、未写RAM。未运行测试（用户未要求）。
- AI：Codex（GPT-6）。

### DEV-20261122-27｜用户要求试设 12 Nm 大 yaw 力矩上限

- 将`SENTRY_BIG_YAW_TORQUE_NM`设为12.0Nm，积分限幅仍0.8Nm。注意`modules/motor/DMmotor/dmmotor.h`定义MIT扭矩映射范围为±10Nm，`dmmotor.c`的`float_to_uint()`会把超范围值饱和；因此虽然控制器上限配置为12，当前实际CAN扭矩指令仍不超过10Nm。要真实请求12Nm，需先确认电机/调试工具T_MAX可安全配置到12并同步修改驱动映射，不能只扩大软件PID限幅。
- `cmake --build build --parallel 8`通过，唯一固件`build/basic_framework.elf`，SHA256 `782D7B81CCF840709EB98FE6EC08BDDA489CD55DC489A12221FB71C0DB7CD0E7`。未烧录、未写RAM；现有newlib `_close/_lseek/_read/_write`链接告警仍存在。
- AI：Codex（GPT-6）。

### DEV-20261122-28｜提高大 yaw 角度环回正速度

- 用户反馈带载回正偏慢。本轮仅将`SENTRY_BIG_YAW_ANGLE_KP`由12提高到15 1/s；速度环Kp0.9、Ki0.3、积分限幅0.8Nm、控制器限幅12Nm及其它参数不变。角度环给出的速度目标仍受90deg/s上限约束；MIT报文扭矩映射仍在±10Nm饱和。
- `cmake --build build --parallel 8`通过，唯一固件`build/basic_framework.elf`，SHA256 `CBFCB3248CF906D8A5F2FBEA43404D3B0B17746E1FA9E73BC6E3E0DBC4590173`。未烧录、未写RAM。待用户烧录并在安全条件下观察目标/实际速度、扭矩饱和及回正是否更快；若实际速度持续落后目标且力矩触顶，需再调整速度环或核对机械负载，单独提高角度Kp不能解决扭矩饱和。
- AI：Codex（GPT-6）。

### DEV-20261122-30｜2026-11-22｜发射接入 CAN2(摩擦轮 1/2、拨盘 3)并按低阻尼重定拨盘增益

- **目标与触发**：用户要求按参考工程 `RM-JIT-CR-Sentinel` 把 CAN2 摩擦轮拨码 1/2、拨盘 3 写入本工程；硬件几乎一致，但本车拨盘硬件阻尼比参考小很多，拨盘环不得照抄参考增益。本次请求即对该结构级变更（新增配置宏、改 `robot.c` 接线）的明确授权。
- **功能变化**：①发射 ID 与 PID 从 `shoot.c` 的硬编码收敛到 config：`FRICTION_ID_L/R`=1/2、`LOADER_ID`=3（均 CAN2）；②新增 `SENTRY_SHOOT_CONTROL_ENABLE`（默认 1），使 `CHASSIS_TUNING_ONLY=1` 时也执行 `ShootInit()/ShootTask()`——此前该构型下发射完全不注册，CAN2 三台电机无实例、无反馈、无出力；③拨盘速度环按低阻尼下调：参考 6.0/0.5/800/8000 → 本车 3.0/0.4/600/5000；④拨盘角度环由参考的 0（等于禁用，单发请求会一直不动直到 `LOADER_SINGLE_TIMEOUT_MS` 超时）改为 2.5/0.1/6000，单发/三发才有可能真正走完一发。
- **文件与符号**：`config/robot_infantry_2026.h`（新增"发射电机接线与 PID"段、`SENTRY_SHOOT_CONTROL_ENABLE`）、`application/shoot/shoot.c`（`ShootInit()` 改读宏）、`application/robot.c`（两处 `#if !CHASSIS_TUNING_ONLY` 改为 `|| SENTRY_SHOOT_CONTROL_ENABLE`）。
- **接口与调用链**：消息话题不变（`shoot_cmd`/`shoot_feed` 早已在 `RobotCMDTask()` 无条件发布，cmd 侧无需改动）。CAN2 现在同时挂达妙 pitch（发 5 / 收 0，MIT 帧）与 DJI 组帧（id1-3 → 命令 0x200，反馈 0x201~0x203），组 ID 不冲突；摩擦轮镜像沿用左 REVERSE / 右 NORMAL，互锁符号未动。
- **验证**：ARM 全量编译 0 error，本次改动文件 0 warning（仅既有四条 newlib `_close/_lseek/_read/_write` 链接告警）；`tests/run_chassis_tests.ps1` 全 PASS。**构建环境异常**：本机 `build/` 的 CMakeCache 指向不存在的 `C:/Users/10821/AppData/Local/stm32cube/...`，`cmake --build build` 无法执行；验证构建改在 `C:/temp/rm_sb_build`（CMake + Unix Makefiles，arm-none-eabi-gcc **15.3.1**）完成，基线工具链为 14.3.1。已将该 ELF 放回唯一默认路径 `build/basic_framework.elf`，SHA256 `b2889538f0fcf756e2d4f2ec27443405a0adc4a54410a74a604c600b7a907d97`，FLASH 111464 B(10.63%) / RAM 96200 B(73.39%)。两套工具链产物不保证逐字节一致，需 14.3.1 复现时先恢复 STM32CubeCLT。
- **待实测/风险**：拨盘增益是按"阻尼更小 → 同 Kp 等效过增益"减半的经验值，**不是整定结果**；单发角度环首次启用，架空需确认①单发只拨一发、不连拨②一发到位时间 < 1000ms③到位后不超调蹭槽。`LOADER_JAM_CURRENT=1800` 仍是估值，阻尼变小后正常拨弹电流更低（误判风险下降，但真堵判据未复标）。未烧录、未上电、未台架。
- **未触碰**：本轮未改大 yaw / pitch 的 PID 与标定角；config 中 `SENTRY_BIG_YAW_ANGLE_KP 70 / SPEED_KP 3.6 / KI 0.6 / INTEGRAL_NM 2.0 / FRONT_ALIGN_RAD -1.06374` 与 §0、DEV-24/28 记录不一致且无对应条目，仍待用户确认来源。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-31｜2026-11-22｜pitch 轴按达妙 J4310 复核并显式声明 MIT 力矩量程

- **目标与触发**：用户明确本车 pitch 与参考工程不同——是达妙 DM-J4310-2EC，CAN2 ID5，摩擦轮左 1 右 2；要求复核 pitch 轴。
- **复核结论（代码已是 DM 原生路径，无 M3508 残留）**：`CHASSIS_TUNING_ONLY` 构型下旧的 M3508 yaw/pitch 配置整体 `(void)` 不注册；pitch 由 `MITMotorInit(hcan2, tx=5, rx=0)` 注册，电机内 MIT Kp/Kd 均为 0（不叠加第二套位置环），闭环全部在 C 板：`SentryAxisUpdate()` 角环 + 速度 PI → Nm → `DMMotorSetMITTorque()`。闭环量是 4310 输出轴 `total_position`/`velocity`，不经过 19:1、不用 IMU 反馈、不用 `PITCH_HORIZON_ECD`。
- **CAN2 帧表核对（无冲突）**：DJI 组 id1-3 共用一条命令帧 `0x200`（摩擦轮 L 占字节 0-1、R 占 2-3、拨盘占 4-5），反馈 `0x201/0x202/0x203`；达妙 pitch 命令 `0x005`、反馈 `0x000`。CAN2 过滤器 bank 14-27（每实例占一槽，`CANAddFilter()`），不存在"四槽已满"的限制。达妙任务 `DM_TASK_CYCLE=2`（500Hz），CAN2 合计约 5 帧/ms ≈ 60% 负载，仍在可用范围。
- **改动**：config 新增 `SENTRY_PITCH_TORQUE_RANGE_NM=10.0f`，`gimbal.c` 的 pitch MIT 配置显式传入 `±range`。此前依赖驱动默认 `DM_T_MAX=10`，数值相同故**行为不变**，但把"量程必须等于驱动器 TMAX"从隐式约定变成可检查的配置项并写进注释。
- **风险（未修改，待用户裁决）**：同型号的大 yaw 量程是 `±12.5`（DEV-29 按手册峰值改），而 DEV-11 截图里该驱动器量程写作 P/V/T = 12.5/30/**10**。若大 yaw 驱动器 TMAX 仍是 10，则实际出力 = 指令 × 0.8、反馈力矩被高估 25%——DEV-23 记录的"指令约 -0.65Nm"实际约 -0.52Nm。同型号两颗电机用两个量程，至少一颗与驱动器不同步，需先到上位机核对 TMAX 再统一。
- **验证**：ARM 全量编译 0 error，本次改动文件 0 warning；`tests/run_chassis_tests.ps1` 全 PASS；唯一固件 `build/basic_framework.elf`，SHA256 `210b857410b64a264d94c9bbf7283f6b4249d914777dd4785d6ae3f246d8d365`，FLASH 111480 B / RAM 96200 B（arm-none-eabi-gcc 15.3.1，构建环境限制同 DEV-30）。未烧录。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-32｜2026-11-22｜按驱动器 TMAX=10 修正大 yaw 力矩映射量程

- **目标与触发**：用户提供达妙上位机截图（控制幅值页）：PMAX 12.5 / VMAX 30 / **TMAX 10** / KT_OUT 0 / 齿轮系数 1 / 阻尼因子 4。据此确认驱动器力矩量程是 10Nm，而 DEV-29 曾把大 yaw 的 MIT 映射量程按"手册峰值 12.5Nm"改成 ±12.5，二者不一致。
- **问题定性**：MIT 帧力矩是 12 位定点，C 板按 ±12.5 编码、驱动器按 ±10 解码 → 实际出力 = 指令 × 0.8；反馈力矩被按 12.5 解码 → 读数高估 25%。因此 DEV-23 记录的"指令约 -0.65Nm / 反馈约 -0.65Nm"对应真实出力只有约 -0.52Nm，此前对"静摩擦/起动力矩"的判断用了偏高的力矩基数。
- **改动**：`SENTRY_BIG_YAW_TORQUE_RANGE_NM` 12.5→**10.0**（=驱动器 TMAX），`SENTRY_BIG_YAW_TORQUE_NM` 12.5→**10.0**（控制器限幅不得大于可编码量程，否则 PID 在编码不出来的值上积分饱和）。pitch 侧 `SENTRY_PITCH_TORQUE_RANGE_NM=10.0` 与 TMAX 一致，无需改。齿轮系数 1 与代码假设一致（pitch 直连、大 yaw 1:1 同步带，均无额外传动比）。
- **预期影响**：同一 PID 输出下真实力矩提高 25%，反馈力矩读数会比以前低（因为不再高估）。DEV-23 的 1.639deg 静差有可能因此被打破，也可能因出力变大出现过冲；首次复测应低速、小幅度、急停可及。
- **验证**：ARM 编译 0 error，本次改动文件 0 warning；`tests/run_chassis_tests.ps1` ALL PASS；唯一固件 `build/basic_framework.elf`，SHA256 `12d04ab17a071c77f4cc3e5b39622595b07de43b6888db763502004dda883761`，FLASH 111472 B / RAM 96200 B（gcc 15.3.1，构建环境限制同 DEV-30）。未烧录、未写RAM。
- **遗留**：`SENTRY_BIG_YAW_INTEGRAL_NM=2.0` 与速度 Kp3.6/Ki0.6 仍是 DEV-24~28 之后未入账的一组值；量程修正后这组增益的相对强度变了（真实出力 +25%），需要重新评估而不是沿用旧结论。10Nm 为额定约 3.5Nm 的 3 倍，仅允许短时。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-33｜2026-11-22｜小 yaw 更正为 DJI GM6020(非达妙),旧裸通道标废弃

- **目标与触发**：用户更正小 yaw 型号为 DJI GM6020。此前 config 与 `sentry_small_yaw_comm.*` 均按达妙 DM-J4310 编写,属型号错误。
- **事实更正**：小 yaw = GM6020,CAN1,电调拨码 5 → 命令帧 `0x2FF`(id5-7 组)、反馈 `0x209`(反馈恒为 0x204+id)。与四轮(`0x200` / 反馈 `0x201~0x204`)、大 yaw 达妙(`0x005` / `0x000`)均不冲突,**接入不需要改号**。DEV-20261122-31 末尾"小 yaw 同号会触发 CANRegister 死等"的推断随之作废——那是达妙同号(都为 rx 0)才成立,GM6020 不占 rx 0。
- **驱动现状**：框架原生支持 GM6020(`dji_motor.c` 的 GM6020 分支:自动分组 0x1FF/0x2FF、电流帧 0x1FE/0x2FE 由实例 `control_mode` 选择、反馈 0x204+id、三环 + 反电动势/前馈)。但本工程**没有任何小 yaw 实例注册,也没有控制任务**,即"驱动有、接线没接"。
- **改动(仅注释/命名,二进制未变)**：`SENTRY_SMALL_YAW_MODEL` 改 "GM6020";`SENTRY_SMALL_YAW_CAN_ID` → `SENTRY_SMALL_YAW_ESC_ID`(达妙"CAN ID"与 DJI"电调拨码"语义不同);`SENTRY_SMALL_YAW_MASTER_ID` 标废弃(GM6020 无 Master ID);`sentry_small_yaw_comm.c/.h` 文件头标"废弃待删"并修正 .h 写错的 include guard(`SENTRY_BIG_YAW_COMM_H`)。
- **验证**：ARM 编译 0 error 0 warning;ELF SHA256 与 DEV-32 **完全相同**(`12d04ab1…3761`),证明本次只动注释与宏名、未改任何可执行代码;`tests/run_chassis_tests.ps1` ALL PASS。未烧录。
- **接入前待确认**：①实际拨码是否 5(只影响 `SENTRY_SMALL_YAW_ESC_ID`)②电压帧还是电流帧(`control_mode`)③小 yaw 在大 yaw 之内还是之外(决定 `sentry_yaw.c` 里 `small_to_big` 的符号与串联顺序)④控制需求是速度环还是角度环。
- **风险提示**：CAN1 加入 1kHz GM6020 后约 8 帧/ms(≈90% 带宽,四轮 5 + 达妙 1 + GM6020 2)。若出现丢帧/延迟,优先把 `DM_TASK_CYCLE` 由 2 改 4(达妙 500→250Hz),可省下约 1 帧/ms。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-34｜2026-11-22｜小 yaw(GM6020)接入:常开自由稳像,无模式区分

- **目标与触发**：用户要求小 yaw "没有模式区分,始终保持自由——不管跟随还是小陀螺都保持稳定,然后能够自由转动"。本次请求即对该结构级变更(新实例 + 新任务 + 新 config 段)的明确授权。
- **语义落点**：实现为**纯速度阻尼 + 母体角速度反向补偿**——没有位置目标、不跟摇杆、不随 `gimbal_mode` 换策略;安全态(未解锁/断流/失联/IMU 无效)才零力。
- **为什么参考量是 −Gyro[2]**：IMU 装在**大 yaw 平台**(C 板随大 yaw),`Gyro[2]`(rad/s)是大 yaw 的世界角速度,不能直接当小 yaw 的速度反馈。小 yaw 编码器量的是"上部相对大 yaw"的速度 `ω_rel`;上部世界角速度 = `ω_big + ω_rel`,故令 `ω_rel_ref = −ω_big` 即可让上部不随底盘自转。稳定项从**参考侧**注入,反馈仍是编码器(1kHz 本地闭环,不吃 IMU 延迟)。做成角度环则手推不动;只做 `ref=0`(相对速度为零)则小陀螺时上部跟着疯转——两种错法都写进了 config 注释。
- **改动**：config 新增 `SENTRY_SMALL_YAW_ENABLE/SPEED_KP/SPEED_KI/INTEGRAL_LIMIT/MAX_OUT/DEADBAND_DEG_S/OUTPUT_RC_S/CARRY_DIR/CURRENT_MODE`;`gimbal.c` 新增实例注册(CMake 之外的 GLOB 自动收集,无需登记)与 `SentrySmallYawTask()`,并在 `GimbalTask()` 的 tuning 分支调用;`robot.c`/`robot_task.c` 三处 `#if` 条件补 `|| SENTRY_SMALL_YAW_ENABLE`,使只开小 yaw 时 Gimbal/INS 仍会初始化。
- **参数取值**：Kp 20 码/(deg/s)(取本车旧 yaw GM6020 电压模式 20 codes/(rad/s) 的同量级换算);Ki=0(手推过程中积分累积会在松手后变成持续漂移);MaxOut 8000/25000≈32%;死区 3deg/s;输出低通 5ms;默认**电压帧 0x2FF**(电流帧需该电调在 Assistant 打开电流环,否则反馈正常但不出力)。
- **CAN 核对**：GM6020 拨码 5 → 命令 0x2FF(组 5)、反馈 0x209;与四轮(0x200/0x201~0x204)、大 yaw 达妙(0x005/0x000)不冲突,无需改号。CAN1 加 1kHz GM6020 后约 8 帧/ms(≈90%),若出现丢帧/延迟,先把 `DM_TASK_CYCLE` 2→4。
- **验证**：ARM 编译 0 error,本次文件 0 warning(仅既有 4 条 newlib 链接告警);`tests/run_chassis_tests.ps1` ALL PASS;唯一固件 `build/basic_framework.elf`,SHA256 `d6dc99311e34e6cd68ea1c8bac24bbabe4a8c238a7554720b6ccae06d5dbbda7`,FLASH 111844 B / RAM 96216 B(gcc 15.3.1,环境限制同 DEV-30)。未烧录、未上电、未台架。
- **首次通电必查(顺序)**：①架空,只上电不转:听静止有无"滋滋"(有则加 `DEADBAND_DEG_S`);②手推上部:有阻尼、松手停在原处、不自己回位;③右中挡底盘原地转 90deg:上部应基本不转——**若上部转得比底盘快,说明 `CARRY_DIR` 反了,改 −1**;④右下挡小陀螺 300deg/s:上部应纹丝不动,若缓慢跟转则 `SPEED_KP` 或 `MAX_OUT` 不够。
- **已知局限**：板上只有一块 IMU 且在母体上,小 yaw 自身姿态不可观;本方案靠"相对速度跟踪"间接稳像,跟踪残差表现为缓慢漂移(无位置项,属自由轴预期行为)。若要严格锁世界角,需在小 yaw 负载上加第二块 IMU。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-35｜2026-11-22｜小 yaw 接线更正为 CAN2 拨码 4;构建缓存已修好

- **事实更正**：用户确认小 yaw 接 **CAN2、拨码 4**（此前按 CAN1/5 配置）。GM6020 id≤4 → 命令帧 **0x1FF**（组3）、反馈 **0x208**。CAN2 冲突核对：摩擦轮/拨盘命令走 0x200（组4）、反馈 0x201~0x203，达妙 pitch 0x005/0x000 —— 与 0x1FF/0x208 均不重叠。
- **现场证据（改之前）**：板上固件与磁盘 ELF 一致（SHA `1e8796e9…`，烧录于 20:04），读取 `sentry_small_yaw_motor` 指针非 NULL（实例已注册，0x2000E298），但 `measure.angle_single_round/total_angle/speed_aps/real_current` **全为 0**、`feedback_online=0`、`control_active=0`。即 CAN 上一帧都没有 —— 与"监听错总线+错 ID"一致，不是解码或控制问题。
- **改动**：`SENTRY_SMALL_YAW_CAN_BUS` 1→**2**、`SENTRY_SMALL_YAW_ESC_ID` 5→**4**；`gimbal.c` 里原先写死的 `&hcan1` 改为按 BUS 宏派生（这是上一轮的实现漏洞：配置改了也不会生效）。
- **构建环境已修复**：`build/CMakeCache.txt` 现指向本机 `C:/Users/ZhuanZ/AppData/Local/stm32cube/bundles`（gcc **14.3.1** + Ninja 1.13.2，`CMAKE_HOME_DIRECTORY=C:/RoboMaster/RM_shaobin`），`ninja -C build` 增量编译通过。DEV-30/31/32/34 里"用 15.3.1 在 /c/temp 验证构建"的限制即日起作废，后续一律在本工程 `build/` 下用 Ninja 构建，产物即唯一固件。
- **验证**：`ninja -C build` 0 error、本次改动文件 0 warning；唯一固件 `build/basic_framework.elf`，SHA256 `deade855142ff71bb25ade42b831ee8926c8f8e36cba62d5a6959e58fe69b047`。**未烧录**——需用户烧录后才能读到 0x208 的反馈与编码器值。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-36｜2026-11-22｜大 yaw 目标加软化到位死区

- **目标与触发**：用户要求给大 yaw 的目标加死区。背景是 1.1~1.6deg 静差消除不掉、积分长期触顶(DEV-19/21/23),到位后仍在持续顶摩擦,只发热不产生位移,还容易被误判成"力不够"而继续加增益。
- **实现（软化死区，非"进带断力"）**：`band = SENTRY_BIG_YAW_TARGET_DEADBAND_DEG`；带内 `target_eff = 当前实际位置`（误差恒 0），带外 `target_eff = 固定目标 − band·sign(误差)`。边界处等效误差恰好为 0,出力连续——与 `CHASSIS_FOLLOW_DEADBAND` 同一套做法。
  **为什么不做硬切换**:角度环 Kp=70,2deg 误差已使速度目标饱和;若进带瞬间力矩从满值跳到 0、出带再跳回满值,表现为"撞一下就窜"。软化形式避免了这个跳变。
  带内力矩恒 0 并对角环/速度环 `PIDClear()`,防止触顶的 Iout 继续顶着(那正是发热与嗡嗡的来源)。
- **改动**：config 新增 `SENTRY_BIG_YAW_TARGET_DEADBAND_DEG=2.0f`(+取值约束注释);`gimbal.c` 的 `SentryBigYawTask()` 增加 `target_eff`/`in_deadband`(新增 volatile 观察量 `sentry_big_yaw_in_deadband`),停止分支同步清零该标志。
- **取值依据**：必须 ≥ 实测静差 1.639deg(DEV-23),否则永远进不了带,等于没加;2.0deg 是"略高于实测上限"的最小可用值。代价是正前方允许 ±2deg 静态偏差(约 1m 处 3.5cm)。
- **验证**：`ninja -C build` 0 error,本次文件 0 warning(仅 4 条既有 newlib 链接告警);`tests/run_chassis_tests.ps1` ALL PASS。唯一固件 `build/basic_framework.elf`,SHA256 `2c9d9b3bf16d511c58414b317f3d9a7238892e0d5693f5eaaf9fca932a65add6`,FLASH 111324 B / RAM 96216 B(本机 gcc 14.3.1 + Ninja)。未烧录。
- **覆盖缺口**：`tests/build/sentry_task_integration_test.c` 是 15:52 的 gimbal.c 抽取副本,未包含本次改动,且 `run_chassis_tests.ps1` 并未编译它——**哨兵三轴目前没有主机测试覆盖**,死区逻辑只有编译与人工推导保证。
- **实车观察项**：①停车看 `sentry_big_yaw_in_deadband` 是否稳定为 1;②若仍长时间嗡嗡/发热 → 死区仍小于静差,加到 2.5~3.0;③若正前方肉眼可见偏角 → 减回 1.5;④若有线束/整线把轴往外拉,可能出现"缓慢呼吸"(在两侧带边来回),把死区加大或查机械。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-37｜2026-11-22｜删除达妙版小 yaw 裸通道(含启动期撞号隐患)

- **目标与触发**：用户批准删除 `application/gimbal/sentry_small_yaw_comm.c/.h`。该文件是早期按达妙协议写的裸 CAN 收发,小 yaw 确定为 GM6020 后已无用途;且它 `rx_id = SENTRY_SMALL_YAW_MASTER_ID = 0` 而总线宏已改为 CAN2,一旦把 `SENTRY_SMALL_YAW_COMM_ENABLE` 置 1 就会与达妙 pitch 的 `rx=0` 同总线撞号,`CANRegister()` 内 `while(1)` 死等 → 开机挂在 `RobotInit()`。
- **改动**：删除两个源文件;`robot.c` 去掉 `#include "sentry_small_yaw_comm.h"` 与 `#if SENTRY_SMALL_YAW_COMM_ENABLE` 调用块;config 删除 `SENTRY_SMALL_YAW_MASTER_ID`、`SENTRY_SMALL_YAW_COMM_ENABLE` 两个宏(连同"废弃待删"注释);`Makefile` 源列表同步移除(CMake 为 GLOB,已重新 configure)。全仓库已无残留引用(仅 config 一段历史注释提到文件名)。
- **验证**：`cmake -S . -B build` + `ninja -C build` 0 error,仅 4 条既有 newlib 链接告警;`build/build.ninja` 中已无该文件;`tests/run_chassis_tests.ps1` ALL PASS;唯一固件 `build/basic_framework.elf`,SHA256 `5eb0e0d44628f9e2ae42e2b948f935458b68078ec3c2bf996efffff2a44329d2`,FLASH 111324 B / RAM 96216 B(与删除前同尺寸:该文件的函数在 `COMM_ENABLE=0` 下无调用者,已被 `--gc-sections` 回收)。
- **基线审计**：RedFlags **2 → 0**(原两条即本文件在 application 层引用 `hcan1`/`hcan2`),Added 23 → 21,Changed 98 / Missing 0 不变。
- 未烧录。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-38｜2026-11-22｜新增 CAN2 全 ID 诊断采集(临时构建)

- **目标与触发**：小 yaw(GM6020,CAN2 拨码 4)在软件侧已完成核对——`rx_id=0x208`、`tx_id=4`、`can_handle` 与 pitch 同为 hcan2,且同总线上摩擦轮×2/拨盘/达妙 pitch 全部在线,唯独 0x208 零帧。需判定电机是"没上电"还是"拨码不是 4",故照 CAN1 的成品做法给 CAN2 加全 ID 捕获。
- **实现**：`bsp_can.c` 新增 `can2_rx_capture[16]` + `can2_rx_capture_overflow`(与 CAN1 同结构、**独立匿名结构,不动 CAN1 路径**,因为主机联合测试直接引用 `can1_rx_capture` 符号);`CANAddFilter()` 在 `CAN2_DIAGNOSTIC_CAPTURE_ENABLE=1` 时把 CAN2 的第一个实例(bank 14)改成 32 位 IDMASK 全通,使未注册 ID 也能进 FIFO;`CANFIFOxCallback()` 增加 `_hcan == &hcan2` 的按 ID 捕获分支。
- **配置**：`config` 新增 `CAN2_DIAGNOSTIC_CAPTURE_ENABLE`,**本次构建置 1**。
- **代价（已知）**：全通过滤器使未匹配帧也进中断,中断负载上升;同一帧可能经全通 bank 与 IDLIST bank 各进一次 FIFO,已注册实例收到重复帧(解码幂等,可接受,与 DEV-20261122-03 的 CAN1 诊断构建一致);若 FIFO 溢出,`count` 只能作"是否有回包"的定性判据,不能当精确速率。
- **验证**：`ninja -C build` 0 error,无新增 warning;`tests/run_chassis_tests.ps1` ALL PASS;唯一固件 `build/basic_framework.elf`,SHA256 `7ae9b1ecabd2188b290eee4d29d4eec65c151b70afbd86093cf11e648989b8fd`,FLASH 111528 B / RAM 96536 B(RAM 增量 320 B = 16 槽 × 20 B)。未烧录。
- **待办**：烧录后读取 `can2_rx_capture[0..15]` 的 id/count/data;定位完成**必须把该宏改回 0** 再出正式固件。
- AI：WorkBuddy（DeepSeek-V4.1-Flash）。

### DEV-20261122-39｜2026-11-22｜小 yaw 拨码实测为 1(不是 4),按 0x205 修正

- **事实更正**：CAN2 全 ID 捕获显示 0x205 的载荷随手动同步变化;按本工程 `DecodeDJIMotor()` 的布局(`ecd=byte0<<8|byte1`,GM6020 与 C620/C610 **同布局、无 ID 字节**)解析为 角度74/转速0/电流0/温度23℃,完全自洽。系统内不存在 M3508 拨码 5,排除 0x205 的另一来源 → **0x205 就是小 yaw 的 GM6020,拨码 1**。
- **纠正本人上一轮的错误推理**：DEV-38 之后曾以"GM6020 首字节是拨码号、温度在 byte7"为由判定 0x205 是 C620 格式。该判据前提错误——本工程解码路径与老步兵 yaw 标定值(`YAW_CHASSIS_ALIGN_ECD=5892` 必须 >2047)都表明本车 GM6020 反馈**没有** ID 字节,故两种电机在载荷层不可区分,只能靠 ID 公式与"系统内是否存在对应 C620 拨码"来判定。
- **为什么与摩擦轮不冲突**：C620/C610 反馈 = `0x200+拨码`(1→0x201,2→0x202);GM6020 反馈 = `0x204+拨码`(1→0x205),相差 4。真正会撞的是 GM6020 1~4 ↔ M3508/M2006 5~8(1↔5,2↔6,3↔7,4↔8),本车 CAN2 无拨码 5~8 的 3508/2006。
- **改动**：`SENTRY_SMALL_YAW_ESC_ID` 4→**1**;命令帧随之由 0x1FF 字节 6-7 变为 **字节 0-1**(`MotorSenderGrouping()` 按拨码自动算,无需手改),反馈 0x208→**0x205**;`CAN2_DIAGNOSTIC_CAPTURE_ENABLE` 复位为 **0**(诊断完成,全通过滤器不留在正式固件);section 注释与诊断开关注释按实测更新。
- **验证**：`ninja -C build` 0 error,本次文件 0 warning;`build/basic_framework.elf` SHA256 `082ffaa00848ef5732277fe6691a5df9af0e56c7bcc43e63b107fa51d49ab7ae`,FLASH 111320 B / RAM 96216 B(RAM 回落 320 B = 捕获数组移除)。**未烧录**——调试探针当前未连接(`unable to find a matching CMSIS-DAP device`)。
- **下一步**：烧录后读 `sentry_small_yaw_motor->measure.angle_single_round` 确认在线,随后手推左右限位各读一次做行程标定。

### DEV-20261122-40｜2026-11-22｜小 yaw 中位标定与三分区软限位

- **输入**：用户给定机械中位 ecd = **7465**(≈328.05°)。
- **关键风险（先处理再实现）**：7465 距编码器回绕点 8192 仅 727 计数(≈32°)。行程只要超过约 32° 就会跨零(8191→0),**裸比较 ecd 大小会在零点处误判为越界**。故判据统一改为"相对中位解缠":`c = ecd − 7465; |c|>4096 时 ∓8192; joint_deg = c×360/8192`,跨零点时角度连续。
- **实现（三分区）**：①行程内 —— 正常稳像 `ref = −DIR·Gyro[2]`;②缓冲带(|joint| ≥ limit−margin) —— **只允许向行程内的速度**,同向补偿被掐成 0,于是母体继续同向转时上部跟着走、关节角不再增长(贴住缓冲边),母体反向时补偿立刻恢复并重新稳像(因此不需要回中弹簧,不破坏"自由轴"语义);③硬越界 —— 不再稳像,按固定速度往内拉。
- **改动**：config 新增 `SENTRY_SMALL_YAW_CENTER_ECD/LIMIT_DEG/LIMIT_MARGIN_DEG/RETURN_DEG_S/CARRIED_LIMIT_ENABLE`;`gimbal.c` 的 `SentrySmallYawTask()` 增加解缠与三分区,新增观察量 `sentry_small_yaw_joint_deg`(相对中位关节角)与 `sentry_small_yaw_limited`(0 行程内 / 1 缓冲带 / 2 越界)。
- **待实测**：`LIMIT_DEG=90` 是**占位值,不是测量结果**,需按左右限位 ecd 换算后填入;若机械不对称,改为左右两个宏。
- **验证**：`ninja -C build` 0 error,本次文件 0 warning;`build/basic_framework.elf` SHA256 `7ccfb6b90276cf53feb5411539dcef368d49a5fd6bd4b51f2878c3bf5ebee5b2`,FLASH 111572 B / RAM 96224 B。未烧录(探针仍未连接)。

### DEV-20261122-41｜2026-11-22｜小 yaw 行程按 ±100° 定参

- **输入**：用户给定左右各 100°(机械行程),中位 ecd 7465。
- **换算**：100° = 2276 计数 → 左限位 ecd **5189**、右限位 ecd 9741→回绕 **1549**;允许区间为 `[5189..8191] ∪ [0..1549]`(跨零点两段)。缓冲带 15°(≈341 计数),开始掐补偿的位置约在左 5529 / 右 1208,离机械端留 15°。
- **改动**：仅 `SENTRY_SMALL_YAW_LIMIT_DEG` 90→**100**,注释写清换算与区间,便于实车按 ecd 核对。
- **验证**：`ninja -C build` 0 error;固件 SHA256 `2c028981ccadbf7a573eacb6ac0ab7a26552b45362724341e87b4cece154c6d5`,FLASH 111572 B / RAM 96224 B。未烧录。
- **实车必查（方向性）**：缓冲带里用的是"joint_deg 与 ref 同号即掐",前提是"ecd 增大方向 = 电机正转方向"。若某台电机反馈符号相反,表现为**贴边后越顶越死**(而不是停住),此时把该条件取反。判据:推到大角度后 `sentry_small_yaw_limited` 应为 1 且 `joint_deg` 不再增长。

### DEV-20261122-42｜2026-11-22｜小 yaw 改为位置保持 + 左下挡摇杆控制；摩擦轮反向修正

- **需求变更**：用户明确"小 yaw 必须任何情况下保持稳定、手推也推不动,必须有一条控制链路",并指定遥控器左下挡控制小 yaw;拨弹/摩擦轮与步兵一致;现摩擦轮方向反转。**本条目取代 DEV-34/36/40 的"自由阻尼轴"语义**。
- **控制链（角度环在 C 板 200Hz,速度环仍在驱动 1kHz）**：`target_deg`(相对中位 7465,夹在 ±85°) → 角度 P(Kp 25)→ `speed_ref`(限 ±180°/s) → 速度环(Kp 50,输出限 16000/25000≈64%) → 电压码。关节角由单圈 ecd 解缠得到。解锁首拍目标固定 0 = 回中位(解决"没有回正");之后只由摇杆增量改变。
- **为什么不做世界系保持**：IMU 在母体上量不到该轴;大 yaw 锁车体 → 相对保持等价于整个上部相对车体固定。已写入 config 段首。
- **接口**：`Gimbal_Ctrl_Cmd_s` 里原本"预留、无人读取"的 `chassis_rotate_wz` 更名为 `small_yaw_rate_deg_s`(float→float,布局与 sizeof 不变,`_Static_assert` 未触发);`robot_cmd.c` 在左下挡把左横杆映射到该字段,并**同时把底盘转向置 0**(一杆两用会互相打架);键鼠路径与 `ApplySafeOutputs()` 均显式清零,防止残值滞留。
- **限位**：目标角夹在 ±(LIMIT−MARGIN)=±85° 使正常操作到不了机械端;若关节角实际超 ±100°(装配偏置/外力硬推)走 20°/s 固定回程。观察量新增 `sentry_small_yaw_target_out_deg`。
- **摩擦轮反向**：`shoot.c` 两处 REVERSE 对调(左 NORMAL / 右 REVERSE),并按"reverse 只作用于输出"同步互换 `FrictionReady()` 的到速符号(左 +目标 / 右 −目标)。
- **验证**：`ninja -C build` 0 error、本次文件 0 warning;`tests/run_chassis_tests.ps1` ALL PASS——其中 case 15 原断言"左下挡左横杆转底盘"已按新映射改为"左下挡 wz=0 且 small_yaw_rate≠0",并新增 15b 断言"左中挡左横杆仍转底盘、small_yaw_rate=0";case 16 增加"键鼠不控小 yaw"。唯一固件 `build/basic_framework.elf`,SHA256 `81637f8a201007952d4b0803319c67609abb3040854f33044ea8cce94441c223`,FLASH 111856 B / RAM 96240 B。未烧录。
- **待实测**：①`STICK_DIR` 符号(推左应往左转)②"手推不动"是否达标(Kp50/MaxOut16000 是估值)③到位是否抖(配合 0.3° 死区)④摩擦轮方向是否真的对(两轮接触面同朝枪口)。

### DEV-20261122-43｜2026-11-22｜小 yaw 中位改 27、行程收为半圆 180°；摩擦轮延时停 1s

- **中位更正**：用户实测机械中位 ecd = **27**（此前给的 7465 作废）。注意 27 距回绕点仅 27 计数（≈1.2°），比 7465 更贴边，解缠为硬要求。
- **行程收窄为半圆**：用户明确"机构只能转半圆、绝对不能进后半圆"。`SENTRY_SMALL_YAW_LIMIT_DEG` 100→**90**（±90° = ±2048 计数）：
  左极限 ecd **6171**、右极限 ecd **2075**；允许区 `[6171..8191] ∪ [0..2075]`（恰好 4096 计数 = 180°），禁区 `[2075..6171]`。`LIMIT_MARGIN_DEG` 15→**10**（目标夹在 ±80°，离端点留 10° 给过冲）；`RETURN_DEG_S` 20→**60**（20°/s 回程太软，推过去像"没限位"）。
- **摩擦轮延时停**：新增 `FRICTION_STOP_DELAY_MS=1000`，`shoot.c` 增加 `friction_running/friction_run_ms`：松开后继续按目标转速跑 1s 再归零，让末发弹仍吃得到轮速。**急停/SHOOT_OFF 路径不享受延时**（函数前段直接 Stop），安全优先。
- **验证**：`ninja -C build` 0 error、本次文件 0 warning；`tests/run_chassis_tests.ps1` ALL PASS。固件 `build/basic_framework.elf`，SHA256 `32d09e3915e4d9c06228adbea1193436ac07bc9f486cf41b0d1c343e34757e47`，FLASH 111900 B / RAM 96248 B。未烧录（探针掉线）。
- **未解决（待数据）**：用户报"左下挡摩擦轮不转 / pitch 推杆没反应 / 小 yaw 无限位"三现象同时出现，代码指向同一个安全态（未解锁或急停锁存），但探针掉线无法验证 `robot_state`/`gimbal_mode`/`friction_mode`。**下次连上先读这三个量**再改任何逻辑。

### DEV-20261122-44｜2026-11-22｜pitch 操作参数对齐步兵（摇杆/鼠标速率、角环增益、速度上限）

- **目标与触发**：用户反馈 pitch 推杆没反应，要求操作逻辑参考步兵。核对后**操作逻辑本来就一致**（左纵杆增量累加、松杆保持当前位置、命令侧夹限、仅在 ROBOT_READY 且遥控在线时累加），差异全在**三处串联的限制器**上。
- **三处限速（原值 → 新值）**：
  1. `PITCH_REMOTE_RATE_DEG_S` 10 → **60**（步兵同值；满杆 0.33s 走完 ±20°）
  2. `PITCH_MOUSE_RATE_DEG_S` 10 → **150**（=KBM_PITCH_MAX_RATE，原与遥控共用 10°/s，鼠标几乎推不动）
  3. `SENTRY_PITCH_MAX_SPEED_DEG_S` 30 → **90**（角度环输出上限；30 是硬瓶颈：摇杆给 60 也走不过 30）
  另：任务内目标斜坡用的是 `SENTRY_PITCH_RATE_DEG_S`，随 1) 一起到 60，不再二次限速。
- **角环增益**：`SENTRY_PITCH_ANGLE_KP` 4 → **8**（步兵同值）。Kp=4 时凑满 60°/s 需要 15° 误差，体感发黏；8 时 7.5° 即可满速。
- **未改**：`SENTRY_PITCH_TRAVEL_DEG=20`（电机输出轴 ±20° 调试窗口，连杆几何仍未知）、`SENTRY_PITCH_DIRECTION`、速度环 Kp0.6/Ki0.2、限矩 1.0Nm。
- **验证**：`ninja -C build` 0 error、本次文件 0 warning；`tests/run_chassis_tests.ps1` ALL PASS。固件 `build/basic_framework.elf`，SHA256 见本节末尾；FLASH 112000 B / RAM 96248 B。未烧录。
- **遗留**：①若仍"推杆无反应"，先查 `robot_state` 是否 READY（探针掉线未验证）②限矩 1.0Nm 相对步兵(C620 8000 码≈9.8A)偏小，抬连杆可能乏力，必要时提到 2~3Nm。

### DEV-20261122-45｜2026-11-22｜大 yaw 到位抖动软件补偿（降角环增益 + 滞回 + 力矩低通）

- **现象**：用户反馈大 yaw 每次回正到目标后持续抖动,怀疑死区或同步带虚位,要求先做软件补偿。
- **机理判断**：三条叠加。①`ANGLE_KP=70` 使 2° 误差即达 2.45 rad/s(顶满 90°/s 限速),速度环再乘 3.6 立刻满力——虚位下电机端一动负载没跟上、误差仍在,于是继续满力,形成高频抖;②到位判定原为单阈值(2°),轴在边界一进一出就反复"断力→满力";③力矩指令无任何滤波,虚位冲击直通电机。
- **改动**：①`SENTRY_BIG_YAW_ANGLE_KP` 70→**20**(主手段:2° 只要 0.7 rad/s);②新增 `SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG=0.5`,与既有 `TARGET_DEADBAND_DEG=2.0` 构成**施密特滞回**(进入 <0.5°、退出 >2.0°、中间保持);③新增 `SENTRY_BIG_YAW_TORQUE_LPF_RC_S=0.03`,MIT 力矩指令过一阶低通(`sentry_big_yaw_torque_nm` 记录低通后的实际下发值);④新增静止量 `band_latched/torque_lpf/last_ms`。
- **另外补的**：误差先折叠到 ±180°(`total_position` 是多圈,实测差过 371°,不折叠会朝一个方向甩整圈)后才进死区与角度环——这条同时是标定安全的前提。
- **验证**：`ninja -C build` 0 error、本次文件 0 warning;`tests/run_chassis_tests.ps1` ALL PASS。固件 FLASH 112204 B / RAM 73.44%,SHA256 见本节末。未烧录。
- **预期与判据**：抖消失且回正不拖沓 → 合适;仍抖 → Kp 继续降到 12~15;变拖沓 → 先确认不是被 `BIG_YAW_MAX_SPEED_DEG_S=90` 限住,再把 Kp 回调到 25~30。
- **机械侧保留意见**：若降增益后抖动变低频"呼吸"、且反向时出现 2~3° 空程,则为同步带**虚位**,软件只能掩盖不能消除;根治要预紧同步带或改双边张紧。

### DEV-20261122-46｜2026-11-22｜大 yaw 正前方重新标定

- **输入**：用户确认"现在的方向就是正前方"。
- **实测**：固件 `7e6143a5` 匹配状态下读 `total_position = −7.5381474 rad`、`position_raw = 13007`（按 (raw/65535)×25−12.5 反算 −7.538，两者自洽）。另注意该值与 21:30 那次读数**完全一致(7 位小数相同)**，说明这 5 分钟内轴已静止，标定窗口有效。
- **写入**：`SENTRY_BIG_YAW_FRONT_ALIGN_RAD` −1.06374 → **−1.254962**（= −7.5381474 + 2π，归一化到 ±π）。因控制侧误差折叠到 ±180°，本值加整圈等价，故归一化不影响行为，只便于对照（与旧值差 11°，即旧值采集时姿态与现在不同）。
- **标定后自检**：target − 实际 = 2π → 折叠后误差 **恒 0**，解锁瞬间不再甩动。可用 `sentry_big_yaw_in_deadband` 应立刻为 1 来验证。
- **验证**：`ninja -C build` 0 error；固件 `b94ef2ddbd4ef25469d7789cdb61d9d6d8e079ecc3e8ae05c87700f4f1aff3b7`，FLASH 112204 B / RAM 73.44%。未烧录。
- **提醒**：达妙 `total_position` 为上电累计，断电重启后绝对值可能差整圈，靠误差折叠吸收；若换同步带/拆电机必须重标。

### DEV-20261126-01｜2026-11-26｜大yaw标定与解锁回正

- AI：Codex（GPT-6）；trace_id=yaw-align-20261126。
- 用户授权：手摆正后观察sentry_big_yaw_position_rad=-1.52914；明确选择遥控解锁后自动回正，保留急停/失联保护。
- 配置：FRONT_ALIGN_RAD=-1.52914rad，方向=+1、传动比=1；不改编码器零点、不改PID增益与限幅。
- 逻辑：复用SentryBigYawTask串级PID与±π最短路径误差。修复未到位时target_eff扣除2deg导致0.5～2deg反向、2deg人为零误差，改为target_for_ctrl。
- 操作：右上+左中+全杆/拨轮回中解锁，切右中运行后自动回正。无有效命令、急停或失联时零力。
- 到位：0.5deg进入撤力，2deg退出重新纠偏；不是持续高精度位置保持，过冲与停稳待实体测试。
- 验证：cmake --fresh -S . -B build -G Ninja 后 cmake --build build -j 8，退出0；FLASH 112180B/RAM 94KB。4条newlib链接警告（_close/_lseek/_read/_write未实现），未报告零警告。
- 主机：tests/run_chassis_tests.ps1 全通过（控制权/失联/急停/解锁、看门狗、串级PID）；实际gimbal目标代码块抽取编译测试通过（双方向、±2圈、0.5/2deg滞回），产物tests/build/yaw_align_target_test.c。
- 独立只读Verifier复核通过。旧基线脚本路径不存在，无基线审计计数；工程副本无.git，未提交。
- 固件：build/basic_framework.elf。未烧录、未操作实车，用户台架验收尚未完成。

### DEV-20261126-02｜2026-11-26｜正前方实测确认与右摇杆平移方向

- AI：Codex（GPT-6）；trace_id=rc-direction-20261126。
- 用户确认DEV-01实车回正测试通过，正式标记-1.52914rad为机械正前方。
- 本次授权：右杆前推向该正前方移动，后拉退回，并互换左右逻辑；保持原烧录路径。
- 实现：application/cmd/robot_cmd.c RemoteControlSet中forward=-rocker_r1、right=-rocker_r_均改为正通道，平移四向相对上一版反向；保持归一化合速度、死区与回中、左杆转向、键鼠和控制权保护。
- config/robot_infantry_2026.h仅更新标定注释为用户实测通过，数值-1.52914f不变。
- tests/chassis_remote_test.c使用实发DBUS帧验证正负纵向、正负横向、斜向合速度、回中和带内不启动；tests/run_chassis_tests.ps1全通过（含急停/失联/解锁、看门狗、串级PID）。
- cmake --build build -j 8：退出0，FLASH112176B、RAM94KB；仍有newlib四条未实现_close/_lseek/_read/_write链接警告，第一方未见新增警告。
- 独立Verifier只读复核通过。不能从通道翻转测试单独宣称实体前后已验证，本轮实体方向待用户测试。
- build/basic_framework.elf SHA256=F00824F94C8537BED97AEE84B474185268CA502BE5FE6AB9848A2D88293DF7B8。
- 未修改.vscode烧录/调试路径，未代烧录。工程副本无.git，未提交；旧基线脚本缺失，无审计计数。


### DEV-20261126-03｜2026-11-26｜小陀螺大yaw世界稳像

- AI：Codex（GPT-6）；trace_id=world-hold-20261126；用户已确认进入锁当前航向、退出机械回正，C板仅随大yaw转动。
- gimbal.c订阅既有chassis_cmd，不改协议布局/生产者。两路命令有效且反馈健康才允许大yaw出力；底盘零力和断流不能被误当作正常退出。
- CHASSIS_ROTATE时用IMU YawTotalAngle(rad)和Gyro[2](rad/s)进入原串级PID；捕获世界目标仅发生在有效进入沿；普通模式仍使用编码器角/速度和-1.52914rad目标。模式切换清PID、前馈、低通、到位锁存与时间基准。
- 世界稳像期间不执行到位撤力，保持速度阻尼；90deg/s限幅是世界朝向纠偏速度，不是关节对底盘反转速度，无需为300deg/s小陀螺直接放大该值。
- 使用现有QEKF_INS.UpdateCount低32位检测解算推进，20ms停更或世界角/速度非有限时停机；运行中IMU/反馈异常锁存，需原流程解锁恢复。此计数代表解算任务活性，不等同传感器总线新样本证明。
- IMU_DIRECTION=+1为待实体确认的方向映射，IMU角/速度同乘此符号，再沿用关节到电机力矩映射。Gyro[2]按C板接近水平、绕Z轴转动使用；未更改IMU量程/滤波/解算。
- 测试新增tests/run_sentry_world_tests.py，从真实gimbal.c抽取状态、初始化与任务函数，链接真实SentryAxisUpdate/PID，外设IO桩；接入tests/run_chassis_tests.ps1。
- 原主机安全、右杆映射、串级PID测试全部通过；新增覆盖进入锁非零航向、1500拍300deg/s编码器多圈合成运动且世界不变、正负世界扰动、零角误差速度阻尼、退出短路回正、IMU停更/非有限、编码器非有限、反馈超时、底盘命令超时/零力与云台零力。
- 上述合成输入验证反馈域及控制分支，不构成实体动力学稳像证明。世界方向、稳定性、转速能力与长时IMU漂移仍待台架。
- ARM cmake --build build -j 8退出0：FLASH113284B/RAM96304B；4条既有newlib_close/_lseek/_read/_write链接警告。
- 固件仍为build/basic_framework.elf；未改.vscode烧录路径，未代烧录。工程副本无.git，未提交；旧基线审计脚本缺失，无法提供计数。

- 最终补充：计数跨任务采用volatile快照，seen_update防低位0回绕误判；补初始零更新、UINT32_MAX→0→1及停止重进目标回归均通过。独立Verifier复核确认两项边界问题已解除。
- 最终构建FLASH113304B/RAM96312B，退出0；固件SHA256=AFDDEDAD01EB2A6CC064F03B4D4A89E1EB6DA741EED012CB126F05C331831946。
- 测试范围：SentryBigYawTask实际函数与真实PID主机动态回归；SubRegister/GimbalTask订阅与命令喂狗接线仅静态核对，整机集成及实体稳像尚未测试。


### DEV-20261126-04｜2026-11-26｜大yaw速度PI跟速首轮

- AI：Codex（GPT-6）；trace_id=yaw-speed-pi-20261126。
- 用户定性反馈：小陀螺旋转时云台尝试回到世界目标方向，但跟不上快速旋转；未收到定量角误差、角速度或力矩采样。
- 按用户要求仅改config速度Kp3.6→4.5（+25%）、Ki0.6→0.8（约+33%），两者为待实测候选参数；普通回正和小陀螺世界稳像共享该速度PI。
- 不改机械正前方-1.52914rad、角度环、90deg/s纠偏速度上限、10Nm力矩上限、2Nm积分上限、0.012s/0.03s两级输出低通。
- 验证：cmake --build build -j 8退出0，FLASH113304B/RAM96312B；仍4条newlib桩函数链接警告。tests/run_chassis_tests.ps1退出0，包含原安全/右杆/串级控制器及真实大yaw任务回归。
- 独立只读Verifier确认参数接入及限幅保留，无静态阻断。软件测试不证明实体更快或稳定；若已力矩饱和或积分触顶，增PI不会提高对应输出上限。
- 固件路径不变：build/basic_framework.elf；SHA256=46AA204891235E0055C2094220870DA3617EDB61994A28914AB88DFB616E8EEA。未代烧录。基线审计脚本旧路径缺失，无审计计数；工程副本无.git，未提交。
- 下一步：用户烧录比较跟随误差/响应速度、新增抖动或过冲，观察sentry_big_yaw_torque_nm是否长时贴近±10Nm。


### DEV-20261126-05｜2026-11-26｜小陀螺纯世界角速度补偿

- AI：Codex（GPT-6）；trace_id=yaw-rate-only-20261126。
- 用户明确撤销进入小陀螺后追历史世界航向的需求：只需抵消转动，允许被拨偏后留在新朝向；退出小陀螺仍机械回正。
- gimbal.c小陀螺分支直接调用SentryAxisUpdateSpeed(world_speed,0,now_ms)，不经过角度环，不依靠不断重抓角度再做位置闭环。world_target_rad仅兼容调试显示当前世界角，不进入控制；target_rad跟随当前关节角。普通回正仍编码器角度+速度串级，目标-1.52914rad。
- sentry_control.c/.h新增纯速度接口，有限值、首拍历史建立、0<dt<=20ms检查、速度/力矩/积分限幅与异常复位保留；直接调用真实速度PID，角度历史和前馈不参与输出。
- 用户本机当前源码Kp=5.2/Ki=1.2，较上一轮交付4.5/0.8有变化，本轮保留现值并告知，仅修正其过期注释；不擅自覆盖调参。其余限幅/低通/急停/失联/模式切换保护保持。
- 测试：从真实大yaw任务抽取并链接真实PID，新增纯角度±80deg/多圈改变且速度0时无追赶；正负速度阻尼、退出短路回正、IMU停更与低位计数回绕、反馈/命令异常测试通过。控制器追加纯速度限矩/积分/重复时刻/长周期/非有限输入测试通过。原host全通过。
- ARM cmake --build build -j 8退出0，FLASH113576B/RAM96312B，4条既有newlib链接警告。软件测试不构成实体补偿速度及稳定性证明。
- 尚未烧录；原build/basic_framework.elf及.vscode烧录路径保持。基线审计旧脚本缺失，无计数；工程副本无.git，无提交。

- 独立Verifier复核通过：无显式历史航向追踪路径，无新增模式/时序/保护阻断。
- 补充速度脉冲→速度归零→小反向速度扰动回归，通过；保留原PI积分特性。速度PI仍有历史速度误差积分Iout，不承诺无任何残余记忆/回摆，仅确认不再用历史航向计算速度目标；未擅自清I或改Ki1.2。
- 最终主机全回归再次通过，固件源码未再改变，ELF沿用本轮已验证版本；实体待用户测试。


### DEV-20261126-06｜2026-11-26｜小陀螺间歇偏移只读采样

- AI：Codex（GPT-6）。

## 2026-11-26 纯速度小陀螺间歇偏移诊断（yaw-drift-read-20261126）
- 用户描述稳一下再反向偏移、重复出现，授权只读MCU/RTT。MCP因当前cwd父目录匹配失败，EmberProbe技能脚本--workspace指定工程成功恢复桥接。
- config.get确认CMSIS-DAP/stm32f4x与原ELF；variables.read显示firmwareConsistency=match，SHA256=d47e6b832d7f63e43f5d7f552c982cf1d4f8ca1363dbe62230e00231e23d72b8，实时Kp5.2/Ki1.2。此一致性来自扩展烧录记录，并非本轮重新读取Flash逐字节校验。
- RTT状态stopped，本轮未启动RTT；实际证据来自非侵入式variables采样，无暂停/复位/写RAM/烧录。
- 用户手动开启小陀螺后记录300样本19.590s，其中211样本spin=1共13.666s；实际约15Hz，多变量非原子读取，不能据此精确判定高频振荡相位。
- 有效旋转末7.962s：IMU角速度均值+9.344deg/s、标准差25.003deg/s；世界角变化+90.903deg。
- 同窗口速度积分平均-1.9893Nm，范围约[-2,-1.9513]，积分额度2Nm基本用满；下发力矩均值-2.9887Nm、反馈均值-3.0692Nm，旋转全段最大下发绝对值7.9186Nm，未达到10Nm总限矩。
- 旋转全段速度目标0，control_active=1、imu_online=1、rearm_required=0。支持“持续补偿积分额度接近用尽，同时角速度/力矩明显波动”；不能仅归因重量，也不能仅凭此证明PI增益或静摩擦单一根因。
- 第二次120样本7.613s已spin=0，不作为旋转方向或运行稳态证据。结束单次读再次确认控制/小陀螺均为0，参数仍5.2/1.2。
- 原始证据：exports/yaw_rate_drift_20261126.ndjson，第二段exports/yaw_rate_direction_20261126.ndjson，统计exports/yaw_rate_drift_20261126.summary.json。
- 本轮不改代码/参数。下一步应分离积分与环路波动，不能把继续加Ki作为增加积分上限的手段；如进行参数对照，须明确试验条件并保持原固件回退依据。

- 独立Verifier复算确认上述统计。末段121点中91点Iout<=-1.98Nm；采样只能确认所见点未达10Nm，不能排除点间瞬时饱和。
- 多变量非原子、实测采样约15Hz，不据Err与Gyro同一行值推断精确相位或频率。优先建议保留PI/积分/低通，底盘半速单变量对照，再考虑同速反转；本轮不执行参数修改。


### DEV-20261126-07｜2026-11-26｜切换 hibhvkgvck/shaobin 基线并移植 NV v1 导航与共享 USB CDC 自瞄

- **触发与基线**：用户指定同步 `hibhvkgvck/shaobin`（HEAD=`b489e76793f7c7977c868f9a7a340ef4c87e8cbc`）作为 `cod_ws/sentry_firmware` 固件底座，完整保留实车已标定参数与控制逻辑（600×560 mm 底盘几何、大 yaw 机械正前方 `-1.52914 rad`、小陀螺纯世界角速度 PI 补偿 `Kp=5.2, Ki=1.2`、小 yaw 中心 `27` / `±90°` 半圆限位、DM pitch、CAN2 发射），并在其上移植本地已验证的 `NV v1` 32 字节 USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`。原工程备份在 `_ops/backups/pre_shaobin_base_20261126/`。
- **架构与代码改动**：
  - 新增 `modules/navigation/{nav_rtos,nav_protocol,rmcv_pose,ax650_usb}.{h,c}` 与 `application/autonomy/autonomy.{h,c}`；避免与原 `modules/master_machine/ax650_pose.{h,c}` 符号冲突。
  - `bsp/usb/bsp_usb.{h,c}` 新增 `USBIsTransmitting()` / `USBTryTransmit()`；`Src/usbd_cdc_if.c` 在 `USBD_CDC_ReceivePacket` 前先拷贝 RX 缓冲防并发覆盖。
  - `modules/motor/DMmotor/dmmotor.{h,c}` 在 CAN Rx ISR 中记录 `rx_sample_us`（FreeRTOS `xTaskGetTickCountFromISR()*1000`）；`application/gimbal/gimbal.{h,c}` 暴露 `GimbalGetBigYawFeedback()` 原子快照相对 `-1.52914 rad` 的大 yaw 角度与采样时间戳。
  - `modules/imu/ins_task.c` 在四元数更新后喂入 `ax650_usb_update_quaternion()`；`config/robot_infantry_2026.h` 启用 `AX650_USB_ENABLE=1`、关闭 `AX650_UART1_ENABLE=0`，默认安全关闭 `SENTRY_AUTONOMY_ENABLE=0`（`autonomy_get_command` 硬清零并返回 0），配置保守导航限速 `0.35 m/s` / `0.50 rad/s` / `300 ms` 看门狗。
  - `application/robot_def.h` 枚举新增 `CHASSIS_NAVIGATION`（保持 `sizeof(Chassis_Ctrl_Cmd_s)==21` 静态断言不变）；`application/cmd/robot_cmd.c`、`application/chassis/chassis.c`、`application/robot.c`、`modules/referee/referee_task.c` 接入导航分支与 UI 字符分支。
  - 新增 `CMakePresets.json`、`build.ps1`（自动定位 `cmake`/`ninja`/`arm-none-eabi-gcc` 并补入 PATH）、`COLCON_IGNORE` 及 `tests/navigation/`。
- **验证结果（仅本地编译与主机测试，严禁小电脑编译）**：
  - 本地 Windows ARM GCC 14.2.1（xPack v14.2.1-1.1，ASCII 目录联接 `C:\sentry_fw`）：`build.ps1 sentry -Current` 140/140 全量清洁编译通过，第一方 0 error、0 warning（仅 3 条上游 `#pragma message` 与 4 条 `libc_nano` nosys 链接警告）。
  - 内存占用：`FLASH 116,324 B / 1 MB (11.09%)`、`RAM 97,352 B / 128 KB (74.27%)`、`CCMRAM 0 B / 64 KB (0.00%)`（`text=99120, data=17196, bss=80160`）。
  - 产物：`firmware/sentry.elf` 与 `firmware/current.elf`（均 `1,263,812 B`，`SHA256=A2B36454BC67EF9E59262B8C8877C09233BF08D774F268AE4E5FC7B635B17C8F`）。
  - WSL 本地 4 组导航+自瞄主机测试（`feedback-only`/`drive-enabled` × `-O0`/`-Ofast -flto`）全部通过，`NV`/`0x0503`/`0x1021` C/Python 黄金字节一致；13 文件语法检查全通过。
  - WSL 本地原 `shaobin` 4 套主机回归（`chassis_remote_test`、`cmd_watchdog_test`、`sentry_control_test`、`run_sentry_world_tests.py`）全部通过。
- **未验证边界**：本次未烧录 MCU，未做实体 USB `/dev/ttyACM0` 联调、未验证实车大 yaw `-1.52914 rad` → ROS `0 rad` 零位符号与实车导航运动。下一步由用户本地烧录后架空四轮，以 `enable_drive:=false` 验证时间同步、大 yaw 反馈与 `0x1021` 姿态流。
