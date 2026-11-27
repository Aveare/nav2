# 哨兵 Sentry 开发台账

> 哨兵专属台账；分支 `sentry`。公共内容见根 `RM_DEVLOG.md`；文档索引见 `.Doc/robots/sentry/README.md`。
> 条目标题统一 `[SENTRY]` 前缀，便于与步兵/公共条目区分。

## 0. 当前状态

| 项目 | 内容 |
| --- | --- |
| 交付身份 | 2026 哨兵 Sentry，RMUC，单板 ONE_BOARD |
| 应用/参数 | `apps/sentry/` + `config/sentry_2026/`（伞头 + 子头） |
| 构建 | `.\build.ps1 sentry -Current` → `firmware/sentry.elf` |
| 功能状态 | 底盘/三轴云台/发射/遥控键鼠/仲裁/安全机制已手动整机实装；**NUC 是唯一上位机，通过 USB CDC 承载 NV v1 导航 + RMCV `0x1022/0x0503` 视觉**（旧 `0x1021` 仅兼容）；USART1 = 三轴编码器 VOFA；哨兵未装配 AX650；**pitch 已恢复使能且闭环稳定性符号已更正**（编码器闭环 `TORQUE_SIGN +1`、方向取 11-27 实测 `-1`、上电校准 0 位、枪管度软限位 ±15°、越界断力；摇杆 20°/s、死区 40；`SENTRY_AUTO_AIM_PITCH_ENABLE=0` 视觉不驱动 pitch，见 DEV-20261128-043140 / 040133）；**底盘功率控制临时关闭**（`CHASSIS_POWER_CTRL_EN=0`，自转掉电复测中） |
| 证据等级 | 三轴零位/方向/限位/PID 均 `[待实测]`，首次上电必须架空逐轴确认符号后再闭环；**大 yaw 小陀螺速度环 Ki=2.0 已台架验证**（2026-11-27，拖拽 163→81°）；pitch 方向/硬限位/水平位为 11-27 单次手推读数；**pitch 稳定性符号 `+1` 为推导值（大 yaw 同款电机台架外推 + 主机被控对象收敛/发散对照），硬件前提待首推 5° 确认**；pitch 上电 0 位/±15°/保持力矩 `[待实测]`（上电前枪管放行程中间）；自瞄世界 yaw 偏置、pitch 平台补偿、功率模型 `MODEL_SCALE/MARGIN` 待台架标定 |
| 已知缺口 | 哨兵功率模型参数沿用步兵，未在哨兵实车复核；`0x1022` 已传完整原始姿态但三轴轴向/符号/零位与 100 Hz 实链仍 `[待实测]`，pitch 水平绝对零位有效位保持 0；`est_wz` 低估 ~25%（SENTRY-02）；自瞄 pitch 映射把上电零位当作 0° 俯仰，未标定前关闭 |
| 最近验证 | 已合入最新 `origin/sentry` e36aad9；ARM 0E0W，text 126,288 / data 740 / bss 128,936；profile/pitch/sentry USB 全绿；未烧录/台架 |
| 下一步 | pitch 四步台架（零力手推方向 → 可选传动比 → 首推 5° 确认稳定性符号 → 松杆保持/重力前馈标定）→ 自转掉电复测（功率控制关闭）→ 实体 USB `0x1022` 抓包 → 0x0503 世界 yaw 对齐 → pitch 平台补偿标定（合成世界俯仰反馈）→ 功率模型复核 → 视觉连发台架 |

## 1. 链路与操作

- **USART1 = 云台三轴编码器 VOFA**（921600 8N1，JustFloat，3×float + `00 00 80 7F`）。
- **USB CDC = 唯一上位机链路**：NV v1 导航（`COMMAND/FEEDBACK/SYNC`，32 B）+ RMCV 视觉（`0x1022` 61 B / `0x0503` 39 B；旧 `0x1021` 兼容）；两协议共享字节流、各自按帧头重同步，`autonomy` 是唯一所有者。
- **USART6 = 裁判系统**（115200 8N1）。
- 自瞄激活：左中挡或键鼠 Shift；视觉 `shoot` 驱动连发；断帧 300 ms 回手动。
- 导航接管：`SENTRY_AUTONOMY_ENABLE=1`，左右拨杆中位 + 左右摇杆回中 + NV COMMAND 有效；摇杆动作立即回手动。
- 遥控/键鼠操作表见 `.Doc/robots/sentry/README.md`（迁移快照，以 `config/sentry_2026/` 为准）。

## 2. 参数与设计入口

| 内容 | 路径 | 状态 |
| --- | --- | --- |
| 参数证据 | `params/参数证据表.md`（待建，从 `config/sentry_2026/` 与 DEV 条目整理） | **待建** |
| 调参结论 | `params/调参结论.md` | **待建** |
| 设计说明 | `.Doc/robots/sentry/design/` | 现有 `RM_shaobin2功能移植.md` |
| 操作/协议 | `.Doc/robots/sentry/ops/导航协议与上位机.md` | USB 六类消息、帧格式与放行顺序 |
| 协议合同 | `apps/sentry/app/sentry_def.h` | 带布局守卫 |

## 3. 问题台账

| ID | 现象 | 状态 | 下一步/关闭条件 |
| --- | --- | --- | --- |
| SENTRY-01 | 大 yaw 传动比：1.30 推算作废（基准 est_wz 错），电机 192 / 真实底盘 188 ≈ **1.02 → 维持 1.0** | 已关闭 | DEV-20261127-091528；数齿可作为最终确认 |
| SENTRY-02 | `chassis_fb.est_wz` 低估底盘角速度 ~25%（IMU 实测 190 vs est 150，线缆钩住堵转时取得） | 待核实 | 外部核对（IMU 角速度/固定圈数计时）后标定轮半径或运动学系数；影响 NV 导航速度估计 |
| DEV-20261127-084627-A | 框架整链 harness 在未修改的步兵 `gimbal_imu_data` 重复定义处链接失败 | 基线问题 | 共享修复应走 `fix/shared-*`；本哨兵分支不跨兵种修改 |
| DEV-20261127-084627-B | G4 清单仍是 U3/U4 前旧基线，当前 `origin/sentry` 自带 9 处新增红旗 | 基线问题 | 在共享分支重建并评审 G4 基线；本提交未新增 HAL/句柄/死循环红旗 |

## 4. 近期开发条目

<!-- 格式:### DEV-YYYYMMDD-HHMMSS｜日期｜[SENTRY] 标题 -->

### DEV-20261128-042342｜2026-11-28｜[SENTRY] USB 上行切换 `0x1022` 完整姿态

**触发与批准**。用户提供 NUC—STM32 统一协议并明确要求按 `0xA5` 帧实现旧姿态 `0x1021` 与完整姿态 `0x1022`；该请求明确批准新增函数、扩展哨兵消息字段与调用链。

**实现**。
- 哨兵 USB 线上姿态由 31 B `0x1021` 切换为 61 B `0x1022`；旧 `vision_link_build_pose()` 和共享 `ax650_pose_pack()` 保留兼容。
- `sentry_gimbal_feed_t` 新增小 yaw/pitch 采样时间；大 yaw、pitch 使用 DM CAN `rx_sample_us`。DJI 实例尚无 CAN 硬时间，小 yaw 按协议约定暂取在线时 200 Hz 云台快照 tick，并在文档中明确其证据等级。
- `vision_link` 原子快照大 yaw IMU 四元数、三轴关节角与四路时间，补充弹速、robot_id、program_mode 和有效位。小 yaw 中位已有实测 ECD，bit4 置位；pitch 仍为上电参考位，bit5 保持 0。
- `autonomy` 继续独占 USB；NV v1 和 `0x0503` 接收不变，USB 忙时 `0x1022` 顺延且无新 INS 样本不重复发帧。

**验证**。共享层跨语言 61 B 黄金向量见 DEV-20261128-042039；`run_sentry_usb_tests.ps1`、`run_ax650_tests.ps1`、`run_chassis_tests.ps1`、`run_profile_tests.ps1` 均 PASS；同步最新 `origin/sentry` 后 ARM `build.ps1 sentry -Current` 0 error / 第一方 0 warning，text 126,244 / data 732 / bss 128,936；兵种范围、链接、命名、注释、源清单、宏、ABI、段占用均 PASS。

**既有基线问题**。`run_fwk_tests.ps1` 到 fullchain harness 时仍因步兵 `gimbal_imu_data` 重复定义失败；G4 旧清单仍报告当前 `origin/sentry` 已有的 8 处 HAL 句柄/`while(1)` 红旗，本次 `0x1022` 改动未新增这些调用点。

**待实测**。实体 USB 枚举与 100 Hz 帧率、四路时间单调性、三轴方向/零位、NUC 枪管姿态合成均需用户烧录后按 `ops/功能检查表.md` 验证；本次未烧录、未动实车。

**AI**：OpenAI Codex（GPT-5），2026-11-28。

### DEV-20261128-043140｜2026-11-28｜[SENTRY] pitch 根因更正：编码器闭环稳定性符号反了（正反馈）+ 枪管度/传动比口径 + 键鼠灵敏度对齐步兵

**触发**。用户要求参考步兵 pitch 的控制逻辑,并猜测"是不是电机的转动参数给大了,指令转 1° 实际转 10°"。期间合并了 `origin/sentry`(c0103df):含 NUC 配置重构(041135)、小陀螺大 yaw(041921),以及 11-27 晚 main 上的 pitch 实测(`DIRECTION -1`、水平位 `-2.58163`、硬限位 `-112.2°~-181.7°`,DEV-20261127-203112/211402)。

**更正 DEV-20261128-040133 第 6 条**。当时写"符号核对无问题(与 11-23 一致)",依据的是本分支旧基线里的 11-23 记录;上游 11-27 手推已证明**上抬时 `total_position` 减小**。更重要的是当时漏推了下面这一步。

