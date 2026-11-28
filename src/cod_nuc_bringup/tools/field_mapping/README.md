# NUC 桌面实车扫描建图与地图中心点

这套现场工具在 NUC 桌面的 **建图与中心点** 文件夹提供一组原生 Linux 可执行程序，用于按顺序完成环境检查、扫描建图、地图保存、定位和出生点记录。双击程序即可运行，不会再打开 `.desktop` 文本。内部脚本固定关闭底盘速度、自瞄和自动决策输出，只读取传感器与关节状态。

这些程序由 NUC 上的 GCC 编译，实际 Shell 脚本保存在隐藏目录 `/home/rm/Desktop/建图与中心点/.scripts/`。现场操作时不要修改或直接运行该目录中的文件。

## 启动前检查

- MID360S 已供电，雷达地址为 `192.168.1.144`。
- STM32 虚拟串口已连接，稳定设备路径存在：

  ```bash
  ls -l /dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_305E356D3034-if00
  ```

- 大 yaw 平台与雷达安装关系没有变化，MID360S 内部 IMU 外参已确认。
- 清空机器人周围人员和障碍，准备通过下位机遥控或人工方式缓慢移动。NUC 在本模式下不会输出速度。

## 启动扫描建图

1. 打开 NUC 桌面的 **建图与中心点** 文件夹，双击 **01 建图前安全检查**。
2. 检查通过后双击 **02 启动实车扫描建图**。首次使用如果系统询问，选择 **允许启动**。
3. 终端依次检查串口、雷达网络和重复进程。
4. 看到 `[READY] 可以开始人工移动底盘扫描环境` 后，RViz 会自动打开。
5. 缓慢覆盖赛场可通行区域，尽量形成闭环并重复经过明显结构。不要在 RViz 点击 **Nav2 Goal**。

本脚本启动参数中的安全项固定为：

| 参数 | 值 | 作用 |
|---|---:|---|
| `mode` | `mapping` | 使用 SLAM Toolbox 建图 |
| `start_serial` | `true` | 读取 STM32 大 yaw 关节状态 |
| `start_navigation` | `false` | 不启动 Nav2 导航 |
| `enable_drive` | `false` | 禁止 NUC 向底盘发送速度 |
| `enable_autoaim` | `false` | 禁止 NUC 向自瞄发送控制量 |
| `start_async_workflow` | `false` | 禁止自动决策流程 |

## 保存为 `test` 地图

建图满意后，保持建图终端运行，打开另一个终端执行：

```bash
source /opt/ros/humble/setup.bash
source ~/cod_ws/install/setup.bash
mkdir -p /home/rm/maps
ros2 run nav2_map_server map_saver_cli -f /home/rm/maps/test
```

确认生成：

```bash
ls -lh /home/rm/maps/test.yaml /home/rm/maps/test.pgm
```

保存成功后双击 **04 停止扫描建图**。程序会停止本次 ROS 2 建图进程和 RViz，不应遗留后台建图节点。

> 重新保存同名 `test` 会覆盖现有地图。比赛地图需要留档时，先换一个明确名称，例如 `/home/rm/maps/competition_2026_day1`。

## 为新地图记录红、蓝出生点

出生点属于地图的 `map` 坐标系。地图只需要建并保存一次；随后在同一张 `test` 地图上分别点击红方和蓝方出生位置。只要重新建图或更换地图，就必须重新记录两个出生点，不能直接沿用旧坐标。

先停止建图并用新地图启动定位与 RViz，确认 RViz 的 **Fixed Frame** 为 `map`。在新终端运行：

```bash
source /opt/ros/humble/setup.bash
source ~/cod_ws/install/setup.bash

ros2 run cod_nuc_bringup record_center_point.py \
  --output /home/rm/maps/test_red_spawn.csv \
  --yaw-deg 0

ros2 run cod_nuc_bringup record_center_point.py \
  --output /home/rm/maps/test_blue_spawn.csv \
  --yaw-deg 180
```

每次启动一个记录命令并保持终端等待，在 RViz 顶部选择 **Publish Point**，然后在对应颜色的出生位置单击一次。终端显示“中心点已保存”后，再启动另一颜色的记录命令。

- `--yaw-deg 0` 表示车头指向 `map` 正 X；`180` 表示指向 `map` 负 X。应根据实际地图方向调整。
- 文件已存在时工具会拒绝覆盖。确认要重标后才增加 `--force`。
- 不要用 **2D Pose Estimate** 记录出生点，它会修改机器人初始定位。
- 不要用 **Nav2 Goal** 记录出生点，它会发起真实导航任务。

桌面程序入口为：

```text
06A 选择红方出生点
06B 选择蓝方出生点
```

程序会自动备份同名旧文件，再等待一次 `Publish Point` 点击。可选场地中心点使用 **06 选择test中心点（可选）**。

需要在 RViz 中同时确定位置和车头方向时，可使用 **Add Waypoint**：在中心点按下鼠标，拖向期望的车头方向，松开后在 `WaypointEditorPanel` 中点击 **Save WPs**，保存为 `/home/rm/maps/test_center.csv`。

## 每天到场显示和核对中心点

启动 `test` 地图定位并完成初始位姿设置后，在新终端运行：

```bash
source /opt/ros/humble/setup.bash
source ~/cod_ws/install/setup.bash

ros2 run cod_nuc_bringup show_center_point.py \
  --input /home/rm/maps/test_center.csv
```

RViz 中会显示橙色箭头和 `CENTER` 标签。它只发布显示标记，不会启动导航。核对具体坐标：

```bash
ros2 topic echo --once /field_center_pose
```

## 故障检查

### 提示串口被占用

先确认没有另一个导航或串口桥在运行：

```bash
pgrep -af 'nuc_hardware|sentry_usb_bridge|cod_serial_bridge'
```

不要同时启动两套串口桥。

### 雷达不可达

```bash
ping -c 3 192.168.1.144
ip -br address
```

检查雷达供电，并确认 NUC 雷达网口仍在 `192.168.1.0/24` 网段。

### RViz 没有地图

```bash
ros2 topic hz /map
ros2 topic hz /scan
ros2 topic hz /lio/world_cloud
```

本次启动日志保存在 `/home/rm/cod_deploy/logs/hardware_mapping_日期_时间.log`。

只做启动条件检查、不实际启动建图时，可以运行：

```bash
/home/rm/cod_deploy/start_hardware_mapping.sh --check
```
