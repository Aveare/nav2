#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G8 对拍器:金标准(迁移前代码生成)vs 候选(迁移后重放)逐列比对。

判据(方案 §5-G8a/G8b):rtol/atol ≤1e-6 + **模式字/枚举/分支标记精确相等**;
G8b 整链 CSV 还要求 **tick 序列逐拍对齐**(缺行/多行/乱序都 FAIL)。
金标准文件在 B0 由迁移前代码生成并入库(.Doc/gate/,不入 build/),迁移批
(B5/B6a/B6b/B8)重放同一输入序列产生候选 CSV 后用本工具比对,禁止自证
(金标准不是迁移后代码跑出来的)。

CSV 约定:首行表头;第一列 = tick(整数,严格递增或至少单调不减且逐行对齐);
含 mode/enum 字样的列 = 模式字,必须精确相等;其余数值列按 rtol/atol 比对;
支持 NaN(两边同为 NaN 记相等)。

用法:
  python tools/check_golden.py --golden .Doc/gate/arch-baseline/g8b-fullchain.csv \
         --candidate tests/build/g8b-fullchain.csv [--rtol 1e-6] [--atol 1e-6] [--tolerant-missing-col]
  python tools/check_golden.py --selftest          # 内置自检(对拍器自身正确性)
退出码:0=PASS;1=FAIL;2=用法错误。
"""
import argparse
import csv
import math
import sys
from pathlib import Path

MODE_HINTS = ("mode", "state", "enum", "flag")


def read_csv(path):
    rows = list(csv.reader(Path(path).open(encoding="utf-8", newline="")))
    if not rows:
        raise SystemExit(f"FAIL: 空 CSV {path}")
    header, body = rows[0], rows[1:]
    return header, body


def is_mode_col(name):
    low = name.lower()
    return any(h in low for h in MODE_HINTS)


def num_eq(a, b, rtol, atol):
    if a == b:
        return True
    try:
        fa, fb = float(a), float(b)
    except ValueError:
        return False
    if math.isnan(fa) and math.isnan(fb):
        return True
    if math.isnan(fa) != math.isnan(fb):
        return False
    if math.isinf(fa) or math.isinf(fb):
        return fa == fb
    return abs(fa - fb) <= max(atol, rtol * max(abs(fa), abs(fb)))


def compare(golden_csv, cand_csv, rtol, atol, tolerant_missing_col=False):
    gh, gb = read_csv(golden_csv)
    ch, cb = read_csv(cand_csv)
    problems = []

    if not tolerant_missing_col and gh != ch:
        problems.append(f"表头不一致: golden={gh} candidate={ch}")

    if gh and ch and gh[0].lower() not in ("tick", "t", "idx"):
        problems.append(f"首列不是 tick(金标准首列={gh[0]});tick 对齐判据无法执行")

    # tick 序列逐拍对齐(G8b)
    gticks = [r[0] if r else "" for r in gb]
    cticks = [r[0] if r else "" for r in cb]
    if gticks != cticks:
        n = min(len(gticks), len(cticks))
        bad = next((i for i in range(n) if gticks[i] != cticks[i]), n)
        problems.append(
            f"tick 序列不对齐: 第 {bad} 拍 golden={gticks[bad:bad+3]} cand={cticks[bad:bad+3]};"
            f" 行数 g={len(gticks)} c={len(cticks)}")

    # 数值/模式字逐列比对
    ncols = min(len(gh), len(ch))
    gidx = {name: i for i, name in enumerate(gh)}
    cidx = {name: i for i, name in enumerate(ch)}
    for name in gh:
        if name not in cidx:
            if not (tolerant_missing_col and name not in ("tick",)):
                problems.append(f"候选缺列: {name}")
            continue
        gi, ci = gidx[name], cidx[name]
        for row in range(min(len(gb), len(cb))):
            gv = gb[row][gi] if gi < len(gb[row]) else ""
            cv = cb[row][ci] if ci < len(cb[row]) else ""
            if is_mode_col(name):
                if gv != cv:
                    problems.append(f"模式字精确相等 FAIL: {name} @tick{gb[row][0]}: {gv!r} != {cv!r}")
            elif not num_eq(gv, cv, rtol, atol):
                problems.append(f"数值超差: {name} @tick{gb[row][0]}: {gv} != {cv}")

    print(f"=== G8 对拍: {golden_csv} vs {cand_csv} ===")
    print(f"行数 g={len(gb)} c={len(cb)};列 g={len(gh)} c={len(ch)};"
          f" rtol={rtol} atol={atol};模式字列="
          f"{[h for h in gh if is_mode_col(h)]}")
    if problems:
        for p in problems[:20]:
            print(f"  [FAIL] {p}")
        print(f"  (共 {len(problems)} 条)")
        print("RESULT: FAIL")
        return 1
    print("RESULT: PASS")
    return 0


def selftest():
    """对拍器自身正确性:构造金/候选对,断言各判定路径行为符合判据。"""
    import tempfile, os
    d = tempfile.mkdtemp()
    gp, cp = os.path.join(d, "g.csv"), os.path.join(d, "c.csv")
    checks = []

    def case(name, golden, cand, expect):
        Path(gp).write_text(golden, encoding="utf-8")
        Path(cp).write_text(cand, encoding="utf-8")
        rc = compare(gp, cp, 1e-6, 1e-6)
        checks.append((name, rc == expect, rc, expect))

    case("完全一致", "tick,ref,mode\n0,1.0,1\n1,2.0,2\n", "tick,ref,mode\n0,1.0,1\n1,2.0,2\n", 0)
    case("数值在容差内", "tick,ref\n0,1.0000000001\n", "tick,ref\n0,1.0\n", 0)
    case("数值超差", "tick,ref\n0,1.1\n", "tick,ref\n0,1.0\n", 1)
    case("模式字差异(必须精确相等)", "tick,mode\n0,1\n", "tick,mode\n0,2\n", 1)
    case("tick 缺拍", "tick,ref\n0,1\n1,2\n", "tick,ref\n0,1\n", 1)
    case("NaN==NaN", "tick,ref\n0,nan\n", "tick,ref\n0,nan\n", 0)
    case("表头不一致", "tick,ref\n0,1\n", "tick,REF\n0,1\n", 1)

    failed = [c for c in checks if not c[1]]
    for name, ok, got, want in checks:
        print(f"  {'ok ' if ok else 'FAIL'} {name}: got={got} want={want}")
    print("RESULT:", "PASS" if not failed else "FAIL")
    return 0 if not failed else 1


def main():
    ap = argparse.ArgumentParser(description="G8 金标准对拍器")
    ap.add_argument("--golden")
    ap.add_argument("--candidate")
    ap.add_argument("--rtol", type=float, default=1e-6)
    ap.add_argument("--atol", type=float, default=1e-6)
    ap.add_argument("--tolerant-missing-col", action="store_true",
                    help="允许候选缺少金标准列(仅过渡期,默认不允许)")
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        raise SystemExit(selftest())
    if not args.golden or not args.candidate:
        ap.error("--golden 与 --candidate 必填(或 --selftest)")
    raise SystemExit(compare(args.golden, args.candidate, args.rtol, args.atol,
                             args.tolerant_missing_col))


if __name__ == "__main__":
    main()
