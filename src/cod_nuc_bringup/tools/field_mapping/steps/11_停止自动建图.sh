#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "11 停止自动建图"

echo "============================================================"
echo "11 停止自动建图和30秒保存"
echo "============================================================"

pid="$(field_read_pid "${FIELD_AUTO_MAPPING_PID_FILE}")" || {
  echo "[INFO] 没有正在运行的自动建图会话。"
  exit 0
}
field_pid_has_token "${pid}" 'auto_mapping_autosave.sh' || {
  echo "[FAIL] PID 文件不是自动建图进程，出于安全原因不终止 PID=${pid}。"
  exit 1
}
field_stop_wrapper "${pid}"
rm -f "${FIELD_AUTO_MAPPING_PID_FILE}"

if pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1; then
  echo "[FAIL] SLAM Toolbox 仍在运行，请查看自动建图日志。"
  exit 1
fi
echo "[PASS] 自动保存、扫描建图、雷达、串口桥和 RViz 已停止。"
