# JIT-RM 1029 · 2026 哨兵整车电控（基于 `hibhvkgvck/shaobin` + USB 导航/自瞄）

## 上位机 USB 导航与共享 CDC 自瞄接入（2026-11-26）

本目录以 `hibhvkgvck/shaobin@b489e76` 为电控底座，完整保留实车参数与控制逻辑（600×560 mm 底盘、大 yaw `-1.52914 rad` 机械零位与小陀螺纯世界角速度 PI 稳像 `Kp=5.2, Ki=1.2`、小 yaw 中心 `27` / `±90°` 限位、DM pitch、CAN2 发射），在其上移植了本地 `NV v1` 32 字节 USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`：

- **模块位置**：`modules/navigation/`（`nav_protocol`、`rmcv_pose`、`ax650_usb`、`nav_rtos`）与 `application/autonomy/`；详见 [NAVIGATION_USB.md](NAVIGATION_USB.md) 与 [AUTOAIM_USB_PROTOCOL.md](AUTOAIM_USB_PROTOCOL.md)。
- **默认安全开关**：`config/robot_infantry_2026.h` 中 `SENTRY_AUTONOMY_ENABLE = 0`（读取导航速度出口强制清零并返回无效，仅上报大 yaw 角度与时间同步响应）、`AX650_USB_ENABLE = 1`、`AX650_UART1_ENABLE = 0`。
- **本地 Windows 一键编译**（如工程位于含中文的路径下，建议通过 ASCII 目录联接如 `C:\sentry_fw` 构建以避开 MinGW `arm-none-eabi-ld` 的中文 map 路径限制）：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File C:\sentry_fw\build.ps1 sentry -Current
  # 产物输出至 firmware/sentry.elf 与 firmware/current.elf，烧录由本地人工完成
  ```
- **主机测试**：导航/自瞄 4 组测试运行 `python3 tests/navigation/run_tests.py`；原底盘/看门狗/大 yaw 回归运行 `tests/run_chassis_tests.ps1` 或在 WSL 中执行对应测试。

## RM_shaobin 底盘调试

本工程从 `RM-JIT-CR` 的本地副本 `RM-JIT-CR-clone`（`7ca9ea0`）导出，目标板为 STM32F407IG。默认开启 `config/robot_infantry_2026.h` 中的 `CHASSIS_TUNING_ONLY`，应用层启用底盘与遥控，CAN1 允许四轮在遥控解锁后出力。将该宏改为 `0` 并重新编译可恢复上游整车配置。

- **接线**　以电池为中心，四个 M3508/C620 接 CAN1：左上 **1**、右上 **2**、左下 **3**、右下 **4**。DBUS 接 USART3。
- **几何参数**　新底盘左右与前后外沿跨度均为 600 mm。按原工程测量方法，轴距约 600 mm，轮距扣除 40 mm 轮宽后为 560 mm，见本车配置文件。
- **操作**　首次通电先架空四轮。右上挡、左中挡、双摇杆和拨轮回中解锁。右中挡用右杆测试平移，前后与左右方向已按新车实测反转；左杆横向控制连续转向。右下挡为小陀螺。任一开关上挡、拨轮急停或遥控失联时进入零力。
- **调参**　四轮速度环位于 [chassis.c](application/chassis/chassis.c) 的 `ChassisInit()`。平移限速和自转速度在 [robot_infantry_2026.h](config/robot_infantry_2026.h)，当前沿用 1500 mm/s 和 300 °/s。
- **构建**　运行 `cmake -G Ninja -S . -B build` 和 `cmake --build build`，固件为 `build/basic_framework.hex`，调试看变量用 `build/basic_framework.elf`。

## 上游整车说明

