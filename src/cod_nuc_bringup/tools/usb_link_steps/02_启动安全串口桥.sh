#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "02" "启动只读安全串口桥"
usb_enable_report_tee
usb_load_ros

if [[ ! -e "${USB_SERIAL_PORT}" ]]; then
  echo "[FAIL] 串口不存在，请回到步骤 01。"
  exit 1
fi
if usb_bridge_is_running; then
  echo "[FAIL] 安全串口桥已经运行，拒绝重复启动。"
  exit 1
fi
rm -f "${USB_PID_FILE}"
if pgrep -af '[s]entry_usb_bridge' >/dev/null 2>&1; then
  echo "[FAIL] 发现其他串口桥，拒绝争用端口："
  pgrep -af '[s]entry_usb_bridge' || true
  exit 1
fi
if command -v fuser >/dev/null 2>&1 && fuser "${USB_SERIAL_PORT}" >/dev/null 2>&1; then
  echo "[FAIL] 串口已被占用："
  fuser -v "${USB_SERIAL_PORT}" 2>/dev/null || true
  exit 1
fi

bridge_log="${USB_STATE_DIR}/safe_bridge_$(date +%Y%m%d_%H%M%S).log"
setsid ros2 run cod_nuc_bringup sentry_usb_bridge.py --ros-args \
  -p port:="${USB_SERIAL_PORT}" \
  -p enable_drive:=false \
  -p enable_autoaim:=false \
  > "${bridge_log}" 2>&1 &
bridge_pid=$!
printf '%s\n' "${bridge_pid}" > "${USB_PID_FILE}"
ln -sfn "${bridge_log}" "${USB_LATEST_LOG}"

started=false
for _ in $(seq 1 50); do
  if ! kill -0 "${bridge_pid}" 2>/dev/null; then
    break
  fi
  if usb_node_exists; then
    started=true
    break
  fi
  sleep 0.2
done

if [[ "${started}" != true ]]; then
  echo "[FAIL] 串口桥未正常启动，日志末尾如下："
  tail -80 "${bridge_log}" || true
  kill -INT -- "-${bridge_pid}" 2>/dev/null || true
  rm -f "${USB_PID_FILE}"
  exit 1
fi

echo "[PASS] 安全串口桥已启动，PID=${bridge_pid}"
echo "[SAFE] enable_drive=false，不会发送有效底盘速度。"
echo "[SAFE] enable_autoaim=false，只发送无目标、shoot=0 的安全帧。"
echo "[INFO] 日志：${bridge_log}"
usb_print_next "03_确认节点与安全参数.sh"
