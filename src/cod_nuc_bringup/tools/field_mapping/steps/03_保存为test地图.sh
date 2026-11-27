#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "03 保存 test 地图"
field_load_ros

echo "============================================================"
echo "03 保存当前扫描结果为 test 地图"
echo "============================================================"

pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1 || {
  echo "[FAIL] 没有检测到 SLAM Toolbox，请先执行步骤 02。"
  exit 1
}
ros2 topic list 2>/dev/null | grep -Fxq '/map' || {
  echo "[FAIL] 当前没有 /map 话题。"
  exit 1
}

timestamp="$(date +%Y%m%d_%H%M%S)"
backup_dir="${FIELD_MAP_DIR}/history/test_before_${timestamp}"
if [[ -e "${FIELD_MAP_BASE}.yaml" || -e "${FIELD_MAP_BASE}.pgm" || -e "${FIELD_MAP_BASE}.png" ]]; then
  mkdir -p "${backup_dir}"
  for old in "${FIELD_MAP_BASE}.yaml" "${FIELD_MAP_BASE}.pgm" "${FIELD_MAP_BASE}.png"; do
    [[ -e "${old}" ]] && cp -a "${old}" "${backup_dir}/"
  done
  echo "[BACKUP] 旧 test 地图已备份到：${backup_dir}"
fi

ros2 run nav2_map_server map_saver_cli -f "${FIELD_MAP_BASE}"
[[ -s "${FIELD_MAP_BASE}.yaml" ]] || {
  echo "[FAIL] 没有生成 ${FIELD_MAP_BASE}.yaml"
  exit 1
}
image_name="$(awk '$1 == "image:" {print $2; exit}' "${FIELD_MAP_BASE}.yaml")"
[[ -n "${image_name}" ]] || {
  echo "[FAIL] test.yaml 中没有 image 字段。"
  exit 1
}
[[ "${image_name}" = /* ]] && image_path="${image_name}" || image_path="${FIELD_MAP_DIR}/${image_name}"
[[ -s "${image_path}" ]] || {
  echo "[FAIL] 地图图像不存在：${image_path}"
  exit 1
}

echo "[PASS] 已保存："
ls -lh "${FIELD_MAP_BASE}.yaml" "${image_path}"
field_print_next "04_停止扫描建图.sh"
