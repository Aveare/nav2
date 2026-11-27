#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "03" "确认节点、端口和安全参数"
usb_enable_report_tee
usb_load_ros

if ! usb_bridge_is_running || ! usb_node_exists; then
  echo "[FAIL] 安全串口桥未运行，请先执行步骤 02。"
  exit 1
fi

port_value="$(ros2 param get "${USB_BRIDGE_NODE}" port)"
drive_value="$(ros2 param get "${USB_BRIDGE_NODE}" enable_drive)"
aim_value="$(ros2 param get "${USB_BRIDGE_NODE}" enable_autoaim)"
echo "${port_value}"
echo "${drive_value}"
echo "${aim_value}"

grep -Fq "${USB_SERIAL_PORT}" <<< "${port_value}" || {
  echo "[FAIL] 节点没有使用指定的稳定串口。"
  exit 1
}
grep -Eiq 'false' <<< "${drive_value}" || {
  echo "[FAIL] enable_drive 不是 false，请立即执行步骤 07。"
  exit 1
}
grep -Eiq 'false' <<< "${aim_value}" || {
  echo "[FAIL] enable_autoaim 不是 false，请立即执行步骤 07。"
  exit 1
}

echo "[PASS] 串口唯一所有者和两项安全门控均正确。"
ros2 node info "${USB_BRIDGE_NODE}"
usb_print_next "04_验证时间同步与大Yaw反馈.sh"
