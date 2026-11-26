# Sentry RM_shaobin2 移植说明

## 结论

本次把本地 `D:\BaiduNetdiskDownload\Nav\RM_shaobin2\RM_shaobin2\RM_shaobin` 的哨兵控制行为
移植到 `RM-JIT-CR` 的 `sentry` profile。移植只改变 `apps/sentry/`、`config/sentry_2026/` 和
Sentry 共用的命令枚举，不把本地哨兵逻辑复制到 infantry/heavy。

目标工程仍保留三条构建路径：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 infantry
powershell -ExecutionPolicy Bypass -File build.ps1 sentry -Current
powershell -ExecutionPolicy Bypass -File build.ps1 heavy
```

哨兵交付固件只认 `firmware/sentry.elf`。`firmware/current.elf` 是调试工具兼容副本，
`build/basic_framework.elf` 是 infantry 构建，不得烧到哨兵。

## 迁移基线

| 项目 | 内容 |
|---|---|
| 本地来源 | `D:\BaiduNetdiskDownload\Nav\RM_shaobin2\RM_shaobin2\RM_shaobin` |
| 来源提交 | `593d6af` |
| 来源工作树 | 非干净；本次按用户指定以工作树为准 |
| 额外采用的未提交参数 | 大 yaw 积分额度 `2.0 → 3.0 N·m` |
| 目标分支 | `sentry` |
| 适用兵种 | 2026 哨兵 Sentry，单板 STM32F407IG |

来源工作树内还有文档、导出日志等与本次移植无关的改动。本次没有复制这些文件，只读取
Sentry 的电机映射、遥控行为和控制参数。

## 保留的目标工程能力

- infantry/sentry/heavy 三个 profile 继续独立编译。
- USB CDC VCP 继续作为独立上位机物理链路，`VISION_USE_VCP` 保持启用。
- AX650 UART1 保持关闭，不恢复旧 `0x0503/0x1021` 自瞄协议。
- USART1 继续以 VOFA JustFloat 输出三轴编码器，不被上位机 VCP 占用。
- USART6 继续用于裁判系统。
- 本次未修改 `.ioc`、`Drivers/`、`Middlewares/`，也未烧录或操作实车。

USB CDC 当前只有物理端口能力，尚未定义 Sentry 上位机应用帧。收到的任意 VCP 字节不会直接
变成控制命令；后续接入必须另行定义帧格式、坐标系、超时、安全态和控制权仲裁。

## CAN 与执行器映射

| 总线 | 执行器 | ID |
|---|---|---|
| CAN1 | 底盘 4×M3508 | ESC 1/2/3/4 |
| CAN1 | 大 yaw DM-J4310 | Slave 5 / Master 0 |
| CAN2 | 小 yaw GM6020 | ESC 1 |
| CAN2 | pitch DM-J4310 | Slave 5 / Master 0 |
| CAN2 | 左/右摩擦轮 M3508 | ESC 1/2 |
| CAN2 | 拨盘 M2006 | ESC 3 |

## 遥控器映射

| 输入 | 当前行为 |
|---|---|
| 任一拨杆上挡 | 急停并锁存零力 |
| 右上 + 左中 + 全杆回中 | 解锁，pitch 以当前位置重新建立相对零点 |
| 右拨杆中挡 | 人工自由移动，不执行底盘跟随大 yaw |
| 右拨杆下挡 | 小陀螺；右摇杆平移仍保留 |
| 右摇杆 | 底盘前后/左右平移 |
| 左摇杆纵向 | pitch 相对目标，范围 ±20°，满杆 60°/s |
| 左拨杆下挡 | 摩擦轮开启并持续请求连发；同时保留小 yaw 控制 |
| 左拨杆下挡 + 左摇杆横向 | 小 yaw 角速度，满杆 120°/s |
| 左拨杆非下挡 + 左摇杆横向 | 底盘转向，限幅 150°/s |
| 拨轮进入扳机区 | 摩擦轮开启；短拨松开单发，按住 250 ms 后连发 |
| 拨轮松开 | 摩擦轮延迟 1 s 停止 |

左拨杆下挡的连发仍受摩擦轮在线/到速互锁、拨盘堵转保护和热量守卫约束。拨回中挡后立即停止
拨弹请求，摩擦轮继续运行 1 s 后停止。

大 yaw 不再由左摇杆直接给位置目标。普通挡锁定机械正前方 `-1.52914 rad`；小陀螺挡把
世界 yaw 角速度目标设为 0，用 IMU Z 轴角速度做阻尼。用户已确认 IMU 安装在大 yaw 平台，
因此 `SENTRY_IMU_AFTER_SMALL_YAW=0`。

## 键鼠映射

| 输入 | 当前行为 |
|---|---|
| W/A/S/D | 底盘平移，Shift 为 0.4 倍慢速 |
| 鼠标横移 | 普通挡控制底盘转向，限幅 60°/s |
| 鼠标纵移 | 控制 pitch；键鼠不控制小 yaw |
| Q | 切换小陀螺 |
| 鼠标右键 | 摩擦轮 |
| 鼠标左键 | 摩擦轮开启时连发 |
| R/E | 单发/三发 |
| F | 当前无动作，旧回中流程已关闭 |

## 主要控制参数

| 轴/模块 | 迁移值 | 证据状态 |
|---|---|---|
| 大 yaw 机械正前 | `-1.52914 rad` | `[用户确认，待本固件复核]` |
| 大 yaw 角度环 | `Kp=20`，死区 `0.02°`，限速 `90°/s` | `[本地工作树，待实测]` |
| 大 yaw 速度环 | `Kp=5.2`，`Ki=1.2`，积分 `3 N·m`，输出 `10 N·m` | `[诊断对照，待实测]` |
| 小 yaw 中位/范围 | ECD `27`，机械限位 `±90°`，命令保留 10° 裕量 | `[本地工作树，待实测]` |
| 小 yaw 串级 | 角度 `Kp=25`；速度 `Kp=50`；电压限幅 `16000` | `[本地工作树，待实测]` |
| pitch 相对行程 | 解锁位置为 0，范围 `±20°`，满杆 `60°/s` | `[本地工作树，待实测]` |
| pitch 串级 | 角度 `Kp=8`；速度 `Kp=0.6`、`Ki=0.2`；输出 `1 N·m` | `[本地工作树，待实测]` |
| 底盘几何 | 轴距 `600 mm`，轮距 `560 mm`，轮半径 `75 mm`，减速比 `19` | `[来源工作树]` |
| 底盘速度环 | `Kp=0.75`，`Ki=1.0`，积分 `3000`，输出 `10000` | `[来源工作树]` |

## 串口数据

USART1 使用 921600、8N1、VOFA JustFloat。每帧为 3 个小端 `float` 加
`00 00 80 7F`，共 16 字节：

| 通道 | 数据 | 单位 |
|---|---|---|
| 0 | 大 yaw DM `total_position` | rad |
| 1 | 小 yaw GM6020 `ecd` | 0～8191 原始码 |
| 2 | pitch DM `total_position` | rad |

## 源码落点

| 能力 | 目标文件 |
|---|---|
| 遥控/键鼠映射 | `apps/sentry/cmd/cmd_remote.c`、`apps/sentry/cmd/cmd_kbm.c` |
| 控制权与消息合同 | `apps/sentry/cmd/cmd_authority.c`、`apps/sentry/app/sentry_def.h` |
| 大/小 yaw 与 pitch | `apps/sentry/gimbal/gimbal.c`、`apps/sentry/gimbal/gimbal_control.c` |
| 底盘模式 | `apps/sentry/chassis/chassis.c` |
| 发射延时停轮 | `apps/sentry/shoot/shoot.c` |
| 参数唯一入口 | `config/sentry_2026/config_*.h` |

## 首次上车检查

1. 先断开发射供弹，架空底盘，确认右上挡能立即进入零力。
2. 检查 USART1 三个 VOFA 通道连续且方向符合机械运动。
3. 先单独确认小 yaw 和 pitch 的方向、软限位，再使机构进入大 yaw 带载测试。
4. 大 yaw 首测只做短时小扰动，持续观察 DM 状态码、转子/线圈温度和同步带阻力。
5. 出现不动但力矩顶到 `10 N·m`、状态 `0xC` 或温升异常时立即急停；不要继续提高 PID。
6. 完成台架验证后，把结论以 `[实测]` 写入 `.Doc/spec/本车调参结论.md` 和
   `.Doc/spec/参数证据表.md`。

编译通过和主机测试通过只证明接口、边界与构建链成立，不构成实车故障关闭。
