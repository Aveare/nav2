#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "07 显示 test 中心点"
field_load_ros

display_pid=""
cleanup() {
  trap - EXIT INT TERM HUP
  field_stop_group "${display_pid}"
  rm -f "${FIELD_CENTER_PID_FILE}"
  echo "[INFO] 中心点显示已停止。"
}
trap cleanup EXIT INT TERM HUP

[[ -s "${FIELD_CENTER_FILE}" ]] || {
  echo "[FAIL] 找不到 ${FIELD_CENTER_FILE}，请先执行步骤 06。"
  exit 1
}
localization_pid="$(field_read_pid "${FIELD_LOCALIZATION_PID_FILE}")" || {
  echo "[FAIL] test 地图定位未运行，请先执行步骤 05。"
  exit 1
}
field_pid_has_token "${localization_pid}" '05_启动test地图定位与RViz.sh' || {
  echo "[FAIL] 定位 PID 文件无效。"
  exit 1
}
if [[ -r "${FIELD_CENTER_PID_FILE}" ]]; then
  old_pid="$(field_read_pid "${FIELD_CENTER_PID_FILE}" || true)"
  if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
    echo "[FAIL] 中心点已经在显示，PID=${old_pid}。"
    exit 1
  fi
  rm -f "${FIELD_CENTER_PID_FILE}"
fi

printf '%s\n' "$$" > "${FIELD_CENTER_PID_FILE}"
setsid ros2 run cod_nuc_bringup show_center_point.py \
  --input "${FIELD_CENTER_FILE}" &
display_pid=$!

pose_ready=false
for _ in $(seq 1 30); do
  if ! kill -0 "${display_pid}" 2>/dev/null; then
    break
  fi
  if timeout 1 ros2 topic echo --once /field_center_pose >/dev/null 2>&1; then
    pose_ready=true
    break
  fi
done
[[ "${pose_ready}" == true ]] || {
  echo "[FAIL] 中心点显示节点没有发布 /field_center_pose。"
  exit 1
}

echo "[READY] RViz 中应出现橙色箭头和 CENTER 标签。"
echo "[SAFE] 这里只显示中心点，不发送导航目标。"
echo "[STOP] 保持此终端运行；完成后执行步骤 08，或在此按 Ctrl+C。"
wait "${display_pid}"
