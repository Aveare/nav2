#!/usr/bin/env bash

USB_STEPS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
USB_STATE_DIR="${USB_STEPS_DIR}/运行状态"
USB_SERIAL_PORT="/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_305E356D3034-if00"
USB_BRIDGE_NODE="/sentry_usb_bridge"
USB_PID_FILE="${USB_STATE_DIR}/safe_bridge.pid"
USB_REPORT_FILE="${USB_STATE_DIR}/本次联调结果.log"
USB_LATEST_LOG="${USB_STATE_DIR}/最新串口桥日志.log"
export TZ=Asia/Shanghai

usb_load_ros() {
  if [[ ! -r /home/rm/cod_deploy/env.sh ]]; then
    echo "[FAIL] 找不到 /home/rm/cod_deploy/env.sh"
    return 1
  fi
  set +u
  # shellcheck disable=SC1091
  source /home/rm/cod_deploy/env.sh
  set -u
}

usb_prepare_state() {
  mkdir -p "${USB_STATE_DIR}"
}

usb_begin_step() {
  local number="$1"
  local title="$2"
  usb_prepare_state
  {
    echo
    echo "============================================================"
    echo "步骤 ${number}：${title}"
    echo "时间：$(date '+%F %T %z')"
    echo "============================================================"
  } | tee -a "${USB_REPORT_FILE}"
}

usb_enable_report_tee() {
  exec > >(tee -a "${USB_REPORT_FILE}") 2>&1
}

usb_read_pid() {
  [[ -r "${USB_PID_FILE}" ]] || return 1
  local pid
  pid="$(tr -dc '0-9' < "${USB_PID_FILE}")"
  [[ -n "${pid}" ]] || return 1
  printf '%s\n' "${pid}"
}

usb_bridge_is_running() {
  local pid
  pid="$(usb_read_pid)" || return 1
  kill -0 "${pid}" 2>/dev/null || return 1
  [[ -r "/proc/${pid}/cmdline" ]] || return 1
  tr '\0' ' ' < "/proc/${pid}/cmdline" | grep -q 'sentry_usb_bridge'
}

usb_node_exists() {
  ros2 node list 2>/dev/null | grep -Fxq "${USB_BRIDGE_NODE}"
}

usb_print_next() {
  echo
  echo "[NEXT] 下一步：$1"
}
