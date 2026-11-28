#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "10 自动建图并每30秒保存"

[[ -x /home/rm/cod_deploy/auto_mapping_autosave.sh ]] || {
  echo "[FAIL] 缺少 /home/rm/cod_deploy/auto_mapping_autosave.sh"
  exit 1
}
exec /home/rm/cod_deploy/auto_mapping_autosave.sh
