#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "08" "查看联调结果和串口桥日志"

echo
echo "==================== 本次联调结果 ===================="
if [[ -r "${USB_REPORT_FILE}" ]]; then
  cat "${USB_REPORT_FILE}"
else
  echo "没有结果文件，请从步骤 01 开始。"
fi

echo
echo "==================== 串口桥日志末尾 =================="
if [[ -r "${USB_LATEST_LOG}" ]]; then
  tail -120 "${USB_LATEST_LOG}"
else
  echo "没有串口桥日志。"
fi

echo
if usb_bridge_is_running; then
  echo "[WARN] 安全串口桥仍在运行，请执行步骤 07。"
  exit 1
fi
echo "[DONE] 安全联调流程结束。正式移动测试需要另行授权并重新检查电控使能。"