基于跃鹿 [basic_framework](https://github.com/HNUYueLuRM/basic_framework) 架构 (STM32F407IGHx + FreeRTOS) 开发的单板步兵工程。
分层为 APP → Module → BSP → HAL,参数收敛于 `config/robot_infantry_2026.h` 唯一改动点,
配套主机单元测试、遥测环形缓冲与基线审计工具链。

> 上手顺序：先读本文 → 《.Doc/架构与开发指南.md》→ 《.Doc/必须做&禁止做.md》。换车/换赛季只改 `config/robot_infantry_2026.h`。

## 目录结构

```
application/   应用层：robot(接线) robot_task(任务) cmd(仲裁) chassis gimbal shoot
config/        本车参数唯一改动点(robot_infantry_2026.h)
modules/       设备与算法模块：电机/IMU/遥控/消息中心/守护/遥测/裁判/超电...
bsp/           片上外设服务：can/usart/spi/pwm/flash/dwt/log...
Src/ Inc/      CubeMX 生成代码(USER CODE 区外不要手改)
Drivers/       ST HAL + CMSIS
Middlewares/   FreeRTOS / USB / DSP库 / SEGGER RTT
tests/         主机单元测试(纯逻辑,无需硬件)
.Doc/          传承文档：架构指南/调参结论/PID整定/必须做&禁止做
```

## 本车接线(2026 步兵)

| 总线 | 设备 | 电调拨码/ID |
|---|---|---|
| CAN1 | 轮电机 M3508+C620 | 左上=1 右上=2 左下=3 右下=4（以电池为中心） |
| CAN1 | 云台 YAW GM6020 | 5 |
| CAN2 | 摩擦轮 M3508+C620 | 左=1 右=2(REVERSE) |
| CAN2 | 拨盘 M2006+C610 | 3 |
| CAN2 | 云台 PITCH M3508+C620 | 4 |
| USART3 | DBUS 遥控器 | 100000 8E1 反相 |
| SPI1 | BMI088 IMU | — |

## 编译与烧录

```powershell
# 完整构建(需 arm-none-eabi-gcc,STM32CubeCLT)
mingw32-make -j24          # 或 cmake -G Ninja -B build && ninja -C build

# 烧录
# VSCode tasks: download_dap / download_jlink;或 J-Flash 打开 stm32.jflash

# 运行日志
JlinkRTTClient             # SEGGER RTT,支持彩色 LOGERROR

# 主机单元测试(无需硬件,需 gcc)
powershell -File tests\run_chassis_tests.ps1
```

## 遥控器操作

| 操作 | 效果 |
|---|---|
| 右上挡(任意) | 急停+锁存 |
| 右上+左中+全杆回中 | 解锁(云台自动回正前方) |
| 右中挡 | 底盘跟随云台 |
| 右下挡 | 小陀螺 |
| 左中/左下 | 遥控模式(下挡开摩擦轮) |
| 摩擦轮开+拨轮下压 | 扳机式:短拨松手=单发×1,按住≥250ms=连发(松手停) |
| 拨轮>300 | 急停锁存 |

## 键鼠操作(图传链路,任意键/鼠标动作即激活,超时自动回落遥控)

| 操作 | 效果 |
|---|---|
| W/A/S/D(Shift) | 底盘平移(Shift慢速 ×0.4) |
| 鼠标移动 | 云台(线性 1:1,无加速度曲线) |
| 右键按住 | 摩擦轮 |
| 左键按住 | 连发(需摩擦轮已开) |
| R | 单发×1 |
| E | 三发×3 |
| Q | 小陀螺切换 |
| F | 触发yaw回正(摇杆接管/超时自动中止) |
| V | 鼠标灵敏度降档(×0.85,下限 0.2) |
| B | 鼠标灵敏度升档(上限 2.5) |
| G | 灵敏度重置为 1.0 |

> V/B/G 调档后通过 RTT 打印当前倍率。灵敏度基准见 `config` 的 `KBM_YAW_SENS` / `KBM_PITCH_SENS`。
> 单发/三发按 R/E 前需先开摩擦轮,否则请求会挂在待发队列,开轮后才补发。

## 文档索引

- 《.Doc/架构与开发指南.md》——分层/数据流/任务表/如何加新模块
- 《.Doc/注释规范.md》——注释风格与参数证据标记规则(改代码前必读)
- 《.Doc/注释重写规划.md》——注释规范化分批计划与质量闸门
- 《.Doc/参数证据表.md》——每个参数的证据等级与待实测清单(调参前先看这张表)
- 《.Doc/本车调参结论.md》——实车标定值与调参经验(改参数前必读)
- 《.Doc/PID参数整定.md》——Ozone 示波整定方法
- 《.Doc/必须做&禁止做.md》——框架纪律
- 《.Doc/团队协作指南.md》——git/GitHub 日常协作操作(新成员从这里接入)
