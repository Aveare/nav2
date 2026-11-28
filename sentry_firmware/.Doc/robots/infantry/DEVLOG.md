# 步兵 Infantry 开发台账

> 步兵专属台账；分支 `infantry`。公共内容见根 `RM_DEVLOG.md`；文档索引见 `.Doc/robots/infantry/README.md`。
> 条目标题统一 `[INFANTRY]` 前缀。

## 0. 当前状态

| 项目 | 内容 |
| --- | --- |
| 定位 | 不再作为交付目标；保留为共享框架回归 + 历史参数 + **唯一功率控制实现**（哨兵未接） |
| 应用/参数 | `apps/infantry/` + `config/infantry_2026/` |
| 构建 | `.\build.ps1 infantry`（产物 `build/basic_framework.elf`）；Makefile 只支持步兵 |
| 参数与结论 | `.Doc/robots/infantry/params/`（调参结论、参数证据表、功率模型标定） |
| 设计依据 | `.Doc/robots/infantry/design/`（yaw/pitch/底盘/功率/平移/发射） |
| 最近验证 | 以开发史附录 I 的 `[INFANTRY]` 条目为准；2026-11-26 功率标定/小陀螺/平移在册 |
| 下一步 | 发射硬件修复后完成 RM-002；需要时评估把功率控制移植到哨兵（结构级，需单独批准） |

## 1. 问题台账

| ID | 现象 | 状态 | 关闭条件/证据 |
| --- | --- | --- | --- |
| RM-002 | 发射机构硬件问题无法正常运行（单发/三发/射速代码已实装，DEV-20261030-01） | **挂起（硬件）** | 单发逐发出弹无连击/无卡弹；连发射速与堵转保护实车确认 |
| RM-003 | pitch 偶发抖动消失：速度环反馈改欧拉 pitch 角速度 | 已关闭 | 用户实车确认（DEV-20261030-02） |
| RM-004 | pitch 粘滑/异响/松杆续滑收敛 | 已关闭 | 用户实车确认（DEV-20261030-03~11） |

> 其余已关闭项（RM-006~012）见开发史附录 I；证据以对应 DEV 条目为准。

## 2. 近期开发条目

<!-- 格式:### DEV-YYYYMMDD-HHMMSS｜日期｜[INFANTRY] 标题 -->

### DEV-20261128-122628｜2026-11-28｜[INFANTRY] 比赛启用 0x0208 允许发弹量硬闸门(治"开赛即断发射机构")

**触发**。比赛实测:发射机构一开赛就断电,开赛前正常,**无判罚提示**;赛后拔掉全部电源口
空载、单插底盘(分电板仅四电调)复测仍提示——排除负载问题。定位:`heat_guard.c:245`
设计注释明示 **2026 机制=0x0208 允许发弹量归零后裁判直接锁发射机构(断电,非判罚)**,
而 `HEAT_GUARD_ALLOWANCE_ENABLE=0`——守卫没启用这道闸,裁判先动手。

**改动**。`config_heat.h`:`HEAT_GUARD_ALLOWANCE_ENABLE 0→1`。额度<1发时守卫先禁发,
抢在裁判锁枪之前。0x0208 解析链路已核(referee_protocol.h 0x0208 → referee.h
AllowanceData → heat_guard)。裁判在线但未开赛时额度可能为 0 → 禁发属预期(保护);
台架无裁判不受影响。额度耗尽/恢复有 `LOG_WARN "[heat] allowance ..."` 可 RTT 抓取。

**验证**。`build.ps1 infantry -Current` 0 error;`run_profile_tests.ps1` 全绿;
固件 SHA `A4D5DDEE…`。未烧录未赛场。

**待实测**。①下次比赛/裁判在线测试:开赛瞬间 0x0208 额度值(RTT 抓
`[heat] allowance` 日志)——若服务器延迟下发额度,开赛头几秒禁发属预期;
②射速×额度预算:按初始额度与射速估算可持续时间,超预算由守卫降档;
③断电形态复核:额度锁枪=摩擦轮+拨盘同时停。

**AI**:GLM-5.3-Flash,2026-11-28。

### DEV-20261127-202746｜2026-11-27｜[INFANTRY] pitch 方向符号回正：PITCH_ECD_SIGN=-1、KBM_PITCH_DIR=+1

**触发**。工作区里两项 pitch 方向符号改动早于本轮刹车/功率工作，DEV-20261127-184657 曾注明“本地未提交、与本次无关”；用户指示本次一并提交。

