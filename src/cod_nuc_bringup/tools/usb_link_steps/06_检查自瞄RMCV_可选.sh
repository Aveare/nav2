#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "06" "检查可选 RMCV 0x1021 自瞄姿态"
usb_enable_report_tee
usb_load_ros

if ! usb_bridge_is_running || ! usb_node_exists; then
  echo "[FAIL] 安全串口桥未运行，请先执行步骤 02。"
  exit 1
fi

pose_file="$(mktemp)"
trap 'rm -f "${pose_file}"' EXIT
set +e
timeout 6 ros2 topic echo --once /autoaim/pose > "${pose_file}" 2>&1
status=$?
set -e

if [[ ${status} -eq 0 ]]; then
  cat "${pose_file}"
  echo "[PASS] 收到 RMCV 0x1021，对端应为已接入共享 USB 的功能分支固件。"
else
  echo "[EXPECTED] 6 秒内没有 /autoaim/pose。"
  echo "[INFO] 正式 origin/sentry@4c3dfb63 只接入 4 类 NV 消息，此结果符合当前正式分支。"
  echo "[INFO] 只有未合并的 feat/sentry/autoaim-nav-power-youyucat 才接入 USB RMCV。"
fi
usb_print_next "07_停止安全串口桥.sh"
