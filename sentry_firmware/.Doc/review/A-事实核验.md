# 域A 事实核验报告 ——《自主研发框架升级总方案》定量/可核实断言

> 审查对象：`.Doc/plan/自主研发框架升级总方案.md`（v1.4 头注，358 行，2026-11-22）。
> 依据：**本地工作树**（工作区可能领先 git HEAD；仅用工作区文件作"现状"依据）。
> 环境：Windows / `pwsh` 实为 PowerShell 5.1，读中文一律 `Get-Content -Encoding utf8`；Python 用 `C:/Users/ZhuanZ/.workbuddy/binaries/python/versions/3.13.12/python.exe`。
> 实测时间：2026-11-22 12:37–12:52（本地）。
> 只读约束：本报告是唯一写入的文件；`refactor_gate.py save-baseline` 未运行（会写基线）。

## 0. 口径说明（本报告使用）

| 口径名 | 定义 |
|---|---|
| 业务层文件集 A | `application/ modules/ bsp/ config/` 下 `.c/.h`（不含 `.agents/` 快照、`Drivers/`、`Middlewares/`、`build/`） |
| A+测试 | A + `tests/` |
| A+CubeMX | A + 测试 + `Inc/`、`Src/`（CubeMX/HAL 生成与 `main.c`、`freertos.c` 等） |
| #if 系使用点 | 行首 `#if`、`#ifdef`、`#ifndef`、`#elif`（各算 1，`#endif/#else` 不算） |
| 裁剪点文件 | 含上述 `#if` 系使用点的文件（**不含** `config/` 内的定义点文件） |
| malloc 调用点 | `malloc(`/`user_malloc(`/`calloc(`/`realloc(` 调用行（不含注释与 `#define user_malloc`） |

---

## 1. 核验表

