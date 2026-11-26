# 域B 评审报告:架构与核心子系统技术设计

> 评审对象:`.Doc/plan/自主研发框架升级总方案.md`(v1.4 工作树版,358 行)§1 T0 判据、§2 总体架构、§3 核心子系统、附A、附B。
> 现状基准:**本地工作树**(未用 git HEAD)。全部代码结论来自本次实际读到的文件与行号。
> 评审人:arch-reviewer(2026-11-22)。只读审查,本文件之外未改任何仓库文件。

---

## 一、总体判断

1. 方向正确("原地绞杀 + 逐批零行为 + 对拍"),但**三个前提性假设不成立**:framework "零硬件依赖"、§3.5 "全员心跳才喂狗"、§3.8 "不变量清单已完备"。
2. §2.2 单向依赖的真实违约点不在 bsp(found 0 处),而在 `modules/referee/referee_task.c:14-15` 反向 include `heat_guard.h`/`robot_cmd.h`;T01 的"现状"列把它写错了。
3. §3.2 写"12 函数"却列了 **15** 个;同时缺 `kernel_start`、`kernel_is_in_isr`、`kernel_current_task` 三个**被方案自身其它条款引用**的原语(§3.4 单写者检测就要第三个)。
4. §3.5 的喂狗策略会与 `referee_task.c:72-84`(无裁判时 `while(robot_id==0) osDelay(1000)`,不连接则永不返回)以及 `:73 vTaskDelete(NULL)` 直接冲突:**默认 `TASK_UI_ENABLE=1` 时,不接裁判系统就是 2s 复位循环**。这是本方案唯一会直接毁掉实车可用性的缺陷。
5. §3.3/§3.8 对 bsp_can 的"语义良好"判断偏乐观:CAN 错误路径为 0(无 `HAL_CAN_ErrorCallback`、无 SCE、滤波器耗尽静默、`HAL_CAN_ConfigFilter` 返回值丢弃),这些恰好是 nm/size 看不见的那类不变量。
6. §3.8 只有 6 条,至少漏 8 条一等不变量(§二 B 段列出,含每条的行号证据)。
7. §3.9 四类账本**不互斥、不穷尽**:容量宏、派生开关、mode 宏、空定义板型宏、构建系统注入宏各自落在分类之外;且"B 类命名 `<MODULE>_<ENABLE>`"与"G1' 既有宏不改名"直接矛盾。
8. §1 中 T03/T07/T11 是**假验收**(不可证伪),T05/T06 缺边界定义;G5 在 `-flto` 下按符号比尺寸本身不可靠(`CMakeLists.txt:36`)。
9. 附B 清单不够开工:最硬的一条是 **CMake 的 GLOB 不含 `framework/`**(`CMakeLists.txt:72-79`),按清单新建的框架文件根本不会进编译,"0E0W" 是空验;且 `refactor_gate.py:37` 把 ELF 名硬编码为 `basic_framework.elf`。
10. 建议:**方案结构不动,但 B1 前必须补三件事**——§3.5 喂狗白名单/存活契约、§3.8 补全不变量、§3.9 改四类为五类并先把分类器边界写死;否则 B3/B4/B7 会在"零行为"名义下把行为改掉。

---

## 二、问题清单

> 严重度:阻断 = 不修不能开工/会毁车;高 = 会引入隐性行为变化或使验收失效;中 = 会返工;低 = 表达/口径。

### A 段 分层与依赖(§2.2 / §2.4 / T01 / T05)

**A1 [阻断] framework "零硬件依赖"不成立,§2.4 的"DWT 留 framework"是错误归类**

- 方案位置:§2.2(85-90)、§2.4 表格第 108 行"纯算法(DWT 时间)留 framework"。
- 现状证据(逐个"捅破点"):
  - `bsp/dwt/bsp_dwt.c:46-52` `CoreDebug->DEMCR`、`DWT->CYCCNT/DWT->CTRL`;`:64,:77,:86,:129,:132` 全部直读 `DWT->CYCCNT` → **DWT 是 Cortex-M 核内调试外设(0xE0001000),不是算法**;POSIX/裸机后端无此寄存器。
  - `bsp/dwt/bsp_dwt.c:12` `#include "cmsis_os.h"`(虽未使用)→ 把 FreeRTOS 头拖进所谓"零依赖"层。
  - `bsp/can/bsp_can.c:82-83,97,103-104,120,127` `__get_BASEPRI/__set_BASEPRI`;`application/robot.c:90,153` `__disable_irq/__enable_irq` → CMSIS intrinsic 属于 kernel/port,不是 framework。
  - `modules/telemetry/telemetry.h:34` `__attribute__((section(".ccmbss")))`、`Src/freertos.c:52` `.ccmbss+aligned(8)` → 链接脚本契约,属 port。
  - `bsp/fault/bsp_fault.c:82` `__builtin_snprintf`;`bsp/log/bsp_log.c:175,177` `sprintf`;`:172-173` `abs` → 工具链/newlib 依赖,framework 不能用。
  - `bsp/log/bsp_log.c:18` `extern SEGGER_RTT_CB _SEGGER_RTT`、`:33` `aUp[BUFFER_INDEX]` → J-Link 专有件;方案已承认"SEGGER 归 ports",但 §3.5(161)又说"日志 … 全部随框架",自相矛盾。
  - `bsp/pwm/bsp_pwm.c:106,114` `HAL_RCC_GetPCLK1Freq()/APBPrescTable[RCC->CFGR]`、`bsp/pwm/bsp_pwm.c:78,88` `__HAL_TIM_SetAutoreload/__HAL_TIM_SetCompare` → 定时器时钟树是 port。
- 为什么是问题:"零硬件依赖"若按字面执行,上述每一个都要重写;若按"零 include 依赖"执行,则目前只有 include 图变干净、**行为契约仍在 framework 里硬编 168MHz/0x50/CMSIS 优先级**。验收(T01/T05 的 include 扫描)看不见这个差别,等于用假指标换真风险。
- 修改建议:把结论改写为**"framework 零平台 include 依赖,但必须显式消费 3 类 port 供给"**,并在 §2.2 增一张"platform contract"表:
  1. `kernel`:临界区/中断屏蔽、tick 与高分辨率时间(DWT 由 `ports/stm32f4` 实现 `kernel_time_us`)——**DWT 从 §2.4 第一行移到 ports**;
  2. `diag/storage`:遥测环存储由 port 提供 `xr_telemetry_storage_t{void*,size_t,align}` 注入(见 D2);
  3. `log sink`:RTT 是 port 的 sink,framework 只保留"格式字符串禁用 %f"的**纯解析器**(`FmtHasFloatSpec` 已是纯函数,可原样上移)。
  同时把 `DWT_Init(168)`(`bsp/bsp_init.h:26`)改为 `DWT_Init(SystemCoreClock/1000000)` 或在 port 里断言,否则换 A 板 180MHz 时时间基静默错 7%。

**A2 [高] T01 现状描述与事实相反,且 include 扫描必须"传递闭包"才有效**

- 方案位置:§1 T01(34)"bsp 反向 include 少量存在";§2.2(89)。
- 现状证据:
  - bsp → modules/application:**0 处**(全 `bsp/` 目录 grep 无 `#include "robot*/chassis/gimbal/...`)。
  - 真实违约:`modules/referee/referee_task.c:14` `#include "heat_guard.h"`、`:15` `#include "robot_cmd.h"` → components→apps 反向。
  - 传递闭包证据:`application/robot_task.h:17` include `ins_task.h` → `modules/imu/BMI088driver.h:12` `#include "main.h"`;`modules/remote/remote_control.h:16` `#include "main.h"`;`modules/referee/rm_referee.h:20`/`referee_custom.h:13` `#include "FreeRTOS.h"`。**单跳扫描会全绿,实际边界早已破**。
- 为什么是问题:CI 若按单跳实现,T05 的"kernel 外禁 FreeRTOS.h"在 `cmsis_os.h`/`task.h` 通过 3 层间接到达 apps 时不会报。
- 修改建议:扫描器按 `gcc -M` 或自写递归 `#include` 闭包算"每个 TU 的传递依赖集合",规则写成:闭包中含 `FreeRTOS.h|task.h|cmsis_os.h` 的 TU 必须位于 `framework/kernel/` 或 `ports/`;闭包含 `main.h|stm32f4xx*.h|cmsis_os.h` 的 TU 必须在 `ports/stm32f4/`;并显式登记 `referee_task.c:14-15` 为待消除例外(B6 拆 UI 类型头时修)。

**A3 [高] "components 不 include apps"与 B5/B6"逻辑零改动"不可同时成立(dji_motor 硬编总线句柄)**

- 方案位置:§2.2(89)、§2.4(113)、B5(256)"逻辑零改动,注册改接口"、B6(257)。
- 现状证据:`modules/motor/DJImotor/dji_motor.c:60-67` `sender_assignment[6]` 用 `&hcan1/&hcan2` 静态初始化;`:95,:100,:126,:131` 按 `config->can_handle == &hcan1` 选择发送组;`:213`;`:316-318` 用 `can_handle == &hcan1` 判总线级零输出;`modules/can_comm/can_comm.c:102` 同理。
- 为什么是问题:§3.3 的实例头是 `{type,id,...}` 无句柄,业务必须改用 `bus_id` 比较 → `dji_motor.c` 至少 8 处逻辑要改,直接与 B5"零改动"和 G5"逐符号尺寸"冲突;更严重的是没人会在评审时意识到这是**行为批**。
- 修改建议:B5 明确划出"接口改写清单"(逐文件逐行),并把 `can_handle==&hcan1` → `bus_id==1` 这类替换列为**行为变化声明项**(需 G8a 覆盖 `DJIMotorControl` 的出帧内容),不能再用 G5 兜底。同时把 `sender_assignment` 伪实例改成 `bus_can_tx()` 显式传入 `tx_id`(它现在是绕过注册表的"地下实例",见 B5)。

**A4 [中] apps 仍直接 include CubeMX 生成头以取句柄**

- 方案位置:§2.2、§3.6(165)"句柄注入仍走注册制"。
- 现状证据:`application/chassis/chassis.c:27` `#include "usart.h"`;`application/gimbal/gimbal.c:26` 同;`application/robot_task.h:12` `#include "main.h"`;`chassis.c:94,154`、`gimbal.c:139,187`、`shoot.c:79,120`、`robot_cmd.c:126` 直接写 `&hcan1/&hcan2`。
- 为什么是问题:与 §2.2"apps 不触 HAL"冲突;profile 化(B8)时句柄必须由 port 以"逻辑总线 id"暴露,否则 `apps/infantry_skeleton/profile.h` 里仍会出现 `&hcan1`。
- 修改建议:profile 只写 `{BUS_CAN_CHASSIS, 0x1ff}` 之类的逻辑 id;`ports/stm32f4` 维护 `bus_id → CAN_HandleTypeDef*` 映射表(唯一 `&hcan?` 出现处),并在 T01 扫描里把 `#include "(usart|can|tim|spi|i2c|gpio)\.h"` 限制在 ports/。

