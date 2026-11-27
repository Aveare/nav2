#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "00 建图与中心点执行顺序"
GUIDE="${HERE}/00_按顺序执行.md"
if [[ -t 1 ]] && command -v less >/dev/null 2>&1; then
  less -R "${GUIDE}"
else
  cat "${GUIDE}"
fi
