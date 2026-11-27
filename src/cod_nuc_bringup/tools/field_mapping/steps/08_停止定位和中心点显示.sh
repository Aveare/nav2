#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "08 停止定位和中心点显示"

echo "============================================================"
echo "08 停止中心点显示、test 定位和 RViz"
echo "============================================================"

field_stop_owned_session "${FIELD_CENTER_PID_FILE}" '07_显示test中心点.sh' '中心点显示'
field_stop_owned_session "${FIELD_LOCALIZATION_PID_FILE}" '05_启动test地图定位与RViz.sh' 'test 地图定位'

if pgrep -af '[s]how_center_point.py|[n]uc_hardware.launch.py|[s]entry_usb_bridge' >/dev/null 2>&1; then
  echo "[WARN] 仍检测到不属于本套 PID 文件的相关进程，未自动终止："
  pgrep -af '[s]how_center_point.py|[n]uc_hardware.launch.py|[s]entry_usb_bridge' || true
else
  echo "[PASS] 中心点显示、定位、雷达、串口桥和 RViz 会话均已停止。"
fi
field_print_next "09_核对test地图和中心点文件.sh"