| # | 方案断言(行号) | 核验方法 | 实测值 | 判定 | 证据(file:line 或命令) |
|---|---|---|---|---|---|
| 1 | L47/L191「config 层 220 个宏」 | A 口径下 `config/**/*.h` 逐行匹配 `^\s*#\s*define\s+[A-Za-z_]\w*` | **220**，与断言一致。分布：`gimbal.h 69 / chassis.h 45 / shoot.h 34 / ax650.h 29 / heat.h 18 / kbm.h 15 / remote.h 4 / board.h 3 / protect.h 3 / 伞头 0`。全部为对象式宏（函数式 0），其中 28 个值为 0/1 的开关宏、2 个无值开关宏（`ONE_BOARD`、`VISION_USE_VCP`） | **证实** | `config/robot_infantry_2026/gimbal.h:152`（`YAW_ESC_CURRENT_MODE 1`）、`config/robot_infantry_2026/board.h:12`（`ONE_BOARD`）；命令见 §4-C1 |
| 2 | L47/L191「全仓 186 个 `#if` 系使用点」 | 三档口径分别复算；再反推能得出 186 的算法 | A=**193**；A+测试=**202**；A+CubeMX=**376**。`186` 唯一可复现解释：**口径漏掉 `#elif`**——193−7=186，7 处 `#elif` 全在 `modules/imu/`（BMI088driver.c ×2、BMI088driver.h ×4、BMI088Middleware.h ×1）。另把 git HEAD 同口径复算仍是 193/202/376（诊断用，不作现状依据），故 **186 不是"HEAD 旧快照"造成，是统计口径错误** | **证伪**（数字错；算法可复现为漏 `#elif`） | `modules/imu/BMI088driver.c:77,405`；`BMI088driver.h:47,52,57,62`；`BMI088Middleware.h:35`；命令见 §4-C2 |
| 3 | 同上：「`#if` 系使用点」未注明是否含 CubeMX 生成文件 | 逐文件统计 A+CubeMX | 193→376 的增量 183 中：`Inc/stm32f4xx_hal_conf.h 61`、`Src/system_stm32f4xx.c 29`、`Src/usbd_conf.c 13`、`Src/usbd_desc.c 10`、`Inc/usbd_conf.h 6`、`Inc/usbd_cdc_if.h 3` 等；且 193 中 **78 处是头文件 include guard**（`#ifndef X` + 紧跟 `#define X`），`robot.c:21` 的 `#ifndef ROBOT_DEF_PARAM_WARNING` 才是功能判断。剔除 guard 后真正的条件编译/裁剪点 = **115** | **部分证实**（口径未写清，缺 guard/生成文件的界定；用 186 建宏账本会把 ~45% 噪声写进账本） | 命令见 §4-C2；guard 判定：`#ifndef X` 后 1–2 行内出现 `#define X` |
| 4 | L191「开关宏散布面最大的是 `ONE_BOARD`(6 文件)」 | 全仓逐文件检索 `\bONE_BOARD\b`，区分"定义点/裁剪点/注释提及" | 出现 **7 文件**：`chassis.c`（50,64,88,164,167,203,209,216,250,421,549）、`chassis_kinematics.c`（23）、`robot_cmd.c`（44,47,119,122,208,305）、`robot.c`（26,30,141,147,172,178）、`robot_def.h`（21 注释、23–24 板型冲突守卫）、`config/.../board.h`（12，定义点）、`robot.h`（5，**仅注释**）。**含预处理指令的文件=6**；**裁剪点文件=5**（去掉 board.h 定义点） | **部分证实**（6=定义1+裁剪5 的混合口径；与本句其余开关"各 3 文件"的纯裁剪点口径**不自洽**。按同口径应写 5，或明确写"预处理指令 6 文件 / 其中裁剪点 5 文件 + robot.h 注释"） | `application/robot.h:5`（注释）；`application/robot_def.h:23`；`config/robot_infantry_2026/board.h:12` |
| 5 | L191「`AX650_UART1_ENABLE` 各 3 文件」 | 逐文件列出使用点 | 裁剪点 **3**：`robot_cmd.c:240`、`robot_task.c:62`、`ins_task.c:172`（另 `tests/chassis_remote_test.c:189`、`config/ax650.h:11` 定义+19 冲突守卫、`chassis.h:288` 注释） | **证实**（裁剪点口径） | `application/cmd/robot_cmd.c:240`；`application/robot_task.c:62`；`modules/imu/ins_task.c:172` |
| 6 | L191「`YAW_AUTO_VOFA_USART6_ENABLE` 各 3 文件」 | 同上 | 裁剪点 **3**：`chassis.c`（54,133,216,538）、`gimbal.c`（24,33,256,291,497）、`robot_task.h:31`（定义 `config/ax650.h:70`） | **证实** | `application/robot_task.h:31` |
| 7 | L191「`YAW_AUTO_KF_ENABLE` 各 3 文件」 | 同上 | 裁剪点 **3**：`cmd_autoaim.c:25`、`cmd_remote.c:60,75,86`、`robot_cmd_internal.h:49`（定义 `config/ax650.h:48`） | **证实** | `application/cmd/cmd_remote.c:60` |
| 8 | L191「`GIMBAL_RECENTER_ENABLE` 各 3 文件」 | 同上 | 裁剪点 **3**：`cmd_authority.c:115`、`cmd_kbm.c:195`、`robot_cmd.c:252`（定义 `config/gimbal.h:53`） | **证实** | `application/cmd/cmd_kbm.c:195` |
| 9 | L191「派生宏藏开关：`YAW_ESC_OUT_MAX`/`YAW_CURRENT_DOMAIN_SCALE` 由 `YAW_ESC_CURRENT_MODE` 三元派生」 | 读定义与使用点 | 属实：`gimbal.h:161` `(YAW_ESC_CURRENT_MODE ? 16384.0f : 25000.0f)`；`:165` `(… ? 0.6554f : 1.0f)`；开关 `:152 =1`。使用点：`gimbal.c:162`（MaxOut）、`gimbal.c:124-125,158,417,461`（DOMAIN_SCALE 5 处） | **证实** | `config/robot_infantry_2026/gimbal.h:152,161,165`；`application/gimbal/gimbal.c:162` |
| 10 | L191「死三元式 `YAW_SPEED_KP_HIGH` 两分支同为 40.0f」 | 读定义 | 属实：`gimbal.h:117` `#define YAW_SPEED_KP_HIGH (YAW_ESC_CURRENT_MODE ? 40.0f : 40.0f)`。旁证：上方 124–132 行注释记载"电流模式取 30"，代码已同值 40 → 三元式确实是死式（历史语义丢失） | **证实** | `config/robot_infantry_2026/gimbal.h:117`；使用点 `application/gimbal/gimbal.c:436` |
| 11 | L191「伞头子文件不互 include、**展开顺序不敏感**」 | 查 `config/` 内 include 与 `#if` 的跨文件依赖 | "不互 include"属实（仅伞头 include 9 个子头）。"顺序不敏感"**不成立**：`config/ax650.h:19` 的 `#if defined(CHASSIS_VOFA_UART1_ENABLE) && …` 依赖 `chassis.h:286` 的定义，而伞头顺序是 chassis.h（第 2）先于 ax650.h（第 4）；若调换顺序，该 `#error` 守卫会静默跳过（`defined()` 为假），两个使用者同时打开也发现不了 | **部分证实**（脆弱性判断正确，但"顺序不敏感"已有反例；§3.9 落地机制第 3 条"从纪律变成可验证"应改为"消除 config 内跨子文件 `#if`"） | `config/robot_infantry_2026/ax650.h:19`；`config/robot_infantry_2026/chassis.h:286`；`config/robot_infantry_2026.h:20-28`（include 顺序：board/chassis/gimbal/ax650/…） |
| 12 | L40/L251「实测 **21 处运行期可达** malloc，**集中在 5 个活文件**（bsp_can/buzzer/message_center/dji_motor/user_lib）」 | 全仓 malloc 族调用点复算 + 逐点判定调用路径（初始化期/周期任务） | **全仓 47 处 / 17 文件** = `kalman_filter.c 27` + 其他 **20 处 / 16 文件**（message_center 3；LK9025 2；bsp_iic 2；bsp_can/bsp_gpio/bsp_pwm/bsp_spi/buzzer/user_lib/can_comm/ist8310/dji_motor/HT04/servo_motor/super_cap/tfminiplus 各 1）。`21` 最接近的读法是"非 kalman 20 处"+1（无法精确复现）。**"集中 5 活文件"不成立**：所列 `user_lib.c:28` 在 `zmalloc()` 内，而 `zmalloc`/`MatInit` **无任何调用者（死）**；真正活路径是 **7 处 / 5 文件**：`bsp_can.c:153`、`bsp_pwm.c:40`（buzzer.c:29 调 `PWMRegister`）、`buzzer.c:52`、`message_center.c:54,66,72`、`dji_motor.c:221`。**"运行期可达"判据不成立**：全部位于 `Register/Init` 函数，经 `robot.c:141-149` 或 `robot_task.c:99`（任务启动前/首拍）执行一次，无一处位于周期循环 → 应为"**启动期可达**"，稳态已零 malloc | **证伪**（处数、文件集、运行期判据三点均不成立；你的初测"bsp_can.c 无 malloc"也不对——`bsp_can.c:153` 确有） | `bsp/can/bsp_can.c:153`；`bsp/pwm/bsp_pwm.c:40`；`modules/alarm/buzzer.c:29,52`；`modules/message_center/message_center.c:54,66,72`；`modules/motor/DJImotor/dji_motor.c:221`；`modules/algorithm/user_lib.c:26,28`（无调用者）；`application/robot.c:141-149`；命令见 §4-C3 |
| 13 | L40/L8「kalman_filter.c 27 处 `user_malloc` 为**启动期一次性**；被 `QuaternionEKF.h:15` 引用；1kHz 调用」 | 追引用链与执行次数 | 三点均属实：`QuaternionEKF.h:15 #include "kalman_filter.h"`；`QuaternionEKF.c:68 Kalman_Filter_Init(&…,6,0,3)` → `kalman_filter.c:155-250` 共 **27 处** `user_malloc`；`INS_Init` 有幂等守卫 `ins_task.c:94-97`（第二次调用直接 `return`），虽然 `INS_Init` 被 3 处调用（`chassis.c:90`、`gimbal.c:135`、`robot_task.c:61`），malloc 只执行一次。更新在 1kHz 任务：`ins_task.c:169`，任务周期 `robot_task.c:73 osDelay(1)`、`ins_task.c:142` 注释"以1kHz运行" | **证实** | `modules/algorithm/QuaternionEKF.h:15`；`modules/algorithm/QuaternionEKF.c:68`；`modules/imu/ins_task.c:94-97,125,169` |
| 14 | L251「DM/HT 为活桩（`robot_task.c:53` / `motor_task.c:26` 调用，空转早退）」 | 读调用点与早退条件、查构建登记与注册调用者 | 属实：`robot_task.c:53 HTMotorControlInit();`、`motor_task.c:26 DMMotorTask();`；早退：`HT04.c:219-220 if (!idx) return;`（`idx` 静态计数，`HTMotorInit` 无调用者）、`dmmotor.c DMMotorTask` 首行 `if (idx_dm == 0) return;`（`DMMotorInit` 无调用者）；且被编译进固件：`Makefile:141-143`、CMake `GLOB_RECURSE`（`CMakeLists.txt:72`） | **证实** | `application/robot_task.c:53`；`modules/motor/motor_task.c:26`；`modules/motor/HTmotor/HT04.c:219`；`Makefile:141-143` |
| 15 | L251「死代码 ~2100 行（oled/ist8310/bluetooth/TFmini/unicomm/standard_cmd/bsp_spi/bsp_iic/bsp_gpio/LK9025）」 | 逐目录按行统计 | 合计 **2124 行**：oled 777（3 文件）、bsp_spi 299、LKmotor 283、bsp_iic 273、bsp_gpio 169、ist8310 164、bluetooth 95、TFminiPlus 64、unicomm 0、standard_cmd 0。偏差 +1.1%，"~2100"**成立** | **证实**（行数口径） | 命令见 §4-C4；`modules/unicomm/*`、`modules/standard_cmd/*` 为 0 字节占位 |
| 16 | 同上：「零行为、独立提交」的删除可执行性 | 查删除对象被活代码引用情况 | 行数正确但**不是纯删文件**：`motor_task.c:9 #include "LK9025.h"`（活文件包含待删头，必须同步删该行），`motor_task.c:12 #include "step_motor.h"`（`step_motor.c/h` 均 0 字节但不在删除清单）；`Makefile:115-117,130,135,141-143,147,157-159` 共 10+ 条源清单需同改（CMake 为 GLOB 自动）。删除顺序须"依赖者先删"（bsp_iic/bsp_gpio 被 ist8310/tfminiplus 引用） | **部分证实**（"零行为"成立；"独立提交"需附带 1 个活文件的 include 删除 + Makefile 改动，应在 B0b 范围里写明） | `modules/motor/motor_task.c:9,12`；`Makefile:115-117,141-143,157-159` |
| 17 | L35/L126「bsp 注册路径 **11+ 处**裸 `while(1)`」 | 全仓 `while (1)` 逐点分类（含所属函数） | 全仓 first-party：**33 处 `while(1)` + 6 处任务体 `for(;;)`**。分类：**注册/初始化路径 18 处**（bsp_can 141,148；bsp_iic 19；bsp_pwm 38；bsp_spi 21,42；bsp_usart 91,96；bsp_tools 48；buzzer 50；ist8310 101；dji_motor 81,116,144,162,170；message_center 24,34）；**运行期传输/发送错误路径 12 处**（bsp_iic 47,55,72,80,91,108,127；bsp_spi 67,94,141,150；bsp_usart 203）；故障复位 1（bsp_fault 61）；INS 启动 fail-fast 1（ins_task 111）；任务体 7（robot_task 66,83,101,119,138；bsp_tools 38；HT04 164）。**纯 bsp 注册路径=9 处**，"11+"既不等于 bsp 的 9，也不等于全 first-party 的 18。另 L126 列举的 **servo 并无 `while(1)`**（`servo_motor.c` 只有 `LOGERROR`） | **部分证实/证伪**（数字与列举均有误；措辞把"bsp"与"模块"混算） | 命令见 §4-C5；`modules/motor/servo_motor/servo_motor.c:38-40`（无 while(1)） |
| 18 | L128/§3.1「红旗扫描 `while(1)` **仅剩白名单 4 处**」 | 用方案 §3.1 的白名单类别映射到代码点 | 白名单 = 4 **类**，落到代码是 **10 个点**：BMI088 重试 `ins_task.c:111`(1) + DJI 分组/非法ID `dji_motor.c:81,116,144,162,170`(5) + CAN 实例超限 `bsp_can.c:141,148`(2) + msg 注册校验 `message_center.c:24,34`(2)；按文件是 4 个文件。若验收用 `grep` 计数"仅剩 4 处"，**永远达不到 10** | **部分证实**（4 只能是"4 类/4 文件"，写成"4 处"验收判据不可执行） | `modules/imu/ins_task.c:111`；`modules/motor/DJImotor/dji_motor.c:81,116,144,162,170`；`bsp/can/bsp_can.c:141,148`；`modules/message_center/message_center.c:24,34` |
| 19 | L147「现用 6 话题」 | 汇总 `PubRegister/SubRegister` 调用点去重 | **6** 个：`chassis_cmd / chassis_feed / gimbal_cmd / gimbal_feed / shoot_cmd / shoot_feed`。注册点：`chassis.c:165-166`、`robot_cmd.c:114-121`、`gimbal.c:267-268`、`shoot.c:161-162`。现存静态表上限是 `MAX_TOPIC_COUNT 12`（方案 §3.4 的 `MSG_TOPIC_MAX=16` 是新设计，不冲突） | **证实** | `modules/message_center/message_center.h:20`；`application/cmd/robot_cmd.c:114-121` |
| 20 | L136「任务表 INS 1kHz / MOTOR 1kHz / DAEMON 100Hz / ROBOT 200Hz / UI」 | 读 `robot_task.c` 的 `osDelay` 与优先级 | 全部属实：INS `osDelay(1)`(73)、MOTOR `osDelay(1)`(90)、DAEMON `osDelay(10)`(109)、ROBOT `osDelay(5)`(127)、UI `osDelay(1)`(141)。优先级：INS `osPriorityAboveNormal`(35)，其余 `osPriorityNormal`(38,41,44,49)。注：UI 实际是 1kHz 轮询而非"按需"（`TASK_UI_ENABLE=1`，`robot_task.h:34`） | **证实** | `application/robot_task.c:35-50,73,90,109,127,141` |
| 21 | L160/L279「遥测 27KB CCM、RAM 49%（DEV-20261122-02）、`ucHeap` 在 `freertos.c:52` 用 `.ccmbss`」 | 读 `.map` 段与符号地址、读源文件、查 DEVLOG | 段名/大小全部吻合：`.ccmbss 0xd6a0 = 54,944B @0x10000000`；`ucHeap @0x10000000`（`freertos.c:52`，`section(".ccmbss"), aligned(8)`）；5 环 `0x10006d60…0x1000c190`，单环 5386B、合计 **26,930B**（"27KB"按 1000 进制=26.93KB ✓）。主 RAM：`.data 844 + .bss 22,560 + 保留 40,960 = 64,364B ≈49.1%`（DEVLOG 记 64,368B/49.11%）。`DEV-20261122-02` 条目存在且写明 91.02%→49.11%、`configAPPLICATION_ALLOCATED_HEAP=1`、`configTOTAL_HEAP_SIZE=28000` | **证实** | `build/basic_framework.map`；`Src/freertos.c:52`；`Inc/FreeRTOSConfig.h:67,71`；`modules/telemetry/telemetry.h:33-37`；`modules/telemetry/telemetry.c:18-26`；`RM_DEVLOG.md:2232`（DEV-20261122-02）|
| 22 | L262「`DEVICE_USART_CNT=3` 已用满（USART1/3/6）」 | 查宏、板级初始化与注册点 | 宏 `bsp_usart.h:24 #define DEVICE_USART_CNT 3`；CubeMX 只初始化 3 个：`Src/usart.c:27-29`（huart1/3/6）；注册点：huart1 `chassis.c:142,146`（另一分支 `ax650_uart1.c:46`）、huart3 `robot_cmd.c:111`、huart6 `chassis.c:134`/`gimbal.c:260`/`rm_referee.c:168`。断言成立（"用满"= bsp 池上限+现有注册点，不等于硬件全部 USART 通道） | **证实** | `bsp/usart/bsp_usart.h:24`；`Src/usart.c:27-29`；`application/cmd/robot_cmd.c:111` |
| 23 | L46/L251「tools/ 下 8 个 emberprobe 脚本按 DWARF 路径直读」 | `tools/` 逐文件统计 + 查 `.js` 是否 require `agent-client` 并含 DWARF 表达式 | `tools/` 共 21 文件；**根目录恰好 8 个 `.js`**（chassis_trace、doctor、follow_trace、gimbal_axis_probe、gimbal_trace、odom、pitch_calib、pitch_trace），8 个全部 `require(".agents/skills/_emberprobe/agent-client.js")` 且含表达式（如 `motor_lf->measure.speed_aps`、`INS.Pitch`、`gimba_IMU_data->YawTotalAngle`）。**边界未写清**：`tools/lib/preflight.js` 也持 DWARF 表达式（:189-191 `motor_lf`/`gimba_IMU_data`/`chassis_last_ms`，被 `doctor.js:275`、`odom.js:142` 调用），是第 9 个"读符号"文件；`tools/analyze_trace.py:12` 只吃 CSV 派生列名，不是 DWARF 直读。另"**静默失效（不报错）**"不准确：多数脚本会报错/报 0 样本（`pitch_trace.js:157`、`gimbal_trace.js:224` 打印失败块），且 `doctor.js/odom.js` 有固件身份预检 | **部分证实**（8 = "根目录 .js"口径成立；需写清不含 `lib/`；"静默"言过其实） | `tools/chassis_trace.js:36,55`；`tools/pitch_trace.js:38-42,157`；`tools/lib/preflight.js:189-191`；`tools/analyze_trace.py:12` |
| 24 | L213/L214「manifest 与 audit_baseline.py 现状、`save-baseline/check/check-links` 已实现可用」 | 查文件存在性、读 `refactor_gate.py` 分发、跑只读子命令 | 全部存在且已实现：manifest 81,101B；`audit_baseline.py` 12,434B（实测 `--project .` **exit 0**）；`refactor_gate.py:177 cmd_save`、`:219 cmd_check`、`:393 cmd_links`、`:426 main` 分发齐全。`check-links` 实测 **PASS exit 0**；`check --no-build --no-tests` 实测可运行但**当前 2 FAIL**：G1 宏集合"新增 30 / 消失 12"（示例 `AX650_UART1_ENABLE 1→0`、`CHASSIS_FOLLOW_DAMP/FF 消失`）、G4 审计"当前 295/104/61/61/4 vs 基线 301/98/61/57/2"——基线是 2026-11-20 的 `build/refactor_baseline`（`macros.txt` 610 行），落后于工作区 11-21/11-22 的改动 | **证实**（"可运行"成立；但"现行闸门全绿"不成立 → B0b 的 `save-baseline` 重锚是**必要前置**，不是可选项） | `tools/refactor_gate.py:85-90,177,219,393,426`；`build/refactor_baseline/*`；命令见 §4-C6 |
| 25 | L224「G1' 参数宏集合为上批超集，可机器校验」 | 看现有宏导出机制 | **可机器校验**：`dump_macros()` 用 `arm-none-eabi-gcc -E -dM -Iconfig config/robot_infantry_2026.h` 导出全部宏文本（基线 610 行 = 220 参数宏 + 编译器内建），`--allow-new-macros` 分支（`refactor_gate.py:245-247`）已是"无消失即超集"判据。可靠性限制：比较的是**定义文本**而非展开值（`YAW_ESC_OUT_MAX` 文本里带开关名），因此"派生值变化"只能靠开关自身值变化被捕获；`board.h:18` 的条件定义（`#ifndef` 保护）仍能正常导出，config 内无"被禁用 `#if` 包住的定义" | **证实** | `tools/refactor_gate.py:85-90,234-250`；`build/refactor_baseline/macros.txt` |
| 26 | 代码侧对应件：heat_guard 的 allowance/buff/两级闸门、`modules/super_cap`、referee 0x0201/0x0204/0x0208/0x0209 解析 | 逐项查实现 | 全部存在：①两级闸门 `heat_guard.c:75 CalcGates`（gate_single/gate_hold）+ 状态机 `NORMAL/SINGLE/HOLD`（`heat_guard.h:25-27`，`heat_guard.c:262-285`）；②allowance 硬闸门 `heat_guard.c:248-259,328,384`（宏 `heat.h:17 HEAT_GUARD_ALLOWANCE_ENABLE 0`）；③冷却增益 `heat.h:88 HEAT_GUARD_COOLING_USE_BUFF 1` + `heat_guard.c:172-173` 读 `ref->BuffMusk.cooling_buff`；④`modules/super_cap/super_cap.c/h` 存在（`SuperCapInit` 无调用者=未启用，与"2027 战略件"一致）；⑤referee：`referee_protocol.h:82/85/88/89` 定义 0x0201/0x0204/0x0208/0x0209，`rm_referee.c:103-127` 逐 ID `memcpy` 解析，0x0201 结构含 `chassis_power_limit`（`referee_protocol.h:194`）；`PowerControl` 已无实现（`motor_task.c:6,25` 注释）与"随 R8 删除"一致 | **证实** | `application/shoot/heat_guard.c:75,172-173,248-259,262-285`；`modules/referee/referee_protocol.h:82,85,88,89,194`；`modules/referee/rm_referee.c:103-127` |
| 27 | （扩展）L250/L8「先收尾旧重构（实车回归 + **R5b/R4b**）」「B0a：实车回归→**R5b/R4b**→再实车回归」 | 查 DEVLOG 闭环状态 | **已过时**：R5b 已完成（`RM_DEVLOG.md:1776` DEV-20261120-11"共享类型下移（反向依赖归零）+ 死路径清理"）；R4b **已决定不做**（`RM_DEVLOG.md:1805` "R4b（Task 内联块抽取）**决定不做**"）；重构后实车回归 2026-11-20 PASS（`:18`、`:1766` DEV-20261120-10） | **证伪**（状态过时：B0a 多算了 R5b/R4b，实际只剩"重新锚定基线 + 现树回归") | `RM_DEVLOG.md:1776`、`:1805`、`:1766` |
| 28 | （扩展）L42「3 套主机测试(chassis/ax650/cmd_watchdog)」 | 查 `tests/` 与两个 runner | 结构属实：`run_chassis_tests.ps1` 内含 `chassis_remote_test`（两变体）+ `cmd_watchdog_test`；`run_ax650_tests.ps1` 内含 `ax650_pose_test` + `ax650_uart1_test`（O2/Ofast 双档）。但 **RM-012 未闭环**：`run_ax650_tests.ps1` 编译失败（`RM_DEVLOG.md:66` 记"CI 因此持续红"），故"3 套全绿"不成立（G3 作为现行闸门的前提需打折） | **部分证实** | `tests/run_chassis_tests.ps1:5-34`；`tests/run_ax650_tests.ps1:8-23`；`RM_DEVLOG.md:66` |
| 29 | L251「`super_cap`[2027 战略件]、kalman[活]等"明确不删"判断」 | 查引用链 | super_cap：模块存在、`SuperCapInit` 无调用者（死组件但战略保留，判断合理）；kalman：`QuaternionEKF.h:15` 引用、`ins_task.c:169` 1kHz 调用（活，判断正确）；DM/HT：见 #14（活桩，正确） | **证实** | `modules/super_cap/super_cap.c:32`（无调用者）；`modules/algorithm/QuaternionEKF.h:15` |
| 30 | L160「遥测环 5 通道」/「`.ccmbss` NOLOAD 启动不清零」 | 查 telemetry 与链接脚本 | 5 环属实（`telemetry.c:18-26`），宏 `TELEMETRY_CCM` 在 `__arm__` 下为 `section(".ccmbss")`（`telemetry.h:33-37`），`TelemetryInit` 运行期清零（`telemetry.c:28-43`，`robot.c:139` 调用）；`ucHeap` 同段（`freertos.c:52`）| **证实** | `modules/telemetry/telemetry.h:33-37`；`application/robot.c:139`；`Src/freertos.c:52` |