**A5 [低] `bsp/bsp_init.h:12` 自包含 `#include "bsp_init.h"`**

- 现状:自 include 自身(无害但说明该头被当作实现文件用,`BSPInit()` 定义在 .h 里)。
- 建议:B4 拆分时消灭"头文件写实现"的写法(`bsp_init.h:24-28`),否则 components 引用会引发多处定义风险。

---

### K 段 kernel(§3.2 / T05)

**K1 [高] "12 函数最小集"数字与枚举不符,且缺 3 个被本方案自身引用的原语**

- 方案位置:§3.2(130-136)。
- 现状证据(逐条数):`create/destroy`(2)+`delay`(1)+`mutex(3)`+`queue(4)`+`time_ms/us`(2)+`enter/exit_critical`(2)+`yield`(1)= **15**,不是 12。
- 缺失原语与后果:
  | 缺失 | 谁需要它 | 缺了会怎样 |
  |---|---|---|
  | `kernel_start()`(调度器启动) | `Src/main.c:131` `osKernelStart()` 目前在 CubeMX 生成区 | posix/baremetal 后端无法接管启动;framework 仍被生成代码绑定,与 T05 冲突 |
  | `kernel_current_task()`/`kernel_task_id()` | §3.4(151)"首发布记 owner task" | 单写者检测无法实现,该条只能降级为人工约定 |
  | `kernel_is_in_isr()` | §3.7(169-178)、§3.4(150)、§3.1(ISR 投递) | ISR 契约无法在代码里断言,只能靠文档;`err_report` 无法自动降级 |
  | `kernel_time_us64()` | `DWT_GetDeltaT64`(`bsp_dwt.c:73`)、遥测 `tick_ms`(uint32 会 49.7 天溢出) | 高分辨率时间退化为 ms |
  | `kernel_stats_stack()` | §3.5 心跳监督需要栈水位才能提前发现溢出 | 仅靠 `configCHECK_FOR_STACK_OVERFLOW=2`(`Inc/FreeRTOSConfig.h:85`)事后复位 |
- 现状原语普查(全仓 first-party,已 grep):`osThreadDef/osThreadCreate` 6 处(`application/robot_task.c:35,38,41,44,49`、`Src/freertos.c:110`)、`osThreadTerminate` 1(`Src/freertos.c:132`)、`vTaskDelete` 1(`modules/referee/referee_task.c:73`)、`osDelay` 9(`robot_task.c:73,90,109,127,141`、`rm_referee.c:195`、`referee_vt.c:73,79`、`referee_task.c:83`)、`osSignalWait` 1(`bsp/bsp_tools.c:41`,**死代码**)。**队列/互斥量/软件定时器/事件组/任务通知:0 使用。**
- 结论与建议:§3.2 应写"**首版 15 个 + 3 个契约原语(is_in_isr/current_task/start)**";`mutex(3)/queue(4)` **全部推迟到有真实调用者的批次**(见 §四),否则只是把 CMSIS-OS v1 换成自己写的、同样没人用的第二个封装层。

**K2 [高] 弃 CMSIS-OS v1 的迁移成本被低估:优先级数值是隐式行为契约**

- 方案位置:§3.2(134)"优先级 5 档 … 后端映射表写死并注释"。
- 现状证据:`Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c:102-111`: `fpriority = tskIDLE + (priority - osPriorityIdle)`,`osPriorityIdle=-3`(cmsis_os.h 枚举)→ 今天 `osPriorityNormal(0)=FreeRTOS 3`、`osPriorityAboveNormal(1)=FreeRTOS 4`;`configMAX_PRIORITIES=7`(`Inc/FreeRTOSConfig.h:65`)。
- 为什么是问题:"5 档 RT/HIGH/NORMAL/LOW/IDLE"若按 `NORMAL = configMAX_PRIORITIES/2` 之类映射,INS 任务会从 4 掉到 3 与 MOTOR/ROBOT 同级 → `robot_task.c:29` 注释所依赖的"INS 高于 Normal"消失,1kHz 姿态解算被同级时间片抢占 = 行为变化,而 G5/G8a 都测不出来(G8b 要等 B8)。
- 修改建议:把映射**写成数字常量并加静态断言**:`#define XR_PRIO_NORMAL 3`、`XR_PRIO_HIGH 4`、`XR_PRIO_RT 5`、`XR_PRIO_LOW 2`、`XR_PRIO_IDLE 0`,并 `_Static_assert(XR_PRIO_HIGH==4 && XR_PRIO_NORMAL==3, "...")`;同时在 B8 验收里加一条"任务优先级表 = `{INS:4, MOTOR:3, DAEMON:3, ROBOT:3, UI:3}` 逐项等于迁移前"。
- 附带成本:`osThreadDef` 是**编译期静态属性表**(宏展开为文件作用域静态对象),无法放入 profile 循环;`bsp/bsp_tools.c:55-61` 手填 `osThreadDef_t` 证明运行期可行,但该文件本身是死代码(见 F1)。故 kernel 首版应直接走 `xTaskCreate`(栈从 `ucHeap` 即 CCM 分配,`Src/freertos.c:52`),**不引入 `xTaskCreateStatic`**,否则要同时打开静态分配路径并补 `vApplicationGetTimerTaskMemory`(`configUSE_TIMERS` 当前未定义)。

**K3 [中] `configASSERT` 是"运行期 while(1) 挂死"的最大单点,T02 的盘点漏掉它**

- 方案位置:§3.1(126)"其余现存 11+ 处裸 while(1) 全部改 err_report";T02(35)。
- 现状证据:`Inc/FreeRTOSConfig.h:135` `#define configASSERT(x) if((x)==0){taskDISABLE_INTERRUPTS(); for(;;);}`;任一 CMSIS/FreeRTOS API 误用(ISR 里调队列、`vPortExitCritical` 无 enter、`port.c:399` 的人为断言)都会**关中断死循环**,只能等 IWDG 2s 复位且无任何死因记录。
- 为什么是问题:新框架的每个 `kernel_*` API 都会新增触发面(例如在 ISR 里 `msg_publish` 若不 `configASSERT` 就会静默破坏订阅者链表)。T02 用 grep 扫 `while(1)` 永远扫不到宏内的 `for(;;)`。
- 修改建议:B1 就把 `configASSERT` 改为 `err_report(XR_E_KERNEL_ASSERT, XR_POLICY_FAULT_RECORD, …)`(`FaultRecordDetail` 已具备:写 BKP + `NVIC_SystemReset`,`bsp_fault.c:35-63`),蜂鸣码 4 即可现场判读;check_macros 增加一条"`configASSERT` 不得含 `for(;;)`"。

**K4 [低] `Src/freertos.c` 的 `defaultTask` 是无意义负担,但它在 CubeMX 生成区**

- 现状:`Src/freertos.c:110-111` 建 128 word 任务,`:127-133` 唯一动作是 `osThreadTerminate(NULL)`;`:128` 调 `MX_USB_DEVICE_Init()`。
- 影响:kernel 抽象若只接管 `robot_task.c`,`defaultTask`(及其 512B 栈、`.ioc` 再生成风险)会留在体系外;T05"framework 不直接引用 RTOS API"会被 `freertos.c` 噎住。
- 建议:在 §3.2 明确 `freertos.c` 的处置——要么 B8 让 `MX_FREERTOS_Init()` 变空(需用户批准 USER CODE 区外改动),要么把 USB 初始化移到 `ports/stm32f4` 的一次性 init,并在 `.ioc` 登记"再也不生成 task"。**这一项必须写进附B,否则 B1 之后没人会想起它。**

---

### B 段 bus 与一等不变量(§3.3 / §3.8)

**B1 [高] "TX 邮箱 ISR 续传定死"结论正确,但漏掉 4 条同族语义(丢帧/重试/错误计数/晚绑定)**

- 方案位置:§3.3(142)、§3.8 全段。
- 现状证据与遗漏:
  1. **满丢最老无计数**:`bsp_can.c:85-88` 丢弃最老帧,但不累加任何计数器(对比 USART 有 `err_cnt`,`bsp_usart.c:152,252`);`bsp_can.c:77-80` 的 `LOGERROR` 在临界区**外**且每次入队都执行 → 满载时 1kHz×6 组(`dji_motor.c:385-391`)可打出 6000 行/s,直接把 RTT 1024B 冲爆(§3.9 的日志预算表对此零覆盖)。→ 不变量须写:"丢最老时 `tx_drop_cnt++` 且日志限流 1/100ms"。
  2. **发送失败语义**:`bsp_can.c:119-122` `HAL_CAN_AddTxMessage != HAL_OK` 时**直接 return,帧留在队列**,等下一个邮箱空中断;若总线 Bus-Off,邮箱永远不空 → 队列填满 → 静默丢最老。无 Bus-Off 检测(全仓无 `HAL_CAN_ErrorCallback`,grep 仅 `HAL_UART_ErrorCallback` 命中;`Src/can.c:166` 打开了 `CAN2_SCE_IRQn` 但 `bsp_can.c:62-72` 未 `ActivateNotification(CAN_IT_ERROR/CAN_IT_BOF)`)。
  3. **`tx_id` 晚绑定**:`CAN_FIFO_Data_s.can_instance` 存的是**实例指针**(`bsp_can.h:52`),`bsp_can.c:113` 在出队时才读 `->tx_id`。今天 DJI 靠注册期改写 `sender_assignment[group].tx_id`(`dji_motor.c:139-147`)依赖此行为;迁移若改成"入队时拷贝帧头",GM6020 电流/电压模式切换会在切换瞬间发错 ID。**这是 nm/size/对拍都看不见的契约。**
  4. **第三条总线静默丢帧**:`bsp_can.c:171-177` 只处理 `&hcan1/&hcan2`,else 分支不存在;`bsp_can.h:18` `DEVICE_CAN_CNT 2` 与实现不一致(数量宏是装饰)。
- 修改建议:§3.8 增加 4 条(丢帧计数、邮箱失败重试窗口、`tx_id` 晚绑定、总线数上限=2 的显式拒绝),并把 `err_cnt` 字段(`bsp_can.h:24-36` **当前没有**)纳入 §3.3 的统一头 → 这是新增字段,须在 B4a 声明为"新增可观测量"(不是行为变化,但要进 G5 的 data 预算解释)。

**B2 [阻断] FIFO 归属"显式配置与现顺序逐一等价"——验证手段不存在,且方案的现状描述不准**

