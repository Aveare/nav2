#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "06 RViz 点击并保存中心点"
field_load_ros

echo "============================================================"
echo "06 在 RViz 点击并保存 test 中心点"
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
  echo "[FAIL] 已有中心点记录器运行，请回到原终端完成点击或按 Ctrl+C。"
  exit 1
fi

if [[ -e "${FIELD_CENTER_FILE}" ]]; then
  timestamp="$(date +%Y%m%d_%H%M%S)"
  backup_dir="${FIELD_MAP_DIR}/history/center_before_${timestamp}"
  mkdir -p "${backup_dir}"
  cp -a "${FIELD_CENTER_FILE}" "${backup_dir}/"
  echo "[BACKUP] 旧中心点已备份到：${backup_dir}"
fi

yaw_deg="${CENTER_YAW_DEG:-0}"
echo "[ACTION] 回到 RViz，确认 Fixed Frame=map。"
echo "[ACTION] 点击顶部 Publish Point，然后在场地中心单击一次。"
echo "[INFO] 中心点默认车头方向 yaw=${yaw_deg}°；不要点击 2D Pose Estimate 或 Nav2 Goal。"

ros2 run cod_nuc_bringup record_center_point.py \
  --output "${FIELD_CENTER_FILE}" \
  --yaw-deg "${yaw_deg}" \
  --force

[[ -s "${FIELD_CENTER_FILE}" ]] || {
  echo "[FAIL] 中心点文件没有生成。"
  exit 1
}
echo "[PASS] 中心点已保存：${FIELD_CENTER_FILE}"
cat "${FIELD_CENTER_FILE}"
field_print_next "07_显示test中心点.sh"
