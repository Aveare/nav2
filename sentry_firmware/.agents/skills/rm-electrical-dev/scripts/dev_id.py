#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成/校验 DEV 时间戳编号(DEV-YYYYMMDD-HHMMSS)。

规则:.Doc/spec/兵种分支协作规范.md §7。时间戳只求全局唯一,不要求时钟精准;
同一编号已存在时追加 b 后缀。检查范围:根 RM_DEVLOG、三个兵种 DEVLOG、开发史、git log --all。

用法:
  python dev_id.py --new              # 生成并检查重复
  python dev_id.py --check DEV-20261127-000119
  python dev_id.py --template         # 打印 DEVLOG 标题模板
退出码:0=成功/唯一;1=编号重复或格式非法。
"""

from __future__ import annotations

import argparse
import datetime as dt
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = (
    "RM_DEVLOG.md",
    ".Doc/history/开发史.md",
    ".Doc/robots/infantry/DEVLOG.md",
    ".Doc/robots/sentry/DEVLOG.md",
    ".Doc/robots/heavy/DEVLOG.md",
)
ID_RE = re.compile(r"^DEV-\d{8}-\d{6}b?$")


def read_sources() -> str:
    text = ""
    for rel in SOURCES:
        p = ROOT / rel
        if p.exists():
            text += p.read_text(encoding="utf-8", errors="replace")
    return text


def git_grep(dev_id: str) -> bool:
    p = subprocess.run(
        ["git", "log", "--all", "--oneline", "--grep", dev_id],
        cwd=str(ROOT), capture_output=True, text=True,
        encoding="utf-8", errors="replace",
    )
    return bool(p.stdout.strip())


def used(dev_id: str, text: str) -> bool:
    return dev_id in text or git_grep(dev_id)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="DEV 时间戳编号")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--new", action="store_true")
    g.add_argument("--check", metavar="ID")
    g.add_argument("--template", action="store_true")
    args = ap.parse_args(argv)

    text = read_sources()
    if args.template:
        print("### DEV-YYYYMMDD-HHMMSS｜YYYY-MM-DD｜[兵种/域] 标题")
        print("\n**背景**。\n\n**变更**。\n\n**验证**。\n\n**未执行**。\n\n**AI**：<模型>,YYYY-MM-DD。")
        return 0

    if args.check:
        dev_id = args.check.strip()
        if not ID_RE.match(dev_id):
            print(f"FAIL: 格式非法(期望 DEV-YYYYMMDD-HHMMSS): {dev_id}")
            return 1
        print(f"{'FAIL: 已存在' if used(dev_id, text) else 'PASS: 唯一'} {dev_id}")
        return 1 if used(dev_id, text) else 0

    base = "DEV-" + dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    dev_id = base
    suffix = 0
    while used(dev_id, text):
        suffix += 1
        dev_id = f"{base}{chr(ord('a') + suffix - 1)}"
        if suffix > 26:
            print("FAIL: 同一秒内编号冲突过多", file=sys.stderr)
            return 1
    print(dev_id)
    return 0


if __name__ == "__main__":
    sys.exit(main())