- 方案位置:§3.3(141,143)、§3.8 第 1 条(182)。
- 现状证据(推导链已按启动顺序核到行):
  - 编号与 FIFO:`bsp_can.c:37` 两个静态计数器(can1=0,can2=14);`:42-46` **先**用计数器奇偶定 FIFO(`(idx & 1) ? FIFO0 : FIFO1`),`:50` 才后置自增 → **第 1 个注册的实例是 filter 0 = 偶数 = FIFO1**,方案写"奇 FIFO0/偶 FIFO1"在"模块序号"口径下是反的,在"过滤器序号"口径下才对。
  - 实际归属(`application/robot.c:142 RobotCMDInit → 143 GimbalInit → 144 ShootInit → 148 ChassisInit`):
    - CAN1(filter 0..5):`robot_cmd.c:126`(rx 0x311)→F1;`gimbal.c:139`(yaw)→F0;`chassis.c:114-124` lf/rf/lb/rb → filter 2/3/4/5 → F1/F0/F1/F0。
    - CAN2(filter 14..17):`gimbal.c:187`(pitch)→F1;`shoot.c:110,113` 双摩擦轮 → F0/F1;`shoot.c:120`(loader)→F0。
  - 即 **CAN1-FIFO1 = {comm 0x311, lf 0x205, lb 0x207}、CAN1-FIFO0 = {yaw 0x209, rf 0x201, rb 0x203}、CAN2-FIFO1 = {pitch 0x205, friction_r 0x202}、CAN2-FIFO0 = {friction_l 0x201, loader 0x203}**。这套分布是 `#if ONE_BOARD` 下**启动调用顺序**的函数,而调用顺序写在 `robot.c` 里,不写在任何表里。
- 为什么是问题:方案要求"显式配置与现顺序逐一等价"才可合入,但等价性的参照物(启动顺序)没有机器可读形式;而且 `#if ONE_BOARD/CHASSIS_BOARD/GIMBAL_BOARD`(`robot.c:26-34,141-149`)意味着**不同板型下注册顺序不同** → 现 FIFO 分布本身就不是单一事实。
- 修改建议(可执行):
  1. B4a 前先产出 `tools/can_filter_golden.txt`:由**现有代码**在启动后调用一次新加的 `bus_can_dump_filters()`(读 `CAN1->sFilterRegister[]`/`FM1R`/`FFA1R`,或直接回吐注册表)把 `{bus, filter_idx, fifo, rx_id}` 打到 RTT,落盘成金标准;
  2. 显式配置版本用同一 dump 比对新旧文件,逐行 diff 为空才算等价;
  3. 同时把"启动注册顺序"写成 profile 表(B8 本来就要做),用主机单测跑"表 → 期望 FIFO"的纯函数,与金标准交叉验证。
  没有 1+2 这两步,"逐一等价"只是口头承诺。

**B3 [高] 滤波器预算/模式是缺失的一等不变量:越界与模式错配都静默**

- 方案位置:§3.3(141)"IDLIST 过滤器分配规则"、§3.8 第 1 条。
- 现状证据:
  - `bsp_can.h:17` `MX_CAN_FILTER_CNT (2*14)` **定义了但全仓无引用**;`bsp_can.c:37` 无上界检查,`CAN_MX_REGISTER_CNT=16`(`bsp_can.h:16`)是**全局**上限而 F4 每条总线只有 14 个 bank(CAN1:0-13,CAN2:14-27,`:48` `SlaveStartFilterBank=14`)。
  - `bsp_can.c:53` `HAL_CAN_ConfigFilter(...)` **返回值丢弃**;bank 越界时 HAL 返回 HAL_ERROR,过滤器不生效 → 该实例**永远收不到报文**,表现为"电机在线但无反馈",没有任何日志。
  - `bsp_can.c:49` `FilterIdLow = rx_id << 5` 配 `CAN_FILTERSCALE_16BIT`(`:40`):16bit 模式下 `FilterIdLow` 的 bit[15:5] 才是 STDID,当前写法对 **11 位标准帧**成立,但一旦有人用扩展帧(`IDE=CAN_ID_EXT`)或把 `FilterScale` 改成 32BIT,`<<5` 会变成静默错配。
  - `bsp_can.c:39` 常量 IDLIST 模式意味着**每实例一个 bank**,16 实例 × 每实例 1 bank 与 14 bank/总线冲突;方案 §3.8 未登记此预算。
- 修改建议:§3.8 增加不变量"**每总线 14 bank 预算 + 越界必须 FAILFAST + `HAL_CAN_ConfigFilter` 返回值必须检查**",并在 `bsp_can.h` 把 `MX_CAN_FILTER_CNT` 改成真正的断言(`_Static_assert(CAN_MX_REGISTER_CNT <= 14)`)或按总线分别限(can1≤14, can2≤14, 合计≤28)。

**B4 [高] §3.8 漏掉的 USART 不变量:环形缓冲"写指针不得追平读指针"与"装配/消费"契约**

- 方案位置:§3.8 第 4/6 条(185,187)、§3.3(141)"USART 循环 DMA+任务侧消费"。
- 现状证据:
  - `bsp_usart.c:132-146`:写指针由 `__HAL_DMA_GET_COUNTER` 反推,读指针只在 `USARTProcessAll` 推进;`:141-146` 注释自认"若写指针追平读指针,DMA 已覆盖未读数据",但**不计数、不告警**。消费周期 5ms(`robot.c:168`,200Hz),`USART_RX_RING_SIZE=256`(`bsp_usart.h:25`)在 921600 波特下仅 **2.8ms** 的数据量(AX650 链路,`ax650.h:80`)→ 余量本就不足,一旦某拍被拖慢就静默丢字节。
  - `bsp_usart.c:149-153` 装配溢出时 `assemble_len=0; err_cnt++`,而 `USARTGetErrCount`(`bsp_usart.c:183-186`)**全仓无调用者**(grep 仅定义处)→ 计数存在但无人观测;方案 §3.3 说"错误上报",现状是"错误记账但不播报"。
  - `bsp_usart.c:170-179` `recv_len` 协议(回调改小表示已消费,余量 memmove)是**唯一**的半帧拼装契约,已进 §3.8 第 4 条 ✓;但与之配套的"**回调不得返回 0 而不设标志**"未写:若回调把 `recv_len` 置 0,`assemble_len` 不前进,`recv_buff` 512B(`bsp_usart.h:26`)会在 `:149` 判溢出后**整块丢弃**,表现为长期丢帧。
  - `bsp_usart.c:216-228` `USARTConfigure8E1` 调 `HAL_UART_Init`,会**重置 `hdmarx->Init.Mode` 为 CubeMX 的 NORMAL**;而 `dma_configured[]`(`:33,50-62`)不会感知 → 若在 `USARTRegister` 之后调用,循环 DMA 静默退化为一次性,收满 256B 即永久停收。当前唯一调用点 `ax650_uart1.c:43` 在注册前(安全),但这条**顺序依赖没有任何断言**。
- 修改建议:§3.8 补 4 条:(a) 写指针追平读指针必须计数并可观测;(b) `err_cnt` 必须接入 §3.5 遥测/日志;(c) 回调消费 0 字节的语义(允许等待,但装配缓冲满时行为定义);(d) `HAL_UART_Init` 类重配必须在注册前,且 `dma_configured` 失效机制明确。另:把 `USART_RX_RING_SIZE` 从 256 提到 ≥1024 或把断言写进 B4b 验收(921600 × 5ms = 4.6KB,B4b 建议按波特率算而不是拍脑袋)。

**B5 [中] 伪实例:`sender_assignment[6]` 绕过注册表,迁移时会被"统一实例头"打碎**

- 现状:`dji_motor.c:60-67` 6 个静态 `CANInstance` 仅用于发送(无滤波器、不入 `can_instance[]`、不受重复 ID 检查约束),`CANTransmit(&sender_assignment[i], 8)`(`:389`)靠 `tx_buff`+`tx_id` 直接入队;其 `tx_mailbox`(`bsp_can.h:28`)字段被 `HAL_CAN_AddTxMessage` 写入但无人读 → 死字段。
- 为什么是问题:§3.3 的"统一实例头 `{type,id,event_cb,err_cb,err_cnt,owner}`"要求所有发送走同一注册路径,而 DJI 的分组发送天然是"6 个虚拟端点"。若 B4a 把 `CANTransmit` 改成必须传 `bus instance*`,这 6 个伪实例要么转正(占用 6 个注册位,与 `CAN_MX_REGISTER_CNT=16` 的预算和"重复 rx_id"检查冲突——伪实例无 rx_id),要么改 API。
- 建议:§3.3 明确区分 `bus_endpoint`(注册、带 rx 过滤)与 `bus_tx_frame`(仅 tx_id+payload),`dji_motor` 走后者;并把 `tx_mailbox` 从实例头删掉。

**B6 [中] CAN 接收路径的 O(n) 线性扫描与中断内的 memcpy 回调,方案未登记为不变量**

- 现状:`bsp_can.c:196-208`:每条报文遍历 `can_instance[]`(最多 16)比较 handle+StdId,然后 `:202-204` `memcpy(8B)` 到实例 `rx_buff` 并**在中断里**调 `DecodeDJIMotor`;`dji_motor.c:180-208` 该回调里做多圈角度计算 + `DaemonReload`。
- 为什么是问题:§3.3 说"I/O 异步"、§3.7 说"ISR 只写缓冲/置标志",而现状是**在 ISR 里跑业务解码**(乘加 + `measured`/`last_ecd` 更新)。若 B4a 把解码搬到任务侧,时序变化(反馈延迟 0→1 拍)= 行为变化,§3.8 未把"解码在 ISR 内完成"登记为契约;若保持不动,则 §3.7 的表格与现实不符。
- 建议:§3.7/§3.8 明确"CANN 帧分发与实例缓冲写入在 ISR,模块解码回调也在 ISR(现状),迁移不得下沉到任务";并把"单条报文到回调的延迟=0 拍"写成可对拍项(G8a 无法覆盖,需 G8b/台架)。

**B7 [中] 临界区嵌套:BASEPRI 手写与 FreeRTOS critical 不可组合**

