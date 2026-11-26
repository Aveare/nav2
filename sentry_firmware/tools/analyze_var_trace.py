#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""变量轨迹离线分析:对 trace_*.csv 给出振荡/静止/单向漂移判据。

背景(DEV-20261112 经验):采样会暂停核心的读数会污染现象,pitch_trace.js 改走
EmberProbe variables.sample 后仍需要**判据**而不是肉眼看波形 —— 本脚本把
pitch_trace.js 的三态判据(往复/静止/单向)推广到任意列。

判据(对指定的角度类列):
  DEADBAND   相邻两点变化 ≤ DEADBAND 记为噪声(默认 0.02,单位跟随列);
  REVERSALS  有效变化方向翻转次数 ≥ 6 且峰峰值 ≥ RANGE → 往复(振荡);
  峰峰值 < STILL_RANGE(默认 0.3) → 静止;
  其余 → 单向/慢漂(给出净位移与时长)。

用法:
  python tools/analyze_var_trace.py build/trace_yaw_oscillation.csv --col 0
  python tools/analyze_var_trace.py build/x.csv --col gimbal_cmd_recv.yaw --deadband 0.05
退出码:0=有结论(不区分好坏);2=无法分析(列缺失/样本不足)。
"""
import argparse
import csv
import sys


def load_csv(path):
    with open(path, encoding="utf-8", errors="replace", newline="") as f:
        rows = [r for r in csv.reader(f) if r]
    if len(rows) < 2:
        raise SystemExit("FAIL: CSV 行数不足")
    header, data = rows[0], rows[1:]
    # 跳过 label 列(非数值列)
    cols = []
    for i, h in enumerate(header):
        try:
            float(data[0][i])
            cols.append((i, h))
        except ValueError:
            pass
    return cols, data


def series(data, idx):
    out = []
    for r in data:
        try:
            out.append(float(r[idx]))
        except (ValueError, IndexError):
            continue
    return out


def analyze(name, values, deadband, still_range, range_min):
    if len(values) < 10:
        return (name, None, f"样本不足({len(values)}<10)")
    reversals = 0
    prev_dir = 0
    peak_min = peak_max = values[0]
    valid_moves = 0
    for a, b in zip(values, values[1:]):
        d = b - a
        if abs(d) <= deadband:
            continue
        valid_moves += 1
        cur = 1 if d > 0 else -1
        peak_min = min(peak_min, b)
        peak_max = max(peak_max, b)
        if prev_dir and cur != prev_dir:
            reversals += 1
        prev_dir = cur
    rng = peak_max - peak_min
    net = values[-1] - values[0]
    if reversals >= 6 and rng >= range_min:
        verdict = f"往复/振荡:反转 {reversals} 次,峰峰值 {rng:.3f}"
    elif rng < still_range:
        verdict = f"静止:峰峰值 {rng:.3f}"
    else:
        verdict = f"单向/慢漂:净位移 {net:.3f},峰峰值 {rng:.3f},有效变化 {valid_moves} 点"
    stats = (f"n={len(values)} min={min(values):.3f} max={max(values):.3f} "
             f"mean={sum(values)/len(values):.3f} last={values[-1]:.3f}")
    return (name, verdict, stats)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="pitch_trace.js/var trace 输出的 CSV")
    ap.add_argument("--col", default="0", help="列序号或列名(默认第 0 数值列)")
    ap.add_argument("--deadband", type=float, default=0.02)
    ap.add_argument("--still-range", type=float, default=0.3)
    ap.add_argument("--range-min", type=float, default=1.0)
    ap.add_argument("--all", action="store_true", help="分析全部数值列")
    args = ap.parse_args()

    cols, data = load_csv(args.csv)
    if args.all:
        targets = cols
    else:
        if args.col.isdigit():
            idx = int(args.col)
            matches = [c for c in cols if c[0] == idx]
        else:
            matches = [c for c in cols if c[1] == args.col]
        if not matches:
            print(f"FAIL: 找不到数值列 {args.col}(可用: {cols})")
            return 2
        targets = matches

    print(f"=== 轨迹分析 {args.csv} ===")
    for idx, name in targets:
        name_, verdict, stats = analyze(name, series(data, idx),
                                        args.deadband, args.still_range, args.range_min)
        print(f"[{name_}] {verdict}\n    {stats}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
