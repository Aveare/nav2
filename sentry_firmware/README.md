# 哨兵电控本地导航通信副本

本目录由用户下载的 `RM-JIT-CR-main/RM-JIT-CR-main` 提取，仅包含
`apps/sentry`、`apps/common`、`config/sentry_2026` 和公共组件、驱动、构建依赖。
未复制步兵/重装应用及其配置。原始下载目录保留不动。
本目录没有 `.git`，没有 GitHub 远程关联，也未执行提交、推送或远端修改。

导航算法、LIO、Nav2 继续运行在 `../cod_ws` 的 ROS 工作区；STM32 只执行
底盘指令、原安全仲裁和反馈。不能把 ROS/Nav2 程序直接编入 STM32 固件。

## 本次接入

- USB CDC 使用唯一的 `autonomy` 接收者，32 字节 NV v1 帧；旧导航的 15 字节包不兼容。
- ROS `/cmd_vel` 的 x/y/z 为前/左/逆时针，m/s、m/s、rad/s。
  电控接收端转换为 mm/s、右正、deg/s；上位机不重复换算。
- 新 `CHASSIS_NAVIGATION` 保留上位机角速度，跳过云台角对平移速度的二次旋转。
- 每轴最大 0.35 m/s，角速度最大 0.5 rad/s；导航专用平移斜坡 350 mm/s²。
  原手动、云台 PID、机械零位和电机 ID 保留源工程数值。
- USB 命令 CRC 错误、无效数值、越限、重复/倒退序号不刷新有效期。
  300 ms 无新有效命令后导航失效；排队时效从 USB 接收时刻计算。
- 大 yaw 使用原电控已换算的关节角，附带该次 CAN 接收的 MCU tick 时间，分辨率 1 ms。
- 导航协议没有开火/云台目标字段，本次不接自主开火、目标选择或交战决策。

原始审计工具报告其上游快照 `LICENSE` 哈希不匹配，无法用它作可信基线；没有修改
原始许可证或快照。`SOURCE_MANIFEST.json` 记录复制前的文件哈希，`LOCAL_CHANGES.json`
记录本次本地修改。历史 `RM_DEVLOG.md` 保留，当前条目为 DEV-20261125-LOCAL-NAV。

## 默认只联调通信

`SENTRY_AUTONOMY_ENABLE=0`：USB 以 50 Hz 持续回传新鲜的大 yaw CAN 角度，
但 `autonomy_get_command()` 会把已解析的导航速度整帧清零并返回无效，导航命令
不能进入底盘 PID。若大 yaw CAN 反馈停止，固件不会用旧角度伪造新采样时间。
`SENTRY_BIG_YAW_FEEDBACK_ONLY_ENABLE=1` 会使能大 yaw DM 通信，但把 MIT 的
位置增益、速度增益和力矩保持为 0；遥控、导航和标定测试都不能触发大 yaw PID。
ROS 桥接 `enable_drive=false`：继续回传角度与测时，速度命令标记为无效。
通信联调不需要解除这两个开关。原电控的手动功能不是被这两个开关禁用的。

架空、方向与超时测试通过后，实际导航控制需要同时满足：

1. 电控 `config/sentry_2026/config_autonomy.h` 中 `SENTRY_AUTONOMY_ENABLE=1` 并重新编译。
2. ROS 桥接以 `enable_drive:=true` 启动，收到新鲜 `/cmd_vel`、`/Odometry` 和已测时的有效大 yaw。
3. 保留原遥控解锁步骤；左中、右中且右杆回中才选择导航，推动右杆回手动。
4. 原遥控失联、急停、STOP 锁存和底盘台架锁仍具有最终控制权。

失效后选择 `CHASSIS_ZERO_FORCE`，这是撤销电机输出，不是经过实测的制动距离保证。
底盘轮径、轮距、方向和制动能力仍须以本车实测为准。

## 编译与验证

完整 ARM 构建仅支持哨兵：安装原工程所需 arm-none-eabi-gcc、CMake、Ninja 后执行：

```powershell
.\build.ps1 sentry
```

本地副本只保留 CMake 构建；新增导航 C 源由原 GLOB 收集，不使用原仓库的步兵 Makefile。
不要使用旧目录中的烧录脚本；本次未执行烧录。

WSL/Linux 主机测试（需要 gcc、Python）：

```bash
python3 tests/navigation/run_tests.py
python3 tests/navigation/check_syntax.py
```

第一条测试实际 C 协议与 autonomy 接收模块（外设桩），分别覆盖 `-O0` 和
`-Ofast -flto`，并逐字节对照 ROS Python 编码。第二条只检查改动生产文件的
主机 C 语法，不等价于 ARM 编译链接。

两端协议与 ROS 启动说明见 [NAVIGATION_USB.md](NAVIGATION_USB.md)。
