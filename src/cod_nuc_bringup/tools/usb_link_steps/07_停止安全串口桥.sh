#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "07" "停止本套脚本启动的安全串口桥"
usb_enable_report_tee
usb_load_ros

pid="$(usb_read_pid)" || {
  echo "[INFO] 没有本套脚本记录的 PID，不会终止其他进程。"
  usb_print_next "08_查看本次结果与日志.sh"
  exit 0
}

if ! kill -0 "${pid}" 2>/dev/null; then
  echo "[INFO] PID=${pid} 已经退出。"
  rm -f "${USB_PID_FILE}"
  usb_print_next "08_查看本次结果与日志.sh"
  exit 0
fi
if [[ ! -r "/proc/${pid}/cmdline" ]] || \
   ! tr '\0' ' ' < "/proc/${pid}/cmdline" | grep -q 'sentry_usb_bridge'; then
  echo "[FAIL] PID 文件指向的不是 sentry_usb_bridge，出于安全原因不终止它。"
  exit 1
fi

kill -INT -- "-${pid}" 2>/dev/null || kill -INT "${pid}" 2>/dev/null || true
for _ in $(seq 1 50); do
  kill -0 "${pid}" 2>/dev/null || break
  sleep 0.1
done
if kill -0 "${pid}" 2>/dev/null; then
  kill -TERM -- "-${pid}" 2>/dev/null || kill -TERM "${pid}" 2>/dev/null || true
  sleep 1
fi
if kill -0 "${pid}" 2>/dev/null; then
  kill -KILL -- "-${pid}" 2>/dev/null || kill -KILL "${pid}" 2>/dev/null || true
fi
rm -f "${USB_PID_FILE}"

for _ in $(seq 1 20); do
  usb_node_exists || break
  sleep 0.1
done
echo "[PASS] 本套安全串口桥已停止。"
echo "[SAFE] 节点退出时会尽力发送 VALID=0 零速帧和 shoot=0 无目标帧。"
usb_print_next "08_查看本次结果与日志.sh"
