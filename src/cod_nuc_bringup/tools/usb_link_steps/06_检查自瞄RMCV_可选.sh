#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "06" "检查 RMCV 0x1022 完整姿态与串口桥统计"
usb_enable_report_tee
usb_load_ros

if ! usb_bridge_is_running || ! usb_node_exists; then
  echo "[FAIL] 安全串口桥未运行，请先执行步骤 02。"
  exit 1
fi

pose_file="$(mktemp)"
status_file="$(mktemp)"
trap 'rm -f "${pose_file}" "${status_file}"' EXIT
set +e
timeout 6 ros2 topic echo --once /autoaim/pose > "${pose_file}" 2>&1
status=$?
set -e

if [[ ${status} -eq 0 ]]; then
  cat "${pose_file}"
  if grep -Fq 'full_pose: true' "${pose_file}"; then
    echo "[PASS] 收到 RMCV 0x1022 完整云台姿态。"
  else
    echo "[PASS] 收到旧版 RMCV 0x1021 姿态（尚未升级 0x1022）。"
  fi
else
  echo "[WARN] 6 秒内没有 /autoaim/pose。"
  echo "[INFO] 若烧录的是纯 NV 固件（未开 SENTRY_NUC_RMCV_ENABLE），此结果为预期；"
  echo "       若已启用 0x1022 上行，请检查 EKF 初始化和 USB TX 拥塞统计。"
fi

set +e
timeout 4 ros2 topic echo --once /sentry/usb_bridge_status > "${status_file}" 2>&1
bridge_status=$?
set -e
if [[ ${bridge_status} -eq 0 ]]; then
  echo "[INFO] /sentry/usb_bridge_status："
  cat "${status_file}"
fi
usb_print_next "07_停止安全串口桥.sh"
