#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T14 宏重复/冲突检查(附D 清单的机器化,2026-11-22 B12c 交付)。

判据:
  - 同一文件内同名宏多次定义 = 条件编译分支,合法(不计);
  - 跨文件同名宏:
      * 值不同 → **冲突 FAIL**(同名不同值,包含顺序决定语义,禁止);
      * 值相同 → 视为重复定义,须在 `.Doc/gate/macro-dups-allow.txt` 登记理由,
        未登记 → FAIL;
  - 单字母宏(N10)一律 FAIL。
值比较前剥离行尾 `//`/`/* */` 注释;函数式宏按原文比较。

用法: python tools/check_macro_dups.py
退出码:0=PASS;1=FAIL。
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# config 只扫当前 profile:多兵种配置目录共存时,跨 profile 同名宏(如 AX650_UART1_ENABLE
# infantry=1/heavy=0)不是冲突,是编译期选择。默认步兵,可用 --profile 切换。
import argparse

_arg = argparse.ArgumentParser()
_arg.add_argument("--profile", default="infantry_2026", help="当前兵种配置目录(config/<profile>)")
_PROFILE = _arg.parse_known_args()[0].profile
if not (ROOT / "config" / _PROFILE).is_dir():
    print(f"FAIL: config/{_PROFILE} 目录不存在")
    raise SystemExit(1)
DIRS = ("apps", "components", "framework", "ports", f"config/{_PROFILE}")
ALLOW = ROOT / ".Doc/gate/macro-dups-allow.txt"
DEFINE_RE = re.compile(r"^[ \t]*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b(.*)$", re.M)


def strip_comment(value: str) -> str:
    value = re.split(r"/\*", value, maxsplit=1)[0]
    value = re.split(r"//", value, maxsplit=1)[0]
    return value.strip()


def load_allow() -> dict:
    allow = {}
    if ALLOW.is_file():
        for line in ALLOW.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name, _, reason = line.partition("|")
            allow[name.strip()] = reason.strip()
    return allow


def main() -> int:
    defs = defaultdict(list)
    for d in DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if p.suffix not in (".c", ".h"):
                continue
            rel = p.relative_to(ROOT).as_posix()
            text = p.read_text(encoding="utf-8", errors="replace")
            for m in DEFINE_RE.finditer(text):
                defs[m.group(1)].append((rel, strip_comment(m.group(2))))

    allow = load_allow()
    conflicts, dups, singles = [], [], []
    for name, locs in sorted(defs.items()):
        if len(name) == 1:
            singles.append((name, sorted({f for f, _ in locs})))
        files = {f for f, _ in locs}
        if len(files) < 2:
            continue
        if name in allow:
            continue  # 登记豁免:同名同值约定,或同构条件块(如 user_malloc 平台选择)
        values = {v for _, v in locs}
        if len(values) > 1:
            conflicts.append((name, sorted(locs)))
        else:
            dups.append((name, sorted(files)))

    for name, locs in conflicts:
        print(f"[FAIL] 冲突(同名不同值): {name}")
        for f, v in locs:
            print(f"        {f}: {v[:70]}")
    for name, files in dups:
        print(f"[FAIL] 重复(同名同值,未登记): {name} <- {files}")
    for name, files in singles:
        print(f"[FAIL] 单字母宏(N10): {name} <- {files}")

    bad = len(conflicts) + len(dups) + len(singles)
    if bad:
        print(f"RESULT: FAIL({bad} 项;登记重复见 {ALLOW.relative_to(ROOT).as_posix()})")
        return 1
    print(f"RESULT: PASS(跨文件冲突 0/未登记重复 0/单字母宏 0;豁免 {len(allow)} 项)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
