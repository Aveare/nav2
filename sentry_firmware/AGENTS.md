# AGENTS.md — AI 协作契约(工程根)

> 按 agents.md 标准四段式维护,保持简短、可执行、随赛季更新(过期的规则比没有更糟)。
> 每条规则要么改变一个动作、要么产生可检查的证据,否则删掉。
> 规范依据:skill rm-ec-dev(references/ai-constraints.md)。

## Map(工程地图)

- 工程:JIT-RM 1029,2026 步兵,单板 STM32F407IG + FreeRTOS,跃鹿架构 APP→Module→BSP→HAL 分层(2026-10-29 初版)。
- 代码地图:skill rm-ec-dev references/code-map.md(症状→文件/函数定位,唯一事实源)。
- 参数唯一改动点:`config/robot_infantry_2026.h`;协议合同:`application/robot_def.h`(有布局守卫)。
- 固件唯一默认路径:`build/basic_framework.elf`；统一使用CMake/Ninja，禁止新增专用固件目录或另名复制ELF。编译、烧录、调试均使用此路径；不与旧Make构建交替生成固件。
- 基线:`C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/assets/jit-rm-1029.manifest.json`(2026-10-29 固定,初版全验证状态)。
- 历史调参结论与踩坑:`.Doc/本车调参结论.md`(改参数前必读)。
- 注释规范:`.Doc/注释规范.md`(写/改注释前必读);参数证据等级:`.Doc/参数证据表.md`(标注 `[实测]/[理论]/[待实测]`,未实测不得写调参现象)。

## Before editing(会话开始三步)

1. 读 `RM_DEVLOG.md` §0 AI 冷启动摘要(只读这一节即可接手);
2. 跑基线审计:`"C:/Users/ZhuanZ/.workbuddy/binaries/python/versions/3.13.12/python.exe" "C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/scripts/audit_baseline.py" --project "<工程根>"`(只读;`--skill` 默认取脚本上级目录,无需显式传);
3. 对受影响文件核对审计结果与预期,发现未记录改动先问人,不要在未知状态上继续堆叠。

## Boundaries(禁区)

- 禁改:`Drivers/`、`Middlewares/`(第三方);`Src/Inc/` 仅 USER CODE 区;`.ioc` 需用户明确批准。
- 结构级变更需用户对该次操作明确批准:新增/删除 C/H、新函数/任务/消息字段、改调用链、改 modules/bsp 接口、改构建文件。参数级(config 数值/PID/注释)可直接改。
- 逻辑层(application/)禁触硬件句柄 `hcan*/husart*/hspi*/hi2c*/htim*`,一律经模块实例。
- 运行期错误禁止 `while(1)` 挂死:LOGERROR+切安全态(启动期 fail-fast 例外,基线内已有的不在此列)。
- 单写者:每个消息话题/共享结构体只有一个生产任务。
- 任务启停只用 `robot_task.h` 宏,禁止注释启停。
- 未授权不烧录、不动实车;主机测试与 ARM 编译已获授权可直接跑。
- 新增 .c 后 Makefile 必须同步登记(CMake 是 GLOB 自动收集)。

## Checks + Evidence(检查与证据)

```powershell
# 基线审计(只读,静态检查;skill 装在用户级目录,工程内无 skill/ 子目录)
"C:/Users/ZhuanZ/.workbuddy/binaries/python/versions/3.13.12/python.exe" "C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/scripts/audit_baseline.py" --project "<工程根>"
# PowerShell 孪生版(等价输出)
powershell -ExecutionPolicy Bypass -File "C:/Users/ZhuanZ/.workbuddy/skills/rm-ec-dev/scripts/audit_baseline.ps1" -ProjectRoot "<工程根>"

# 主机单元测试
powershell -File tests\run_chassis_tests.ps1

# ARM 全量编译(CMake+Ninja, arm-none-eabi-gcc 14.3.1)
cmake -G Ninja -B build && ninja -C build    # 期望 0 error,第一方 0 warning
```

- 交付证据:DEV 条目(RM_DEVLOG,注明 AI 模型名)+ 审计计数 + 各层验证结果(未执行项注明)。
- 编译通过/主机测试通过不构成实体故障关闭;烧录/台架/整车始终由用户执行。
- 被否决的方案写入 RM_DEVLOG 的 DEC 台账;经验结论沉淀 `.Doc/本车调参结论.md`。
