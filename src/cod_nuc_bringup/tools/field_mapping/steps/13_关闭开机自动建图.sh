#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "13 关闭登录后自动建图"

target_file="/home/rm/.config/autostart/COD自动建图.desktop"
if [[ ! -e "${target_file}" ]]; then
  echo "[INFO] 开机自动建图当前未开启。"
  exit 0
fi
disabled_file="${target_file}.disabled_$(date +%Y%m%d_%H%M%S)"
mv "${target_file}" "${disabled_file}"
echo "[PASS] 已关闭登录后自动建图。备份文件：${disabled_file}"
