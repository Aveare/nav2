# RTT 日志与调试通道

> 本车唯一的程序日志出口是 SEGGER RTT(SWD 口),不走任何串口。
> 本文整理它的配置参数、带宽预算、以及当前镜像会打出的**全部**日志行的字段含义。
> 现场读日志前看 §5,改配置前看 §3/§4。

---

## 1. 链路结构

```
STM32F407 ──SWDIO/SWCLK/GND── 调试器(有线或无线) ──USB/网络── PC(RTT Viewer / Ozone / J-Link RTT Client)
             ↑ 只占调试口，不占任何 UART
             ↑ 数据是目标端 RAM 里的一块环形缓冲，调试器直接读内存，目标端不参与发送时序
```

- **不需要 SWO 引脚**。RTT 走的是"调试器直接读内存",与 SWO 的单向 trace 是两套机制。
- **与 USART6 完全独立**。插着调试器看日志,同时 USART6 收裁判系统 0x0201/0x0202,
  是正常配置;裁判系统的热量/冷却/弹速/功率/UI 固定走 USART6。
- **USART1 的调试波形只看 UART1 宏**:`CHASSIS_VOFA_UART1_ENABLE=1` 时走 USART1
  JustFloat;`AX650_UART1_ENABLE=1` 时 USART1 归 AX650 自瞄。两个宏由 config 伞头互斥检查。
- **USART6 可以单独切到自瞄 yaw VOFA**:`YAW_AUTO_VOFA_USART6_ENABLE=1` 时暂停裁判 UI,
  输出 yaw 目标/反馈/误差和 PID 中间量;置 0 时恢复裁判系统。它与 UART1 的系统调试互不替代。

出处:`ports/stm32f4/log_port.c:25`(`SEGGER_RTT_vprintf`)、`ports/stm32f4/log_port.h:16`(`LOG_BUFFER_INDEX 0`)。

---

## 2. 当前配置参数

配置文件:`Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h`

| 参数 | 当前值 | 行号 | 含义 |
|---|---|---|---|
| `LOG_BUFFER_INDEX` | `0` | `log_port.h:16` | 只启用 0 号上行缓冲 |
| `BUFFER_SIZE_UP` | `1024` B | `:95` | 目标 → 主机,日志出口 |
| `BUFFER_SIZE_DOWN` | `16` B | `:99` | 主机 → 目标,本车不用 |
| `SEGGER_RTT_MAX_NUM_UP_BUFFERS` | `3` | `:83` | 最多 3 个上行缓冲,实际只用 1 个 |
| `SEGGER_RTT_MAX_NUM_DOWN_BUFFERS` | `3` | `:91` | 同上,未用 |
| `SEGGER_RTT_PRINTF_BUFFER_SIZE` | `64` B | `:103` | 单次 printf 的分块中转缓冲 |
| `SEGGER_RTT_MODE_DEFAULT` | `NO_BLOCK_SKIP` | `:107` | **满则丢,永不阻塞**——控制循环不会被日志卡住 |
| `SEGGER_RTT_MEMCPY_USE_BYTELOOP` | `0` | `:123` | 用 `memcpy`,不用逐字节循环 |
| `SEGGER_RTT_MAX_INTERRUPT_PRIORITY` | `0x20` | `:147` | 临界区 BASEPRI 门限(F407 = ARM_ARCH_7EM,走 BASEPRI 分支) |
| `DISABLE_LOG_SYSTEM` | 未定义 | — | `#if 0`,即日志全开 |

**`NO_BLOCK_SKIP` 是这里最重要的一条**:缓冲区满时直接丢新数据,`SEGGER_RTT_vprintf` 立即返回。
200Hz 的控制任务调用 `LOGINFO` 不会被调试器拖慢,这是能在控制周期里打日志的前提。

### 一处可选优化(未实施)

`SEGGER_RTT_MAX_INTERRUPT_PRIORITY = 0x20`,而 FreeRTOS 自己的门限是
`configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 << (8-4) = 0x50`(`FreeRTOSConfig.h:111,118`,F407 为 4 位优先级)。
BASEPRI 写 `0x20` 会连优先级 2~4 的中断一起挡掉,比 FreeRTOS 严格。

- 现状不构成问题:临界区内只有一次 ≤100 B 的 `memcpy`,量级 1 µs 以内;而且**所有日志都在任务里打,
  没有任何中断里打日志**,理论上这个锁甚至可以更松。
