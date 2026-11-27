# AGENTS.md — AI 协作契约(工程根)

> 按 agents.md 标准四段式维护,保持简短、可执行、随赛季更新(过期的规则比没有更糟)。
> 每条规则要么改变一个动作、要么产生可检查的证据,否则删掉。
> 规范依据:`.Doc/spec/`(架构/命名/注释/纪律/分支/文档存放);AI 侧技能入口 `.agents/skills/rm-electrical-dev/`。

## Map(工程地图)

- 工程:RM-JIT-CR,单板 STM32F407IG + FreeRTOS。**自研分层 `apps → components → framework → ports → HAL`**
  (B0a~B12 已收口);`application/`、`modules/`、`bsp/` 已删除,历史条目里的这些路径按当时语义理解。
- **执行入口**:
  - 当前任务与路线 = `.Doc/plan/当前任务与路线.md`(结构轴 M3 → M3′、功能轴按兵种负责);
  - 兵种状态 = `.Doc/robots/<robot>/DEVLOG.md` §0(台账) + `README.md`(索引);
    根 `RM_DEVLOG.md` 只放公共内容(身份/分支/构建/公共 DEC/公共 DEV)。
  - 旧《升级总方案》《总计划书》等已归档 `.Doc/history/archive/plan/`,只作追溯,不再作执行入口。
- 文档三层:**共用** `spec/guide/plan`、**兵种专属** `robots/<robot>/`(含 `DEVLOG.md`)、
  **历史** `history/`(+`archive/`);目录即归属,规则见 `.Doc/spec/文档存放与管理规则.md`。
  **所有 DEV 标题与台账行带类型标记 `[公共]`/`[INFANTRY]`/`[SENTRY]`/`[HEAVY]`。**
- 代码地图:`.agents/skills/rm-electrical-dev/references/code-map.md`(症状→文件/函数,唯一事实源)。
- **兵种分支与协作规范:`.Doc/spec/兵种分支协作规范.md`(每个新对话开工前读一次,同一对话内不重复读)**:
  `main` 共享主干,`infantry`/`sentry`/`heavy` 长驻兵种分支;多人并行时每人从兵种分支切
  `<type>/<robot>/<功能>-<用户>` 短分支,**用户确认功能正常后** `--no-ff` 合并回兵种分支并删除;
  兵种分支禁改其他兵种目录(含 `.Doc/robots/<别的兵种>/`),共享层改动只经 `main` 下行;
  守卫 `python tools/check_robot_scope.py`(已接入 `refactor_gate.py check`)。
- 参数与协议:参数唯一改动点 = 构建所选 profile 的 `config/<robot>_2026/config.h`(伞头 + 子头);
  协议合同 = `apps/<robot>/app/<robot>_def.h`(带布局守卫);应用入口 = `apps/<robot>/`。
- 装配层:`apps/{infantry,heavy,sentry}/app/`。CMake 无参数默认 infantry(历史兼容);
  **交付构建必须显式兵种**:`build.ps1 <robot> -Current` 或 `-DROBOT_PROFILE=<robot>`。
- 现行结构基线:tag `arch-after-U3U4` + `.Doc/gate/manifest-arch-after-U3U4/`;
  **已完全脱离上游**(2026-11-27):无 yuelu 快照、无 `audit_baseline`、无 `check_plan`(DEC-017)。
- 规范入口:命名 `.Doc/spec/命名规范.md`;注释 `.Doc/spec/注释规范.md`;
  纪律 `.Doc/spec/必须做&禁止做.md`;架构 `.Doc/spec/架构与开发指南.md`。
- 参数与调参结论按兵种库:`.Doc/robots/<robot>/params/`(未实测不得写调参现象)。

## Before editing(会话开始四步)

0. **核对兵种**:按 `.Doc/spec/文档阅读分级.md` §1 读完 A 层(5 份,本会话各读一次),
   确认当前分支(兵种分支或本人短分支)/兵种/profile,并跑 `python tools/check_robot_scope.py`;
   分支与兵种不符先问人,不在未知状态上堆叠;
1. 读根 `RM_DEVLOG.md` §0 + 当前兵种 `.Doc/robots/<robot>/DEVLOG.md` §0(状态、未闭环项、下一步);
2. 跑红旗基线:`python tools/g4_baseline.py check --baseline .Doc/gate/manifest-arch-after-U3U4`;
   同时 `git status -sb` 确认工作区无来源不明改动;
3. 对受影响文件核对基线结果与预期,发现未记录改动先问人,不要在未知状态上继续堆叠。

首条回复须声明:`兵种 / 分支 / profile / 验证方式`。

## Boundaries(禁区)

- 禁改:`Drivers/`、`Middlewares/`(第三方);`Src/Inc/` 仅 USER CODE 区;`.ioc` 需用户明确批准。
- 结构级变更需用户对该次操作明确批准:新增/删除 C/H、新函数/任务/消息字段、改调用链、
  改 components/framework/ports 接口、改构建文件。参数级(config 数值/PID/注释)可直接改。
- 逻辑层(apps/components/framework)禁触硬件句柄 `hcan*/husart*/hspi*/hi2c*/htim*`,一律经实例注入。
- 逻辑层禁 `HAL_GetTick`(时基走 `kernel_tick_ms`/`kernel_time_us`;ports 启动期/桩路径豁免,RM-009)。
- **句柄口径**:Init 时把 `&hcan*/&huart*` 注入模块/端口配置 = 允许;Task/控制函数里直接访问 = 禁止。
- **依赖方向**:`components/`、`framework/`、`ports/` 不得 include `apps/` 头;组件只 include 构建所选
  profile 的 `config.h` 伞头;**framework 零 RTOS include**(后端唯一落点 `ports/freertos`);
  ports 只经 HAL/寄存器落地平台。
