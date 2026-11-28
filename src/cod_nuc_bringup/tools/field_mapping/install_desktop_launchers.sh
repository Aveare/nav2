#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
STEP_SOURCE="${ROOT}/steps"
DESKTOP_DIR="${FIELD_DESKTOP_DIR:-/home/rm/Desktop/建图与中心点}"
SCRIPT_DIR="${DESKTOP_DIR}/.scripts"
LAUNCHER_BINARY="${SCRIPT_DIR}/field_step_launcher"

PROGRAM_NAMES=(
  "00_查看执行顺序"
  "01_建图前安全检查"
  "02_启动实车扫描建图"
  "03_保存为test地图"
  "04_停止扫描建图"
  "05_启动test地图定位与RViz"
  "06_选择test中心点（可选）"
  "06A_选择红方出生点"
  "06B_选择蓝方出生点"
  "07_显示test中心点"
  "08_停止定位和中心点显示"
  "09_核对地图和出生点"
  "10_启动自动建图并30秒保存"
  "11_停止自动建图"
  "12_开启开机自动建图"
  "13_关闭开机自动建图"
)

mkdir -p "${DESKTOP_DIR}" "${SCRIPT_DIR}"

for script in "${STEP_SOURCE}"/*.sh; do
  install -m 0755 "${script}" "${SCRIPT_DIR}/$(basename -- "${script}")"
done
install -m 0644 "${STEP_SOURCE}/00_按顺序执行.md" "${DESKTOP_DIR}/00_按顺序执行.md"
install -m 0644 "${ROOT}/README.md" "${DESKTOP_DIR}/地图中心点说明.md"

gcc -std=c11 -O2 -Wall -Wextra -Werror \
  "${ROOT}/field_step_launcher.c" -o "${LAUNCHER_BINARY}"
chmod 0755 "${LAUNCHER_BINARY}"

shopt -s nullglob
legacy_scripts=("${DESKTOP_DIR}"/*.sh)
if ((${#legacy_scripts[@]})); then
  legacy_dir="${DESKTOP_DIR}/.legacy-sh-$(date +%Y%m%d_%H%M%S)"
  mkdir -p "${legacy_dir}"
  mv -- "${legacy_scripts[@]}" "${legacy_dir}/"
  echo "[BACKUP] 原桌面脚本已移到：${legacy_dir}"
fi

legacy_desktop=("${DESKTOP_DIR}"/*.desktop)
if ((${#legacy_desktop[@]})); then
  legacy_dir="${DESKTOP_DIR}/.legacy-desktop-$(date +%Y%m%d_%H%M%S)"
  mkdir -p "${legacy_dir}"
  mv -- "${legacy_desktop[@]}" "${legacy_dir}/"
  echo "[BACKUP] 原 .desktop 入口已移到：${legacy_dir}"
fi

for program_name in "${PROGRAM_NAMES[@]}"; do
  install -m 0755 "${LAUNCHER_BINARY}" "${DESKTOP_DIR}/${program_name}"
done

echo "[PASS] 建图步骤已安装为可双击运行的 Linux 程序：${DESKTOP_DIR}"
echo "[INFO] 维护脚本保存在隐藏目录：${SCRIPT_DIR}"
