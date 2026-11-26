#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""源文件清单一致性检查:Makefile 的 C_SOURCES 与磁盘上的第一方 .c 必须一一对应。

背景:CMake 用 GLOB 自动收集,Makefile 是显式列表。两边集合一旦漂移,
就会出现"某文件在 CMake 构建里编译、在 make 构建里消失"的静默差异
(2026-11-20 盘点:4 个 .c 未登记,其中 3 个是 0 字节占位)。

用法: python tools/check_source_lists.py [--fix-help]
退出码:0 一致;1 有漂移。
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TREES = ("Drivers", "Src", "Middlewares", "bsp", "modules",
         "framework", "ports", "components", "apps")  # B8:application→apps/<兵种>_skeleton  # B1 起框架目录、B5 起组件目录入默认扫描
# --trees 仅用于临时扩扫(G10 验证 apps/components 等未落地目录)


# Makefile 是步兵单兵种清单;heavy/sentry 的 app 源由 CMake GLOB 收集,
# 不进 Makefile(见 apps/<robot>/app/assembly.c 与 CMakeLists ROBOT_PROFILE)。
MAKEFILE_APP_PROFILE = "infantry"


def disk_sources(extra=()):
    found = set()
    for tree in tuple(TREES) + tuple(extra):
        base = ROOT / tree
        if not base.is_dir():
            continue
        for p in base.rglob("*.c"):
            rel = p.relative_to(ROOT).as_posix()
            # 非步兵 profile 的 app 源不在 Makefile 管辖内,跳过
            if rel.startswith("apps/heavy/") or rel.startswith("apps/sentry/"):
                continue
            found.add(rel)
    return found


def makefile_sources():
    text = (ROOT / "Makefile").read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    try:
        start = next(i for i, l in enumerate(lines) if re.match(r"^C_SOURCES\s*=", l))
    except StopIteration:
        raise SystemExit("Makefile 里找不到 C_SOURCES")
    entries = set()
    i = start
    cur = lines[i].split("=", 1)[1].strip()
    while True:
        item = cur.rstrip("\\").strip()
        if item.endswith(".c"):
            entries.add(item)
        if not cur.endswith("\\"):
            break
        i += 1
        if i >= len(lines):
            break
        cur = lines[i].strip()
    return entries


def main():
    ap = argparse.ArgumentParser(description="Makefile C_SOURCES 与磁盘 .c 一致性")
    ap.add_argument("--trees", default="",
                    help="扩展扫描目录(空格分隔,如 'framework ports apps components';分隔符以本 --help 为准)")
    args = ap.parse_args()
    extra = tuple(t for t in args.trees.split() if t)
    disk, mk = disk_sources(extra), makefile_sources()
    missing = sorted(disk - mk)
    extra = sorted(mk - disk)
    if not missing and not extra:
        print(f"PASS: Makefile C_SOURCES 与磁盘 .c 一致({len(disk)} 个)")
        return 0
    print(f"FAIL: 漂移 {len(missing) + len(extra)} 条")
    for p in missing:
        print(f"  磁盘有、Makefile 缺: {p}")
    for p in extra:
        print(f"  Makefile 有、磁盘缺: {p}")
    print("处理:新增/删除 .c 后同步 Makefile 的 C_SOURCES(CMake 是 GLOB,无需改)。")
    return 1


if __name__ == "__main__":
    sys.exit(main())