**根因(新)**。pitch 自 11-26 迁入 RM_shaobin2 起是**电机编码器闭环**(`position = total_position`),而 `SENTRY_PITCH_TORQUE_SIGN = -1` 是 11-23 **世界俯仰(IMU)闭环**时按"正力矩枪口下压"取的反号,迁移时原样保留("输出符号 -1 保留")。编码器闭环的稳定性符号只由电机决定:同款大 yaw DM4310 已台架验证 MIT 正力矩 → `total_position` 增大,负反馈要求 PID 正输出 → 正力矩,即 **+1**。保留 -1 等于**正反馈**:任何一点杆量产生的误差都被放大,直到撞软限位被"越界断力"拦下——与杆量大小无关。这就是"动了一下摇杆直接上抬到限位";用户猜测的"指令 1° 实际 10°"是同一现象的直观描述,但不是比例系数。
- **主机复现**:`tests/run_sentry_pitch_tests.py` 用"正常达妙 + 轻枪管"(`v' = 20τ − 10v`)跑真实 `gimbal_pitch_task`:符号 +1 时杆推 10° 在 2 s 内收敛到 0.5° 内并保持、反向 10° 同样收敛;符号 -1 时同一命令发散到越界断力(`assert(motor.enabled)` 失败,已用临时改 config 复核)。
- **硬件前提未在 pitch 上实测**:"pitch 电机与大 yaw 同向(正力矩 → 位置增大)"。台架首推 5° 即可确认,见下。

**"10×"逐项核对(固件链路无比例错误)**。度/弧度常数正确;`cmd_task` 与 `gimbal_task` 同在 `run_200hz` 任务,`RATE × 0.005` 积分口径正确;DM 反馈映射 ±12.5 rad / ±30 rad/s / ±10 N·m 与大 yaw 台架数据吻合(46.4 s 24 圈 ↔ 192°/s);MIT 帧 Kp=Kd=0,位置字段不参与。机构传动比:11-27 手推硬限位电机轴 69.5° ↔ 枪管 69.5°(下压 35.7°/上抬 33.8°)→ **1:1**。**但确有一处 10.75 倍:`KBM_PITCH_SENS` 哨兵 0.43 vs 步兵 0.04**(仅键鼠路径;`KBM_PITCH_MAX_RATE 150°/s` 下轻碰即满速冲 ±15°),已对齐步兵。

**参考步兵 pitch 的结论**。控制结构(角度环 P → 速度环 PI、输出侧重力前馈、目标角速度前馈、到位死区)哨兵已有(`sentry_axis_*`),差异只在:①**反馈源**——步兵闭在 IMU 枪管俯仰上,哨兵 IMU 在大 yaw 座上不随枪管俯仰,只能闭编码器(11-23 曾用"底座 IMU + 关节角"合成世界俯仰,可作下一步);②步兵的保持质量来自**台架标定的重力前馈**(DEV-20261116-03/04),哨兵钩子在、值为 0,且 `SENTRY_PITCH_INTEGRAL_NM 0.3` 偏小——这才是下一步"稳定保持"的关键,不是继续调杆速。

**改动**(`config_gimbal.h` / `config_kbm.h` / `gimbal.c` / `sentry_def.h` 注释 / `ops/` 两份现场文档)。
- `SENTRY_PITCH_TORQUE_SIGN -1 → +1`;新增运行期 `sentry_rt_pitch_torque_sign`(只认 ±1,其余回退 config),供首推反向时在线翻转。
- 方向取上游 11-27 实测 `SENTRY_PITCH_DIRECTION -1`;软限位改为**枪管俯仰角**口径(负=下压,正=上抬),固件按 `DIR / 传动比` 换算到电机轴并排出上下界(DIR=-1 时"上抬"落在电机轴负侧,不能直接当 min/max)。
- `SENTRY_PITCH_TRANSMISSION_RATIO`(原先定义了但无人引用)接入 pitch 映射,定义为**电机轴/枪管**(与大 yaw 同口径),默认 1.0 行为不变;新增运行期 `sentry_rt_pitch_ratio`;爬升限速按枪管度 × R。
- 手动/自瞄映射与 `sentry_pitch_abs_deg` 同步除以传动比;`KBM_PITCH_SENS 0.43 → 0.04`。
- pitch 段 6 个**定义了但控制链不引用**的宏(`ANGLE_IL / SPEED_IL / SPEED_MAXOUT / OUTPUT_LPF_RC / TRAVEL_DEG / MOUSE_RATE_DEG_S`)加注"未引用,生效的是 xxx",防止调错参数(实际生效:积分限幅 `SENTRY_PITCH_INTEGRAL_NM`、力矩上限 `SENTRY_PITCH_TORQUE_NM`、输出低通 `SENTRY_AXIS_OUTPUT_RC_S`)。
- 测试:`run_sentry_pitch_tests.py` 重写——加入被控对象的收敛/发散对照、DIR 与传动比用例;`ops/功能检查表.md` 写入四步台架顺序,`ops/手柄操作手册.md` §8 补 4 个 pitch 运行期符号。

**台架顺序(按序,前一步不过不进下一步)**。
1. **方向**(零力):手抬枪管,`sentry_pitch_position_rad`(VOFA 通道 2)应**减小**;增大则先改 `DIRECTION +1`。
2. **传动比**(可选):断电手推枪管 10°,`R = Δsentry_pitch_position_rad × 57.3 / 10`,≈1 不动。
3. **首推符号**:枪管放行程中间上电 → 解锁 → 左纵杆轻推约 5° 立刻回中:上抬一点并稳住 = 对;**加速离开目标/冲向限位** = 右上急停,写 `sentry_rt_pitch_torque_sign = -1` 重试并回头改 config。
4. **保持**:松杆纹丝不动。若缓慢下垂,读 `sentry_pitch_torque_nm`:恒 ±0.3 = 积分限幅不够,顶 ±1.0 = 力矩上限不够 → 按步兵 P2 方法标重力前馈,或确认连杆已修复后放大限幅。

**验证**。`build.ps1 sentry -Current`:0 error、第一方 0 warning,text 126,924 / data 740 / bss 128,800,FLASH 127,672 B(12.18%)、RAM 74,592 B(56.91%)、CCM 54,944 B(83.84%);`run_profile_tests.ps1` 全绿(含 pitch 收敛/发散对照、DBUS pitch 用例、上游新增用例);`run_sentry_usb_tests.ps1` PASS;`check_robot_scope.py` PASS(兵种 7 / 共享 0);check-links / macros / golden(310/310) / dups / naming / comment_style / source_lists / abi 全 PASS。**未烧录、未台架**;稳定性符号的硬件前提待第 3 步确认。

**AI**:GLM-5.4(WorkBuddy),2026-11-28。

### DEV-20261128-041921｜2026-11-28｜[SENTRY] 小陀螺大yaw稳定性：摩擦前馈 + 小yaw母体角速度注入 + 重锚自动解除

**触发**。用户反馈底盘小陀螺时大 yaw/小 yaw 稳定性差,大 yaw 尤其差。

**根因分析**(代码级)。
1. 大 yaw 小陀螺 = 纯世界速度环(`sentry_axis_update_speed`,目标 0),摩擦稳态负载 3.2~3.4 N·m
   而 Kp=4.2,PI 从零积分起步填满需 ~9 s(与 Ki=2.0 台架"进入拖拽 81°/9s"吻合);
   每次进/出小陀螺或模式切换都 `sentry_axis_reset` 清积分,拖拽反复发生。
2. 小 yaw"母体世界角速度从参考值注入"只有 init 注释、从未实现(追溯 b8669a4 迁入原始版本即无):
   载体晃多少枪口跟多少,小 yaw(直驱 GM6020,带宽潜力高)完全没参与世界稳像。
3. 单帧任务超时(dt>20ms)或反馈掉帧即 `sentry_axis_reset` + rearm 锁存,而 rearm 只能切模式清除
   ——一次毛刺 = 大 yaw 永久卸力到手动重进模式。
4. 另:底盘小陀螺本身开环(`CHASSIS_ROTATE` 直接 wz=300 进运动学),est_wz 只做遥测,本轮不动。

**改动**。
- `config_gimbal.h`:新增 `SENTRY_BIG_YAW_FRICTION_FF_NM`(默认 0=关)、
  `SENTRY_BIG_YAW_FRICTION_FF_RAMP_RAD_S 1.0`、`SENTRY_SMALL_YAW_CARRIER_INJ`(默认 0=关)、
  `SENTRY_BIG_YAW_REARM_HEALTHY_CYCLES 20u`。
- `gimbal.c`:①大 yaw 小陀螺时按关节速度软符号注入摩擦前馈 `τ_ff=FF·clamp(ω_j/RAMP)`,
  总力矩仍夹在运行期力矩上限内(不放大 DM MIT 量程包络);②新增 `sentry_rt_big_yaw_fric_ff_nm`、
  `sentry_rt_small_yaw_carrier_inj` 运行期变量(默认=config=0,上电行为不变);
  ③新增 `sentry_big_yaw_world_speed_rad_s` 共享观察量,小 yaw 在角度 P 后注入
  `ref -= INJ·载体世界角速度`(仅 IMU 在线时生效,方向不对把系数取负);
  ④rearm 锁存改为连续 20 拍(100ms)健康自动解除,硬故障(超温/失联/掉使能)期间 healthy=false 不解除。
- `tests/run_sentry_axis_tests.py`:生成器前缀补两个新 rt 变量(移植测试白名单)。

**验证**。`build.ps1 sentry -Current`:0 error、第一方 0 warning,text 126,808 / data 732 /
bss 128,800,CCM 83.8%≤90%;`run_profile_tests.ps1` 全矩阵 PASS(含大 yaw 任务移植回归:
FF=0 时合成 300°/s 关节运动力矩恒 0、重锚冻结-恢复路径语义不变);`run_chassis_tests.ps1` PASS;
`check_robot_scope.py` PASS。未烧录、未台架。

**待实测(台架,前馈/注入默认关,烧录后行为与现版一致)**。
1. 原地自转 300°/s:运行期写 `sentry_rt_big_yaw_fric_ff_nm` 从 2.0 起调(实测稳态负载 3.2~3.4),
   判据:进入 1s 内世界角拖拽 <10°、稳态 |世界角速度| <2°/s、无 ≥3 Hz 振荡;
