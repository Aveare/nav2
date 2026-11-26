#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G7 记录与回退检查(自主研发框架升级总方案 §5-G7,B1 交付;B1 前人工核对)。

判据:①本批 DEV 条目存在(RM_DEVLOG 中有含批次的 DEV 标题行);
②`arch-before-B{上一批}` 标签存在且指向上一批收工提交。
R 系旧标签只读保留(本工具不检查)。

用法:
  python tools/check_tags.py --batch B1            # 校验 B1:DEV 条目含 B1 + 标签链
  python tools/check_tags.py --batch NOPE          # fixture:验证 FAIL 路径
退出码:0=PASS;1=FAIL。
"""
import argparse
import re
import subprocess
import sys

DEVLOG = "RM_DEVLOG.md"


def run_git(*args):
    return subprocess.run(["git"] + list(args), capture_output=True, text=True)


def main():
    ap = argparse.ArgumentParser(description="G7 记录与回退")
    ap.add_argument("--batch", required=True, help="本批编号,如 B1 / B1b")
    args = ap.parse_args()
    problems = []

    devlog = open(DEVLOG, encoding="utf-8").read()
    dev_heads = re.findall(r"^## (DEV-\d{8}-\d+)｜([^\n]+)", devlog, re.M)
    hit = [(i, t) for i, t in dev_heads if args.batch in t]
    if hit:
        print(f"[PASS] 本批 DEV 条目: {hit[0][0]} {hit[0][1][:50]}")
    else:
        problems.append(f"RM_DEVLOG 无含 {args.batch} 的 DEV 标题行(批次条目缺失)")

    m = re.fullmatch(r"B(\d+)(b?)", args.batch)
    if not m:
        problems.append(f"批次编号不合法: {args.batch}(期望 B<n> 或 B<n>b)")
        print("=== G7 记录与回退 ===")
        for p in problems:
            print(f"  [FAIL] {p}")
        print("RESULT: FAIL")
        return 1
    prev_num = int(m.group(1)) - 1
    prev = f"B{prev_num}" + ("b" if args.batch.endswith("b") and prev_num else "")
    # B1b 之类小批:上一标签 = 同号主批
    if args.batch.endswith("b") and prev_num:
        prev = f"B{prev_num}"
    tag = f"arch-before-{args.batch}" if prev_num <= 0 else f"arch-before-{prev}"
    # 语义:本批收工前应存在的"上一批"标签 = arch-before-<prev>;B0/B1 特判允许 arch-before-B0/up
    if tag == "arch-before-B0":
        cand = ["arch-before-B0", "arch-before-B1"]
    elif prev_num <= 0:
        cand = [f"arch-before-{args.batch}"]
    else:
        cand = [f"arch-before-{prev}"]
    # 非顺序批(方案 §6.0:B9/B10 不执行):B11 的上一收工点 = M3 里程碑标签(B8 收口);
    # 标签命名约定 = arch-before-<下一批>,故 B11 收工标签是 arch-before-B12。
    prev_tag_override = {"B11": "arch-before-M3", "B12": "arch-before-B12"}
    if args.batch in prev_tag_override:
        cand = [prev_tag_override[args.batch]]

    found = None
    for c in cand:
        r = run_git("rev-parse", "-q", "--verify", c)
        if r.returncode == 0:
            found = (c, r.stdout.strip())
            break
    if found:
        print(f"[PASS] 标签 {found[0]} -> {found[1][:12]}")
    else:
        problems.append(f"上一批收工标签缺失(尝试 {cand})")

    print("=== G7 记录与回退 ===")
    for p in problems:
        print(f"  [FAIL] {p}")
    print("RESULT:", "PASS" if not problems else "FAIL")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