**改动**。
- `config/infantry_2026/config_gimbal.h`：`PITCH_ECD_SIGN +1.0f → -1.0f`（编码器方向，使 `sign*total_angle/ratio` 与 IMU Pitch 同向）。
- `config/infantry_2026/config_kbm.h`：`KBM_PITCH_DIR -1.0f → +1.0f`（鼠标 pitch 方向，上滑 = 枪口上抬）。
- `.Doc/gate/arch-baseline/macros-probe.txt`：按 `--update` 重建宏基线（310/310），消除这 2 项偏差。

**依据**。方向符号属参数级改动，以实车当前配置为准；本次对话未重新做方向标定，提交的是工作区既有值。

**验证**。`check_macro_golden.py --update` 后 310/310 PASS；ARM 强制重编 19 个文件 0 error / 第一方 0 warning，ELF SHA-256 与上一版一致
`8291066A2F600FF24A29B82E44816D5FE1941831994433C2AC02E5A0514AFF6E`（说明上一版构建已包含这两项符号，本次只是把配置纳入版本库）；RAM 55.6% / CCM 83.8% / FLASH 12.4%。

**风险**。若实车 pitch 方向实际未确认，上滑/回正会反向；首次上车先低速点动确认，不对就单独翻回对应一项，不要两项一起翻。

**AI**。DeepSeek-V4.1-Flash。

### DEV-20261127-200745｜2026-11-27｜[INFANTRY] 制动轮退出功率预算：刹车不再被压到 20%

**触发**。用户反馈纯平移 2000 mm/s 松杆后仍滑行；复盘发现功率模型用 `|i|·|ω|` 把回馈制动当成正功率，2000 mm/s 满刹车被判成约 450 W，`scale` 压到 0.201（下限 0.20）。DEV-20261127-184657 把 `BRAKE_RAMP` 提到 8000 在这条路径下基本无效——刹车电流先被功率 scale 削掉约 80%。

**改动**。
- `apps/infantry/chassis/chassis_power.c`：求和时跳过 `i·ω < 0` 的制动轮；`S0/S1/S2` 只统计电动轮子集，全轮制动时返回 `s=1.0`。静止（ω=0）仍按电动处理。
- `apps/infantry/chassis/chassis_kinematics.c`：`chassis_limit_output` 改 per-wheel `output_scale`——制动轮保持 1.0，电动轮用 `power_scale`；积分限幅按各自 scale 同步。近零速（|ω|<30 deg/s）按电动处理，避免换向符号抖动。
- `chassis_power.h` 与源文件头注释同步口径（“制动工况按正功率保守估计”已删除）。
- `tests/simulation/chassis_power_sim.py`：`predict()` 增加 `include_braking` 开关，新增 `[5]` 制动/混合工况对照与断言（PASS，退出码 0）。

**模型结果（88 W 目标）**。
| 工况 | 旧口径 s / 预测 | 新口径 s / 预测 |
| --- | --- | --- |
| 500 mm/s 满刹车 | 0.613 / 175 W | **1.000 / 0** |
| 1000 mm/s | 0.393 / 266 W | **1.000 / 0** |
| 1500 mm/s | 0.273 / 358 W | **1.000 / 0** |
| 2000 mm/s | 0.201 / 450 W | **1.000 / 0** |
| 2500~3000 mm/s | 0.200 / 543~637 W | **1.000 / 0** |
| 混合 2 电动 + 2 制动 @2000rpm | 0.345 / 297 W | **0.651 / 150 W（限幅后 88.0 W）** |
| 电动基准 2000 mm/s i=800 | 43.0 W / s=1 | **43.0 W / s=1（不变）** |

**验证**。仿真 `[5] PASS`；另用 `gcc` 主机直编 `chassis_power.c` 核对 4 组工况（纯制动 s=1.000/pred=0、电动基准 43.0 W、混合 0.6505/post=88.0、电动满指令 0.3449/post=88.0）全部 PASS（一次性校验程序未入库）；
ARM 0 error / 第一方 0 warning，text 129,512 / data 820 / bss 126,984，RAM 55.6% / CCM 83.8% / FLASH 12.4%；
`firmware/current.elf` SHA-256 `8291066A2F600FF24A29B82E44816D5FE1941831994433C2AC02E5A0514AFF6E`；
`tests/run_chassis_tests.ps1` ALL PASS；源清单/命名/注释风格/段占用 PASS。
`check_robot_scope` 仍因工作区同时含步兵/哨兵改动报 FAIL（既有状态，与本次无关）；本轮未提交、未烧录、未实车。