2. 前馈生效后 `sentry_rt_big_yaw_speed_ki` 可从 2.0 回落 1.0~1.5 降风振;力矩 LPF
   `SENTRY_BIG_YAW_TORQUE_LPF_RC_S` 0.03→0.015 试探提带宽;
3. 小 yaw:`sentry_rt_small_yaw_carrier_inj` 从 0.3 起调,方向不对取负;配合把
   `SENTRY_SMALL_YAW_DEADBAND_DEG_S` 3→1 试探(死区内 PID 输出清零=无阻尼);
4. 单帧毛刺恢复:大 yaw 运行中碰线/松 CAN,应 100ms 内自行恢复出力而非永久卸力。

**AI**:GLM-5.3-Flash,2026-11-28。

### DEV-20261128-040133｜2026-11-28｜[SENTRY] pitch 摇杆可控化：对称 ±15° 软限位 + 杆速 60→20°/s + 死区 10→40 + 0 位严格指上电位置

**触发**。用户反馈"上电动了一下摇杆,pitch 直接上抬到限位,还是无法正常使用";要求逻辑:上电标定当前位置为 0 位 → 稳定保持 → 上下限位 → 可自由控制。

**根因(静态排查,与现象逐条对上)**。
1. **杆速与行程不匹配**:`cmd_remote.c` 的 `gimbal_cmd.pitch` 按 60°/s×杆量积分,而命令层夹在 `SENTRY_PITCH_MIN/MAX_ANGLE` = **[0°, 15°]**(095624 把下限设成 0):满杆 0.25 s、半杆 0.5 s 就积分到 15° 上限,任何轻碰都是"一推就到限位"。
2. **死区只有 10 刻度**(满量程 660):回中偏置略超 10 就持续单向积分,慢慢爬到限位。
3. **行程不对称 [0, 15]**:摇杆下推被夹在 0,只能上不能下,与"上下限位、自由控制"不符。
4. **并列风险**:左中挡 = 自瞄待命,`SENTRY_AUTO_AIM_PITCH_ENABLE=1` 时视觉绝对角直接接管 pitch;该映射把"上电零位"当作相对平台 0° 俯仰,而零位是任意位置,未标定的视觉 pitch 同样会把枪管推到软限位。
5. **"保持 0 位"不严格**:手动映射以解锁瞬间的位置为锚点(`start_rad`),上电到解锁之间电机无出力、枪管若下垂,保持的就是下垂后的位置而非上电 0 位。
6. 符号/上电零位/首拍种子核对无问题:杆上推 → `pitch` 增大 → `total_position` 增大 → 抬枪(与 11-23 实测一致);`ZERO_AT_BOOT` 首个有效反馈锁 0 位;解锁首拍以当前角为爬升起点,零输出。**[已更正,见 DEV-20261128-043140:方向按 11-27 手推为 -1;且编码器闭环的稳定性符号当时漏推,`TORQUE_SIGN` 应为 +1——本条其余 1~5 项仍成立。]**

**改动**(`config_gimbal.h` / `cmd_remote.c` / `gimbal.c` / `cmd_authority.c` / `sentry_def.h` 注释)。
- `SENTRY_PITCH_MIN_DEG 0 → -15`,与 `MAX_DEG 15` 对称;`SENTRY_PITCH_MIN/MAX_ANGLE` 别名、gimbal 软限位、运行期 `sentry_rt_pitch_min/max_deg` 默认值自动跟随。
- 新增 `SENTRY_PITCH_STICK_RATE_DEG_S 20`(杆速;原与 gimbal 目标爬升 `SENTRY_PITCH_RATE_DEG_S 60` 共用一个宏,后者只管爬升);新增 `SENTRY_PITCH_STICK_DEADBAND 40`(原硬编码 10)。
- `cmd_remote.c`:杆量死区外按(杆量−死区)线性积分,越过死区从 0 速起步。
- `gimbal.c` pitch 手动映射改为**绝对**:`requested = pitch_zero + DIR × cmd.pitch`,`cmd.pitch` 语义固定为"相对上电零位的角度"(0 = 保持上电位置);删除 `sentry_pitch_start_rad/command_start_deg` 两个锚点状态。解锁/重入时若枪管已偏离上电位,以 60°/s 爬回 0 位。所有重入路径(急停/失联/锁存)都要重新解锁,`cmd.pitch` 同时归 0,口径一致。
- `SENTRY_AUTO_AIM_PITCH_ENABLE 1 → 0`:pitch 恒手动,视觉不再驱动 pitch(大 yaw 自瞄不受影响)。
- 测试:新增 `tests/run_sentry_pitch_tests.py`(抽取真实 `gimbal_pitch_task` + 真实串级 PID:上电锁存、下垂 3° 后回 0 位、首拍零输出、±15° 对称与越界命令夹限、越界断力/重锚/恢复、反馈丢失),接入 `run_profile_tests.ps1`;`tests/sentry_remote_test.c` 新增杆量死区/半杆速率/上下限位用例(须放在鼠标用例之前:鼠标活动会把 `input_source` 切到 KBM,之后不再走 `cmd_remote_set`)。

**操作要求**。**上电前把枪管放到行程中间**——零位在哪,±15° 就从哪算;放在行程端点会有一侧顶硬限位(电机以 ≤1.0 N·m 顶住)。左下挡(或左中挡且无视觉目标)+ 左纵杆上/下推,在 ±15° 内自由控制,满杆 1.5 s 走完半行程。`ops/功能检查表.md` A/C/D 三处 pitch 条目已同步(上电放行程中间、±15°、杆速/死区、视觉不驱动 pitch)。

**验证**。`build.ps1 sentry -Current`:0 error、第一方 0 warning,text 127,588 / data 736 / bss 128,792,FLASH 128,332 B(12.24%)、RAM 74,576 B(56.90%)、CCM 54,944 B(83.84%);`run_profile_tests.ps1` 全绿(含新增 pitch 实任务用例与 DBUS pitch 用例);`check_robot_scope.py` WARN(兵种 7 文件 + 共享 1 文件 = `tests/run_profile_tests.ps1` 一行注册,与此前 `sentry(test)` 提交同口径,待回流 main);check-links / macros / golden / dups / naming / comment_style / source_lists 全 PASS;G4 仍是 U3/U4 前旧清单的 9 处既有红旗(§3 DEV-20261127-084627-B),本次未新增句柄/死循环。**未烧录、未台架。**

**待实测(关闭条件)**。① 解锁后枪管稳稳保持不动(手推有回弹);② 轻碰摇杆只走 1~2°,不再冲限位;③ 上、下各能到 ±15° 并停住;④ 回中不漂移。若保持时缓慢下垂,看 `sentry_pitch_torque_nm`:恒在 ±0.3 附近 = 积分限幅 `SENTRY_PITCH_INTEGRAL_NM` 不够(重力矩 > 0.3 N·m);顶到 ±1.0 = 力矩上限 `SENTRY_PITCH_TORQUE_NM` 不够——先架空再调。**自瞄 pitch 恢复条件**:标定"上电零位对应的枪管世界 pitch"并在退回手动时同步 `gimbal_cmd.pitch` 到当前角(绝对映射下否则会爬回上一个手动命令),再把 `SENTRY_AUTO_AIM_PITCH_ENABLE` 改回 1。

**AI**:GLM-5.4(WorkBuddy),2026-11-28。
### DEV-20261127-211402｜2026-11-27｜[SENTRY] 撤销回退：恢复 1cc9e9f 哨兵配置

**触发**。用户指示"更新回来"，即撤销 DEV-20261127-203844 的回退，恢复被撤的哨兵配置。

**方式**。`apps/sentry/gimbal/gimbal.c`、`config/sentry_2026/config_gimbal.h`、
`config/sentry_2026/config_chassis.h` 恢复为 `1cc9e9f` 内容（`git diff 1cc9e9f -- <file>` 无输出），
并补回被回退移除的 DEV-20261127-203112 条目；`main` 历史不改写。

**验证**。`build.ps1 sentry -Current` EXIT=0，第一方 0 warning；text **126,460** / data 732 / bss 128,784；
`run_profile_tests.ps1`、`run_sentry_usb_tests.ps1`、`tests/run_sentry_axis_tests.py` 全 PASS；
`firmware/sentry.elf`、`current.elf` 已刷新。未烧录。

**注意**。pitch 方向/零位/限位仍为单次台架读数，上电前必须架空低速复核；底盘功率控制仍关闭
（`CHASSIS_POWER_CTRL_EN=0`），自转断电复测后再决定是否开回。

**AI**。DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-203844｜2026-11-27｜[SENTRY] 回退哨兵代码到 6fa98cc（12 点版）

**触发**。用户要求把哨兵代码回退到 2026-11-27 中午 12 点左右那一版；对应提交为 `6fa98cc`
（11:19 `merge(sentry): 合入哨兵自瞄/导航/功率 + 云台整定 + 发射硬推`，12:00~15:17 之间哨兵无提交）。

**方式**。`git revert --no-commit 1cc9e9f`，只撤哨兵那条提交；当天下午的步兵提交、共享工具提交
（`384c415`）和本文档提交均保留。`main` 历史不改写，符合"已推送 `main` 禁 `amend/rebase/force-push`"。
回退后 `apps/sentry/gimbal/gimbal.c`、`config/sentry_2026/config_gimbal.h`、`config/sentry_2026/config_chassis.h`
与 `6fa98cc` **逐字节一致**（`git diff 6fa98cc -- <file>` 无输出）。