- 现状:`port.c:403-427`:`vPortEnterCritical` = `portDISABLE_INTERRUPTS()`(BASEPRI=0x50)+ `uxCriticalNesting++`;`vPortExitCritical` = 归零时 `portENABLE_INTERRUPTS()`(**BASEPRI ← 0**)。而 `bsp_can.c:82-83,97` 是**手写 save/restore BASEPRI**。
- 为什么是问题:两者混用会出现"外层手写临界区 → 内层 FreeRTOS 临界区退出 → BASEPRI 归 0 → 外层并未被保护"。当 `xr_msg`(§3.4 用 `xr_enter_critical`)与 `xr_bus`(§3.3 用 BASEPRI)同时存在、且 `xr_err`/`xr_diag` 在临界区内被调用时,这条路径**必然**被走到。
- 建议:全框架**只用一种**实现:首选 `kernel_enter/exit_critical` 映射到 save/restore BASEPRI 的函数(而不是 `taskENTER_CRITICAL` 宏),并在头文件写死"禁止在有 FreeRTOS 嵌套计数的区域内调用本原语";同时在 `xr_err` 里断言"临界区内不得 `err_report(SAFE_STATE/RESTART)`"(用 §K1 的 `kernel_is_in_critical` 计数)。
- 附带量化:所有外设中断都是抢占优先级 5(`Src/can.c:127,129,162,164,166`、`Src/usart.c:194,240,310`、`Src/dma.c:48-73`、`Src/spi.c:171,199`),而 BASEPRI=0x50 = 优先级 5(`Inc/FreeRTOSConfig.h:123,130`)→ **临界区屏蔽了全部 CAN/USART/DMA 中断**。`bsp_can` 每次 `CANTransmit` 进出 2 次临界区,`dji_motor.c:385-391` × 1kHz = 12 次/ms;`msg` 若每订阅者一次(§3.4)则 200Hz×12 次。总量不大但必须进 §3.8 作为"中断延迟预算"登记,否则 B4b 加长临界区就会打断 USART 消费时序。

**B8 [低] `.ccmbss` 与 `pack(1)` 的对齐:遥测环结构体无显式对齐**

- 现状:`telemetry.h:39-57` `#pragma pack(1)` 包住 `Telemetry_Ring_s`,再 `TELEMETRY_CCM` 段属性(无 `aligned`);`Src/freertos.c:52` 的 `ucHeap` 有 `aligned(8)`。
- 风险:pack(1) 的 ring 结构体若被放进任何需要 4/8 字节对齐的上下文(未来静态池),会出现非对齐访问(CM4 支持但变慢,且 DMA 禁用区不能放)。当前仅 CPU 访问 + `.ccmbss` 段首 8 对齐,可用;但 §3.5(160)"五环保持独立对象"未写对齐要求。
- 建议:§3.8 第 2 条补 "`XR_SECTION` 对象必须显式 `aligned(8)`",与 `freertos.c:52` 保持一致。

---

### M 段 msg(§3.4 / T04 / G-D)

**M1 [高] 默认档语义等价成立,但"多订阅者"与"深度>1"两条是**新行为**,不是等价**

- 方案位置:§3.4(147-154)。
- 现状证据:`message_center.h:21` `QUEUE_SIZE 1`;`message_center.c:110-119` 满则丢最老(即覆盖);`SubGetMessage` (`:91-101`) 取走即出队;`PubPushMessage` (`:103-123`) 对每个订阅者 memcpy。**全仓 6 个话题**(`chassis_cmd/chassis_feed/gimbal_cmd/gimbal_feed/shoot_cmd/shoot_feed`,`robot_cmd.c:114-121`、`gimbal.c:267-268`、`shoot.c:161-162`、`chassis.c:165-166`),每个话题**恰好 1 个发布者 + 1 个订阅者**,且**发布与消费都在同一个 `RobotTask` 线程内**(`robot.c:172-179` 顺序调用;唯一的跨线程例外不存在)。
- 为什么是问题:今天根本没有并发、也无需加锁。新框架加"每订阅者临界区"在**现状调用图**下是纯开销(200Hz×12 次 BASEPRI 操作,见 B7);而"多订阅者 + 深度 N"是新能力,不是"与现语义严格等价"。§3.4 用"与现语义一致"描述默认档是对的,但要用"多订阅者/深度 N 属新增能力、B7 只启用深度 1"的措辞,否则 B7 的"零行为"会被误当成"新能力已验收"。
- 差异点清单(默认档对比,逐条可验):
  | 项 | 现状 | 新框架默认档 | 是否等价 |
  |---|---|---|---|
  | 拷贝次数 | 2(pub→queue,queue→caller) | 2(同) | 等价 |
  | 未取走时新帧 | 覆盖 | 覆盖(满丢最老,N=1 即覆盖) | 等价 |
  | 取走语义 | 出队,`temp_size--` | 同 | 等价 |
  | 返回值 | `PubPushMessage` 恒 `1`(实现),头文件却写"订阅者数量"(`message_center.h:81`) | 若按头文件返回真实数量 | **差异**:有调用者判返回值就会变(当前 0 处判断,但适配头必须保留"返回 1") |
  | 话题上限 | `MAX_TOPIC_COUNT 12` **未实施**,超出静默扩容 | `MSG_TOPIC_MAX=16` 要**检查** | **差异**:init 期新增失败路径 |
  | 名称/长度不匹配 | `CheckLen` 不一致 → `while(1)`(`message_center.c:29-37`) | `pack(1)`+错误码 | **差异**:挂死→错误码(属改进,但属行为变化) |
  | 注册期内存 | `malloc`×(`1+1+N`) | 静态池 | 等价(但 `malloc` 失败→NULL 路径消失) |
- 建议:§3.4 加一行"默认档逐项对照表"(上表),并把"话题上限实施、名称/长度不匹配由挂死改错误码"**显式列入 B7 行为变化白名单**,否则 B7 无法宣称零行为。

**M2 [高] `msg_publish` 非 ISR 安全 + 每订阅者临界区,与 §3.7/§3.1 的 ISR 契约存在隐含冲突**

- 方案位置:§3.4(149-150)、§3.7(176-177)、§3.1(127)。
- 现状证据:今天 ISR 里**没有**任何 `PubPushMessage()` 调用(grep 全仓仅 `robot_cmd.c:306-312`、`gimbal.c:533`、`shoot.c:224,537`、`chassis.c:550`,全部在任务);ISR 只做 `memcpy` 进实例(`bsp_can.c:203`)。
- 为什么是问题:方案自己在 §3.7 允许 ISR 调 `err_report(LOG_ONLY)`。若 `err` 内部把错误"发布"到 diag 的话题(很自然的设计),就会在 ISR 里调 `msg_publish` → 违反 §3.4。**两条契约没有交叉检查机制。**
- 建议:在 §3.4 增加"ISR 可安全子集"(与 §3.7 同表),实现上在 `msg_publish` 首行 `if (kernel_is_in_isr()) { drop_cnt++; return; }`(调试构建直接 `configASSERT`),把冲突变成可测错误而不是静默破坏。

**M3 [中] 单写者检测所需原语缺失(依赖 K1),且"调试构建"在本工程没有定义**

- 现状:CMake 只有 `CMAKE_BUILD_TYPE` 分流(`CMakeLists.txt:89-103`),CI 走默认分支(`-Og -g -gdwarf-2` + `-DESC_DEBUG`),并没有 `XR_DEBUG` 一类的框架调试开关;G2 同时要求 Debug/Release 都 0 warning 更无从谈起。
- 建议:定义 `XR_DEBUG_BUILD`(由 CMake 显式 `-D`),把"单写者检测/临界区断言/`msg` 越界检查"挂在该宏下;否则"调试构建首发布记 owner"无法落地。

**M4 [低] 适配头"零行为"可行,但要注意符号类型被外部持有**

- 现状:各处持有的是 `Publisher_t *` / `Subscriber_t *`(`robot_cmd.c:114-121`、`gimbal.c:267-268`、`shoot.c:161-162`、`chassis.c:165-166`),只做指针透传,从不解引用字段 → 适配头可用"不透明句柄 + 同名 typedef"做到源码零改。
- 需登记的两点:(a) `sizeof(...)` 作为 `data_len` 传入,新框架 `data_len` 若从 `uint8_t` 变 `uint16_t`(`message_center.h:10` 注释已提 255 截断),**行为变化**在 >255B 话题上才显现;(b) 测试脚本直接编译 `modules/message_center/message_center.c`(`tests/run_chassis_tests.ps1:15`),**B7 删除该文件即让 G3 红**,必须在 B2 就把测试改为经适配头/新实现。

---

### E 段 err(§3.1 / T02)

**E1 [阻断] SAFE_STATE 从 ISR/临界区投递没有定义,且现有 log 路径在 ISR 里不是安全的**

- 方案位置:§3.1(124-128)、§3.7(177)。
- 现状证据:
  - `bsp_log.c:121-125` `LogPrintf` 里有**有状态修复分支**:`RttUpBufferSane()` 失败 → `SEGGER_RTT_Init()` 重建控制块后丢弃本次。若该分支在 ISR 中执行、而任务正处在 `SEGGER_RTT_vprintf` 内 → 控制块被重置,正在写的缓冲状态被打断(SEGGER 的 `SEGGER_RTT_LOCK` 只保护写入区,不保护 `_SEGGER_RTT` 整体重置)。
  - `bsp_log.c:108-119` 浮点拦截只 `warned` 一次,无**限流**;§3.7 写"`err_report(LOG_ONLY)`✅(限流)"但现状无任何限流设施。
  - `bsp_log.c:44-49` `LogSetEnabled` 是"日志总闸",与 §3.1 的 `LOG_ONLY` 策略重叠(两套静音机制)。
- 为什么是问题:`err_report` 是**四档唯一出口**,却被要求在 ISR 可用。SAFE_STATE 需要"app 注入的零力动作",而零力动作(`ApplySafeOutputs`,`cmd_authority.c:43-50`)写的是 `gimbal_cmd_send/chassis_cmd_send/shoot_cmd_send`——这些是 200Hz 任务的数据,从 ISR 里写就是**多写者**,直接违反 §3.4 的单写者约定。
- 修改建议:
  1. `err_report` 内部按档分离:`LOG_ONLY` 只做"计数 + 写入 lock-free 环"(ISR 安全,由 diag 任务取走打印);`SAFE_STATE` **只置原子标志** `g_atomic_safe_state_req`,由 `RobotTask` 首拍消费并调注入回调;`RESTART_PORT` 只置"待重启位",由 diag 任务执行;`FAILFAST_BOOT` 才允许直接 `FaultRecordDetail`(`bsp_fault.c:35`)。
  2. 明确"**临界区内禁止 err_report**"(仅 `err_report_deferred` 允许),并在 §3.9 的分类校验里加一条扫描。
  3. 日志限流做成框架常量(如 1 条/100ms/错误码),写进 §3.9 的 RTT 1024B 预算表。

**E2 [高] "白名单 4 处 vs 实际 34 处"的落差被低估,且计数口径混了三类东西**

