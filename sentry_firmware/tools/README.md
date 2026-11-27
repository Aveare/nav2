# tools/ 工具索引

> 一行一个工具;`★` = 日常高频。详细流程与判据见 `.Doc/guide/调试与测试流水线.md`。

## 一、静态闸门(python,提交前必过,已入 CI)

| 工具 | 查什么 |
|---|---|
| check_naming.py | 命名规范(文件/函数/类型/宏/变量) |
| check_macros.py | config 宏账本 + 子头跨文件 #if |
| check_macro_dups.py | 跨文件同名宏冲突/未登记重复/单字母宏(--profile 选兵种) |
| check_macro_golden.py | G1b 宏守恒探针(-E -dM 快照;删除/改值 FAIL) |
| check_abi_golden.py | T11 协议结构 sizeof/offsetof 快照 |
| check_comment_style.py | 注释风格(占位符/TODO/禁用标签) |
| check_robot_scope.py | R1 兵种范围(本地提交前 + refactor_gate;CI 工作区干净无判定意义,不入 CI):兵种分支改其他兵种 = FAIL,改共享层 = WARN |
| check_build_parity.py | Makefile 与 CMake 的 -D 集合 parity |
| check_symbol_contract.py | 调试工具引用的固件符号仍存在 |
| check_source_lists.py | Makefile 源清单与磁盘一致 |
| check_sections.py | RAM/CCM 占用与段属性 |
| g4_baseline.py | 红旗基线(git 树快照/比对;权威机制) |
| site_ledger.py | while(1)/malloc 站点台账生成 |
| check_tags.py / refactor_gate.py | 批次 DEV 条目 / 重构闸门聚合(含 check-links、R1) |

## 二、主机测试(ps1)

| 工具 | 覆盖 |
|---|---|
| run_fwk_tests.ps1 | 框架全家桶(含上面多数闸门+金标准对拍+三骨架) |
| run_chassis_tests.ps1 | cmd 仲裁/看门狗 |
| run_ax650_tests.ps1 | 视觉编解码/串口链路 |
| run_profile_tests.ps1 | 三兵种 profile/assembly 编译 |
| run_fullchain_harness.ps1 | 整链 700 拍对拍(也被 fwk 调用) |
| run_can_golden.ps1 | CAN 过滤器金标准(也被 fwk 调用) |

## 三、上车调试(实时)

| 工具 | 用途 | 典型命令 |
|---|---|---|
| ★ doctor.js | 链路体检:桥接→固件身份→供电→关键指针 | `node tools/doctor.js` |
| ★ pitch_trace.js | EmberProbe 后台变量采样(支持 --preset 症状预设) | `node tools/pitch_trace.js --preset yaw_oscillation` |
| trace_presets.json | 症状→变量集契约 | 改前跑 check_symbol_contract |
| analyze_var_trace.py | CSV 三态判据(振荡/静止/慢漂) | `python tools/analyze_var_trace.py <csv> --all` |
| motor_calib.py | 电机速度环离线标定(K/τ/σ→Kp/Ki 初值) | 见调参诊断手册 §6 |
| gimbal_trace.js / gimbal_axis_probe.js | 云台专项(yaw 全量/轴探针) | `node tools/gimbal_trace.js` |
| chassis_trace.js / follow_trace.js / odom.js | 底盘专项(功率/跟随/里程计) | `node tools/chassis_trace.js` |
| analyze_follow*.py / analyze_trace.py / analyze_emberprobe_csv.py | 采集 CSV 的专项分析 | — |
| smoke_check.py | 烧录后 RTT 标记断言 | `python tools/smoke_check.py log/_console.log` |
| rtt_pwr_to_csv.py | RTT `[pwri]/[pwr]` → 功率标定 CSV(每稳态点一行) | `python tools/rtt_pwr_to_csv.py log/300.log --power 84.6 --append steady.csv` |
| rtt_capture_point.py | 采一个 20s 工况并追加 staging CSV(`--no-append` 只存日志) | `python tools/rtt_capture_point.py --cond spin300 --seconds 20` |
| fit_power_model.py | 稳态点 CSV → M3508 六项功率模型 K0..K5 | `python tools/fit_power_model.py steady.csv` |

## 四、RTT/日志通道

| 工具 | 用途 |
|---|---|
| capture_rtt.bat / stop_capture.bat | 常驻抓 RTT → log/_console.log(双击即用) |
| rtt_read.py | 独立 OpenOCD 读 RTT(不占 EmberProbe) |
| rtt_log.py | RTT 连续落盘变体 |
| check_log_format.ps1 | 日志浮点占位符纪律(%f 禁令) |
| capture_rtt.bat 的日志 | smoke_check.py / 人工 grep 的输入 |

## 五、烧录/维护

| 工具 | 用途 |
|---|---|
| flash_manual.ps1 | 手动烧录(DAP/JLink 参数化) |
| doctor.js --keep | 体检后保留会话,紧接着采样 |
| task.ps1(仓库根) | JLink GDBServer / debug 的 Makefile 片段(不是 PowerShell 脚本) |
| lib/preflight.js | JS 工具共享的桥接预检 |

## 六、Skill 封装(rm-electrical-dev/scripts)

| 脚本 | 用途 |
|---|---|
| verify.py | 一条命令跑 守卫 + 链接 + 可选主机测试,输出单结论 |
| dev_id.py | 生成时间戳 DEV 编号并检查重复(扫 DEVLOG + 开发史 + git log) |
| power_calib.py | RTT 采点 → CSV → 功率模型拟合 的整链封装 |

## 七、新增工具的准入清单

1. 有唯一职责;与现有工具重叠时先扩展现有工具(如 --preset)。
2. 引用固件符号必须登记 `.Doc/gate/tool-symbols.json` 并过 check_symbol_contract。
3. 输出带结论(退出码 0/非 0),不留"人肉读屏"步骤。
4. 在本表登记一行;涉及流程时同步《调试与测试流水线》。