**被撤内容**（原 DEV-20261127-203112，已从台账移除，内容仍可从 `git show 1cc9e9f` 取回）：
- pitch：`SENTRY_PITCH_DIRECTION -1`、`SENTRY_PITCH_ZERO_RAD -2.58163`、`ZERO_AT_BOOT=0`、软限位 `-31.8/+33.7°`。
- 底盘：`MAX_MOTOR_RPM 9000`、`MAX_SPEED 2000`、`ROTATE_SPEED 300`、`POWER_CTRL_EN=0`、功率上限 100 W。
- 回退后恢复为：pitch 方向 `+1`、零位 `-1.935`、上电锁存 `ZERO_AT_BOOT=1`、软限位 `0~15°`；
  底盘 `7000 rpm / 1500 mm/s / 150 deg/s`、`POWER_CTRL_EN=1`、90 W。

**验证**。`build.ps1 sentry -Current` EXIT=0，第一方 0 warning；text **127,628** / data 732 / bss 128,808
（与 11:12 条目数值一致，佐证回到 `6fa98cc`）；`run_profile_tests.ps1`、`run_sentry_usb_tests.ps1`、
`tests/run_sentry_axis_tests.py` 全 PASS；`firmware/sentry.elf` 与 `current.elf` 已按回退后代码刷新。未烧录。

**恢复方法**。若要把 203112 的方案重新应用回来：`git revert <本条目提交>` 即可（历史不改写）。

**AI**。DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-203112｜2026-11-27｜[SENTRY] pitch 固定水平零位/方向/限位 + 底盘 300 自转复测

**触发**。工作区残留在 DEV-20261127-111223 之后的哨兵改动（pitch 零位/方向/限位、底盘参数），用户指示随本次一并推送，本轮补记。

**改动**。
- `config/sentry_2026/config_gimbal.h`：`SENTRY_PITCH_DIRECTION +1→-1`（上抬时 total_position 减小）；`SENTRY_PITCH_ZERO_AT_BOOT 1→0`，改用固定 `SENTRY_PITCH_ZERO_RAD=-2.58163`（水平，-147.917°）；软限位 `0~15°` → `-31.8~+33.7°`（物理上抬 31.8°/下压 33.7°，各内缩 2°）。
- `config/sentry_2026/config_chassis.h`：`CHASSIS_MAX_MOTOR_RPM 7000→9000`、`MAX_SPEED 1500→2000`、`ROTATE_SPEED 150→300`（复测断电）；`CHASSIS_POWER_CTRL_EN 1→0`、功率上限 90→100W（哨兵规则）。
- `apps/sentry/gimbal/gimbal.c`：`sentry_pitch_boot_ref_rad` 用 `#if SENTRY_PITCH_ZERO_AT_BOOT` 包住，`ZERO_AT_BOOT=0` 时不再写入。

**依据**。上述数值均为 config 内联注释记录的 2026-11-27 实测；本条为工作区既有改动补记，本轮未新增台架/实车验证。

**验证**。`build.ps1 sentry` 强制重编 20 个文件，0 error / 第一方 0 warning；RAM 56.9% / CCM 83.8% / FLASH 12.1%。

**风险**。pitch 方向/零位/限位三项耦合，上电前必须架空低速确认；`ZERO_AT_BOOT=0` 后水平零位依赖 `-2.58163` 常量，拆装后要重标。

**AI**。工作区既有改动；本轮由 DeepSeek-V4.1-Flash 补记并编译验证。

### DEV-20261127-111223｜2026-11-27｜[SENTRY] 拨盘升档提速：检测即加档，60 ms 到顶

**触发**。用户反馈 350 ms 才升到 9 A 太慢。

**改动**。`LOADER_ESCALATE_STEP_MS` 150 → **60**、`LOADER_ESCALATE_STEP_CURRENT` 1500 → **1750**；`shoot.c` 增加"检测到堵转立刻先加一档"（快踢）。
效果：堵转判定后**立即 7.25 A**，**约 60 ms 到 9 A**（原 350 ms）；回落逻辑与 1.3 s 锁存、过温保护不变。

**验证**。`build.ps1 sentry -Current`：0 error、0 warning，text 127,628 / data 732 / bss 128,808；`run_profile_tests.ps1` PASS。未烧录。

**待实测**。同一卡弹工况：0.1 s 内应感到拨盘"顶一下"；若仍慢可再降 `STEP_MS` 到 40 或抬高快踢档位。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-110754｜2026-11-27｜[SENTRY] 拨盘改为"堵转才升档"的电流爬升

**目标与触发**。用户要求：不要一直 9 A 硬推，正常用小力度，检测到堵转后再快速升档，既更安全又能顶过管道小堵塞。

**改动**。
- `config_shoot.h`：新增 `LOADER_PUSH_CURRENT 5500`（正常顶弹 5.5 A）、`LOADER_ESCALATE_STEP_CURRENT 1500`（每档 +1.5 A）、`LOADER_ESCALATE_STEP_MS 150`（每档驻留）、`LOADER_ESCALATE_MAX_CURRENT 9000`（上限）；`LOADER_JAM_HOLD_MS` 1000 → **1300**（升档后满力仍卡才锁存）。
- `shoot.c`：`shoot_task` 每拍计算电流上限——正在送弹且转速 <`LOADER_JAM_SPEED_RPM` 时累计堵转时间，每 150 ms 升一档，约 **350 ms 到 9 A**；重新转起来按 2×dt 快速回落；上限仍可用 `sentry_rt_loader_max_current` 运行期覆盖。
- 注意：升的是**电流上限**，不是强制电流；正常送弹时速度环需要多少用多少，只有饱和（真顶住）时才吃到高限。

**保留的保护**。真堵转电流 ≥5000 且 1.3 s → 锁存；C610 ≥80°C 锁存、≤60°C 释放；松扳机复位堵塞锁存。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 127,596 / data 732 / bss 128,808；`run_profile_tests.ps1` PASS。未烧录。

**待实测**。台架卡弹工况：正常弹能过；卡点约 0.35 s 内升到 9 A 并顶过去；顶不过则 1.3 s 锁存；观察温度与弹丸变形。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-105831｜2026-11-27｜[SENTRY] 拨盘硬推到 9A（管道小堵塞）+ 过温锁存

**触发**。确认是供弹管道里有小堵塞，无法临时改硬件，用户要求"安全范围内再加大力度硬推"。

**改动**。
- `config_shoot.h`：`LOADER_SPEED_PID_MAXOUT` 7500 → **9000**（9 A，C610 满量程 10000 的 90%）；`LOADER_JAM_CURRENT` 4000 → **5000**（9 A 正常顶弹不误判，真堵转仍触发）；`LOADER_JAM_HOLD_MS` 800 → **1000**（单次硬推 1 s 后锁存）；新增 `LOADER_TEMP_LIMIT_C 80`、`LOADER_TEMP_RESUME_C 60`。
- `shoot.c`：新增运行期可写 `sentry_rt_loader_max_current`（2000~9500，越界回退 config），台架不重烧即可调力度；新增 **C610 过温锁存**——温度 ≥80°C 立即停转并锁存，降到 ≤60°C 才释放；锁存期间清空待发队列、复用 jam 语义，`LOADER_STOP` 不再无条件清 jam。

**安全边界**。9 A 仍在 C610 10 A 以内；单次堵转最长 1 s 后锁存，不会持续堵烧；过温有独立保护；弹丸仍有被压扁风险，台架需观察。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 127,380 / data 732 / bss 128,800；`run_profile_tests.ps1` PASS。未烧录。

**台架建议**。先用 `sentry_rt_loader_max_current` 从 8000 起试，逐档加；同时读 `loader->measure.temperature` 观察温升；确认能顶过堵塞且不压弹后，再回填 config。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-105202｜2026-11-27｜[SENTRY] 拨弹盘顶弹力矩加大 + 堵转判定放宽

**触发**。用户反馈拨弹盘卡弹，要求加大拨弹力度。

**现状分析**。拨盘为 M2006 + C610 电流环：速度环 `MaxOut=5000`（5 A）就是力矩上限；堵转判定 `1800 raw / 500 ms` 会在满力顶 0.5 s 后锁存停机，所以卡弹时实际只推半秒。

**改动**（`config/sentry_2026/config_shoot.h`）。
- `LOADER_SPEED_PID_MAXOUT` 5000 → **7500**（5 A → 7.5 A，C610 满量程 10000，+50% 力矩）
- `LOADER_JAM_CURRENT` 1800 → **4000**（避免 7.5 A 正常顶弹时误判；真堵转 7.5 A 仍会触发）
- `LOADER_JAM_HOLD_MS` 500 → **800**（给拨盘更多时间顶过卡点）
- `LOADER_ANGLE_PID_MAXOUT` 6000 → **8000**（到位速度上限同步放宽）
- 未动 PID 增益/积分限幅/单发角/超时。

**风险**。更大顶弹力可能压扁弹丸或让拨盘受力变大；7.5 A 连续堵转 0.8 s 后仍会锁存（松扳机复位）。若仍卡，先查机械（拨盘间隙/供弹口毛刺），再考虑 8500。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 126,084 / data 728 / bss 128,800；`run_profile_tests.ps1` PASS。未烧录。

**待实测（关闭条件）**。台架复现原卡弹工况：能顶过去且不压弹；堵转仍能 0.8 s 内锁存停机。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-095624｜2026-11-27｜[SENTRY] pitch 上电校准 + 相对软限位 + 越界断力（恢复使能）

**目标与触发**。用户要求 pitch 也改成上电校准并限制角度行程;对应 11-23 事故三条教训中的"不做自动回中、限位夹在实测行程内、越界断力"。

**改动**。
- `config_gimbal.h`:新增 `SENTRY_PITCH_ZERO_AT_BOOT 1u`、`SENTRY_PITCH_MIN_DEG 0.0f`、`SENTRY_PITCH_MAX_DEG 15.0f`、`SENTRY_PITCH_LIMIT_MARGIN_DEG 2.0f`;`SENTRY_PITCH_MIN/MAX_ANGLE` 改为引用新限位;`SENTRY_PITCH_CONTROL_ENABLE` 0→**1**。
- `gimbal.c`:`gimbal_pitch_task` 在首个有效反馈锁存 `sentry_pitch_boot_ref_rad`(暴露 `sentry_pitch_boot_ref_deg/_valid`),`pitch_zero` 优先用它;新增运行期可写 `sentry_rt_pitch_min_deg/max_deg`;手动与自瞄目标都夹在 `[zero+min, zero+max]`;`healthy` 增加 `pitch_in_range`(限位外 2° 裕量即判失败 → 停力 + 要求重锚)。
- 不再使用对称 `SENTRY_PITCH_TRAVEL_DEG`(宏保留但控制不用)。

