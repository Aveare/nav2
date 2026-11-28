#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "12 开启登录后自动建图"

source_file="/home/rm/cod_ws/src/cod_nuc_bringup/tools/field_mapping/autostart/COD自动建图.desktop"
target_dir="/home/rm/.config/autostart"
target_file="${target_dir}/COD自动建图.desktop"
[[ -r "${source_file}" ]] || { echo "[FAIL] 缺少 ${source_file}"; exit 1; }
mkdir -p "${target_dir}"
install -m 0644 "${source_file}" "${target_file}"
echo "[PASS] 已开启：下次登录 NUC 图形桌面后自动建图并每30秒保存 test 地图。"
echo "[SAFE] 不会控制底盘，请仍使用下位机遥控移动机器人。"
