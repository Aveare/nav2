# 现场中心点记录与每日显示

本工具用于在每张现场地图的 `map` 坐标系中记录一个中心点，并在后续每天到场时重新显示。它只保存坐标和发布 RViz Marker，不会调用 Nav2 Action，也不会发布 `/cmd_vel`。

## 文件与话题

| 项目 | 默认值 | 用途 |
|---|---|---|
| 中心点文件 | `/home/rm/maps/test_center.csv` | 保存位置与朝向，兼容 Waypoint Editor |
| RViz 点击话题 | `/clicked_point` | `Publish Point` 的输出 |
| RViz 显示话题 | `/waypoints` | 当前 RViz 配置已启用的 `MarkerArray` |
| 坐标核对话题 | `/field_center_pose` | 输出当前加载的 `PoseStamped` |

不同地图必须使用不同文件名，例如 `competition_center.csv`、`practice_center.csv`。重新建图后 `map` 原点或方向可能变化，旧中心点不能直接复用。

## 首次记录中心点

先启动对应地图的定位和 RViz，确认 RViz 的 `Fixed Frame` 为 `map`。在新终端执行：

```bash
source /opt/ros/humble/setup.bash
source ~/cod_ws/install/setup.bash

ros2 run cod_nuc_bringup record_center_point.py \
  --output /home/rm/maps/test_center.csv \
  --yaw-deg 0
```

保持终端等待，然后在 RViz 顶部选择 **Publish Point**，在中心位置单击一次。终端出现“中心点已保存”即完成。

`--yaw-deg` 是机器人到达后的车头方向，单位为度，零度指向 `map` 的正 X。文件已存在时工具会拒绝覆盖；确认需要重新标定时才使用：

```bash
ros2 run cod_nuc_bringup record_center_point.py \
  --output /home/rm/maps/test_center.csv \
  --yaw-deg 90 \
  --force
```

## 用拖拽方式记录朝向

如果需要在 RViz 中同时点位置和拖出方向：

1. 点击顶部 **Add Waypoint**。
2. 在中心点按下鼠标，向期望车头方向拖动并松开。
3. 在 `WaypointEditorPanel` 点击 **Save WPs**。
4. 保存为 `/home/rm/maps/test_center.csv`。

该 CSV 与本工具完全兼容。不要使用 **2D Pose Estimate** 记录中心，它会修改机器人定位；不要使用 **Nav2 Goal**，它会发起导航。

## 每天到场显示中心点

启动定位与 RViz 后，在新终端执行：

```bash
source /opt/ros/humble/setup.bash
source ~/cod_ws/install/setup.bash

ros2 run cod_nuc_bringup show_center_point.py \
  --input /home/rm/maps/test_center.csv
```

保持该终端运行。RViz 中会出现橙色箭头和 `CENTER` 标签。它发布到现有 `/waypoints` MarkerArray 显示，不会触发导航。

核对当前中心点：

```bash
ros2 topic echo --once /field_center_pose
```

结束显示按 `Ctrl+C`。需要换标签时可增加 `--label RMUL_CENTER`。

## 未重新编译时直接运行

如果文件刚同步到 NUC、尚未重新构建工作区，可从源码目录直接运行：

```bash
source /opt/ros/humble/setup.bash
python3 ~/cod_ws/src/cod_nuc_bringup/tools/center_point/show_center_point.py \
  --input /home/rm/maps/test_center.csv
```

记录命令同理，将脚本名换成 `record_center_point.py`。重新执行 `colcon build --packages-select cod_nuc_bringup` 后再使用前面的 `ros2 run` 形式。

## CSV 格式

```csv
id,pose_x,pose_y,pose_z,rot_x,rot_y,rot_z,rot_w,command
0,-1.03,-0.94,0,0,0,0,1,
```

`pose_x`、`pose_y` 是中心坐标；`rot_z`、`rot_w` 是平面朝向四元数。示例数字只说明格式，不是任何现场的有效中心点。

## 现场检查

- 中心点必须位于栅格地图空闲区域，且周围留足 footprint 与膨胀半径。
- 显示中心点前先完成 AMCL 初始定位并确认激光与地图墙体重合。
- 地图文件、中心点 CSV 和行为树参数必须属于同一套 `map` 坐标系。
- 本工具只负责记录和显示。正式自动前往中心应通过 `/navigate_to_pose` Action，并单独完成规划、定位和底盘安全验收。

## 常见问题

### RViz 中没有标记

确认 `Displays` 中 `/waypoints` 的 `MarkerArray` 已启用，并检查：

```bash
ros2 topic info /waypoints -v
```

### 提示 frame_id 不匹配

确认 RViz `Global Options → Fixed Frame` 为 `map`，再重新点击 **Publish Point**。

### 更换地图后中心点位置错误

停止显示工具，为新地图换一个 CSV 文件名并重新记录，禁止直接复制旧坐标。