**安全设定**。上电零位=枪管自然最低位;默认只允许 **0~15°(向上)**,向下 0°;力矩上限 1.5 N·m 不变;实测角越界 2° 立即断力。11-23 手推行程约 +7.8~+52.7(水平=0),台架确认后可用 `sentry_rt_pitch_min_deg/max_deg` 放宽,再回填 config。

**验证**。`build.ps1 sentry -Current`:0 error、第一方 0 warning,text 126,084 / data 728 / bss 128,800;`run_profile_tests.ps1` PASS。未烧录。

**首次台架要求**。架空空转、手扶枪管;上电前枪管放最低位;确认解锁首拍不动、只能向上走到 15°;读 `sentry_pitch_boot_ref_deg` 与 `sentry_pitch_position_rad` 核对。异常立即右上急停。

**编码器量程说明**。DM 反馈位置字段 ±12.5 rad(±716°,每 1432.4° 回绕);pitch 只在 0~15° 内,远离回绕点;big yaw 故意不限(小陀螺需连续旋转),靠圈数累加 + 上电校准。

**AI**:DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-095301｜2026-11-27｜[SENTRY] 大 yaw 改上电校准车头（替代编译常量）

**触发**。用户反馈"大 yaw 标定的车头编码器值总会自己变"。静态排查：① 驱动器位置字段只有 ±12.5 rad（±716°），每 **1432.4°** 回绕；② 固件 `total_round` 从**上电起累加**，反馈中断期间关节转过 >716° 时只补 ±1 圈，会永久偏 k×1432.4°；③ 1432.4° 不是 360° 整数倍，所以原始多圈值在多圈后看起来"莫名其妙"。`big_yaw_rel_deg` 有 `remainderf(2π)` 兜底，但 `fixed_target` 依赖编译常量，跨上电/多圈不可靠。

**改动（结构级，用户批准）**。
- `config/sentry_2026/config_gimbal.h`：新增 `SENTRY_BIG_YAW_FRONT_ALIGN_AT_BOOT 1u`；`SENTRY_BIG_YAW_FRONT_ALIGN_RAD(-1.52914)` 保留为 `AT_BOOT=0` 时的回退值。
- `apps/sentry/gimbal/gimbal.c`：新增 `gimbal_front_align_rad()`；`gimbal_big_yaw_task` 在**首个有效反馈**（`online && isfinite(position)`）锁存 `sentry_big_yaw_front_align_rad`，并暴露 `sentry_big_yaw_front_align_deg` / `..._valid` 供调试器读；`fixed_target` 与 `gimbal_fb.big_yaw_rel_deg` 改用锁存值。锁存后连续旋转、多次小陀螺都不再影响回正参考。

**操作要求**。上电前把大 yaw 摆到车头标记；上电后该姿态即"正前"。已写入 `ops/功能检查表.md`、`params/待标定表.md`、`params/待实测参数表.md`。

**测试适配**。`tests/run_sentry_axis_tests.py`：`function()` 泛化支持 `static float`，并额外抽取 `gimbal_front_align_rad`；世界环用例在首个 Tick 前把电机位置摆在车头（对应上电校准语义）。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 125,020 / data 724 / bss 128,760，FLASH 125,752 B（11.99%）、RAM 74,536 B（56.87%）、CCM 54,944 B（83.84%）；`run_profile_tests.ps1` PASS。未烧录。

**待实测**。烧录后按"车头摆正→上电→读 `sentry_big_yaw_front_align_deg`"核对；小陀螺旋若干圈后回正仍指同一物理方向。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-094558｜2026-11-27｜[SENTRY] 小 yaw 位置环降增益 + 放大死区（压高频抖）

**依据（capture sentry_smallyaw_02，小陀螺 26.6 s，active=1/limited=0）**。
- 小 yaw 速度 std **18.9°/s**（−70.8~+48.0），而关节角只在 **−0.6~+2.0°** 内动；
- 参考（角度环输出）均值 **−9.2**、std 8.9，与关节误差×Kp25 吻合，且 **corr(ref, 大yaw陀螺)=0.73**；
- 速度频谱 1~5 Hz 均匀无主峰 → 典型摩擦极限环：亚度级位置抖动被 Kp=25 放大成 ±9~12°/s 指令，GM6020 电压模式零速附近"粘—滑"。

**改动**（`config/sentry_2026/config_gimbal.h`）：`SENTRY_SMALL_YAW_ANGLE_KP 25.0 → 10.0`；角度死区 `0.3° → 0.8°`（`SENTRY_SMALL_YAW_ANGLE_DEADBAND` 与 `_DEG` 两处同值）。速度环/电流模式/输出低通未动。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning；FLASH 125,664 B（11.98%）、RAM 74,528 B（56.86%）、CCM 54,944 B（83.84%）；`run_profile_tests.ps1` PASS。未烧录。

**待实测（关闭条件）**。小陀螺下小 yaw 速度 std 降到 ≤8°/s，且关节不出现 >3° 的漂移；若仍抖，下一步试 `SENTRY_SMALL_YAW_CURRENT_MODE 0→1`（电压切电流）。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-093431｜2026-11-27｜[SENTRY] 合并 origin/sentry:USB 协议采用 Avear 实测版

**合并对象**。`origin/sentry` `1aa9ee1`(含 `375ea97` USB CDC 双协议 DEV-20261127-084627,以及 main 更名/分支规则)。

**冲突与策略(8 文件)**。
- `apps/sentry/autonomy/vision_link.c/.h`:**采用 Avear 实测版为协议核心**(帧格式/超时/重同步不动),在其上恢复 `vision_link_set_mode/projectile_speed/robot_id` 运行期注入,供本分支自瞄/连发使用;`build_pose` 用注入值替代固定占位。
- `autonomy.c`:保留本分支的裁判数据刷新 + 0x1021 上行;沿用实测版的注释语义。
- `config_ax650.h`:消除重复 `AX650_USB_ENABLE`(自动合并产生两份),`AX650_TX_*` 保留为运行期默认值。
- `README.md`/`DEVLOG.md`/`ops/导航协议与上位机.md`:协议描述以实测版为准,状态段按本分支实际(0x0503 进入自瞄、弹速/ID 运行期刷新)修正;两侧 DEV 条目全部保留。

**适配(测试)**。补 `tests/vision_stubs/quaternion_ekf.h`、`tests/usb_pipeline_stubs/quaternion_ekf.h`、`tests/sentry/usb_protocol/stubs/referee_task.h`(NULL,保持占位 0);`vision_stubs/kernel.h` 加空临界区桩;`sentry_vision_link_test.c` 改为实测版语义——**新帧要求 INS 有新样本**,推进 tick 同时递增 `UpdateCount`。

**验证**。`build.ps1 sentry -Current`:0 error、第一方 0 warning;`run_profile_tests.ps1` PASS;`run_sentry_usb_tests.ps1` 双档(-O0/-Ofast)PASS;`check-links` PASS;兵种范围守卫 WARN(合并带入的共享文档,非本次新增)。未烧录。

**待实测**。实体 USB 六消息抓包(先保持接管关闭验协议)→ 再开自瞄/导航接管。

**AI**:DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-091946｜2026-11-27｜[SENTRY] 实验:大 yaw 积分限幅 3.0→6.0（稳态漂移，待台架）

**依据（capture 06，基线 ratio 1.0 / Kp 4.2 / Ki 2.0，小陀螺 58.8 s）**。
1. 进入段复现 04：+1/2/3/5/8/9 s 累计拖拽 39/61/69/83/95/94°（04 为 39/56/68/78/83/81），**Ki=2.0 的改善可复现**；本轮无堵转（力矩饱和样本 0）。
2. 稳态有持续 +2~5°/s 世界角漂移，力矩 **−3.2~−3.4 N·m** > 积分限幅 **3.0 N·m** → **积分饱和**；残余误差由 P 项承担，`4.2 × (2.4°/s ÷ 57.3) ≈ 0.18 N·m` 与实测多出的力矩吻合。
3. 后段力矩 −3.1→−3.4、漂移 +2.4→+4.5°/s 递增，符合线缆随旋转圈数（59 s ≈ 31 圈）越缠越紧。

**改动**。`config/sentry_2026/config_gimbal.h`：`SENTRY_BIG_YAW_INTEGRAL_NM 3.0f → 6.0f`。Ki 2.0、Kp 4.2、力矩限 10 不动。进入段积分累积约 1.8 N·m，远低于 6，预计不增加过冲。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 124,932 / data 724 / bss 128,760；`run_profile_tests.ps1` PASS。未烧录。

**待实测（关闭条件）**。① 稳态 1 s 漂移 ≤1°/s；② 进入段拖拽与过冲不劣于 06；③ 不满足则回退 3.0 并记录。

**机械并行项**。小陀螺连续旋转的线缆走线/滑环必须处理（已出现一次钩线堵转、一次后段负载上升），否则积分增益只是在掩盖机械阻力。

**哨兵影响**。[SENTRY] 仅 `config/sentry_2026/config_gimbal.h` 与哨兵台账。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-091528｜2026-11-27｜[SENTRY] 更正:传动比 1.30 作废回退 1.0；est_wz 低估 ~25%

**结论（推翻 DEV-20261127-090727）**。capture 05 进入小陀螺后 0.8~4.2 s 出现电机堵转：命令力矩 −10.0 N·m 饱和、电机位置不动，而上部被底盘带着以 **IMU 实测 ~190 °/s** 转、`chassis_fb.est_wz` 只有 **150 °/s**。用户确认为**线缆钩住**导致关节锁死。由此：
1. 堵转时上部随底盘刚性转动 → **IMU 190 ≈ 底盘真实转速**，`est_wz` **低估约 25%**（轮速推算，非传动比问题）；
2. 稳态电机轴 192 °/s / 真实底盘 188 °/s、世界角≈0 → **电机/关节 ≈1.02，`TRANSMISSION_RATIO=1.0` 原本正确**。DEV-090727 的 1.30 是把 `est_wz(150)` 当基准算出来的，基准本身错 25%，作废。