- 若要收紧,把 `:147` 改成 `0x50` 与 FreeRTOS 对齐即可,收益是高速中断抖动的理论改善,实测无感。
- **禁止改成 `BLOCK_IF_FIFO_FULL`**:那会让写日志的 200Hz 任务在缓冲满时原地等待,直接拖垮控制周期。

---

## 3. 带宽预算

按当前镜像**实际开启**的三个日志估算(`[换算]`,以各行的字符数为准,含前缀与 CRLF):

| 日志 | 频率 | 单行约 | 占用 | 开关宏 | 当前 |
|---|---|---|---|---|---|
| `[heat]` 心跳 | 1 Hz | 42 B | 42 B/s | `HEAT_GUARD_LOG` | **1** |
| `[shootcal]` | 2 Hz | 93 B | 186 B/s | `SHOOT_CALIB_LOG` | **1** |
| `[chcal]` | 2 Hz | 70 B | 140 B/s | `CHASSIS_CALIB_LOG` | **1** |
| 合计 | | | **≈ 370 B/s** | | |

结论:

- 缓冲 1024 B ÷ 370 B/s ≈ **2.8 s**,这是"调试器完全不取"时的容忍时间;
- 调试器只要以 ≥1 Hz 的节奏取一次就不会丢,任何有线/无线调试器都远超这个要求;
- **缓冲区不是瓶颈**。真的出现缺行,先怀疑链路掉线或调试器 RTT 未使能,而不是缓冲太小。

对照:配置文件里"四个 `*cal` 日志同开会 1.6 s 内溢出"的说法,对应的是五路全开
(≈ 786 B/s → 1024/786 ≈ 1.3 s,与 1.6 s 同量级)。它描述的是**零排空的最坏情况**,
调试器在线时并非如此。这条注释是保守估计,不是实测。

### 扩容方法(仅在确认真丢行时)

```c
// Middlewares/Third_Party/SEGGER/Config/SEGGER_RTT_Conf.h:95
#define BUFFER_SIZE_UP  (4096)   // 原 1024
```

RAM 余量:`[换算 2026-11-20 构建]` 107920 B / 131072 B,剩约 15 KB,加 3 KB 无压力。

---

## 4. 排查速查

| 现象 | 先查 |
|---|---|
| 终端完全空白 | 调试器不支持 RTT / RTT Viewer 未连 / 芯片未运行(卡在 `Error_Handler`) |
| 有启动行、无周期行 | 对应的 `*_LOG` 宏为 0,或该任务未启动(`robot_task.h` 里的 `TASK_*_ENABLE`) |
| 周期行时有时无 | 链路问题;其次才是缓冲(见 §3) |
| `[heat]` 一直 `bypassed (bench mode)` | 裁判系统未上线,热量守卫直通。**此时枪口无保护** |
| `[chcal]` 的 `p=` `be=` 恒为 -1 | 裁判系统未连/未上线(USART6);USART6 专职裁判,不应被 VOFA 占用 |
| 移植到新板后全是乱码/空白 | 检查 `BUFFER_SIZE_UP` 是否被 0 覆盖、`SEGGER_RTT_Init` 是否被调用 |

---

## 5. 日志字段字典

前缀由宏决定:`I:`(绿)/`W:`(黄)/`E:`(红),见 `log_port.h:29`。
**`LOGxxx` 不支持浮点格式化**,所有小数都经 `log_float_to_str()` 转成字符串再打。

### 5.1 启动期(一次性)

| 行 | 含义 |
|---|---|
| `[freeRTOS] xxx Task Start` | 各任务启动确认,按 INS → MOTOR → Daemon → ROBOT → UI 顺序 |
| `[freeRTOS] xxx Task is being DELAY! dt = [%d ms]` | 该任务超期,值 = 实际周期。**调度被拖垮的第一个信号** |
| `[referee] waiting for robot_id` | 1Hz 重复:裁判主控未连/未上电/机器人 ID 不匹配 |
| `[referee] robot_id received (N)` | 裁判通信建立,`N` 为主控里配置的 ID |
| `[heat] no referee heat data, guard bypassed (bench mode)` | 从未拿到有效 Q0,守卫直通 |
| `[CMD] input source: remote control / keyboard-mouse` | 当前输入源 |
| `[calib] 已加载标定: ...` / `无有效标定记录` | Flash 标定记录加载结果 |