- 方案位置:§3.1(126,128)、T02(35)。
- 实际清点(first-party,`while(1)`/`for(;;)` 已逐个定位;B0b 删除 iic/spi/ist8310 后复核):
  | 分类 | 位置 | 数量 | 应否白名单 |
  |---|---|---|---|
  | 注册期 fail-fast(实例超限/重复注册/参数非法) | `bsp_can.c:141,148`;`bsp_usart.c:91,96,203`;`bsp_pwm.c:38`;`message_center.c:24,34`;`dji_motor.c:81,116,144,162,170`;`HTmotor/HT04.c:164`(活桩) | 14 | 是(其中 11 条可改 `err_report(FAILFAST_BOOT)`,不必 while) |
  | 启动期硬件自检 | `ins_task.c:111`(BMI088 50 次重试后)、`modules/alarm/buzzer.c:50` | 2 | 是(方案只写了 BMI088) |
  | **不可达尾巴**(复位/异常之后的 `while(1)`) | `bsp_fault.c:61-62`(`NVIC_SystemReset()` 之后) | 1 | **否**(删或保留都不影响;计入白名单会让统计失真) |
  | **死代码** | `bsp_tools.c:48`(`CreateCallbackTask` 全仓无调用者) | 1 | 否(应随 B0b 删除,见 F1) |
  | CubeMX 生成区(不可手改) | `Src/main.c:136`、`stm32f4xx_it.c:97,139,154,169` | 5 | 否(排除扫描域) |
- 结论:"白名单 4 处"若按**站点**计,目标值约为 16(14 注册期 + 2 启动期,B7 删 message_center 后降为 14);若按**类别**计应写"4 类"。方案必须给出**逐站点清单**(file:line),否则 T02 的 grep 验收第一天就红。
- 建议:§3.1 的验收改为"`while(1)` 站点集合 = 白名单文件(逐行)之并集",白名单文件由 B1 产出;并把 `bsp_fault.c:61`、`bsp_tools.c:48`、CubeMX 区显式排除。

**E3 [中] 错误码分段与现有"数码语义"没有对齐,蜂鸣码是其唯一现场出口**

- 现状:`robot.c:41-47` 定义 8 类鸣叫码(0-5),`FaultConsume` 只能表达 `FAULT_CODE_HARDFAULT/STACKOVF`(`bsp_fault.c:125-139`),而 §3.1 的四段错误码 `0x00xx/0x10xx/0x20xx/0x30xx` 无处落地(备份寄存器只有 8 位 code,`bsp_fault.c:40` `code & 0xFF`)。
- 建议:§3.1 明确"`err_report` 的 code 高字节写入 `BKP0R` 低 8 位,与蜂鸣码映射表固定"(`0x10xx`→两声短,等),否则 `SAFE_STATE/RESTART_PORT` 在现场**没有任何可读痕迹**,违背该章"对接 bsp_fault 留痕"的承诺。

---

### D 段 diag(§3.5 / T03 / G9)

**D1 [阻断] "全员活才喂狗"与 referee/UI 任务的合法永久不活直接冲突 → 不接裁判即 2s 复位循环**

- 方案位置:§3.5(158)"profile 注册周期任务→心跳位掩码;低优先级监督任务检查全活才 IWDGFeed"。
- 现状证据:
  - `application/robot_task.h:31-35`:`TASK_UI_ENABLE` 默认 **1**(仅在 `YAW_AUTO_VOFA_USART6_ENABLE=1` 时为 0;该宏当前为 0,`config/ax650.h:70`)。
  - `application/robot_task.c:131-144` `StartUITASK` → `MyUIInit()`(`modules/referee/referee_task.c:70`)。
  - `referee_task.c:72-73`:裁判未初始化 → `vTaskDelete(NULL)`(**UI 任务自我删除**);`:74-84`:裁判已初始化但未收到 `robot_id` → `while (... ) { ...; osDelay(1000); }` **无限循环,不返回**。
  - 结论:UI 任务要么不存在、要么长期停在 `while` 里,**都永远不会喂心跳**。若 UI 被注册进"全员"掩码 → 永不喂狗 → IWDG(`bsp_iwdg.c:28-31`,2s)复位 → 死循环复位。
- 为什么是问题:裁判系统不接(台架、装车前、调试)是**常态**,方案把它当"启动窗口"处理就必然踩。§3.5 的"唯一必答实车项"只写了"启动窗口/调试冻结",漏了"合法离线/自我终止"。
- 修改建议:
  1. 心跳分两类:**必活集合**(INS/MOTOR/ROBOT/DAEMON,与裁判无关)与**可选集合**(UI/referee/AX650);喂狗只看必活集合,done 可选集合只上报 `online` 位到遥测。
  2. 支持**运行期注销**:`diag_watch_unregister(id)`(`vTaskDelete(NULL)` 前调用,`referee_task.c:73`),否则掩码位永久悬空。
  3. `MyUIInit` 的 1Hz 等待循环改为由 `UITask` 主循环驱动状态机(不改行为但让它能喂心跳);若不想改,则 UI 必须**永不**进掩码。
  4. G9 必须增加一条反向验收:"**不接裁判系统冷启动 60s 不复位**"(这条比"挂起任务 2s 复位"更重要)。

**D2 [高] 遥测环的 `XR_SECTION` 机制自相矛盾:段属性是平台概念,放 framework 里就不是零平台依赖**

- 方案位置:§3.5(160)"框架代码用 `SECTION_CCM` 等段限定宏,宏由各 port 定义(stm32f4=`__attribute__((section(".ccmbss")))`,linux/sim=空)"。
- 现状证据:`modules/telemetry/telemetry.h:33-37` 就是把"`__arm__` → `.ccmbss`"写成宏的现有做法;`Src/freertos.c:52` 是 `.ccmbss + aligned(8)` 的先例;链接脚本 `.ccmbss (NOLOAD) > CCMRAM`(在 `STM32F407IGHx_FLASH.ld`,与 `MEMORY{CCMRAM: ORIGIN=0x10000000, LENGTH=64K}` 配套)。**段名是链接脚本契约**——即"port 定义宏"必须与"port 的 .ld 文件"同步,framework 只拿到一个神秘的字符串字面量。
- 为什么是问题:(a) 与 §2.2"framework 零硬件依赖"字面冲突(方案自己也把它列成 C 类例外,说明知道别扭,但没有给出自洽形态);(b) 迁移到 `framework/diag` 后,**对象定义点**必须跟着走,否则 `.ccmbss` 内容由 modules/ 与 framework/ 分摊,`nm -S` 与 G5 都不易核对;(c) 主机单测里宏为空 → 测试对象落主 .bss,**测试通过不代表目标行为**。
- 更自洽的替代设计(建议采纳):
  ```c
  /* framework/diag/telemetry.h —— 无段属性、无平台宏 */
  typedef struct { void *storage; size_t bytes; size_t count; } xr_telemetry_arena_t;
  void xr_diag_telemetry_attach(const xr_telemetry_arena_t *arena); /* 启动期一次 */
  ```
  `ports/stm32f4/diag_storage.c` 里定义 5 个环对象并加 `__attribute__((section(".ccmbss"), aligned(8)))`,把地址交给 framework;`ports/linux` 用普通静态;`sim` 用可注入缓冲。好处:framework 零平台宏、零段名;`27KB 保证`由 port 的编译单元 + `.ld` 一处负责;主机测试可指向任意缓冲并断言越界;`objcopy`/`.ld` **不需要迁移批改动**(段名不变,只是定义点换文件)。
- 若坚持 `XR_SECTION` 方案:必须在 §3.8 增加不变量"**段名 `.ccmbss` 同时被 port 头与 port 的 .ld 引用,任一侧改名即静默丢 27KB**",并加 CI 检查(`.ld` 里 grep `.ccmbss` 与 port 头里的字面量一致)。

**D3 [高] 27KB CCM 的"保证措施"不足:只靠人工纪律,无自动回归**

- 现状:`telemetry.c:18-26` 5 个环共 ≈26.3KB(256×21B+8B)×5;`.ccmbss` 还有 `ucHeap[28000]`(`Src/freertos.c:52`,`Inc/FreeRTOSConfig.h:67`)→ CCM 64KB 已用 ≈54.3KB(85%)。
- 风险点(方案未覆盖):
  1. **CCM 无 ECC/无 DMA**,但 `ucHeap` 里的任务栈若被 CCM 搬运影响(如 `vApplicationStackOverflowHook` 读取栈顶)不受影响;真正的风险是**后续新增 CCM 对象(如静态池 mem pool)把 64KB 撑爆 → 链接期才报 region overflow**,而 §3.5 没有"CCM 预算表"。
  2. `--gc-sections`(`CMakeLists.txt:36`)+ 静态库(附B 第 5 条)可能**丢弃无人引用的 `.ccmbss` 对象**(若 framework 只通过弱符号访问),表现为"遥测没了但不报错"。
  3. `-flto` 会重排/合并节,`.ccmbss` 的对象顺序变化 → Ozone 里按偏移观察的习惯失效(工具脚本受影响,T13 未覆盖)。
- 建议:§3.5 增加"CCM 预算表(`.ccmbss` 已用/上限,新增对象必须登记)";CI 增加 `size -A build/*.elf | grep cc` 的阈值检查(现 G5 只比 text/data/bss 总量);遥测对象**不得**放进静态库,而应放在 port 的显式对象文件里(与 D2 的替代设计一致)。

**D4 [中] 调试冻结路径写对了(DBGMCU),但与 G9 的测试方法自相矛盾**

- 现状:`bsp_iwdg.c:36-37` `DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP` → 调试器**暂停核心时 IWDG 停计数**。
- 影响:G9/T03 要求"挂起任一周期任务 → 2s 内复位"。用调试器"挂起任务"实际是挂起**整个核心** → IWDG 被冻结 → **永远不复位**;用断点同理。
- 建议:G9 的注入方式改为**固件内注入**:新增 `volatile uint8_t g_diag_test_mute_mask`(telemetry/变量注入口),置位后对应任务跳过 `diag_watch`;验收判据改为可无人值守读出的证据:`RCC->CSR` 的 `IWDGRSTF`(`robot.c:103-113` 已打印)+ 开机三短鸣(`robot.c:112-113`)→ "重启后 `[boot] RST cause: IWDG=1`" 即为 PASS。这条能让 G9 从"台架人工"降级为"半自动"。

**D5 [中] `daemon` 的"次数而非毫秒"耦合是 §3.8 级别的隐形契约,清单未列**

- 现状:`daemon.h:22,32,44-46`:`reload_count` 单位是 **`DaemonTask` 调用次数**,`DaemonTask` 固定 100Hz(`robot_task.c:109`);`daemon.c:33` 默认 100;`dji_motor.c:253` `reload_count=2`(20ms),`rm_referee.c:175` `30`(300ms)。
- 为什么是问题:§3.5 说"daemon 并入 diag,语义不变",但若监督任务的周期或调用点变化(例如并入 200Hz 的 RobotTask),**所有模块的超时判定整体漂移 2 倍**,表现为"电机频繁误判离线 / 遥控失联延迟"。nm/size/对拍都看不见。
- 建议:§3.8 增加一条"`daemon`/`diag_watch` 的超时以**毫秒**在注册时换算、内部仍按拍计,且节拍源(HAL_GetTick / DWT / 任务计数)必须与迁移前一致";迁移批验收加"逐个 `reload_count` 调用点的实际超时(ms)表 = 迁移前"。

---

### G 段 宏治理(§3.9 / T14)

