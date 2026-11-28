#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
UNIT_SRC="${HERE}/cod-sentry-usb-bridge.service"
USER_UNIT_DIR="${HOME}/.config/systemd/user"
UNIT_DST="${USER_UNIT_DIR}/cod-sentry-usb-bridge.service"

[[ -r "${UNIT_SRC}" ]] || {
  echo "[FAIL] Missing ${UNIT_SRC}"
  exit 1
}

mkdir -p "${USER_UNIT_DIR}"
install -m 0644 "${UNIT_SRC}" "${UNIT_DST}"
systemctl --user daemon-reload

echo "[PASS] Installed ${UNIT_DST} (disabled by default for safety)."
echo "[INFO] Start manually when nuc_hardware.launch.py start_serial:=false:"
echo "       systemctl --user start cod-sentry-usb-bridge.service"
echo "       systemctl --user status cod-sentry-usb-bridge.service"
