#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "09 核对地图和中心点结果"

echo "============================================================"
echo "09 核对 test 地图与中心点文件"
echo "============================================================"

[[ -s "${FIELD_MAP_BASE}.yaml" ]] || { echo "[FAIL] 缺少 test.yaml"; exit 1; }
[[ -s "${FIELD_CENTER_FILE}" ]] || { echo "[FAIL] 缺少 test_center.csv"; exit 1; }
image_name="$(awk '$1 == "image:" {print $2; exit}' "${FIELD_MAP_BASE}.yaml")"
[[ "${image_name}" = /* ]] && image_path="${image_name}" || image_path="${FIELD_MAP_DIR}/${image_name}"
[[ -s "${image_path}" ]] || { echo "[FAIL] 缺少地图图像：${image_path}"; exit 1; }

echo "[PASS] 地图文件："
ls -lh "${FIELD_MAP_BASE}.yaml" "${image_path}"
echo
echo "[PASS] 中心点文件："
cat "${FIELD_CENTER_FILE}"
echo
echo "[DONE] test 地图与中心点已经配套保存。重新建图后必须重新执行步骤 06。"