**风险**。修好后 8000 mm/s² 的刹车会真正落地（约 0.82g），可能超过麦轮附着极限；建议台架/实车先把 `CHASSIS_VX/VY_BRAKE_RAMP` 降到 5500~6000 试，并观察四轮温度（若 C620 不回馈而是耗在电机里）。首轮实车需用 VOFA 看电流与温度趋势确认回馈方向。

**AI**。DeepSeek-V4.1-Flash。

### DEV-20261127-184657｜2026-11-27｜[INFANTRY] 平移减速段非对称刹车：加速 5000/20000 不变，减速 8000/40000

**触发**。用户反馈纯平移提速到 2000 mm/s 后，松杆停车会往前滑行；要求优化刹车。

**改动**。
- `config/infantry_2026/config_chassis.h`：新增 `CHASSIS_VX/VY_BRAKE_RAMP=8000`、`CHASSIS_VX/VY_BRAKE_JERK=40000`。
- `apps/infantry/chassis/chassis.c`：新增运行期可写 `chassis_vx/vy_brake_ramp/_jerk`（<=0 回退加速段参数）；斜坡调用点按“`cur*target<0` 或 `|target|<|cur|`”判定减速段，加速段仍用原 5000/20000。起步时 `cur=0` 不触发减速段。
- 小陀螺刹车优先 `chassis_spin_brake_factor()` 仍按加速段 accel 归一，原整定不动。

**理论停车（2000 mm/s，按 S 曲线参考轨迹）**：5000/20000 = 0.645 s / 650 mm → 8000/40000 = 0.445 s / 450 mm；6000/30000 = 0.510 s / 532 mm。减速度约 0.82g，麦轮存在打滑风险；实车若轮子尖叫/抱死反而滑更远，运行期降到 6000/30000 或写 0 回退。**本轮未烧录、未实测。**

**验证**。ARM 0E0W；text 129,424 / data 820 / bss 126,984；RAM 55.6% / CCM 83.8%；
`tests/run_chassis_tests.ps1` ALL PASS；`firmware/current.elf` SHA-256
`1DD6422A3A556266229195D6EB9F734EB588F69142DB85280FC1316D7805F5AB`。
宏金标准已补 4 条新宏（310/310）；`PITCH_ECD_SIGN`/`KBM_PITCH_DIR` 2 项偏差
已由 DEV-20261127-202746 提交；命名/宏/分区/源清单/宏重复检查 PASS；`check_robot_scope`
因工作区同时含步兵/哨兵改动报 FAIL（既有状态）。

**AI**。DeepSeek-V4.1-Flash。

### DEV-20261127-152347｜2026-11-27｜[INFANTRY] 88W 功率控制全包线仿真：无 floor 顶穿，最大降额 scale 0.496

**触发**。用户要求“性能解开限制后功率控制不要掉链子，再仿真模拟一下”。

**方法**。新增 `tests/simulation/chassis_power_sim.py`（主机、纯 stdlib）：逐宏读取 `config_chassis.h` 的 K0~K5 / SCALE / CURRENT_DIV / LIMIT / MARGIN / MIN_SCALE，复刻 `chassis_power.c` 的二次解 + 快降慢升 + floor；扫描 `(i_cmd 0~10000, 轮速 0~7500rpm)` 全包线，并覆盖新三状态稳态点。

**结果**。
- 稳态：纯平移 2000=**43W**、纯自转 520=**76W**、组合 2000+285=**58W**、1500+344=68W、2000+285 对角=71W，**全部 s=1，不触发限幅**。
- 压力：速度环满输出 i=10000 时，0rpm=84W(s=1)、2000rpm=119W(s=0.835)、4000rpm=155W(s=0.684)、7453rpm=216W(s=0.496)；**全包线 p_post=88.0W，floor 0.20 从未触发**（最低 s=0.496）。
- 88W vs 85W：同点 scale 高约 2~3%（7453rpm/i=10k：0.496 vs 0.482），收益小但真实。
- 模型盲区：80W 以上是外推（实测只到 55W），偏差按 ±5~10W 估；2W 余量下真实可能短时 90~95W，靠 60J 缓冲吸收。