---

## 2. 证伪与部分证实汇总（按严重度）

### S1 —— 会把工作量/风险算错（必须改）

1. **T07 malloc 断言整体失真（#12，L40/L251 + 摘要行 L8）**
   方案写"21 处**运行期可达** malloc 集中在 **5 个活文件**（含 user_lib）"。实际：全仓 47 处；非 kalman 20 处、分布 **16 文件**；其中活路径只有 **7 处 / 5 文件**（`bsp_can`/`bsp_pwm`/`buzzer`/`message_center`/`dji_motor`），`user_lib` 的 malloc 在**无调用者**的 `zmalloc()` 里；且**没有一处**在周期任务循环内——它们是启动注册期一次性分配，稳态已经零 malloc。
   危害：①把"待去 malloc"的范围从 7 处虚报成 21 处；②点名了一个死文件、漏掉了 `bsp_pwm.c`（蜂鸣器 `PWMRegister` 真正走的分配点）；③"运行期可达"会让 T07 验收脚本按"运行期"判据写，结果与代码语义对不上（应写成"启动期豁免清单 7 处 + 稳态 0"）。
2. **`#if` 基数 186 是口径错误（#2/#3，L47/L191/L252）**
   实际 193（业务层）／202（+tests）／376（+CubeMX 生成文件）；186 = 只差漏掉 7 处 `#elif`（全在 `modules/imu/`）。另外 193 里 78 处是 include guard，真条件编译点 115。
   危害：B1 的 `check_macros.py` 按 186 建账本，起点就少 7 条，且会把 78 条 include guard 当"裁剪点"登记——账本噪声 ~45%，直接影响 B1 的"3~5 会话"工期估计与 T14 验收。