### 5.2 `[heat]` —— 热量守卫(1 Hz)

```
I: [heat] q=118/240 c=40/s state=NORMAL
W: [heat] SINGLE -> HOLD (q=213/240 c=40/s)
```

| 字段 | 单位 | 含义 |
|---|---|---|
| `q` | 热量点 | **估计器内部账本**,不是裁判原始值。打一发 +10,按 `c/10` 阶梯扣冷却 |
| `/240` | 热量点 | `Q0` 热量上限,每帧从 0x0201 重取;升级瞬间会跳变 |
| `c` | 点/s | 冷却速率 = 0x0201 `shooter_barrel_cooling_value` + 0x0204 `cooling_buff` |
| `state` | — | `OFF` 直通 / `NORMAL` / `SINGLE` 只放单发 / `HOLD` 拒发 |
| `(degraded)` | — | 曾上线但 0x0202 断流 >300ms,已禁止花余热并把射速压到 `c/10` |

状态迁移只在**跳变时**打 `W:` 行——操作手看到的"枪卡了一下"对应这行。

### 5.3 `[shootcal]` —— 发射标定(2 Hz,`SHOOT_CALIB_LOG`)

```
I: [shootcal] fl=4002.0 fr=3998.0 ldr=0.0 cur=12.0 icl=812.0 icr=805.0 ifl=790.0 ifr=783.0
```

| 字段 | 单位 | 含义 |
|---|---|---|
| `fl` `fr` | RPM(转子) | 左/右摩擦轮实际转速,带符号。**正常时符号相反**:左轮 `motor_reverse_flag = REVERSE`,目标是 −4000;右轮为 +4000 |
| `ldr` | RPM(转子) | 拨盘转速,带符号 |
| `cur` | 电调原始码 | **拨盘**实际电流(C610 满量程 10000),堵转判据 `LOADER_JAM_CURRENT` 用的就是它 |
| `icl` `icr` | 电调原始码 | 左/右摩擦轮**电流指令**(速度环输出,上限 6000 码 ≈ 7.3 A,C620 满量程 16384) |
| `ifl` `ifr` | 电调原始码 | 左/右摩擦轮**实际电流反馈**(电调回传,已低通) |

指令与反馈分列的意义:

| 现象 | 含义 |
|---|---|
| `icl` 长期贴 6000 且 `fl` 到不了目标 | 速度环饱和:负载偏重或 Kp 偏小 |
| `icl` 正常但 `ifl` 明显更小 | 电调没吃满:电池跌压 / C620 限流 |
| 两轮 `ifl` 差很多 | 摩擦轮预压/轴承/装配不对称 |

### 5.4 `[chcal]` —— 底盘标定(2 Hz,`CHASSIS_CALIB_LOG`)

```
I: [chcal] vx=120.0 vy=-3.2 wz=299.5 gz=0.0 p=12.4 be=60.0
```

| 字段 | 单位 | 含义 |
|---|---|---|
| `vx` `vy` | mm/s | 速度估计(轮速逆解),即反馈 |
| `wz` | deg/s | 逆解得到的底盘角速度 |
| `gz` | deg/s | **IMU 世界系** yaw 角速度。小陀螺瞄准保持时云台世界系静止,读数为 0,**不能当底盘转速用** |
| `p` | W | 底盘功率,0x0202 offset 4(float) |
| `be` | J | 缓冲能量,0x0202 offset 8 |

`p` / `be` 恒为 `-1` 说明裁判数据不可用(见 §4)。

### 5.5 `[yawcal]` / `[pitchcal]` —— 云台标定(2 Hz,默认关)

```
I: [yawcal] out=812.0 spd=145.3 cur=790.0 ff=36.5
I: [pitchcal] ang=12.4 out=530.0 spd=88.1 cur=512.0
```

| 字段 | 含义 |
|---|---|
| `out` | 该轴最终控制输出(电调原始码) |
| `spd` / `ang` | 关节角速度(deg/s)/ 角度(deg) |
| `cur` | 实际电流反馈(电调原始码) |
| `ff` | yaw 前馈分量(摩擦 + 惯量),与 `out` 分开看才能判断前馈是否过量 |