**G1 [高] 四类不互斥、不穷尽:至少 5 类真实宏无处安放**

- 方案位置:§3.9 表格(193-200)、规则"每个宏必须归入一类"(193)。
- 逐类举证(全部为 config 真实宏):
  | 无法归类的宏 | 位置 | 为什么归不进四类 |
  |---|---|---|
  | `TELEMETRY_RING_SIZE` / `TELEMETRY_LOG_DIV` | `modules/telemetry/telemetry.h:26-27` | **容量/预算宏**:不是参数(不在 config)、不是 0/1 开关、不是平台、不是函数式;但它决定 27KB CCM 与 RTT 带宽,是 §3.5 的核心量 |
  | `CAN_Send_FIFO_SIZE` / `CAN_MX_REGISTER_CNT` / `MX_CAN_FILTER_CNT` | `bsp/can/bsp_can.h:16-19` | 同上,且 `MX_CAN_FILTER_CNT` 是死宏(无引用) |
  | `DEVICE_USART_CNT` / `USART_RX_RING_SIZE` / `USART_RX_ASSEMBLY_SIZE` | `bsp/usart/bsp_usart.h:24-26` | 同上;且 `DEVICE_USART_CNT=3` 被 §6 B10 当作"链路已满"的依据 → 属设计常量 |
  | `DAEMON_MX_CNT` | `modules/daemon/daemon.h:14` | 容量宏 |
  | `QUEUE_SIZE` / `MAX_TOPIC_NAME_LEN` / `MAX_TOPIC_COUNT` | `message_center.h:19-21` | 容量宏 + 死宏(`MAX_TOPIC_COUNT` 未实施) |
  | `TASK_UI_ENABLE` | `application/robot_task.h:31-35` | **派生开关**,定义点在 **app 层**而非 config,且由 `YAW_AUTO_VOFA_USART6_ENABLE`(config)派生 → 违反"B 类唯一来源 = config" |
  | `DISABLE_LOG_SYSTEM` | 仅 `Makefile:229` / `Makefile.upgrade:110`,CMake **不定义**;`bsp_log.h:43-44` 由 `BSP_LOG_OFF` 再派生 | **构建系统注入宏**:定义点计数 = 0(源文件里没有 `#define`),`check_macros` 的"定义点数应=1"直接误报;且两套构建**行为不同** |
  | `ONE_BOARD` / `VISION_USE_VCP` | `config/board.h:12,16` | **空定义宏**(`#define ONE_BOARD` 无值)→ 与 B 类规则"值域校验 `(X==0||X==1)`"不兼容 |
  | `YAW_ESC_CURRENT_MODE`(mode 0/1)+ 派生 `YAW_ESC_OUT_MAX`/`YAW_CURRENT_DOMAIN_SCALE` | `config/gimbal.h:152,161,165` | 前者的语义是"模式选择"而非"功能裁剪",后者是**A 类外表 + B 类内核**(见 G2) |
  | `HEAT_GUARD_RATE_MODE` / `YAW_INERTIA_FF_SRC` | `config/heat.h:57`、`gimbal.h:206` | 同上,取值为枚举式整数(0/1/2),既非纯数值也非 0/1 开关 |
- 建议:改四类为**五类 + 两个属性**:
  - A 参数 / B 开关 / C 平台 / D 函数式 / **E 容量预算(新)**(定义点限 framework 头,必须进预算表);
  - 属性1 `derived_from`(派生源,可为空)→ 允许 `YAW_ESC_OUT_MAX` 标注 `derived_from=YAW_ESC_CURRENT_MODE`;
  - 属性2 `injected`(构建系统注入,如 `DISABLE_LOG_SYSTEM`)→ 定义点校验跳过、改为"必须出现在 `CMakeLists.txt` 或 `Makefile` 的同一处"。
  这样"每宏必能归类"才成立。

**G2 [高] "A 类禁 `?:` 藏开关"不可静态判定;应改为依赖闭包规则**

- 方案位置:§3.9 表格 A 类规则(197)。
- 同形反例(两条在本仓并存):
  - 真开关:`config/gimbal.h:161` `YAW_ESC_OUT_MAX (YAW_ESC_CURRENT_MODE ? 16384.0f : 25000.0f)`;`:165` `YAW_CURRENT_DOMAIN_SCALE (YAW_ESC_CURRENT_MODE ? 0.6554f : 1.0f)`。
  - 非开关的钳位:`config/gimbal.h:275-276` `PITCH_GRAVITY_FF_LIMIT (1.2f * ((PITCH_GRAVITY_A < 0.0f ? -PITCH_GRAVITY_A : PITCH_GRAVITY_A) + ...)` —— 同一 `?:` 语法,纯算术(abs 语义)。
  - 死三元式:`config/gimbal.h:117` `YAW_SPEED_KP_HIGH (YAW_ESC_CURRENT_MODE ? 40.0f : 40.0f)`(两支同值)。
- 为什么是问题:"禁 `?:`" 会把 `PITCH_GRAVITY_FF_LIMIT` 判错(它不是开关);"允许 `?:`" 会漏掉 `YAW_ESC_OUT_MAX`。**语法层面不可判定。**
- 修改建议:规则改为"**A 类宏的 `derived_from` 闭包中不得含 B 类宏**"——这是可以用 `gcc -E -dM` + 定义体文本做静态推导的(把宏定义体里的标识符递归展开,遇到 B 类名即报错)。`YAW_SPEED_KP_HIGH` 这类死三元式用"两支文本相同"检测(正则可做)。
- 附带:`YAW_SPEED_KP_HIGH` 两支同值意味着**该开关从未生效**,属于"配置坏死";应在 B 类账本登记为"待清理",但清理会改动 G1' 守恒的字节 → 必须在某一批显式声明(见 G3)。

**G3 [高] "B 类单一裁决点"与"既有宏不改名(G1' 守恒)"直接矛盾**

- 方案位置:§3.9 表格 B 类"命名 `<MODULE>_<ENABLE>`"(198)、落地机制第 5 条(207)"既有宏不改名"、G1'(224)。
- 现状证据(命名不合"`<MODULE>_<ENABLE>`"的 B 类实存):`CHASSIS_M3508_FORCE_DISABLE`(`config/board.h:19`)、`CHASSIS_FOLLOW_AFF_ACCEL_ON`(`chassis.h:246`)、`CHASSIS_FOLLOW_NO_OVERSHOOT`(`chassis.h:231`)、`LOADER_BACKOFF_EN`(`shoot.h:123`)、`YAW_ESC_CURRENT_MODE`(`gimbal.h:152`)、`YAW_INERTIA_FF_SRC`(`gimbal.h:206`)、`HEAT_GUARD_RATE_MODE`(`heat.h:57`)。
- 为什么是问题:按 B 类规则改名 → 违反 G1'(参数宏集合逐字节守恒,改名即 changed);不改名 → B 类规则无法执行;而"单一裁决点"本身要求把 28 处 `ONE_BOARD`(`application/chassis/chassis.c:50,64,88,164,167,203,209,216,250,421,549`、`chassis_kinematics.c:23`、`robot_cmd.c:44,47,119,122,208,305`、`robot.c:26,30,141,147,172,178`、`robot.h:5`、`robot_def.h:21,23,24`)**收拢** → 一定会改 `#if` 站点(可以不改 `#define`,但 G1' 只校验宏定义,不校验使用点 → 这一项实际上不冲突,必须写清楚)。
- 修改建议:把两条规则的边界写明:
  1. **G1' 只守恒"config 伞头闭包的宏名 + 展开值"**,使用点收拢不受它约束(允许并鼓励);
  2. B 类命名规则**只对新宏生效**(`XR_`/未来宏),既有宏按"已登记例外"处理,不强制改名;
  3. 收拢 `ONE_BOARD` 属于**代码结构变化**,必须计入 G4(审计 Changed)与 G6 不适用声明,不能混在"零行为"里悄悄做。

**G4 [中] `check_macros.py` 的能力边界必须写进方案,否则会被当成"宏治理已完成"**

- 做得到(静态扫描):
  1. 定义点计数(需先排除构建系统注入宏 G1 表里的 `DISABLE_LOG_SYSTEM`);
  2. `#if` 使用点清单(按宏→file:line 反向索引,186 个已可枚举);
  3. B 类值域 `(X==0||X==1)`;A 类数值 diff(复用 `tools/refactor_gate.py:85-90` 的 `gcc -E -dM` 输出);
  4. 依赖闭包含 B 类宏(G2 的替代规则);
  5. config 子文件互不 include、每开关定义点唯一、死三元式两支同值;
  6. `config/` 外新增 A 类宏(config 闭包与 framework 头分域比对)。
- 做不到:
  1. 判断某 `#if` 站点"关掉后语义是编译消失还是运行静默"(需要语义,不是文本);
  2. 判断 A 类宏的**数值是否有物理意义**(域断言只能人工写,方案已用 `_Static_assert` 兜,✅);
  3. 判断运行期 `if (HEAT_GUARD_ENABLE)` 与编译期 `#if` 的**选择是否正确**(`heat_guard.c:150` vs `heat.h:13`)——只能人工定准则;
  4. 识别经 `#define` 拼接/`#include` 生成的宏(`YAW_SPEED_KP_HIGH` 依赖 `YAW_ESC_CURRENT_MODE`,闭包展开需要处理 `##`、函数式宏);
  5. 任何"宏导致的代码体积/栈深度变化"。
- 建议:§3.9 落地机制第 1 条补一句"本脚本只保证**可枚举 + 可分类**,不保证语义正确;语义项由 B 类账本的'关掉后语义'列人工填写并在批次验收签字"。

**G5 [中] 伞头"展开顺序不敏感"的脆弱约定,check_macros 只能部分验证**

- 现状:`config/robot_infantry_2026.h:20-28` 按 9 个子文件顺序 include;`config/robot_infantry_2026.h:16` 明文"子文件之间不互相 include(宏展开在包含点,顺序不敏感)"。这句只在"宏值不依赖彼此定义顺序"时成立:反例 `config/gimbal.h:117` 引用 `YAW_ESC_CURRENT_MODE`,而后者定义在 `:152`(同文件内**后**定义)→ 依赖"宏在**使用点**展开"这一 C 预处理特性,不是顺序不敏感,而是**惰性展开**。
- 建议:术语改为"子文件互不 include(惰性展开,定义点可在使用点之后)",并把"跨子文件引用"列为必须登记的依赖边(`check_macros` 输出依赖图,人工确认无环/无跨文件隐藏耦合)。

---

### T 段 T0 判据与闸门(§1 / §5)

**T1 [高] T03 "2s 内复位"不可执行(与 D4 同因),判据不可证伪**