3. **while(1) 的"11+"与"白名单 4 处"两个数字都不能当验收判据（#17/#18，L35/L126/L128）**
   实测 33 处 `while(1)`（+6 处任务体 `for(;;)`）：注册/初始化路径 18 处（纯 bsp 9 处）、运行期传输路径 12 处、故障复位 1、INS 启动 fail-fast 1、任务体 7。白名单 4 类落在代码上是 **10 个点**（ins 1 + dji 5 + can 2 + msg 2），写成"仅剩白名单 4 处"的 grep 验收永远不通过。L126 列的 `servo` 根本没有 `while(1)`。
   危害：T02 的验收判据不可执行；B1/B3 排程若按"11+ 处要改"估算，实际要动的是 18 处（其中 15 处属死代码文件 bsp_iic/spi/gpio 内的 11 处，B0b 删文件后自然消失——这个依赖关系方案没写，容易重复计工）。

### S2 —— 会影响验收/设计前提（应改）

4. **伞头"展开顺序不敏感"已有反例（#11，L191）**：`ax650.h:19` 的 `#if` 依赖 `chassis.h:286`，顺序敏感；§3.9 落地机制第 3 条把"顺序不敏感"当既有属性来"变成可验证"，前提是错的。
5. **死代码删除不是"独立提交"的纯删文件（#16，L251）**：需同步删 `motor_task.c:9` 的 `#include "LK9025.h"` 并改 `Makefile` 10+ 条；否则 Q 编译断裂。B0b 的"零行为、独立提交"应补这句。
6. **旧重构 R5b/R4b 已闭环（#27，L250/L8）**：R5b 已完成、R4b 已决定不做、实车回归已 PASS；B0a 按方案写法会多算一个批次。
7. **现行闸门不绿（#24/#28）**：`refactor_gate.py check` 当前 G1/G4 双 FAIL（基线停在 2026-11-20），`run_ax650_tests.ps1` 编译失败（RM-012）；"B0a 用现行 G1~G7"与"G3 ALL PASS"作为现行前提需先重锚/修测试。