- 任务创建/延时/临界区一律走 `framework/kernel`;禁止 apps/components 直接调 FreeRTOS/CMSIS-RTOS。
- 运行期错误禁止 `while(1)` 挂死:走 `err_report` + 安全态(启动期 fail-fast 例外 = `.Doc/gate/redflags`)。
- 单写者:每个消息话题/共享结构体只有一个生产任务。
- 任务启停只用 `robot_task.h` 的 `ROBOT_TASK_*_ENABLE` 宏或 profile 装配,禁止注释启停。
- 未授权不烧录、不动实车;主机测试与 ARM 编译已获授权可直接跑。
- 新增 .c 后 Makefile 必须同步登记(CMake 是 GLOB 自动收集;Makefile 只支持步兵)。
- 每次修改源码/config/构建文件,必须同步更新相关 Markdown:兵种改动追加
  `.Doc/robots/<robot>/DEVLOG.md` §4,公共改动追加根 `RM_DEVLOG.md` §6;标题带类型标记
  (`[公共]`/`[INFANTRY]`/`[SENTRY]`/`[HEAVY]`),提交标题
  `<robot>(<scope>): <说明> (DEV-YYYYMMDD-HHMMSS)`(共享改动用 `shared` scope);
  **2026-11-26 起 DEV 编号 = 本地时间戳,旧的 `-NN` 不改写**。
- **分支纪律**:兵种开发只在对应兵种分支或个人短分支上做,一次改动只碰一个兵种;
  短分支在**用户确认功能正常后** `--no-ff` 合并回兵种分支;兵种分支验证后合入 `main`,
  **合入 `main` 并推送后立即删除短分支(本地+远端)**;共享/文档短分支合入 `main` 后同样即删。
  **长驻分支只有 `main` + `infantry`/`sentry`/`heavy` 四条**,其余分支不保留。
  共享层改动走 `fix/shared-*` → `main` 再下行同步;越界即被 `tools/check_robot_scope.py` 判 FAIL。
  收工跑 `git branch --merged main` + `git fetch --prune`,已合并未删的分支当场清理(找回方法见
  `.Doc/spec/兵种分支协作规范.md` §3.5)。
- 每次开工前 `git pull --ff-only origin <当前分支>`;收工只暂存本次文件,提交并
  `git push origin <当前分支>`,再用 `git status --short --branch` 与
  `git rev-list --left-right --count HEAD...@{upstream}` 核对。推送失败须明确报告,不得称已同步。

## Checks + Evidence(检查与证据)

```powershell
# 红旗基线(只读;权威机制 = g4 执行器 + .Doc/gate/redflags)
python tools/g4_baseline.py check --baseline .Doc/gate/manifest-arch-after-U3U4

# 主机单元测试(chassis/fwk/ax650/profile 矩阵;fwk 聚合整链 700 拍金标准)
powershell -ExecutionPolicy Bypass -File tests\run_chassis_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_fwk_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_ax650_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\run_profile_tests.ps1

# ARM 编译(CMake+Ninja;工具链 arm-none-eabi-gcc 15.3.1 @ C:/Toolchain/bin,
# PATH 上的 14.2.1 不要混用;cmake 不在 PATH 时用 STM32Cube bundle)
cmake -G Ninja -B build && ninja -C build          # 0 error / 第一方 0 warning
powershell -ExecutionPolicy Bypass -File build.ps1 sentry -Current   # 兵种构建 + firmware/sentry.elf

# 零行为闸门(结构批用;G4 已随脱离上游移除)
python tools/refactor_gate.py save-baseline   # 每批开工前重锚一次
python tools/refactor_gate.py check           # G1 宏/G1a/G2 编译/G3 测试/G5 符号/G6 搬家/R1 兵种范围
python tools/refactor_gate.py check-links     # .Doc 引用路径存在性
python tools/check_robot_scope.py             # R1 兵种范围(--strict-shared 连共享层一起判)
python tools/check_source_lists.py            # Makefile 与磁盘 .c 一致
python tools/check_sections.py                # 段占用(CCM≤90%/RAM≤70%)
python tools/check_symbol_contract.py         # 工具符号契约
python tools/check_naming.py                  # 命名规范
python tools/check_macros.py                  # 宏账本/跨子头 #if
python tools/check_macro_golden.py            # 宏守恒探针
python tools/check_abi_golden.py              # 协议 ABI 快照
python tools/check_comment_style.py           # 注释风格
python tools/check_build_parity.py            # 双构建 -D parity
python tools/check_macro_dups.py              # 宏重复/冲突/单字母宏
python tools/check_tags.py --batch <Bx>       # 批次 DEV 条目(扫公共/兵种 DEVLOG + 开发史)
# skill 封装:python .agents/skills/rm-electrical-dev/scripts/verify.py  # 守卫+链接+测试单结论
```

- 交付证据:DEV 条目(注明 AI 模型名)+ 各层验证结果(未执行项注明)+ Markdown 路径 +
  commit SHA/分支/推送结果。
- 编译通过/主机测试通过不构成实体故障关闭;烧录/台架/整车始终由用户执行。
- 被否决的方案写入 `RM_DEVLOG.md` DEC 台账;经验结论沉淀对应兵种的 `robots/<robot>/params/`。
