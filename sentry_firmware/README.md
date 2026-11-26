# 哨兵机械人 · 2026 电控工程

基于跃鹿 [basic_framework](https://github.com/HNUYueLuRM/basic_framework) 架构（STM32F407IGHx + FreeRTOS）开发的哨兵机械人单板电控工程，上游基线同步自 `hibhvkgvck/shaobin`（当前已合入至 `593d6af`）。分层为 APP → Module → BSP → HAL，本车参数统一收敛于 `config/robot_infantry_2026.h` 唯一改动点，配套主机单元测试、遥测环形缓冲与基线审计工具链。

> 上手顺序：先读本文 → 《.Doc/架构与开发指南.md》→ 《.Doc/必须做&禁止做.md》。换车/换赛季只改 `config/robot_infantry_2026.h`。

## 上位机 USB 导航与共享 CDC 自瞄接入（2026-11-26）

在 `hibhvkgvck/shaobin` 实车参数与控制逻辑之上，本工程集成了本地 `NV v1` 32 字节 USB 导航协议与共享 USB CDC 自瞄 `0x1021/0x0503`：

- **模块位置**：`modules/navigation/`（`nav_protocol`、`rmcv_pose`、`ax650_usb`、`nav_rtos`）与 `application/autonomy/`；详见 [NAVIGATION_USB.md](NAVIGATION_USB.md) 与 [AUTOAIM_USB_PROTOCOL.md](AUTOAIM_USB_PROTOCOL.md)。
- **默认安全开关**：`config/robot_infantry_2026.h` 中 `SENTRY_AUTONOMY_ENABLE = 0`（读取导航速度出口强制清零并返回无效，仅上报大 yaw 角度与时间同步响应）、`AX650_USB_ENABLE = 1`、`AX650_UART1_ENABLE = 0`。
- **本地 Windows 一键编译**（禁止在小电脑编译；如路径含中文，建议使用 ASCII 目录联接 `C:\sentry_fw` 构建以避开 MinGW `arm-none-eabi-ld` 的中文 map 路径限制）：
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File C:\sentry_fw\build.ps1 sentry -Current
  # 产物输出至 firmware/sentry.elf 与 firmware/current.elf，烧录在本机人工完成
  ```
- **主机测试**：导航/自瞄 4 组测试运行 `python3 tests/navigation/run_tests.py`；底盘/看门狗/大 yaw 回归运行 `tests/run_chassis_tests.ps1` 或在本地 WSL 执行对应测试。

## 功能组成

- **底盘**：四轮 M3508/C620 速度环控制，支持遥控平移、连续转向与小陀螺（轴距 600 mm、轮距 560 mm，外沿跨度 600×600 mm）。
- **大 Yaw**：达妙 J4310（CAN1，MIT 模式），角度环 + 速度环串级 PID 带前馈，支持相对底盘正前方（`-1.52914 rad`）锁定与基于 IMU 的小陀螺纯世界角速度 PI 稳像（`Kp=5.2, Ki=1.2`）。
- **小 Yaw**：DJI GM6020（CAN2），角度环位置保持（手推不动），左下挡摇杆控制目标角，带半圆 ±90° 软硬行程限位（中心编码器 `27`）。
- **Pitch**：达妙 DM-J4310-2EC（CAN2，MIT 模式），串级 PID 控制。
- **发射**：两路 DJI Snail 2305 + C615 摩擦轮与 M2006 P36 + C610 拨盘，支持单发/连发仲裁。
- **人机链路**：DBUS 遥控器（USART3）与图传键鼠，指令仲裁逻辑见 `application/cmd`。

> 当前 `config/robot_infantry_2026.h` 默认 `CHASSIS_TUNING_ONLY = 1`（底盘调试），大 Yaw、Pitch、发射三个哨兵功能宏均已启用；将该宏改为 `0` 并重新编译可恢复整车配置。

## 本车接线

| 总线 | 设备 | 型号 / ID |
|---|---|---|
| CAN1 | 底盘轮电机 ×4 | M3508 + C620，ID 1~4（左上1 右上2 左下3 右下4，以电池为中心） |
| CAN1 | 大 Yaw | 达妙 J4310，MIT，ID 5，TMAX 10 Nm |
| CAN2 | 小 Yaw | DJI GM6020，拨码 1（反馈帧 0x205） |
| CAN2 | Pitch | 达妙 DM-J4310-2EC V1.2，MIT，ID 5 |
| CAN2 | 摩擦轮 ×2 | DJI Snail 2305 + C615，ID 1 / 2 |
| CAN2 | 拨盘 | M2006 P36 + C610，ID 3 |
| USART3 | DBUS 遥控器 | 100000 8E1 反相 |
| SPI1 | BMI088 IMU | — |

## 目录结构

```
application/   应用层：robot(接线) robot_task(任务) cmd(仲裁) chassis gimbal shoot autonomy(USB导航)
config/        本车参数唯一改动点(robot_infantry_2026.h)
modules/       设备与算法模块：电机/IMU/遥控/消息中心/守护/遥测/裁判/超电/navigation(USB协议)...
bsp/           片上外设服务：can/usart/spi/pwm/flash/dwt/log/usb...
Src/ Inc/      CubeMX 生成代码(USER CODE 区外不要手改)
Drivers/       ST HAL + CMSIS
Middlewares/   FreeRTOS / USB / DSP库 / SEGGER RTT
tests/         主机单元测试(纯逻辑,无需硬件)
.Doc/          传承文档：架构指南/调参结论/PID整定/必须做&禁止做
```

## 编译与烧录

```powershell
# 完整构建(需 arm-none-eabi-gcc,STM32CubeCLT)
powershell -NoProfile -ExecutionPolicy Bypass -File C:\sentry_fw\build.ps1 sentry -Current
# 或 cmake -G Ninja -B build && ninja -C build

# 烧录
# VSCode tasks: download_dap / download_jlink;或 J-Flash 打开 stm32.jflash

# 运行日志
JlinkRTTClient             # SEGGER RTT,支持彩色 LOGERROR

# 主机单元测试(无需硬件,需 gcc)
powershell -File tests\run_chassis_tests.ps1
```

## 文档索引

- 《.Doc/架构与开发指南.md》——分层/数据流/任务表/如何加新模块
- 《.Doc/团队协作指南.md》——git/GitHub 日常协作操作(新成员从这里接入)
- 《.Doc/注释规范.md》——注释风格与参数证据标记规则(改代码前必读)
- 《.Doc/参数证据表.md》——每个参数的证据等级与待实测清单(调参前先看这张表)
- 《.Doc/本车调参结论.md》——实车标定值与调参经验(改参数前必读)
- 《.Doc/PID参数整定.md》——Ozone 示波整定方法
- 《.Doc/必须做&禁止做.md》——框架纪律