### S3 —— 数字/口径不准，但影响小（可顺手改）

8. **`ONE_BOARD` "6 文件"是混合口径（#4）**：7 文件出现／6 文件含预处理指令／5 文件为裁剪点；与本句"各 3 文件"口径不一致。
9. **tools 8 个（#23）**：仅指 `tools/*.js`，未说明不含 `tools/lib/preflight.js`（它也读 DWARF 静态量）；"静默失效"言过其实（多数会报错/0 样本）。
10. **`3 套主机测试`（#28）**：结构成立但 ax650 套当前红，属"存在≠全绿"。

### 证实项（无需改，供复核）

config 220 宏（#1）、四个开关各 3 文件（#5–#8）、派生宏/死三元式（#9/#10）、kalman 27 malloc 启动期一次性 + QEKF 引用 + 1kHz（#13）、DM/HT 活桩（#14）、死代码 2124 行（#15）、6 话题（#19）、任务周期/优先级（#20）、遥测 26,930B/CCM/RAM 49.1%/ucHeap（#21）、DEVICE_USART_CNT=3（#22）、manifest/audit/gate 存在可用（#24）、G1' 可机器校验（#25）、heat_guard/super_cap/referee 解析（#26）。

### 无法核实（说明原因）

- **RAM 49% / 27KB 的"实车"结论**：静态段与 map 已证实（#21），但"实车不 HardFault、遥测 tag 正常"需上电验证（方案本身也标"待实车"），本次只读审查无法核实。
- **1kHz"实测"**：只能核实名义周期（`osDelay(1)`），无示波/计数证据；方案未声称实测值，按名义值成立。
- **G9（挂任务 2s 内复位）**：需台架注入，不在只读范围。