开关:`YAW_FF_CALIB_LOG` / `PITCH_FF_CALIB_LOG`,默认 0。

### 5.6 异常行(按需)

| 行 | 触发条件 | 处理 |
|---|---|---|
| `[shoot] loader jammed, output latched off; release dial to reset` | 拨盘堵转锁存 | 拨轮回中解除,别硬按 |
| `[shoot] loader jammed during feed, queue aborted` | 拨弹过程中堵转 | 同上,已发弹数清零 |
| `[shoot] heat HOLD, N-bullet request rejected` | 守卫拒发 | 等冷却,别连点 |
| `[shoot] heat SINGLE, 3-bullet degraded to 1` | 余热只够单发 | 正常降级 |
| `[shoot] feed wait timeout (friction not ready)` | 2000ms 内摩擦轮未到速 | 检查摩擦轮供电/目标转速 |
| `[CMD] authority: RC lost / kill switch / dial stop, zero force` | 控制权丧失 | 零力保护,不是故障 |
| `[gimbal] ECD回正目标异常` | 编码器标定值离谱 | 重新标定,别硬跑 |
| `[ax650] USART1 init failed` | 视觉链路 | 自瞄不可用,其余正常 |

---

## 6. 开关矩阵(改前必读)

| 宏 | 位置 | 当前 | 用途 |
|---|---|---|---|
| `DISABLE_LOG_SYSTEM` | 构建定义,当前**未定义** | 日志全开 | 上车比赛前应定义,整条日志系统编译期消失 |
| `HEAT_GUARD_LOG` | `config/infantry_2026/config.h:256` | 1 | 热量守卫 1Hz 心跳 |
| `SHOOT_CALIB_LOG` | `:169` | 1 | 发射标定 6~8 列 |
| `CHASSIS_CALIB_LOG` | `:354` | 1 | 底盘标定 6 列 |
| `YAW_FF_CALIB_LOG` | `:638` | 0 | yaw 前馈标定 |
| `PITCH_FF_CALIB_LOG` | `:703` | 0 | pitch 前馈标定 |

规矩:**标定结束就把对应开关置回 0**。不是为了省带宽(§3 已证明不是瓶颈),
而是避免新旧日志互相淹没,以及在比赛镜像里留下不必要的字符串(占 FLASH)。

---

## 7. 与 Ozone 的分工

- **RTT 终端**:看结构化日志(状态迁移、异常、标定数值)。人读。
- **Ozone live watch / 全局 `telemetry_*` 环**:看波形。机器读。
  `telemetry_yaw/pitch/chassis_cmd/chassis_fb/shoot` 五个环,各 256 样本 × 24 B,
  200Hz 下覆盖 1.28 s 历史,环形覆盖、无锁、静态 `.bss`。
- 两者共用 SWD,可同时开:Ozone 里既能看 watch 也能开 Terminal。

详见 `.Doc/guide/VSCode+Ozone使用方法.md`。

---

## 8. 连续落盘:tools/rtt_log.py

固件写不了文件(无文件系统),所以"存成 `.log`"一定是 **PC 端把 RTT 搬进文件**。

```bash
# 等解锁标记出现才开始记,下电自动收尾
python tools/rtt_log.py

# 连开机日志一起记
python tools/rtt_log.py --from-boot

# 加快轮询 / 防止忘关
python tools/rtt_log.py --interval 0.2 --max-min 30
```

**会话边界**:起点 = 固件打出 `[CMD] reinstate, robot ready`(`robot_cmd.c` 解锁沿);
终点 = 目标下电(连续 3 次读不到)或 Ctrl-C。文件名 `log/YYYYMMDD_HHMMSS.log`。

**为什么不用 `rtt server start`**:OpenOCD 0.12.0-dev 在本机的 rtt server 能连上但不转发
(WrOff 已涨、RdOff 为 0、TCP 收 0 字节),见 `tools/rtt_read.py` 文件头。

**为什么常驻一个 OpenOCD**:`rtt_read.py` 每读一次就 spawn 一次(启动+attach+shutdown),
而 1024 B 缓冲在 370 B/s 下只够撑 2.7 s,轮询一慢就开始覆盖丢数据。
`rtt_log.py` 只 attach 一次,之后走 telnet 命令口(4444)读内存,单轮毫秒级。

**两个必须记住的细节**:

