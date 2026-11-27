# 步兵 Infantry 文档库

> 本目录只放**步兵专属**文档。共用规范/方法在 `.Doc/spec/`、`.Doc/guide/`。
> 存放规则见 `.Doc/spec/文档存放与管理规则.md`。

## 当前定位（2026-11-27）

> **台账**：[DEVLOG.md](DEVLOG.md)（状态、问题、近期 DEV；标题带 `[INFANTRY]` 标记）。

- 本仓库**不再以步兵为交付目标**；`apps/infantry/` 与 `config/infantry_2026/` 保留为共享框架回归、
  历史参数与功率控制实现（**功率控制目前只在步兵 profile 编译**，哨兵未接）。
- 构建：`.\build.ps1 infantry`（产物 `build/basic_framework.elf`）；
  Makefile 只支持步兵，CMake 是 GLOB。
- 历史：`.Doc/history/开发史.md` 第一~十五章与附录 I。

## 目录

| 路径 | 内容 |
|---|---|
| `design/` | yaw/pitch/底盘/功率/平移/发射的设计与优化计划（战绩与判据） |
| `params/` | `调参结论.md`、`参数证据表.md`、`功率模型标定.md` |
| `plans/` | `小陀螺抗自瞄改造计划.md` |
| `ops/` | （待建）步兵专属烧录/台架步骤 |

## 接线参考（历史，非烧录依据）

> 下表来自 2026-11 步兵实车，哨兵接线不同；保留用于共享代码回归与历史追溯。

| 总线 | 设备 | 电调拨码/ID |
|---|---|---|
| CAN1 | 轮电机 M3508+C620 | 左前=4 右前=1 左后=2 右后=3 |
| CAN1 | 云台 YAW GM6020 | 5 |
| CAN2 | 摩擦轮 M3508+C620 | 左=1 右=2（右轮 REVERSE） |
| CAN2 | 拨盘 M2006+C610 | 3 |
| CAN2 | 云台 PITCH M3508+C620 | 4 |
| USART3 | DBUS 遥控器 | 100000 8E1 反相 |
| USART6 | 裁判系统 | 115200 8N1 |
| SPI1 | BMI088 IMU | — |

## 枪口热量守卫（步兵实现）

`apps/infantry/shoot/heat_guard.c` 在发射前裁决"能否发 / 发多快"，数据来自裁判
`0x0201/0x0202/0x0204`。三条不变量：反馈只校正热量估计 Q̂、记账点前移到放行瞬间、
误差只允许往安全方向。

- 两级闸门：连发保留余量 30 → 降 `SINGLE`；单发余量 10 → `HOLD`；裁判离线 `OFF` 直通，
  曾上线但断流 300 ms 降级限速。
- 射速律：`HEAT_GUARD_RATE_MODE=1` 恒定摊平（窗口 4 s）；230/14 档连发约 7 发/s，
  约 4 s 后落到 1.4 发/s。
- 战术档：键鼠 X 循环 `ECO(1.4) → NORMAL(6) → BURST(12)` 发/s，实际放行仍由守卫裁决。
- `HEAT_GUARD_ALLOWANCE_ENABLE` 步兵默认 0（台架额度恒 0，正式比赛前置 1）。
- 参数：`config/infantry_2026/config_heat.h`；设计依据：`.Doc/spec/热量管理设计.md`。

## 入口

- 应用：`apps/infantry/`；参数：`config/infantry_2026/config.h`。
- 共享回归：`tests/run_profile_tests.ps1`、`tests/run_fwk_tests.ps1`。
