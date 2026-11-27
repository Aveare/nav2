#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
GUIDE="${HERE}/00_按顺序执行.md"

if [[ ! -r "${GUIDE}" ]]; then
  echo "找不到说明：${GUIDE}"
  exit 1
fi

if [[ -t 1 ]] && command -v less >/dev/null 2>&1; then
  less -R "${GUIDE}"
else
  cat "${GUIDE}"
fi