**回退**（`config/sentry_2026/config_gimbal.h`）：`TRANSMISSION_RATIO` 1.0、`SPEED_KP` 4.2、`SPEED_KI` 2.0（保留 11-27 台架验证）、`INTEGRAL_NM` 3.0、`ANGLE_KP` 20、`KD_NEAR/FAR` 0.8/0.2、`MAX_SPEED_DEG_S` 90、`AUTO_AIM_BIG_YAW_MAX_SPEED` 150。`tests/run_sentry_axis_tests.py` 的 ratio-aware 写法保留（RATIO=1 时等价，未来改比也不失效）。

**作废的实验**。DEV-20261127-090921 的 `Kp=6.5` 不成立：其前提（传动比 1.3）错误，且 capture 05 的进入拖拽是线缆钩住而非控制问题。Kp 仍为 4.2。capture 03/04 的 Ki 0.8→2.0 对比发生在 ratio=1.0 时期，**仍然有效**。

**新增问题 SENTRY-02**。`chassis_fb.est_wz` 低估底盘角速度 ~25%（150 vs 190）。该值来自轮速逆解，若同源误差进入导航 `est_vx/est_vy/est_wz`，会影响 NV 定位/速度闭环。关闭条件：外部核对（IMU 角速度/转固定圈数计时）并标定轮半径或运动学系数。

**机械提醒**。小陀螺时大 yaw 相对底盘连续旋转（capture 03：46.4 s 内 24 圈），线束/滑环必须支持无限旋转；本次钩线会造成"堵转 + 被底盘拽走"的假控制故障。台架前先手动慢转两圈确认走线。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，text 124,932 / data 724 / bss 128,760；`run_profile_tests.ps1` PASS。未烧录。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-090921｜2026-11-27｜[SENTRY] 实验:大 yaw 速度环 Kp 5.46→6.5（待台架）

**目标与触发**。传动比修正后复看 capture 04 进入段：底盘 `est_wz` 在 **0.2 s** 内就到 150°/s（无超调），而大 yaw 电机侧从 0 被拉到 −190°/s 约需 **2 s**（−111→−160→−180→−190），期间世界角速度 +30~60°/s、力矩只有 −4~−5 N·m（量程 10 N·m 未用满）。**剩余拖拽主要是关节加速段 P 项不够硬，不是积分/限幅问题。**

**改动**。`config/sentry_2026/config_gimbal.h`：`SENTRY_BIG_YAW_SPEED_KP 5.46f → 6.5f`（关节侧 +19%）。**Ki 2.6、积分限幅 3.9、力矩限 10 均不动**，单变量实验。

**风险**。历史 11-26 曾在等效 ~6.76 关节侧出现左右振荡，6.5 已接近该点；且 Kp 会放大陀螺 2~3 Hz 振动。若台架出现持续抖动/极限环，直接回退 5.46。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning；FLASH 125,724 B（11.99%）、RAM 74,536 B（56.87%）、CCM 54,944 B（83.84%）；`run_profile_tests.ps1` PASS。本版同时含 DEV-20261127-090727 的传动比修正（等比换算，物理手感不变），烧录一次即可同时生效。

**待实测（关闭条件）**。① 进入小陀螺的前 1 s 世界角拖拽由 ~39° 降到 ~30° 以内且无回摆过冲；② 稳态 2~3 s 尺度无持续抖动；③ 不满足则回退 Kp=5.46 并记录现象。

**哨兵影响**。[SENTRY] 仅 `config/sentry_2026/config_gimbal.h` 与哨兵台账。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-090727｜2026-11-27｜[SENTRY] 大 yaw 传动比 1.0→1.30 + 环内增益等比换算

**目标与触发**。DEV-20261127-085816 采集时发现：小陀螺稳态世界角≈0 时，大 yaw 电机轴 −192~−207°/s，而底盘实测 `est_wz=149.8±1.7°/s`（命令 150），二者差 ~1.3 倍，config `SENTRY_BIG_YAW_TRANSMISSION_RATIO=1.0` 与机械不符。

**推算方法（角度积分，避开位置毛刺）**。`ratio = Δ电机角 / (Δ世界角 − ∫底盘角速度 dt)`：
- capture 03（46.4 s，底盘按命令 150°/s）：−8737° / (−6759°) = **1.293**；8 s 分段 1.310 / 1.311 / 1.289；
- capture 04（14.3 s，逐帧 `est_wz`）：−2699° / (−2056°) = **1.313**。
两版一致，取 **1.30**（≈20:26 带轮）。误差主要来自轮速打滑，量级 ±0.05，**待数齿复核**。

**改动（等比换算保手感）**。原 RATIO=1.0 时关节侧力矩/角度都比环内值大 1.3 倍。为不改变已台架验证的手感，环内参数同比 ×1.3：
- `TRANSMISSION_RATIO` 1.0 → **1.30**；`SPEED_KP` 4.2 → **5.46**；`SPEED_KI` 2.0 → **2.6**；`INTEGRAL_NM` 3.0 → **3.9**；
  `ANGLE_KP` 20 → **26**；`KD_NEAR/FAR` 0.8/0.2 → **1.04/0.26**；`MAX_SPEED_DEG_S` 90 → **117**；`AUTO_AIM_BIG_YAW_MAX_SPEED` 150 → **195**。
- 不动：`TORQUE_NM=10`（换算后即关节 10 N·m，比原先等效 13 更保守；实测峰值 8.8 未触及）、到位滞回阈值（原值在关节侧被放大 30%，现在才是真值）、`FRONT_ALIGN_RAD`（电机读数，代码已除 RATIO，自动正确）。
- 物理手感理论不变；实际收益：角度/目标/自瞄世界换算变准、力矩上限口径变真。

**测试修正**。`tests/run_sentry_axis_tests.py` 的世界环退出用例把 `total_position = FRONT_ALIGN + 2π + 0.2` 当电机读数写，RATIO≠1 后余数落错边导致 `motor.torque<0` 断言失败；改为 `FRONT_ALIGN + (2π+0.2)·RATIO`，期望目标改为 `FRONT_ALIGN/RATIO + 2π`，与代码同口径。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，FLASH 125,724 B（11.99%）、RAM 74,536 B（56.87%）、CCM 54,944 B（83.84%）；`run_profile_tests.ps1` PASS（含修正后的 world task 用例）。未烧录。

**待实测**。烧录后按原场景复采小陀螺（或直接下一轮积分限幅实验），确认手感无回归；数齿后把 RATIO 定死并撤掉"推算"标记。

**哨兵影响**。[SENTRY] 仅 `config/sentry_2026/config_gimbal.h`、`tests/run_sentry_axis_tests.py`、哨兵台账。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-085816｜2026-11-27｜[SENTRY] 大 yaw 小陀螺速度环 Ki 0.8→2.0（台架实测）

**目标与触发**。用户报告左中+右下小陀螺时大 yaw"不稳定停住、被底盘拽着走"。采集 `log/capture/sentry_smallgyro_bigyaw_03.ndjson`（Ki=0.8，小陀螺 46.4 s）定位：不是掉线/重锚（`control_active=1`、`rearm=0`、IMU/反馈全程在线、`rx_refresh_dt≤1`、`in_deadband=0`），而是**进入小陀螺后的积分建立拖拽**——每秒世界角变化 44/25/27/18/17/10/10/6/6/2.5°，9 s 累计 **163°**。

**根因**。稳态需恒定阻力矩约 −2.8 N·m，速度环 `Ki=0.8`、积分限幅 `SENTRY_BIG_YAW_INTEGRAL_NM=3.0`，积分约 8~10 s 才建立，期间枪被底盘拖着走。运行期把 `sentry_rt_big_yaw_speed_ki` 写 2.0 后用户体感拖拽变短。

**验证（04 采集，Ki=2.0，小陀螺 14.3 s）**。进入后 1/2/3/5/8/9 s 累计拖拽 **39/56/68/78/83/81°**（Ki=0.8 为 44/69/96/131/157/163°），9 s 累计**减半**；8→9 s 回 −2° **无过冲**；稳态 1 s 角速度 std 1.91 vs 2.44°/s；力矩范围 −5.5~−2.1（Ki=0.8 为 −6.8~−1.1）。注：04 稳态窗口仅 2.3 s，稳态 band（4.3° vs 26.4°）窗口长度不同，**不作等价对比**。

**改动**。`config/sentry_2026/config_gimbal.h`：`SENTRY_BIG_YAW_SPEED_KI 0.8f → 2.0f`，注释改实测标记。Kp/力矩限幅/积分限幅未动。

**顺带发现（未改值）**。同场采 `chassis_fb.est_wz=149.8±1.7°/s`（与命令 150 一致），而稳态世界角≈0 时大 yaw 电机轴 −192~−207°/s → 实际电机/关节比 **≈1.28**，config `SENTRY_BIG_YAW_TRANSMISSION_RATIO=1.0` 疑过期。对本速度环无影响（反馈用 IMU 世界角速度），但自瞄/回正角度换算会差 ~28%，登记 §3 SENTRY-01。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning；text 124,932 / data 724 / bss 128,760（FLASH 125,664 B 11.98%、RAM 74,536 B 56.87%、CCM 54,944 B 83.84%，均为实测）；`run_profile_tests.ps1` PASS（含 big-yaw 速度环与 DM port 用例）。台架数据来自运行期变量写入，未烧录，掉电后仍回 config 值。

**下一步**。① 核实传动比（数同步带齿数或断电手转对比）；② 积分限幅 3.0→6.0 再测（稳态力矩均值 −3.29 N·m，积分仍顶在限幅上）；③ 若第一秒拖拽仍大，再考虑小陀螺进入沿阻力前馈。

