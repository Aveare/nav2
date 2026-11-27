#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "01 建图前安全检查"
field_load_ros

echo "============================================================"
echo "01 建图前安全检查"
echo "============================================================"

[[ -x /home/rm/cod_deploy/start_hardware_mapping.sh ]] || {
  echo "[FAIL] 缺少实车建图启动器。"
  exit 1
}
[[ -r /home/rm/cod_deploy/hardware_mapping.rviz ]] || {
  echo "[FAIL] 缺少建图 RViz 配置。"
  exit 1
}
[[ -r "${FIELD_RVIZ_CONFIG}" ]] || {
  echo "[FAIL] 缺少定位与中心点 RViz 配置。"
  exit 1
}
[[ -e "${FIELD_SERIAL_PORT}" && -r "${FIELD_SERIAL_PORT}" && -w "${FIELD_SERIAL_PORT}" ]] || {
  echo "[FAIL] STM32 稳定串口不存在或不可读写：${FIELD_SERIAL_PORT}"
  exit 1
}
ping -c 1 -W 1 192.168.1.144 >/dev/null 2>&1 || {
  echo "[FAIL] MID360S 192.168.1.144 不可达。"
  exit 1
}

if pgrep -af '[n]uc_hardware.launch.py|[a]sync_slam_toolbox_node|[s]entry_usb_bridge' >/dev/null 2>&1; then
  echo "[FAIL] 检测到会占用雷达、串口或 TF 的现有进程："
  pgrep -af '[n]uc_hardware.launch.py|[a]sync_slam_toolbox_node|[s]entry_usb_bridge' || true
  echo "       请先停止对应终端；若是串口联调脚本，执行："
  echo "       /home/rm/COD导航总目录/07_C板USB通信/07_停止安全串口桥.sh"
  exit 1
fi
if command -v fuser >/dev/null 2>&1 && fuser "${FIELD_SERIAL_PORT}" >/dev/null 2>&1; then
  echo "[FAIL] STM32 串口被其他程序占用："
  fuser -v "${FIELD_SERIAL_PORT}" 2>/dev/null || true
  exit 1
fi

mkdir -p "${FIELD_MAP_DIR}" "${FIELD_LOG_DIR}"
echo "[PASS] MID360S、STM32 串口、ROS 2 工具和 RViz 配置均可用。"
echo "[SAFE] 后续脚本固定关闭底盘速度、自瞄和自动决策输出。"
field_print_next "02_启动实车扫描建图.sh"