---

## 3. 我给方案作者的修订清单（可直接粘贴替换）

> 行号指现行 358 行版本；"原文"为可定位片段。

**R1｜L47（T14 现状列）**
- 原文：`config 220 宏、全仓 186 个 #if 点、开关散布至 6 文件、派生宏藏开关、死三元式、编译期/运行期关断混用无准则`
- 建议：`config 220 宏（对象式，分布 gimbal 69/chassis 45/shoot 34/ax650 29/heat 18/kbm 15/remote 4/board 3/protect 3）、业务层 193 个 #if 系使用点（application+modules+bsp+config 的 .c/.h 含 #if/#ifdef/#ifndef/#elif；含 tests 202、含 CubeMX 的 Inc/Src 376；其中 78 处为 include guard，真条件编译点 115）、开关宏散布最多的是 ONE_BOARD（预处理指令 6 文件／裁剪点 5 文件，另 robot.h 仅注释）、派生宏藏开关、死三元式、编译期/运行期关断混用无准则`

**R2｜L191（§3.9 现状实测）**
- 原文：`现状实测:config 层 220 个宏、全仓 186 个 #if 系使用点;开关宏散布面最大的是 ONE_BOARD(6 文件)、AX650_UART1_ENABLE/YAW_AUTO_VOFA_USART6_ENABLE/YAW_AUTO_KF_ENABLE/GIMBAL_RECENTER_ENABLE(各 3 文件)`
- 建议：`现状实测:config 层 220 个宏;业务层 193 个 #if 系使用点(口径:application+modules+bsp+config 的 .c/.h,含 #if/#ifdef/#ifndef/#elif;含 tests 202、含 CubeMX Inc/Src 376;其中 78 处为 include guard,真条件编译点 115);开关宏裁剪点散布面最大的是 ONE_BOARD(5 文件,另有 board.h 定义点与 robot.h 注释各 1 处)、AX650_UART1_ENABLE/YAW_AUTO_VOFA_USART6_ENABLE/YAW_AUTO_KF_ENABLE/GIMBAL_RECENTER_ENABLE(各 3 文件,均不含 config 定义点)`