1. **读完之后必须回写 `RdOff`**(`mww`),否则目标端认为缓冲仍是满的,
   `NO_BLOCK_SKIP` 会把后续日志全部丢掉。脚本每轮都写了。
2. **脚本会剥掉 ANSI 颜色码**再落盘(`--raw` 可保留),否则文件里全是 `\x1b[1;32m`。

**丢数据自检**:脚本每 5 s 打印环占用;若某次轮询发现 `WrOff` 越过游标一整圈,
会把丢失字节数累加并以 `dropped=` 收尾。**收尾行 `dropped 0 B` 才说明这次日志完整**。

**前置**:探针空闲(EmberProbe 扩展 / Ozone / RTT Viewer 不能同时占着),
目标上电且固件已跑起来,否则搜不到控制块。

`log/` 下只放现场数据,已在 `.gitignore` 里排除 `*.log`,不入库。

### 8.1 自动启动

自动启动的难点不是"把进程拉起来",而是**进程要能在没车的时候等**。
否则计划任务在开机时拉起一次、发现探针不在、退出,之后就再也不会抓了。

所以顺序是:`--loop` 让脚本自己等 → 再交给 Windows 拉起。

```bash
python tools/rtt_log.py --loop            # 常驻:下电收尾后回到等待,等下次解锁
python tools/rtt_log.py --loop --loop-delay 3
```

`--loop` 的行为:

| 阶段 | 表现 |
|---|---|
| 探针/目标不在 | 每 `--loop-delay` 秒重试一次,**只在首次和每 60 s** 打一行 `[等待]`,不刷屏 |
| 目标上电且解锁 | 新建 `log/<时间戳>.log`,开始记 |
| 目标下电 | 写收尾行、关文件,**回到等待**,不退出 |
| Ctrl-C | 整个进程结束 |

三种拉起方式,按省事程度排:

| 方式 | 操作 | 适合 |
|---|---|---|
| 双击 | `tools/capture_rtt.bat` | 台架单次调试 |
| 启动项 | `Win+R` → `shell:startup` → 放 `.bat` 的快捷方式 | 个人工作站,登录即生效 |
| 计划任务 | `schtasks /create /tn "RM-RTT-Logger" /tr "<工程>\tools\capture_rtt.bat" /sc onlogon /f` | 队伍公用机,可设"不管用户是否登录" |

`tools/capture_rtt.bat` 做四件事:`cd` 到工程根、按顺序找 Python(本地安装 → 本机绝对路径 →
`python`)、确保 `log/` 存在、以 `--loop` 运行并把控制台输出追加到 `log/_console.log`。
纯 ASCII,不含中文,避免 cmd 代码页把路径搞坏。

**单实例锁**:脚本启动时占住本地 4456 端口当互斥量。自启 + 手动再开一个时,第二个会立刻
打 `[错误] 已有一个抓取进程在跑` 并退出,不会两个进程抢 4444 端口互相打死。进程退出即释放,
不需要清理锁文件。

**想要完全无窗口**:把 `.bat` 里的 `"%PY%"` 换成同目录下的 `pythonw.exe`(输出仍然进
`log/_console.log`,只是没有黑框)。台架调试建议保留窗口,能直接看到 `[等待]` / `[会话开始]`。

### 8.2 关闭

| 场景 | 做法 |
|---|---|
| 有控制台窗口 | 窗口里 `Ctrl-C`(走正常收尾,文件里有 `session end` 行);或直接关窗口 |
| 无窗口(pythonw / 计划任务) | 双击 `tools/stop_capture.bat` |
| 命令行 | `netstat -ano \| findstr :4456` 拿到 PID → `taskkill /F /PID <pid>` |
| 取消自启 | 删掉启动项快捷方式;计划任务用 `schtasks /delete /tn "RM-RTT-Logger" /f` |

**强杀会留下 `openocd.exe`**:父进程被 `/F` 杀掉时 `finally` 不执行,常驻 OpenOCD 变成孤儿,
继续占着探针。后果有两个——下次抓取报 `unable to find a matching CMSIS-DAP device`,
以及烧录/Ozone 也连不上,很容易误判成硬件故障。
`stop_capture.bat` 会顺手把 `openocd.exe` 一起清掉;手动 taskkill 时要记得补一条
`taskkill /F /IM openocd.exe`。