**结论**。控制律本身不会掉链子：降额同拍生效、全包线不需要 floor 兜底；风险只剩模型外推误差与真实功率标定。实车看 `[pwr] po` 与裁判功率，持续 >90W 回 `MARGIN=5`。

**验证**。`python tests/simulation/chassis_power_sim.py` 输出如上；本次仅新增仿真脚本，未改固件，无需重新构建。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

暂无（2026-11-27 分库后新增条目从这里开始）。

### DEV-20261127-151552｜2026-11-27｜[INFANTRY] 底盘三状态提速：纯平移 2000 / 纯自转 520 / 组合 governor 夹低 + 功率目标 88W

**触发**。用户要求：纯平移限到 2000；纯小陀螺尽量高；给出平移+小陀螺的最佳组合；功率压到 88W 以内并尽量贴住。

**改动**（`config/infantry_2026/config_chassis.h`，参数级；`chassis.c`/`chassis_spin.c` 注释同步）：
- **纯平移**：`CHASSIS_MAX_SPEED` 1500→1800→**2000**（2000 轮速 3,420rpm、约 43W，限制在打滑/操控不在功率）。
- **纯小陀螺**：`CHASSIS_ROTATE_SPEED` / `ROTATE_SPEED_TRANS` 330/280→**520/520**；`SPIN_HIGH_DPS` 390→**520**；`SPIN_WHEEL_RPM_MAX` 6800→**7500**，`CHASSIS_MAX_MOTOR_RPM` 7000→**7500**（= 空载 8,911 的 84%）。
- **变速档**：`SPIN_CRUISE_DPS` 280→**310**、`MOD_A1/A2` 0.24/0.12→**0.451/0.226**、`MOD_LIMIT` 0.36→**0.677** → **100~520**；`SPIN_LOW` 180→**100**；`ALPHA_MAX` 800→**1500**。
- **组合**：`CHASSIS_SPIN_MAX_SPEED` 1200→**2000**（允许平移拉满，自转由 7,500rpm governor 自动夹低：1,000 平移约 404、1,500 约 344、2,000 约 285）；`SPIN_TRANS_HIGH_DPS` 340→**450**。
- **功率**：`CHASSIS_POWER_MARGIN_W` 5→**2** → 控制目标 **88W**（上限仍 90W）。

**依据**。45° 麦轮：平移 1 mm/s = 10.26deg/s 转子，自转 1 deg/s = 86deg/s 转子，四轮共用 `10.26v+86wz` 预算。裁判实测 390 纯自转 50~55W、1500 平移 29~36W、1000+300=44W，线性外推 88W 允许 ~650deg/s / ~4.1m/s，均高于 7,500rpm 机械线——**瓶颈是轮速不是功率**。7,500rpm 下纯自转 7,453rpm（99% 软限）、约 70W；组合 2000+285 约 81W，留 7W 给模型误差与云台/发射。

**风险**。
- 7,500rpm = 空载 84%，只剩 ~16% 转速余量，扭矩/电流环余量小、温升快，连续满载必须复测温度，**不再上调**。
- 88W 功率模型在 80W 以上属外推（标定只到 55W），实际可能短时到 90~95W，靠缓冲吸收；赛前未重拟合建议回 85W。
- 2000 平移 / 520 自转 / 100~520 变速均未落地实测；`ALPHA_MAX=1500` 下 100~520 快速分量仍会被圆滑到约 130~490。

**验证**。`build.ps1 infantry`：0 error、第一方 0 warning（仅 newlib `_close/_lseek/...` 既有 stub 警告），text 129,140 / data 804 / bss 126,984，FLASH 129,952，RAM 55.6% / CCM 83.8%；`run_chassis_tests.ps1` ALL PASS；`check_macro_golden.py --update` 后 PASS（306 宏）；`check_sections.py` PASS。
`run_profile_tests.ps1` 未过：sentry USB pipeline 断言依赖工作区未提交的台架锁 `SENTRY_AUTONOMY_ENABLE=0`，与本改动无关。

**待实测**。`[chcal]`/`[pwr] p/po/s`：加速瞬态 po 是否贴 88、稳态是否只有 60~80W；纯自转 520 的电流/温升；组合 2000+285 的实际 est_vx/est_wz；变速实际带宽。

**AI**：DeepSeek-V4.1-Flash，2026-11-27。

## 3. 历史

- `.Doc/history/开发史.md` 附录 I：按 `[INFANTRY]` 或日期检索。
- 开发史第一~十五章：步兵从跃鹿模板到首测与功率标定的完整过程。
