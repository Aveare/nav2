# 跃鹿框架集成规范

## 0. 与代码地图的分工

本文件只写**集成时必须遵守的规范条文**（要遵守什么）；架构描述、文件/函数定位、调用链与上游已知陷阱的唯一事实源是 [跃鹿代码地图](code-map.md)（在哪里、是什么）。两处不得重复维护同一张表：新增定位信息去 code-map，新增约束去本文。

## 1. 适用基线与阅读入口

依据 [basic_framework 固定提交](https://github.com/HNUYueLuRM/basic_framework/tree/1a136eb1ed2f101110348d5a5cd182272eada38e) 的 `.Doc`、README 与源码整理；工程维护时先确认其实际派生版本，派生差异记录在工程主记录的 DEC 台账。来源身份见 [来源声明](provenance.md)，原指南保存在随包 `.Doc`。

开发前阅读 `README.md`、`.Doc/架构介绍与开发指南.md`、`.Doc/必须做&禁止做.md` 和相关 APP/Module/BSP 文档。工具链/移植任务查 `.Doc/VSCode+Ozone使用方法.md`；故障定位和调参分别查 `.Doc/如何定位bug.md`、`.Doc/合理地进行PID参数整定.md`。历史环境配置与技术建议须结合当前工具、设备及实现复核。

## 2. 分层修改边界

分层职责与各层符号见 [代码地图 §4](code-map.md#4-关键符号索引按层)。集成时必须遵守：

- Module 按 APP 需要实例化并注册 BSP 服务；通过实例、回调和父实例指针向上通知，底层不包含 APP 头文件。配置中传入 `hcan` 等句柄符合基线方式，实际 HAL 发帧仍由对应底层承担；逻辑层禁止直接持有硬件句柄（见 [代码规范](coding-contract.md) 分层纪律）。
- APP 按机械机构组织；命令生成与执行分离。单板沿用消息中心，多板核对板型宏、板间职责及两端协议；通信与运动学分别封装。
- 模板参数适配优先保留对象关系和现有文件。不要另建平行APP、包装模块或全车调度替代原入口；不在APP直接操作寄存器、填CAN发送缓冲或绕过原模块。具体边界见[代码规范](coding-contract.md#2-模板内适配优先)。

## 3. 初始化与任务生命周期纪律

启动顺序、任务频率与各任务职责见 [代码地图 §3](code-map.md#3-调用链速查)。集成纪律：

- `application/robot_task.h` 含任务函数实现和句柄定义，仅由 `robot.c` 包含；引入多个翻译单元会产生重复定义。
- 任务启停一律使用 `robot_task.h` 的启停宏（如 `TASK_UI_ENABLE`），禁止用注释启停；不得为调试或刷新反馈自动把控制任务迁入电机任务、再建发送任务或增加任务钩子；任务调整只有在已授权功能确有必要时才进行。
- 该提交 `robot.c` 的 `GimbalInit/ShootInit/GimbalTask/ShootTask` 默认被注释；新增功能需要核对初始化、依赖和实际执行路径，不能仅凭文件存在或取消注释判定已完成。`INS_Init()`、`BuzzerInit()` 原有任务入口初始化保留，除非任务确需调整。
- 关中断初始化不得依赖 SysTick/HAL 延时或等待中断完成。`osDelay()` 不是实际控制周期保证；控制计算应核对 dt、耗时和调度。ISR 与任务按上下文选择同步机制，明确共享数据所有者。

## 4. 电机接口合同

路径与符号见 [代码地图 §4](code-map.md#4-关键符号索引按层)；分组发送规则见代码地图 §3 电机控制流。集成要求：

| 接口/对象 | 集成要求 |
| --- | --- |
| `DJIMotorInit(Motor_Init_Config_s *)` | 沿用实际 API；文档期望的 Register 命名不构成改名要求 |
| `DJIMotorSetRef()` | 单位由外环和反馈决定，逐段核对目标换算 |
| `measure.speed_aps` | 经 `RPM_2_ANGLE_PER_SEC` 转换的度/秒反馈；明确电机轴/机构轴与减速比 |
| `DJIMotorChangeFeed()` | 外部反馈指针在实例配置中提供，保证有效期、单位与对应轴 |
| `DJIMotorStop/Enable()` | 停止与零速度闭环不同；链路故障另核对指令可达性 |
| `MotorControlTask()` | APP 设置目标，避免其他任务重复计算/发送同一组 |

设备编号和组帧映射按 [硬件配置规范](hardware-profiles.md) 核对，不将配置字段名直接解释为协议帧 ID。

## 5. 消息中心与 CAN 使用约束

结构、常量与队列语义见 [代码地图 §4](code-map.md#4-关键符号索引按层)。使用约束：

| 对象 | 使用约束 |
| --- | --- |
| `PubRegister/SubRegister` | 同话题长度一致；调用前验证结构长度适配，避免截断 |
| `PubPushMessage/SubGetMessage` | 适合最新状态；不保证事件不丢或跨上下文线程安全 |
| `CheckName()` | 话题名最长 31 字符 |
| `PubPushMessage()` 返回值 | 固定返回 1，不能按旧注释解释为实际接收者数量 |
| `MAX_TOPIC_COUNT` | 宏存在，不能据此认定实现已严格检查容量 |
| `CANRegister()` | 核对 ID、过滤、波特率、资源归属 |
| `CANTransmit(CANInstance *, uint8_t length)` | 填写实例发送缓冲与合法长度；核对中断配置、拥堵和队列满时行为 |

CAN 实现位于 `bsp/can/`；此提交采用队列发送，接口与旧版忙等方式不同，移植须同步调用者及发送相关中断配置。实际行为以 `.h/.c` 为准，不能只根据旧模块说明调用。

## 6. 量纲与构建

基线 `robot_def.h` 的底盘几何以 mm 表示，DJI 速度反馈为 degree/s。底盘命令、运动学到最终参考值需逐段核对；改变单位时同步生产者、消费者、协议和测试，不擅自混用个人派生版本的单位合同。

代码和注释按 [代码规范](coding-contract.md) 保持原风格，公开接口在 `.h`、必要实现说明在 `.c`，新增模块按原指南补文档。

| 构建项 | 固定提交配置与检查点 |
| --- | --- |
| 编译/ABI | C11、`arm-none-eabi-gcc`、Cortex-M4、硬浮点配置 |
| 必要文件 | `STM32F407IGHx_FLASH.ld`、启动汇编、SEGGER RTT 汇编 |
| DSP | `Middlewares/ST/ARM/DSP/Lib/libCMSISDSP.a`；移植核对架构与 ABI |
| 构建路径 | Make 有 `-Werror`，CMake 有 `-Wall`；宏、文件集合及选项分别检查 |

新增源文件或改变 MCU 后检查实际构建系统、链接和依赖。原文的 DWT 宽度、HardFault 临时处理等历史表述应与芯片资料和当前实现交叉核实，不直接作为长期修复依据。