**R3｜L191 末句（伞头约定）**
- 原文：`伞头"子文件不互 include、展开顺序不敏感"是靠纪律维持的脆弱约定。`
- 建议：`伞头"子文件不互 include"成立;但"顺序不敏感"已有反例——ax650.h 的 #if 依赖 chassis.h 定义的 CHASSIS_VOFA_UART1_ENABLE,调换 include 顺序会静默跳过 #error 冲突守卫。故 §3.9 的治理目标应为"消除 config 内跨子文件 #if",而非仅"断言展开顺序不敏感"。`

**R4｜L40（T07 现状列）**
- 原文：`实测 21 处运行期可达 malloc 集中在 5 个活文件(bsp_can/buzzer/message_center/dji_motor/user_lib)`
- 建议：`实测全仓 47 处 malloc 调用点(17 文件)=kalman_filter 27 处(启动期一次性,INS_Init 幂等守卫)+其他 20 处;其中仅 7 处位于启动注册路径的活文件(bsp_can:153/bsp_pwm:40/buzzer:52/message_center:54,66,72/dji_motor:221),其余 13 处在死代码或未接线模块(user_lib 的 zmalloc 无调用者);无一处位于周期任务循环——稳态已零 malloc,待清理的是这 7 处启动期分配`

**R5｜L8（v1.3 变更头注）**
- 原文：`⑤T07 口径修正(活 malloc 仅 5 文件,kalman 启动期豁免)`
- 建议：`⑤T07 口径修正(全仓 47 处 malloc;仅 7 处位于启动注册路径的活文件(bsp_can/bsp_pwm/buzzer/message_center/dji_motor),kalman 27 处为启动期一次性并豁免)`

**R6｜L251（B0b 范围）**
- 原文：`死代码先删(零行为、独立提交,~2100 行:oled/ist8310/bluetooth/TFmini/unicomm/standard_cmd/bsp_spi/bsp_iic/bsp_gpio/LK;明确不删:kalman_filter[活,QEKF 骨架,启动期 malloc 豁免登记]、DM/HT[活桩留置不迁]、super_cap[2027 战略件])`
- 建议：`死代码先删(零行为,1 个提交,实测 2124 行:oled 777/bsp_spi 299/LK 283/bsp_iic 273/bsp_gpio 169/ist8310 164/bluetooth 95/TFmini 64/unicomm 0/standard_cmd 0;需同步删 motor_task.c:9 的 #include "LK9025.h" 与 Makefile 10+ 条源登记,CMake 为 GLOB 自动;明确不删:kalman_filter[活,QEKF 骨架,启动期 malloc 豁免登记]、DM/HT[活桩留置不迁]、super_cap[2027 战略件])`

**R7｜L35（T02 现状列）**
- 原文：`规则有、机制无:bsp 注册路径 11+ 处裸 while(1)`
- 建议：`规则有、机制无:全仓 33 处 while(1)(+6 处任务体 for(;;)),其中注册/初始化路径 18 处(纯 bsp 9 处)、运行期传输/发送错误路径 12 处;B0b 删 bsp_iic/spi/gpio 后 bsp 侧注册路径剩 5 处,其余在 modules(buzzer/ist8310/dji_motor/message_center)`

**R8｜L126（§3.1 白名单段）**
- 原文：`其余现存 11+ 处裸 while(1)(usart/pwm/spi/iic/buzzer/servo 注册错误)全部改 err_report。`
- 建议：`其余现存 18 处注册/初始化路径裸 while(1)(bsp_can/bsp_iic/bsp_pwm/bsp_spi/bsp_usart/bsp_tools/buzzer/ist8310/dji_motor/message_center;servo 无 while(1))全部改 err_report;另 12 处运行期传输错误路径(集中在 bsp_iic/bsp_spi/bsp_usart)随 B4 一并改造。`

**R9｜L128（§3.1 验收）**
- 原文：`红旗扫描 while(1) 仅剩白名单 4 处;每条错误码有处理策略表项。`
- 建议：`红旗扫描 while(1) 仅剩白名单 4 类共 10 个代码点(ins_task:111;B dji_motor:81,116,144,162,170;bsp_can:141,148;message_center:24,34);每条错误码有处理策略表项。若改为文件级白名单,则写"白名单 4 个文件"。`

**R10｜L250（B0a）**
- 原文：`实车回归(重构总方案 §9 清单,用户执行)→ R5b/R4b → 再实车回归`
- 建议：`现树实车回归(重复一次即可;R5b 已完成 DEV-20261120-11,R4b 已决定不做 DEV-20261120-12,重构后回归已 PASS DEV-20261120-10)→ 修 RM-012(ax650 测试桩)→ save-baseline 重锚`

**R11｜L8（v1.3 头注）与 §0/§7 相关表述**
- 原文：`④先收尾旧重构(实车回归 + R5b/R4b)再启动`
- 建议：`④先收尾旧重构(重锚当前基线 + 修 RM-012;R5b 已完成、R4b 已决定不做、重构后实车回归已 PASS)再启动`

**R12｜L46（T13 现状列）**
- 原文：`tools/ 下 8 个 emberprobe 脚本按 DWARF 路径直读 motor_lf->measure.* … 字段改名/结构体拆分即静默失效(不报错,只是读不到数)`
- 建议：`tools/ 根目录 8 个 trace/doctor .js(不含 tools/lib/preflight.js,后者也读 motor_lf/gimba_IMU_data/chassis_last_ms 等静态量)按 DWARF 路径直读 file-scope static…字段改名/结构体拆分即无法取数(多数脚本会报 0 样本/失败块,doctor/odom 另有固件身份预检,但同样不校验符号名)`

**R13｜L42（T09 现状列）**
- 原文：`3 套主机测试(chassis/ax650/cmd_watchdog)`
- 建议：`3 套主机测试(chassis/ax650/cmd_watchdog;注:ax650 套当前编译失败,RM-012 待修)`

