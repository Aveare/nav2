#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "06A RViz 点击并保存红方出生点"
field_load_ros

echo "============================================================"
echo "06A 在 RViz 点击并保存红方出生点"
echo "============================================================"

localization_pid="$(field_read_pid "${FIELD_LOCALIZATION_PID_FILE}")" || {
  echo "[FAIL] test 地图定位未运行，请先执行步骤 05。"
  exit 1
}
field_pid_has_token "${localization_pid}" '05_启动test地图定位与RViz.sh' || {
  echo "[FAIL] 定位 PID 文件无效，请重新执行步骤 05。"
  exit 1
}
ros2 topic list 2>/dev/null | grep -Fxq '/map' || {
  echo "[FAIL] 当前没有 /map 话题。"
  exit 1
}
if pgrep -af '[r]ecord_center_point.py' >/dev/null 2>&1; then
  echo "[FAIL] 已有出生点或中心点记录器运行，请先在原终端完成点击或按 Ctrl+C。"
  exit 1
fi

if [[ -e "${FIELD_RED_SPAWN_FILE}" ]]; then
  timestamp="$(date +%Y%m%d_%H%M%S)"
  backup_dir="${FIELD_MAP_DIR}/history/red_spawn_before_${timestamp}"
  mkdir -p "${backup_dir}"
  cp -a "${FIELD_RED_SPAWN_FILE}" "${backup_dir}/"
  echo "[BACKUP] 旧红方出生点已备份到：${backup_dir}"
fi

yaw_deg="${RED_SPAWN_YAW_DEG:-0}"
echo "[ACTION] 回到 RViz，确认 Fixed Frame=map。"
echo "[ACTION] 点击顶部 Publish Point，然后在红方出生位置单击一次。"
echo "[INFO] 红方默认车头方向 yaw=${yaw_deg}°；不要点击 2D Pose Estimate 或 Nav2 Goal。"

ros2 run cod_nuc_bringup record_center_point.py \
  --output "${FIELD_RED_SPAWN_FILE}" \
  --yaw-deg "${yaw_deg}" \
  --force

[[ -s "${FIELD_RED_SPAWN_FILE}" ]] || {
  echo "[FAIL] 红方出生点文件没有生成。"
  exit 1
}
echo "[PASS] 红方出生点已保存：${FIELD_RED_SPAWN_FILE}"
cat "${FIELD_RED_SPAWN_FILE}"
field_print_next "06B_RViz点击并保存蓝方出生点.sh"
