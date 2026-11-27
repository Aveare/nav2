---
name: rm-electrical-dev
description: 开发、移植和维护 RoboMaster STM32 多兵种整车电控(自研分层 apps/components/framework/ports)。提供当前树符号级代码地图、兵种分支与文档归属规则、历史踩坑清单、模块接入/调参/验证流程、硬件协议速查,以及 verify/dev_id/power_calib 三个整链脚本。覆盖底盘、云台、发射、通信、保护与兵种专用机构。
metadata:
  version: "2.0.0"
  version_date: "2026-11-27"
  updated_at: "2026-11-27T00:00:00+08:00"
  timezone: "Asia/Shanghai"
  author: "金陵科技学院CR战队  Colin / 小猫大王"
---

# RoboMaster 整车电控开发规范

本 skill 服务 **RM-JIT-CR**：STM32F407 + FreeRTOS，自研分层 `apps → components → framework → ports → HAL`，
单仓三兵种（infantry / sentry / heavy）。**2026-11-27 起已完全脱离上游跃鹿快照**：
不再有 `audit_baseline`、`check_plan`、`assets/yuelu-basic-framework/`；字节事实源是 git，
结构基线是 tag `arch-after-U3U4` + `.Doc/gate/manifest-arch-after-U3U4/`。

## 1. 会话入口(先定位,再动手)

1. 读 `.Doc/spec/文档阅读分级.md` §1 的 A 层 5 份（`AGENTS.md`、分级表、兵种分支规范、`README.md`、根 `RM_DEVLOG` §0 + 当前兵种 `DEVLOG.md` §0）。
2. 跑 `python tools/check_robot_scope.py`，确认分支/兵种/profile；分支与兵种不符先问人。
3. 按 [代码地图](references/code-map.md) 找到症状对应的文件/函数/调用链，再改。
4. 需要历史原因时查 `.Doc/history/开发史.md` 附录 I（按 DEV 编号）；不要读归档原文当现行结论。

| 任务类型 | 首要参考 | 必要产出 |
|---|---|---|
| 新功能/移植 | 代码地图 + [工作流](references/workflows.md) + `.Doc/spec/架构与开发指南.md` | 调用链、兼容性说明、验证、DEV 条目 |
| 缺陷修复 | 代码地图 + [历史踩坑](references/lessons.md) + 当前兵种 `DEVLOG.md` §3 | 最小修复、复现/回归、问题状态 |
| 调参/标定 | [工作流](references/workflows.md) §调参 + `.Doc/guide/调参诊断手册.md` | 证据等级、实车结论、参数落点 |
| 硬件/接线核对 | [硬件速查](references/hardware.md) + `config/<robot>_2026/` | 型号/总线/ID/方向/零位确认 |
| 只读审查 | 代码地图 + 兵种库 + 开发史 | 发现、依据、可执行建议 |

## 2. 铁律(与 AGENTS.md 同源,冲突以 AGENTS 为准)

- **一次只碰一个兵种**：兵种私有路径 = `apps/<robot>/`、`config/<robot>_2026/`、
  `tests/*<robot>*`、`.Doc/robots/<robot>/`；跨兵种改动会被 `check_robot_scope.py` 判 FAIL。
- **共享层改动只经 `main`**：`components/framework/ports/tools` 与共用文档走 `fix/shared-*` → `main`，
  再下行同步到兵种分支。
- **短分支模型**：`<type>/<robot>/<功能>-<用户>`，用户确认功能正常后 `--no-ff` 合并回兵种分支并删除。
- **结构级改动需用户明确批准**：新增/删除 C/H、新函数/任务/消息字段、改调用链、改接口、改构建文件。
- **证据等级**：`[实测]/[理论]/[待实测]`；未实测不得写成现象结论，编译通过不等于闭环。
- **不烧录、不动实车**：烧录/台架/整车始终由用户执行；主机测试与 ARM 编译已获授权。

## 3. 分层落点

- `apps/<robot>/`：应用状态、目标值、装配（`profile.h` 编译期 + `robot_task` 任务表）。
- `components/`：设备协议、控制器、组件算法（不得 include `apps/` 头）。
- `framework/`：平台无关 kernel/err/mem/msg/diag（**零 RTOS include**）。
- `ports/stm32f4`：外设落地；`ports/freertos`：唯一 RTOS 后端。
- 任务/延时/临界区走 `framework/kernel`；消息走 `framework/msg`；参数只 include 当前 profile 的 `config.h` 伞头。
- 文档三层：共用 `spec/guide/plan`、兵种 `robots/<robot>/`、历史 `history/`；规则见
  `.Doc/spec/文档存放与管理规则.md`。

## 4. 验证与交付

```powershell
python .agents/skills/rm-electrical-dev/scripts/verify.py          # 守卫+链接(+--tests)
python .agents/skills/rm-electrical-dev/scripts/dev_id.py --new    # 生成时间戳 DEV 编号
python .agents/skills/rm-electrical-dev/scripts/power_calib.py --help  # 功率标定整链
.\build.ps1 <robot> -Current
```

- 默认只改源码/文档/配置；构建与测试已授权，烧录未授权。
- 交付说明：完成项、接口/配置影响、验证结论、未执行项、DEV 条目位置、commit/分支/推送结果。
- 实体故障只有达到关闭条件才能标记已解决。

## 5. 参考

- [代码地图](references/code-map.md)：症状 → 文件/函数/调用链（唯一事实源）。
- [历史踩坑](references/lessons.md)：串口/IWDG/HardFault/功率/云台/发射的反复踩坑与铁律。
- [工作流](references/workflows.md)：模块接入、调参、验证、交付、记录。
- [硬件速查](references/hardware.md)：板型、总线、电机协议、裁判/DBUS 要点。
- [AI 协作约束](references/ai-constraints.md)：授权分级、会话仪式、DEC/DEV 记录、提醒义务。
