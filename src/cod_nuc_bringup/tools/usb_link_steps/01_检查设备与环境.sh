#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_prepare_state
: > "${USB_REPORT_FILE}"
usb_begin_step "01" "检查 USB 设备、ROS 2 环境和端口占用"
usb_enable_report_tee
usb_load_ros

echo "[INFO] ROS 2：${ROS_DISTRO:-unknown}"
echo "[INFO] cod_nuc_bringup：$(ros2 pkg prefix cod_nuc_bringup)"
python3 -c 'import serial; print("[PASS] pyserial", serial.__version__)'

if [[ ! -e "${USB_SERIAL_PORT}" ]]; then
  echo "[FAIL] 未找到 STM32 稳定设备路径：${USB_SERIAL_PORT}"
  exit 1
fi
if [[ ! -r "${USB_SERIAL_PORT}" || ! -w "${USB_SERIAL_PORT}" ]]; then
  echo "[FAIL] 当前账户不能读写：${USB_SERIAL_PORT}"
  exit 1
fi
echo "[PASS] STM32 设备存在：$(readlink -f "${USB_SERIAL_PORT}")"
ls -l "${USB_SERIAL_PORT}"

if usb_bridge_is_running; then
  echo "[FAIL] 本套安全串口桥已经运行。请执行 07 后再从 01 重新开始。"
  exit 1
fi
rm -f "${USB_PID_FILE}"

if pgrep -af '[s]entry_usb_bridge' >/dev/null 2>&1; then
  echo "[FAIL] 检测到不是本套脚本启动的 sentry_usb_bridge："
  pgrep -af '[s]entry_usb_bridge' || true
  exit 1
fi
if command -v fuser >/dev/null 2>&1 && fuser "${USB_SERIAL_PORT}" >/dev/null 2>&1; then
  echo "[FAIL] 串口已被其他程序占用："
  fuser -v "${USB_SERIAL_PORT}" 2>/dev/null || true
  exit 1
fi

echo "[PASS] 串口当前空闲。"
echo "[INFO] 当前双协议共口 USB 消息清单（NV v1 + RMCV）："
echo "       NUC→STM32  NV COMMAND=1（本流程只发 VALID=0 的零速安全帧）"
echo "       STM32→NUC  NV FEEDBACK=2"
echo "       NUC→STM32  NV SYNC_REQUEST=3"
echo "       STM32→NUC  NV SYNC_RESPONSE=4"
echo "       STM32→NUC  RMCV 0x1022（61 B 完整云台姿态 + valid_flags + robot_id）"
echo "       NUC→STM32  RMCV 0x0503（39 B 自瞄目标；本流程固定 target_id=0, shoot=0）"
usb_print_next "02_启动安全串口桥.sh"
