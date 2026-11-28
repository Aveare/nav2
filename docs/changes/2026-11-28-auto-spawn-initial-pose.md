# 根据裁判 ID 与出生点文件自动下发 AMCL 初始位姿

日期：2026-11-28

## 问题背景

1. `cod_nuc_bringup` 在 `mode:=localization` 下采用 `map_server + nav2_amcl` 进行 2D 地图定位，且 `hardware_amcl.yaml` 默认 `set_initial_pose: false`，每次启动都需要人工在 RViz 点击 `2D Pose Estimate` 发布 `/initialpose`。
2. `05_启动test地图定位与RViz.sh` 原先在冲突检测前注册了 `EXIT` 清理钩子，误触发第二次双击时会在报错退出阶段删除前一个正常会话的 PID 文件，导致后续 `06A`/`06B`/`08` 无法识别正在运行的 `05` 会话。

## 变更内容

- 新增 `src/cod_nuc_bringup/scripts/spawn_initial_pose.py`：
  - `spawn_side:=auto`（默认）订阅 `/autoaim/pose` 的 `robot_id`，按 RoboMaster 规则自动识别阵营（`1~99` 为红方，`101~199` 为蓝方），也可通过 `spawn_side:=red|blue|none` 手动指定或关闭。
  - 复用 `center_point_file.load_center_point` 读取对应地图同前缀的出生点文件（例如 `test.yaml` 自动对应 `test_red_spawn.csv` 和 `test_blue_spawn.csv`）。
  - 等待 `/initialpose` 出现订阅者（AMCL 就绪）后自动发布初始位姿，收到新 `/amcl_pose` 确认后自动退出；若未找到出生点文件或已先收到外部手动 `/initialpose`，则安全跳过不覆盖。
- 更新 `src/cod_nuc_bringup/launch/nuc_hardware.launch.py` 与 `CMakeLists.txt`：
  - 新增 `auto_initial_pose`、`spawn_side`、`red_spawn_file`、`blue_spawn_file` 启动参数，在 `mode:=localization` 且启用时自动拉起 `spawn_initial_pose.py`。
- 更新 `src/cod_nuc_bringup/tools/field_mapping/steps/05_启动test地图定位与RViz.sh`：
  - 将 `trap cleanup` 移到写入 PID 文件之后，并在清理时校验 PID 文件归属当前进程 `$$`，避免重复启动冲突退出时误删运行中会话的 PID 文件。
- 新增 `src/cod_nuc_bringup/test/test_spawn_initial_pose.py` 并更新 `src/cod_nuc_bringup/test/test_hardware_launch.py`。
