# 红蓝方出生点桌面快捷脚本

日期：2026-11-28

## 目标

比赛地图只扫描并保存一次；在同一张 `test` 地图上分别记录红方、蓝方出生位置，避免连续执行原中心点脚本时互相覆盖。

## 变更

- 新增 `06A_RViz点击并保存红方出生点.sh`，保存 `/home/rm/maps/test_red_spawn.csv`。
- 新增 `06B_RViz点击并保存蓝方出生点.sh`，保存 `/home/rm/maps/test_blue_spawn.csv`。
- 红方默认 `yaw=0°`，蓝方默认 `yaw=180°`；可分别通过 `RED_SPAWN_YAW_DEG`、`BLUE_SPAWN_YAW_DEG` 覆盖。
- 覆盖前自动备份旧出生点到 `/home/rm/maps/history/`。
- 最终核对脚本要求红、蓝两个出生点均存在；`test_center.csv` 改为可选文件。
- 更新桌面执行顺序和现场操作说明。

## 现场顺序

1. 执行步骤 01～04，只建并保存一次 `test` 地图。
2. 执行步骤 05，并保持定位和 RViz 终端运行。
3. 执行步骤 06A，在 RViz 使用 `Publish Point` 点击红方出生点。
4. 执行步骤 06B，再次使用 `Publish Point` 点击蓝方出生点。
5. 执行步骤 09，核对地图和两个出生点文件。

两个脚本只监听 `/clicked_point` 并保存坐标，不发送 Nav2 目标、底盘速度、自瞄或发射命令。