- 方案位置:T03(36)、G9(232)。
- 证据:`bsp_iwdg.c:36-37` DBGMCU 冻结;`bsp_iwdg.c:28-31` 实际超时 = 1000×2ms = **2s(±LSI 40kHz 容差 → 1.6~2.4s)**;`robot.c:157` IWDG 在 `OSTaskInit()` **之后**启动 → 当前启动窗口 = 0,方案的"启动窗口风险"在现有代码里不存在(除非把 `IWDGInit` 前移)。
- 结论:"2s 内"既无法用调试器注入(冻结),也无法精确到秒级测量(LSI 容差);且"启动窗口"要防的东西现已不存在。
- 建议:判据改写为两条可自动读出的证据:(a) 注入后**复位发生**且 `[boot] RST cause: IWDG=1`(`robot.c:103`)与三短鸣(`robot.c:112-113`);(b) 全员活时**连续运行 N 分钟不复位**。超级电容/示波器测时间不必要。

**T2 [高] T07 "运行期 malloc 调用点=0(扫描)"是假验收**

- 方案位置:T07(40)。
- 证据:现状 malloc 分布(已全量 grep):`bsp_can.c:153`、`bsp_pwm.c:40`、`message_center.c:54,66,72`、`dji_motor.c:221`、`buzzer.c:52`、`can_comm.c:89`、`user_lib.c:28`(且 `user_lib.c:21` `#define user_malloc pvPortMalloc` → **扫 `malloc` 字面量抓不到 pvPortMalloc**)、`kalman_filter.c:155-250`(26 处 `user_malloc`)。方案说"21 处集中在 5 个活文件"——按本次清点,**`can_comm.c:89`、`buzzer.c:52` 也属运行期可达**(`can_comm` 由 `robot_cmd.c:126` 注册;buzzer 由 `daemon` 任务 `BuzzerInit()` `robot_task.c:99`),而 `can_comm` 不在方案的 5 个文件里。
- 为什么是假验收:静态 grep 无法覆盖 `pvPortMalloc`/函数指针/`kf->user_malloc` 宏别名;也无法证明"运行期不调用"(只在启动期调用的代码路径无法静态判定)。
- 建议改为**链接期 + 运行期双重可证伪**:
  1. 链接加 `-Wl,--wrap=malloc -Wl,--wrap=pvPortMalloc -Wl,--wrap=calloc -Wl,--wrap=realloc`,wrapper 内先判 `g_xr_boot_done` → 若已完成启动则 `err_report(XR_E_RT_MALLOC)+FaultRecordAndReset`;启动期则计数放行;
  2. 该计数经遥测环上报 → 运行期证据 = 遥测 `rt_malloc_cnt == 0`;
  3. 豁免清单只对 `kalman_filter.c` 的**调用点**生效(启动期),不是对整个文件。
  这套措施 30 行代码,比任何扫描都硬。

**T3 [高] T11 "编译期断言存在性扫描"是假验收;且现状只有 7 处**

- 证据:`_Static_assert` 全仓 7 处:`application/robot_def.h:139,140,142,143`(ARM/宿主双分支,由 `#if` 区分)、`application/chassis/chassis.c:40`、`modules/master_machine/ax650_pose.c:15`、`modules/referee/referee_protocol.h:215`。
- 为什么是假验收:"文件里有 `_Static_assert`" ≠ "该结构体受布局守卫"。可以在任意位置写一条无关断言让脚本变绿。
- 建议:改为**金标准 ABI 快照**:`tools/abi_dump.c` 用 `offsetof/sizeof` 打印所有 `pack(1)` 帧结构(robot_def.h 全部 + msg 帧 + bus 帧),ARM 与主机各跑一次,与 checked-in `tools/abi_golden/*.txt` diff(空 diff 才 PASS)。这同时解决 `robot_def.h:139-143` 因 `__arm__` 分裂导致的两套值没有交叉校验的问题。

**T4 [中] T05 "include 图扫描(kernel 外禁 FreeRTOS.h)"缺边界定义**

- 见 A2:必须"传递闭包 + 只算 first-party TU + 显式豁免名单"。补充现状:今天违反者至少 `application/robot_task.h:10-11,13`、`Src/freertos.c:21-24`、`bsp/dwt/bsp_dwt.c:12`、`bsp/bsp_tools.c:11`、`bsp/bsp_tools.h:9`、`modules/referee/*`(6 文件)、`modules/motor/HTmotor/HT04.c:12`。**B1 若开此扫描必红**,所以新扫描必须带"存量豁免文件清单 + 每批只减不增"的规则,否则 G2 直接被旧账顶住。

**T5 [中] T06 "语义保持清单逐条打钩"不是验收,是流程**

- 方案位置:T06(39)、§3.3(141-142)。
- 建议:每条不变量配一个**可执行载体**:寄存器/队列快照 golden(B2)、主机单测(B3/B4 的纯函数)、台架观测项(D5 的超时表)、G8a 向量(B6 的解码)。§3.8 表改成三列:`不变量 | 验证载体 | 验证批`,没有载体的条目视为未定稿。

**T6 [中] G5 在 `-flto` 下按符号比尺寸不可靠**

- 证据:`CMakeLists.txt:31-37`:`-ffunction-sections -fdata-sections` + 链接 `-flto` + `--gc-sections`;`tools/refactor_gate.py:93-114` 用 `nm -S` 生成 `(name,type,size)` 多重集。
- 为什么是问题:LTO 跨 TU 内联后,改一个**不相关**文件都可能改变某函数的内联决策 → 符号尺寸集合变化 → G5"逐符号一致"误报;反之,LTO 也可能把"行为变化"内联掉而不改变符号集合。
- 建议:G5 的"零行为批"判据改为**目标文件级**(对 `build/CMakeFiles/*.obj` 用 `arm-none-eabi-size`/`nm`,或关闭该批的 LTO(`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` 的单批构建)做对比);实现替换批仍用总量预算(方案已如此)。

**T7 [中] T08 的 CI 三 target 与现状不符,且两套构建已经在输出不同固件**

- 证据:`.github/workflows/c-cpp.yml:19-24` 同时 `make -j12` 与 `cmake -B build && ninja`;`Makefile:225-229` 有 `-DDISABLE_LOG_SYSTEM`,`CMakeLists.txt:42-46` **没有** → CI 每次都在编两份**行为不同**的固件(一份日志全静默,一份开日志并跑 `Float2Str/sprintf`,`telemetry.c:59-72`)。CI 无 cppcheck/clang-tidy(`.github/workflows/c-cpp.yml:27-35` 只有测试与 links)。
- 建议:B1 就把"单一构建"定死:**Makefile 变成转发 `cmake --build` 的 shim**(或删掉并改 CI),把 `-DDISABLE_LOG_SYSTEM` 收到 `CMakeLists.txt` 的一个 option(默认 OFF,`release` profile 开);否则 T08 的"三 target"建立在两套分歧构建上。

**T8 [中] T13 的"清单存在性"可自动化,方案写弱了**

- 证据:`tools/` 下 8 个 JS/PY 脚本按 DWARF 路径读成员(方案 T13 描述正确)。存在性检查完全可以做成:`arm-none-eabi-nm -C build/*.elf | grep -F "<symbol>"` 逐条,零成本,可进 CI。
- 建议:T13 验收改为 `tools/check_symbol_freeze.py` 跑冻结清单(nm 命中率 100%),并入 G2;清单符号改名时该脚本变红,而不是"静默读不到数"。

---

### F 段 附B B1 开工清单(312-319)

**F1 [阻断] 清单第 1 项新建的 `framework/` 目录不会被编译 → "0E0W"是空验**

- 证据:`CMakeLists.txt:72-79` `file(GLOB_RECURSE SOURCES "Drivers/*.c" "Src/*.c" "Middlewares/*.c" "bsp/*.c" "modules/*.c" "application/*.c")`;**没有 `framework/`、`ports/`、`apps/`**;`CMakeLists.txt:106` 单 `add_executable`。`include_sub_directories_recursively`(`:50-68`)只加 include 路径(:63-69),不加源文件。
- 为什么是阻断:B1 的验收(`附B` 第 6 条"`cmake && ninja` 0E0W + `refactor_gate.py check` 全绿 + 未接线")会在"新文件根本没参与编译"的情况下全绿。
- 建议:附B 第 5 条改写为:
  1. CMake 新增**独立静态库** `add_library(xr_framework STATIC ...)` + `target_include_directories`,**不要**把 `framework/` 加进 GLOB_RECURSE(避免与 lib 重复编译);
  2. `add_executable` 链接 `xr_framework`(即使"不接线",为让符号被链接需显式 `--whole-archive` 或加一个 `xr_version()` 被 `robot.c` 调用的"接线锚点";否则 `.a` 里无人引用的成员会被丢弃,G2 覆盖不到);
  3. G2 的"0 warning"必须显式声明**编译了哪些 TU**(在 CI 打印 `ninja -t commands | wc -l` 或 `--verbose`)。
- 另:`refactor_gate.py:37` 硬编码 `BUILD/"basic_framework.elf"`,`:61` 硬编码 `config/robot_infantry_2026.h`;CMake `project()` 名(`CMakeLists.txt:1` 附近)一旦按 G-E 改名,`G1~G5` 全部失效。B1 必须加"项目名/ELF 名发现"或把改名推到 B12。

**F2 [高] "整车未接线任何新代码(审计 Added 数=声明数)"不可证伪**

- 证据:`tools/refactor_gate.py:143-153` 的审计计数来自上游 manifest 的 `Unchanged/Changed/Missing/Added`(`:60` 调 `.agents/.../audit_baseline.py`),它只覆盖 **460 条基线文件**;`framework/`、`ports/` 等**新目录不在基线里** → 新建多少文件,Added 都是 0。
- 建议:附B 改为 `git status --porcelain` 全量文件清单 **==** 该批批准的清单(逐行),并用 `git diff --stat` 排除对既有文件的改动;两者共同构成"不接线"证据。

**F3 [高] Makefile 与 CMake 双登记是负担而非保障(见 T7)**

- 证据:`Makefile:225-244` 显式 `C_SOURCES`/`C_INCLUDES`;`tools/check_source_lists.py` 已存在用于核对两套(CI `:34` 调用)。
- 建议:附B 第 5 条"Makefile 同步登记"删掉,改为"**Makefile 退化为 shim**,`check_source_lists.py` 随之退役";`Makefile.upgrade`(`:110` 同样带 `-DDISABLE_LOG_SYSTEM`)在 B0b 一并删除(它指向 `TARGET = basic_framework`,`:17`)。

**F4 [高] cppcheck 的"存量豁免"没有可执行形式**

- 证据:CI 现无静态分析(`.github/workflows/c-cpp.yml:27-35`);存量代码含大量已知模式(`while(1)` 16+、`malloc` 30+、`cmsis_os` 调用),直接开 cppcheck 必是告警洪水。
- 建议:附B 明确三件套:(a) `tools/cppcheck_baseline.json`(首跑全量,commit);(b) CI 用 `--suppress=` 读基线 + `--error-exitcode=1` 只对**新增**告警失败;(c) 基线只减不增,G4 记录告警数。