**哨兵影响**。[SENTRY] 仅 `config/sentry_2026/config_gimbal.h` 与哨兵台账；未动共享层。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-083542｜2026-11-27｜[SENTRY] pitch 轴全场景失能（台架安全态）

**目标与触发**。用户要求"先全场景失能 pitch 轴"：pitch 行程仍是移植值（`SENTRY_PITCH_TRAVEL_DEG=20°` 对称），而 11-23 手推记录的真实行程非对称且下硬限位靠近水平；自瞄绝对角路径可把目标压到水平以下，存在复现"顶死下硬限位"事故的风险。恢复实测前先切断 pitch 出力。

**改动**。`config/sentry_2026/config_gimbal.h`：`SENTRY_PITCH_CONTROL_ENABLE 1u → 0u`，并改写注释说明这是临时安全态、恢复条件与"恢复前不得接视觉自瞄"。该宏是 `gimbal_pitch_task()` 内 `command_active` 的唯一门控，置 0 后手动/自瞄/导航全部走 `dm_motor_stop()` + `dm_motor_set_mit_torque(...,0)` 分支；MIT 帧以 `Kp=Kd=0、τ=0` 下发（`dm_motor.c:322-329`），电机零出力、可手推；反馈（`sentry_pitch_position_rad`/`gimbal_fb.pitch_deg`/在线/温度）照常更新，便于手推读行程。未改任何控制逻辑、参数值与接口。

**编译期验证（比"宏置 0"更强）**。`arm-none-eabi-objdump -d --disassemble=gimbal_pitch_task build_sentry/sentry.elf`：函数内只剩 `dm_motor_stop` / `dm_motor_set_mit_torque` / `sentry_axis_reset` 三个调用，**`dm_motor_enable`、`sentry_axis_update`、重力前馈全部被编译器裁掉**——控制通路在目标码层面不存在，不是靠运行期布尔短路。

**验证**。`build.ps1 sentry -Current`：0 error、第一方 0 warning，FLASH 125,664 B（11.98%）、RAM 74,536 B（56.87%）、CCMRAM 54,944 B（83.84%）；`run_profile_tests.ps1` PASS（sentry autoaim / DM port / DBUS / 导航 USB 双档全绿）；`check_macro_golden.py` PASS（306/306）。未烧录、未台架。

**恢复条件（顺序固定）**。① 零力手推把枪管推到上下硬限位，记 `sentry_pitch_position_rad` 实测 min/max，复核 `SENTRY_PITCH_ZERO_RAD(-1.935)` 与真实行程的关系；② 行程改为实测非对称 min/max（夹在限位内 1~2°）；③ 补 pitch 实测角越界断力（结构级，需单独批准）；④ 才能改回 `1u`，且首次上电台架空、手扶枪管。

**哨兵影响**。[SENTRY] 仅 `config/sentry_2026/config_gimbal.h` 与哨兵台账；共享层、其他兵种零改动。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

### DEV-20261127-002745｜2026-11-27｜[SENTRY] USB 自瞄/导航接入 + 视觉连发 + 底盘功率

**目标与触发**。用户要求一次性完成哨兵电控侧：`0x0503/0x1021` USB 接入、与步兵一致的自瞄激活（左中挡 / 键鼠 Shift）、自瞄时允许小陀螺、视觉 `shoot` 走步兵式连发、底盘功率控制移植；交付标准是“只需功能测试”。既有电机/参数不动。

**功能变化**。
1. 视觉链路：新增 `apps/sentry/autonomy/vision_link.c/.h`，复用 `components/ax650/ax650_pose.c`；`autonomy` 仍为 USB 唯一所有者，RX 同一段字节同时喂 NV v1 与 RMCV，TX 增加 `0x1021`（10 ms，`sample_time_ms` 取 `ekf_ins.UpdateCount` 变化后的读取时刻）；`0x0503` 100 ms 超时 + `distance>=0`；裁判 `0x0207` 弹速、`0x0201` robot_id 填入姿态帧。
2. cmd：左中挡或键鼠 Shift 进入自瞄，目标经既有 KF + 二阶平滑后写 `big_yaw/pitch_abs/aim_valid`；断帧保持 300 ms；视觉 `shoot!=0` 且自瞄有效时按战术档位发 `LOADER_BURST`，热量守卫仍是唯一闸门；手动拨轮不变。
3. 云台：AUTO_AIM 走大 yaw 世界位置环（`YawTotalAngle` + `Gyro[2]`），小 yaw 用 `sentry_gimbal_allocate()` 吃 ±80° 内误差、大 yaw 承担剩余；自瞄与右下滑小陀螺可同时使用；pitch 自瞄用 `SENTRY_PITCH_ZERO_RAD + (世界目标 − 平台 Pitch)` 映射关节目标，首次进入以实测角为种子；新增 DM `T_Mos/T_Rotor` 80°C 健康判据。
4. 发射/热量：单发在途时 `LOADER_STOP` 不再写速度环；起步先写角度 ref 再切外环并清 PID；每发到位立即回速度环；热量冷却改为裁判 100 ms 阶梯扣减。
5. 底盘：移植 `chassis_power.c/.h`（304 模型、二次方程衰减、快降慢升），`chassis_limit_output(power_scale, integral_scale)` 直接压 `output_scale` 与速度环积分限幅；裁判 `0x0201` 限功率 + `0x0202` 缓冲衰减。
6. 导航：`SENTRY_AUTONOMY_ENABLE=1`；只有有效 NV COMMAND 才覆盖底盘模式，无命令/掉线保持手动。

**文件与符号**。新增 `vision_link.c/.h`、`chassis_power.c/.h`、`tests/sentry_chassis_power_test.c`；修改 `autonomy.c`、`cmd/{robot_cmd,cmd_authority,cmd_autoaim,cmd_kbm,cmd_remote}.c`、`gimbal.c`、`shoot/{shoot,heat_guard}.c`、`chassis/{chassis,chassis_kinematics,chassis_internal}.c/.h`、`sentry_def.h`、`config/sentry_2026/{config_ax650,config_autonomy,config_board,config_chassis,config_gimbal}.h`；测试桩/脚本同步。

**接口/协议**。`sentry_gimbal_cmd_t` 增 `pitch_abs/aim_valid`；`sentry_gimbal_feed_t` 增 `small_yaw_target_deg/pitch_abs_deg/pitch_online/small_yaw_online`；话题仍为 6 个。USB `0x1021` 31 B / `0x0503` 39 B 与协议文档一致；NV v1 不变。

**配置**。`AX650_USB_ENABLE=1`、`AX650_USB_POSE_PERIOD_MS=10`；`SENTRY_AUTO_AIM_ENABLE/PITCH_ENABLE/FIRE_ENABLE=1`、`HOLD_MS=300`、`BIG_YAW_MAX_SPEED=150`、`YAW_OFFSET=0`、`PITCH_WORLD_DIR=+1`、`DM_TEMP_LIMIT=80`；新增 `CHASSIS_POWER_*`（系数沿用步兵，标 `[待实测-哨兵]`）；`SENTRY_AUTONOMY_ENABLE=1`。

**验证**。ARM sentry 0 error/第一方 0 warning，FLASH 125,484 B、RAM 74,560 B（56.88%）、CCMRAM 54,944 B（83.84%）。主机 `run_chassis_tests`/`run_ax650_tests`/`run_profile_tests` PASS；静态闸门全 PASS。`run_fwk_tests` 的 fullchain 基线漂移为既有问题（HEAD 原始测试桩复跑同样差 110 行）。

**未执行/待实测**。未烧录、未台架、未整车。待标定：① `0x0503` 世界 yaw 与 IMU yaw 偏置；② pitch 平台补偿符号/零位；③ 视觉连发摩擦轮/热量/拨盘；④ 功率模型 `MODEL_SCALE/MARGIN`；⑤ 自瞄+小陀螺 DM 扭矩/温度余量。

**深度仿真（2026-11-27 补充）**。新增 `tests/simulation/sentry_axis_sim.py`：三轴+底盘集中惯量模型（J_big≈0.6、J_small≈0.01、J_pitch≈0.015 kg·m²，Coulomb/粘滞摩擦、DM/GM 力矩饱和与反电动势、底盘一阶加速、pitch 重力），按 `config/sentry_2026` 当前增益跑 200 Hz 控制/1 kHz 被控。
- 几何改为 600×600（轮心对角线 600√2），轮半径 75、减速比 19、转子限速 7000 rpm（同步兵）。
- 仿真暴露并已修正：**小 yaw 不能吃掉全部世界误差**——否则大 yaw 目标等于自身世界角，底盘持续旋转时两轴一起漂。改为大 yaw 快速世界环 + 小 yaw 慢配平（`SENTRY_AUTO_AIM_SMALL_TRIM_LP=0.02`，两轴目标之和恒等于世界目标）。
- 结果（恢复 4.2/0.8、限位 125°、trim LP 0.03 后）：小陀螺 150°/s 静态保持 1.55° max/1.28° RMS；小陀螺 + 0.2 Hz/20° 世界目标 2.75° max/1.57° RMS；小 yaw 60° 阶跃 1.28 s 进 1°；自瞄 140° 阶跃 2 s 末 1.09°；导航 0.5 rad/s 转向 + 20° 目标 19.24° max/末 0.57°。
- pitch 自瞄上限提到 150°/s、1.5 N·m、目标爬升 120°/s；平台 3°/1 Hz 跟踪在理想重力前馈下从 5.65° 降到 1.05°，因此 `SENTRY_PITCH_GRAVITY_A/B` 必须台架标定（默认 0 不改变手动行为）。
- 敏感性网格（J_big 0.4/0.6/0.8、力矩 0.8/1.0/1.2、摩擦 0.7/1.0/1.5）：小陀螺+0.2 Hz/20° yaw max 2.3~3.4°；pitch 平台 3°/1 Hz 无前馈 4.6~7.4°，理想前馈 1.05°。
- 物理包线：J_big≈0.6、10 N·m → 世界角加速度上限 ≈16.7 rad/s²；1 Hz/30° 目标需 ≈20.7 rad/s²，必然力矩受限（仿真 66~70° max）。自瞄+小陀螺只承诺 0.2 Hz/20° 量级；更快目标需降底盘转速或加力矩预算。
- 新增 host 测试：`tests/sentry_vision_link_test.c`（0x0503 分片/负距离/超时 + 0x1021 打包）、`tests/sentry_autoaim_test.c`（有效性/300 ms 保持/pitch 限幅/急停失效）、`tests/sentry_usb_pipeline_test.c`（同一 USB 流 NV COMMAND + 0x0503 双解析、单位/符号、0x1021 上行）、`tests/simulation/sentry_nav_sim.py`（NV→斜坡→麦轮→轮速/超时/符号）；均已接入 `run_profile_tests.ps1`。
- 仿真为集中参数模型、未做惯量辨识；结论用于筛选结构问题，不替代台架。

