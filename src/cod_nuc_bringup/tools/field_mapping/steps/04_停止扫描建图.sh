#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "04 停止扫描建图"

echo "============================================================"
echo "04 停止扫描建图"
echo "============================================================"

pid="$(field_read_pid "${FIELD_MAPPING_PID_FILE}")" || {
  if pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1; then
    echo "[FAIL] 检测到建图节点，但它不是本套脚本记录的会话。请在原启动终端按 Ctrl+C。"
    exit 1
  fi
  echo "[INFO] 没有正在运行的扫描建图会话。"
  field_print_next "05_启动test地图定位与RViz.sh"
  exit 0
}
if ! field_pid_has_token "${pid}" 'start_hardware_mapping.sh'; then
  echo "[FAIL] PID 文件不是实车建图启动器，出于安全原因不终止 PID=${pid}。"
  exit 1
fi
field_stop_wrapper "${pid}"
rm -f "${FIELD_MAPPING_PID_FILE}"

if pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1; then
  echo "[FAIL] SLAM Toolbox 仍在运行，请查看原建图终端。"
  exit 1
fi
echo "[PASS] 扫描建图、雷达驱动、串口桥和建图 RViz 已停止。"
field_print_next "05_启动test地图定位与RViz.sh"
