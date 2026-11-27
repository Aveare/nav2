#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
STEP_SOURCE="${ROOT}/steps"
LAUNCHER_SOURCE="${ROOT}/launchers"
DESKTOP_DIR="${FIELD_DESKTOP_DIR:-/home/rm/Desktop/建图与中心点}"
SCRIPT_DIR="${DESKTOP_DIR}/.scripts"

mkdir -p "${DESKTOP_DIR}" "${SCRIPT_DIR}"

for script in "${STEP_SOURCE}"/*.sh; do
  install -m 0755 "${script}" "${SCRIPT_DIR}/$(basename -- "${script}")"
done
install -m 0644 "${STEP_SOURCE}/00_按顺序执行.md" "${DESKTOP_DIR}/00_按顺序执行.md"
install -m 0644 "${ROOT}/README.md" "${DESKTOP_DIR}/地图中心点说明.md"

shopt -s nullglob
legacy_scripts=("${DESKTOP_DIR}"/*.sh)
if ((${#legacy_scripts[@]})); then
  legacy_dir="${DESKTOP_DIR}/.legacy-sh-$(date +%Y%m%d_%H%M%S)"
  mkdir -p "${legacy_dir}"
  mv -- "${legacy_scripts[@]}" "${legacy_dir}/"
  echo "[BACKUP] 原桌面脚本已移到：${legacy_dir}"
fi

for launcher in "${LAUNCHER_SOURCE}"/*.desktop; do
  destination="${DESKTOP_DIR}/$(basename -- "${launcher}")"
  install -m 0755 "${launcher}" "${destination}"
  if command -v gio >/dev/null 2>&1; then
    gio set "${destination}" metadata::trusted true >/dev/null 2>&1 || true
  fi
done

echo "[PASS] 建图步骤已安装为可双击运行的程序入口：${DESKTOP_DIR}"
echo "[INFO] 维护脚本保存在隐藏目录：${SCRIPT_DIR}"
