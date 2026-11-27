#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "02 实车扫描建图"

echo "[INFO] 启动 MID360S、动态大 yaw、SLAM Toolbox 和建图 RViz。"
echo "[SAFE] enable_drive=false；请用下位机遥控或人工方式缓慢移动。"
echo "[STOP] 建图过程中不要关闭此终端；保存地图后执行步骤 04，或在此按 Ctrl+C。"
exec /home/rm/cod_deploy/start_hardware_mapping.sh
