#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "05" "测量大 yaw ROS 反馈频率"
usb_enable_report_tee
usb_load_ros

if ! usb_bridge_is_running || ! usb_node_exists; then
  echo "[FAIL] 安全串口桥未运行，请先执行步骤 02。"
  exit 1
fi

hz_file="$(mktemp)"
trap 'rm -f "${hz_file}"' EXIT
set +e
timeout 8 stdbuf -oL -eL ros2 topic hz /big_yaw/joint_states > "${hz_file}" 2>&1
set -e
sed '/failed to initialize wait set: the given context is not valid/d' "${hz_file}"

if ! grep -Fq 'average rate:' "${hz_file}"; then
  echo "[FAIL] 未测得反馈频率。"
  exit 1
fi
echo "[PASS] 已测得 /big_yaw/joint_states 频率。协议上限为 50 Hz。"
usb_print_next "06_检查自瞄RMCV_可选.sh"