**重力前馈历史核对（2026-11-27）**。哨兵线**没有标定值**：`fb902ec` 的整机实现只接了代码通路，`SENTRY_PITCH_GRAVITY_A/B/PHASE_DEG` 全为 `0.0`（`[待实测]`）；`a24eeb6`（DEV-20261126-25）迁移 RM_shaobin2 时把整段重力前馈代码与宏**删除**；11-26 下午的 `9ce2f8c/5527068/b8669a4` 均未改重力参数；`sentry-legacy-20261126` 标签配置中已无该宏。仓库里唯一的重力前馈标定是**步兵** 2026-11-15/16 的 `A=1927 / PHASE=-92° / OFFSET=3371 / GAIN=1.0`，单位是 C620 电流码、对象是 M3508 pitch，**不能移植到哨兵 DM4310 的 N·m 域**。本次重新接入 DM 力矩域钩子（默认 0），必须按哨兵实车重标。

**历史参数回填（2026-11-27）**。按 11-26 哨兵线记录逐项核对：
- 大 yaw 速度环 `Kp/Ki 5.2/1.2 → 4.2/0.8`：11-26 18:49 `9ce2f8c` 初调小陀螺稳像，用于降低左右振荡；迁移提交 `a24eeb6` 用的是 RM_shaobin2 5.2/1.2 且注明“未宣称实车有效”，本次恢复哨兵线整定值。
- 小 yaw 控制限位 `90 → 125°`：11-23 阶段 0 手转实测机械行程 `±136°`（`DEV-20261123-24`），留 11° 余量；控制目标再内缩 10°，合 115° 可用。
- DM 过温阈值 `80 → 65°C`：11-26 记录 `0xC` 过温出现在转子 59~67°C（`DEV-20261126-11`），软件阈值必须低于该区间。
- `SENTRY_AUTO_AIM_PITCH_WORLD_DIR` 直接复用 `SENTRY_PITCH_DIRECTION=+1`：11-23 抬枪 `total_position` 增大实测（DEV-20261123-29），不再作为独立待标参数。

**仍缺、记录中没有的**：
- 重力前馈 `A/B/PHASE`：见上，哨兵无标定，步兵 C620 值不可移植。
- `SENTRY_AUTO_AIM_YAW_OFFSET_DEG`：新协议合同，需 NUC 与 `0x1021.yaw` 同世界系，或实车标固定偏置。
- 哨兵功率模型 `MODEL_SCALE/MARGIN`：11-26 功率标定是**步兵底盘**（`apps/infantry/chassis`），哨兵当前沿用同一组电机级系数，未在哨兵实测。
- 大 yaw 传动比：阶段 0 曾测 0.96（`DEV-20261123-29`），11-26 迁移采用 RM_shaobin2 的 1.0；当前保留 1.0，换同步带/机构后必须重标。
- 大 yaw 角度环 `Kp/DeadBand`：11-23 曾因 5~7° 虚位降到 8/2.0°，当前 20/0.02° 是迁移值；若机构仍有虚位，先按实车抖动复标，不能靠仿真定。

**外部资料核对（2026-11-27）**。DM-J4310 官方:连续 3.5 N·m、峰值 12.5 N·m、额定 120 rpm、空载 200 rpm,MIT 映射 P 12.5 rad/V 30 rad/s/T 10 N·m;GM6020 连续 1.2 N·m/1.62 A、空载 320 rpm。公开哨兵设计常用大 yaw 7~7.5 N·m、小 yaw ±45~90°、反馈超时 100 ms。本工程大 yaw 10 N·m/小 yaw 115° 属偏激进,已加运行期力矩/输出上限;DM 驱动把 0 N·m 映射到 MIT 中值(不是裸 0 帧),8 字节全 0 陷阱不适用但不得手发裸 0。视觉链公开经验约 100 ms,仿真 100 ms 延迟下 0.2 Hz/20° 真实误差约 4.8°,首轮先慢目标。

**运行期可写参数（2026-11-27）**。新增 ELF/DWARF 可写变量,默认值等于 config,上电行为不变:大 yaw 角度/速度环增益、小 yaw 限位、pitch 零位/重力前馈、自瞄世界 yaw 偏置、摩擦轮目标 RPM、导航加速度。台架可先用调试器写,确认后回填 config;非有限值/越界值会回退到 config 默认。符号与安全范围见 `ops/手柄操作手册.md` §8。

**现场文档（2026-11-27）**。新增 `ops/手柄操作手册.md`、`ops/功能检查表.md`、`params/待标定表.md`、`params/待实测参数表.md`；按 3 小时时间盒组织，优先重力前馈/世界 yaw 偏置/功率模型/摩擦轮 RPM/导航停止距离。

**哨兵影响**。[SENTRY] 仅 `apps/sentry/**`、`config/sentry_2026/**`、`.Doc/robots/sentry/**` 与测试/仿真；无共享层改动。

**AI**：DeepSeek-V4.1-Flash,2026-11-27。

### DEV-20261127-084627｜2026-11-27｜[SENTRY] USB CDC 接入 NV 与 RMCV 六类消息

- 目标：在哨兵短分支定义并实现 NUC—STM32 共用 USB CDC 字节流，覆盖 NV `COMMAND/FEEDBACK/SYNC_REQUEST/SYNC_RESPONSE` 与 RMCV `0x1021/0x0503`。
- 实现：`autonomy.c` 保持 USB 唯一所有者；新增 `vision_link.c/.h` 复用 `ax650_pose`，完成 `0x0503` 分包/粘包解析、100 ms 新鲜度和 `0x1021` 新 INS 样本上行。
- 安全：`SENTRY_AUTONOMY_ENABLE=0` 未改变；`0x0503` 只缓存，不接云台与发射；`0x1021` 的弹速、模式和本车 ID 暂发配置占位值 0。
- 时间：`0x1021.sample_time_ms` 是 200 Hz 应用任务检测到新 EKF 序号后的 MCU 读取时刻，不冒充 IMU 硬件或 EKF 内部精确采样时刻。
- 文档：更新 `ops/导航协议与上位机.md` 和哨兵 README，记录六类消息、帧布局、CRC、坐标、超时、占位字段和未实现草案。
- 验证通过：`build.ps1 sentry -Current`；`run_chassis_tests.ps1`；`run_ax650_tests.ps1`；`run_profile_tests.ps1`；新增 `run_sentry_usb_tests.ps1` 的 `-O0/-Ofast`；兵种范围守卫；文档链接检查。
- 已执行但未通过：`run_fwk_tests.ps1` 在未修改的步兵整链 harness 重复定义 `gimbal_imu_data` 处失败；G4 旧基线在干净 `origin/sentry` 已存在的 9 处红旗上失败。两项未由本分支改动引入，见问题台账。
- 未执行：烧录、实体 USB 枚举与抓包、NUC 双端闭环、云台/发射/底盘运动；这些仍须用户现场验证。

### DEV-20261128-041135｜2026-11-28｜[SENTRY] 哨兵通信配置改为 NUC 语义

- 目标：按实车事实移除哨兵应用与配置中的 AX650 设备语义；哨兵上位机为 NUC，固定通过 USB CDC 通信。
- 实现：`config_ax650.h` 更名为 `config_nuc.h`，哨兵 USB 开关、周期、超时和默认字段统一为 `SENTRY_NUC_RMCV_*`；删除哨兵 USART1 AX650 装配分支，USART1 继续只供三轴编码器 VOFA；cmd 原始视觉 yaw 诊断符号改为 `nuc_raw_yaw_target` / `cmd_get_nuc_raw_yaw()`。
- 兼容：线上 NV v1 与 RMCV `0x1021/0x0503` 字节协议不变；共享 `components/ax650/ax650_pose` 仅作为历史协议编解码实现保留。因共享 CMake 仍编译未注册的 `ax650_uart1.c`，`config_nuc.h` 底部保留关闭态编译别名，不代表运行期使用 AX650。
- 验证：`run_sentry_usb_tests.ps1`、`run_profile_tests.ps1`、`run_ax650_tests.ps1` 均 PASS；robot-scope、命名、宏/重复宏、注释、文档链接和源码清单检查均 PASS；`build.ps1 sentry -Current` 成功，text 126,604 / data 732 / bss 128,808，输出 `firmware/sentry.elf`。构建仍有既存 CMake CMP0175 开发警告及 newlib syscall 链接警告；会话开工 G4 在干净基线既存的 9 处红旗上失败，本改动未新增红旗。
- 未执行：烧录、实体 USB 枚举与 NUC 双端抓包；实体功能仍需现场验证。
- 哨兵影响：[SENTRY] 仅 `apps/sentry/**`、`config/sentry_2026/**`、哨兵测试和 `.Doc/robots/sentry/**`；共享层与其他兵种无行为改动。
- AI：OpenAI Codex（GPT-5）。

## 5. 历史

- `.Doc/history/开发史.md` 附录 I：按 `[SENTRY]` 或日期检索 2026-11 的哨兵条目。
- 开发史第十六、十七章：哨兵整机与功率标定过程。