**R14｜L251（B0b 的 tools 句）**
- 原文：`出《工具符号冻结清单》(tools/ 8 脚本全部 DWARF 表达式落表)`
- 建议：`出《工具符号冻结清单》(tools/ 根目录 8 个 .js + tools/lib/preflight.js 的 DWARF 表达式全部落表)`

---

## 4. 复现命令（PowerShell 5.1，工作目录 = 仓库根）

**C1 config 宏数与分布**
```powershell
Get-ChildItem -Recurse config -File -Include *.h | ForEach-Object {
  $n = ((Get-Content -Encoding utf8 $_.FullName) |
        Select-String -Pattern '^\s*#\s*define\s+[A-Za-z_]\w*').Count
  "{0,-26} {1}" -f $_.Name, $n
}
# 合计应为 220
```

**C2 #if 系使用点（三档口径 + 186 的反推）**
```powershell
function Count-If([string[]]$dirs) {
  $n = 0
  foreach ($d in $dirs) {
    Get-ChildItem -Recurse $d -File -Include *.c,*.h | ForEach-Object {
      $n += ((Get-Content -Encoding utf8 $_.FullName) |
             Select-String -Pattern '^\s*#\s*(if|ifdef|ifndef|elif)\b').Count
    }
  }
  $n
}
Count-If @('application','modules','bsp','config')                    # 193
Count-If @('application','modules','bsp','config','tests')            # 202
Count-If @('application','modules','bsp','config','tests','Inc','Src')# 376
# 186 的来源：上一条去掉 #elif → 193-7=186（7 处 #elif 全在 modules/imu）
Get-ChildItem -Recurse modules\imu -File -Include *.c,*.h |
  ForEach-Object { (Get-Content -Encoding utf8 $_.FullName) |
    Select-String -Pattern '^\s*#\s*elif\b' }
```

**C3 malloc 调用点分布（活/死路径判定见正文）**
```powershell
Get-ChildItem -Recurse -File -Include *.c |
  Where-Object { $_.FullName -notmatch '\\Drivers\\|\\Middlewares\\|\\build\\|\\.agents\\|\\tests\\|\\Src\\' } |
  ForEach-Object {
    $c = ((Get-Content -Encoding utf8 $_.FullName) |
          Select-String -Pattern '(\bmalloc|\buser_malloc|\bcalloc|\brealloc)\s*\(').Count
    if ($c) { "{0,3}  {1}" -f $c, $_.FullName.Substring((Get-Location).Path.Length+1) }
  }
# 合计 47；kalman 27；其余 20
```

**C4 死代码行数**
```powershell
@('modules\oled','modules\ist8310','modules\bluetooth','modules\TFminiPlus',
  'modules\unicomm','modules\standard_cmd','bsp\spi','bsp\iic','bsp\gpio',
  'modules\motor\LKmotor') | ForEach-Object {
    $n = (Get-ChildItem -Recurse $_ -File | ForEach-Object { (Get-Content -Encoding utf8 $_.FullName).Count } |
          Measure-Object -Sum).Sum
    "{0,-28} {1,5}" -f $_, $n
  }
# 合计 2124
```

**C5 while(1) 全量清单（按函数归类）**
```powershell
Get-ChildItem -Recurse -File -Include *.c |
  Where-Object { $_.FullName -notmatch '\\Drivers\\|\\Middlewares\\|\\build\\|\\.agents\\|\\tests\\|\\Src\\' } |
  ForEach-Object {
    $lines = Get-Content -Encoding utf8 $_.FullName
    for ($i=0; $i -lt $lines.Count; $i++) {
      if ($lines[$i] -match 'while\s*\(\s*1\s*\)|for\s*\(\s*;\s*;\s*\)') {
        "{0}:{1}: {2}" -f $_.FullName.Substring((Get-Location).Path.Length+1), ($i+1), $lines[$i].Trim()
      }
    }
  }
```

**C6 闸门现状（只读命令；不要跑 save-baseline）**
```powershell
$py = 'C:/Users/ZhuanZ/.workbuddy/binaries/python/versions/3.13.12/python.exe'
& $py tools/refactor_gate.py check-links                      # PASS, exit 0
& $py tools/refactor_gate.py check --no-build --no-tests      # 可运行；当前 G1/G4 FAIL（基线 2026-11-20）
& $py tools/refactor_gate.py --help 2>$null; & $py tools/refactor_gate.py  # 打印三子命令说明, exit 2
```

**C7 map 段与遥测/堆对象**
```powershell
Get-Content -Encoding utf8 build\basic_framework.map |
  Select-String -Pattern '^\.(text|data|bss|ccmbss|ccmram)\s'
Get-Content -Encoding utf8 build\basic_framework.map |
  Select-String -Pattern 'ucHeap|telemetry_(yaw|pitch|chassis_cmd|chassis_fb|shoot)'
# .ccmbss 0xd6a0=54944; ucHeap@0x10000000; 5 环 0x10006d60..0x1000c190（单环 5386B）
```

**C8 ONE_BOARD 逐文件行号**
```powershell
@('application\chassis\chassis.c','application\chassis\chassis_kinematics.c',
  'application\cmd\robot_cmd.c','application\robot.c','application\robot.h',
  'application\robot_def.h','config\robot_infantry_2026\board.h') | ForEach-Object {
    $m = (Get-Content -Encoding utf8 $_) | Select-String -Pattern '\bONE_BOARD\b'
    "{0,-46} lines={1}" -f $_, (($m | ForEach-Object { $_.LineNumber }) -join ',')
  }
# 7 文件；robot.h 仅注释；board.h 为定义点
```

---

## 5. 结论一句话

方案的大部分定量断言可复现（220 宏、kalman/DM-HT/话题/任务周期/遥测与 RAM/USART/heat_guard/referee 全部证实），但**三条用于驱动排程与验收的定量断言（T07 的 21 处/5 活文件/运行期、186 个 #if、while(1) 11+/白名单 4 处）不可复现或口径自相矛盾**，另有 R5b/R4b 状态过时、闸门当前为红；建议按 §3 的 R1–R14 修订后再排 B0b/B1。