**F5 [中] 附B 漏掉的前置项(逐条)**

1. **`bsp_tools.c` 整个文件是死代码**(`CreateCallbackTask` 全仓无调用者),且它是**唯一**使用 `osSignalWait`(CMSIS signal)的地方 → 若 B0b 不删,新增的 kernel 会被迫实现 signal 原语;建议 B0b 删除清单加 `bsp/bsp_tools.{c,h}`(`64+22` 行)与 `modules/can_comm`(若双板不恢复;`robot_cmd.c:126` 在用,需用户裁决)。
2. **`MX_CAN_FILTER_CNT`(`bsp_can.h:17`)、`MAX_TOPIC_COUNT`(`message_center.h:20`)、`tx_mailbox`(`bsp_can.h:28`)是死宏/死字段** → 应进宏账本"待清理"列(否则 check_macros 的"定义点=1"全绿却掩盖坏死)。
3. **`config/robot_infantry_2026.h` 之外还有 8 个容量宏在 framework 分层后仍留在 components**(见 G1 表)→ B1 的宏账本扫描根必须显式声明(application/modules/bsp/config;`Src/Inc` 的 173 个 `#if` 属 CubeMX,排除)。
4. **`main.c` 的 `osKernelStart()`(`Src/main.c:131`)与 `MX_FREERTOS_Init()`(`:128`)在生成区** → B1 必须登记"B8 需要用户批准的 `Src/` USER CODE 改动"(AGENTS 规定 `Src/Inc` 仅 USER CODE 区),否则 kernel 后端的 `kernel_start()` 无处接线。
5. **测试脚本硬编源文件**(`tests/run_chassis_tests.ps1:11-16` 含 `modules/message_center/message_center.c`)→ B2 一并改造,否则 B7 删文件时 G3 红。

---

## 三、方案漏项(按"不补会出什么事"排序)

| # | 漏项 | 不补会出什么事 |
|---|---|---|
| 1 | **§3.5 的"必活/可选"心跳分级 + 运行期注销 + 合法离线白名单** | 不接裁判系统即 2s 复位循环(D1),台架/装车前不可用 |
| 2 | **§3.8 补全不变量**(本文 B1~B5、B7、D5 共 12 条) | B4/B5/B6 的"零行为"完全靠人记忆;`tx_id` 晚绑定、filter bank 耗尽、daemon 拍数单位这三类会静默改行为 |
| 3 | **临界区唯一实现 + 临界区内禁 `err_report` 的规则** | BASEPRI 与 FreeRTOS 嵌套计数混用导致临界区被提前打开(B7/E1) |
| 4 | **CCM 预算表 + `.ccmbss` 段名一致性 CI** | 27KB 遥测回迁/段被 gc 丢弃;D2/D3 |
| 5 | **"零行为"对**新增可观测量的口径**(`err_cnt`、`tx_drop_cnt`、心跳遥测) | 新增 data/bss 与遥测字段会被 G5 判成行为变化,或反过来借"新增字段"掩盖真变化 |
| 6 | **`configASSERT` 的处置**(K3) | 新框架每个 API 误用都变成"关中断死等看门狗"且无死因 |
| 7 | **`freertos.c`/`main.c` 生成区的接管方案**(K4/F5-4) | kernel 后端无法接管启动与 defaultTask,`T05` 永远不达标 |
| 8 | **主机测试的"多线程"用例(§3.4 说要线程安全)** | 现状单线程调用图下,"线程安全"无测试载体;posix 后端需 pthread 双线程发布/消费用例才可证 |
| 9 | **`bsp_tools.c`/`can_comm` 的处置裁决** | 决定 kernel 是否需要 signal 原语、B0b 删码清单准确度 |
| 10 | **§3.9 缺"容量预算宏"与"构建系统注入宏"两类,以及 `derived_from`/`injected` 属性** | "每宏必能归类"直接不成立,check_macros 首跑即红 |
| 11 | **A/B/C/D 分类器的依赖闭包规则** | `?:` 判定不可行(G2),账本无法自动化 |
| 12 | **B8 profile 装配与"启动注册顺序"的关系** | CAN FIFO 归属(B2)在 profile 化后再次整体平移,且无金标准可比 |
| 13 | **T0 判据中"可证伪"的统一模板**(每条:注入方式 + 读回证据 + 通过阈值) | T03/T07/T11 继续是假验收 |

---

## 四、过度设计/可裁剪项(2026-11 现状下的负收益)

1. **`kernel_mutex(3)` + `kernel_queue(4)`(§3.2,7 个函数)**:全仓 first-party **零使用**(grep 无 `osMutex/osMessageQ/osSemaphore`)。首版只保留 1 个"互斥量"接口占位(给将来 diag 用),队列/信号量推迟到有真实调用者的批次。裁剪收益:API 面从 15 降到 9,测试面减少一半。
2. **`kernel_task_destroy`**:唯一使用者是 `Src/freertos.c:132` 的 `defaultTask` 自删(该任务本就该删)与 `referee_task.c:73` 的 `vTaskDelete(NULL)`。先改这两处(删 defaultTask、UI 任务不自杀,见 D1),再决定是否保留 destroy。
3. **`kernel_yield`**:零使用;`osDelay(0)` 在 CMSIS 下已能做同样的事。
4. **5 档优先级**(RT/HIGH/NORMAL/LOW/IDLE):实际只用 3 档(3/4 两个数值,`robot_task.c:35,38,41,44,49`)。首版 3 档(NORMAL/HIGH/RT)足够,**但必须钉死 3/4/5**(K2)。
5. **`MSG_TOPIC_MAX=16`**:现用 6 话题;16 个槽 × 名字 32B 的静态表是 512B bss。降到 8(留 2 空位),省一半,且让"超限"更早暴露。
6. **`msg` 深度 2~N 与"事件型"支持**(§3.4):现状 6 个话题全是状态型(`message_center.h:5-8` 注释已说明)。深度>1 是无调用者的能力,建议只留编译期参数、不实现"满丢最老"以外的策略。
7. **`msg` 单写者检测**:依赖 K1 缺失原语 + 工程无"调试构建"定义(M3)。首版可以只做"首发布记 `owner` 到遥测字段"的记录,不做断言,把成本压到 5 行。
8. **`ports/linux` + ROS2(B10)与 `ports/sim` 完整版(B9)**:G-C 的 ③④⑤ 属 M5 出口判据,但里程碑在 M3 之后;在 M3 前它们只能带来"编译通过"的假安全感。建议 B4 只做 **sim-min**(纯函数对拍,方案已如此 ✅),linux/posix 后端**推迟到 M3 之后**,`kernel` 的 posix 后端在 B1 只留**接口 stub + 一个主机 pthread 测试用例**(为 §3.4 线程安全提供唯一载体,见漏项 8),不做完整实现。
9. **`kernel_queue`/`kernel_mutex` 的 posix/baremetal 双实现**:同上,零调用者时是纯维护负担。
10. **`memory_center` 适配头的最小化**:适配头只需 4 个函数 + 2 个不透明类型(M4),不需要"旧结构体字段兼容";`Publisher_t/Subscriber_t` 内部字段外部零引用(已核 `robot_cmd.c`/`gimbal.c`/`shoot.c`/`chassis.c`),别为不存在的兼容需求写映射层。
11. **`MX_CAN_FILTER_CNT`、`MAX_TOPIC_COUNT`、`tx_mailbox`、`bsp_tools.{c,h}`、`Makefile.upgrade`**:直接删,别让它们进入宏账本/迁移清单(见 F5)。
12. **`TELEMETRY_LOG_DIV=100` 的日志通路**(`telemetry.c:62-72`,每环 2Hz × 5 环 = 10 行/s,每行 4 次 `sprintf` via `Float2Str`):在 §3.9 的 RTT 1024B 预算表里这是最大单项之一,且 `Float2Str` 的 `sprintf`(`bsp_log.c:175`)是 newlib 调用,在 200Hz 任务里每 0.5s 一次虽小但属于"框架内禁用 newlib"的破例。建议迁到 framework 时改为**整数定点打印**(`(int)(v*1000)`),顺带把 `%f` 规则从"运行期扫描"降级为"编译期检查"(`tools/check_log_format.ps1` 已存在,可进 CI)。

---

## 附:本次评审的证据采集范围

- 通读方案全文 358 行。
- 通读代码:`bsp/can/bsp_can.{c,h}`、`bsp/usart/bsp_usart.c`、`bsp/pwm/bsp_pwm.c`、`bsp/fault/bsp_fault.c`、`bsp/iwdg/bsp_iwdg.c`、`bsp/log/bsp_log.{c,h}`、`bsp/dwt/bsp_dwt.{c,h}`、`bsp/bsp_tools.c`、`bsp/bsp_init.h`、`modules/message_center/message_center.{c,h}`、`modules/daemon/daemon.{c,h}`、`modules/telemetry/telemetry.{c,h}`、`modules/motor/DJImotor/dji_motor.c`、`modules/can_comm/can_comm.c`、`modules/referee/referee_task.c`、`modules/referee/rm_referee.c`、`modules/imu/ins_task.c`、`modules/master_machine/ax650_uart1.c`、`application/robot.c`、`application/robot_task.{c,h}`、`application/cmd/cmd_authority.c`、`application/cmd/robot_cmd.c`、`application/chassis/chassis.c`、`application/gimbal/gimbal.c`、`application/shoot/shoot.c`、`application/robot_def.h`、`Src/freertos.c`、`Src/main.c`、`Src/stm32f4xx_it.c`、`Inc/FreeRTOSConfig.h`、`config/robot_infantry_2026.h` 及 9 个子文件、`CMakeLists.txt`、`Makefile`、`Makefile.upgrade`、`.github/workflows/c-cpp.yml`、`tools/refactor_gate.py`、`tests/run_*.ps1`、`STM32F407IGHx_FLASH.ld`、`Middlewares/.../cmsis_os.c`、`Middlewares/.../port.c`、`SEGGER_RTT_Conf.h`。
- 全仓 grep 统计:`os*`/FreeRTOS 原语、`while(1)`/`for(;;)`、`malloc`/`user_malloc`、`__attribute__((section))`、平台符号(`DWT->/HAL_/__set_BASEPRI/NVIC_/RCC->/DBGMCU`)、`#if` 计数(186 first-party / 359 含 Src+Inc)、`ONE_BOARD`(28 处/6 文件)、`_Static_assert`(7 处)、文件数(`application 28 / modules 89 / bsp 29 / config 10`)。
- 未执行:编译、测试、烧录(只读审查,无授权需求)。方案中"实测 220 宏 / 186 `#if`"两项已复算,**与实测一致**(220 = config 目录 `#define` 计数;186 = application+modules+bsp+config)。
