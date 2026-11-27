#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""功率模型标定整链封装:RTT 采点 → CSV → K0..K5 拟合。

内部调用(不复制实现):
  1. tools/rtt_capture_point.py --cond <cond> --seconds <n>
  2. tools/rtt_pwr_to_csv.py <log> --power <W> --append <csv>
  3. tools/fit_power_model.py <csv>
需要 RTT 已在 running(先跑 tools/capture_rtt.bat 或 EmberProbe mcu-rtt)。

用法:
  python power_calib.py --cond spin300 --seconds 20 --power 84.6
  python power_calib.py --cond spin300 --power 84.6 --skip-capture      # 复用已有日志
  python power_calib.py --cond spin300 --power 84.6 --skip-append       # 只拟合
退出码:0=全步成功;1=某步失败;2=参数/文件缺失。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
PY = sys.executable


def step(label: str, cmd: list[str]) -> bool:
    print(f"== {label}: {' '.join(cmd)}")
    try:
        p = subprocess.run(cmd, cwd=str(ROOT), text=True, encoding="utf-8", errors="replace")
    except FileNotFoundError as exc:
        print(f"FAIL: 命令不存在: {exc}", file=sys.stderr)
        return False
    print(f"-- {label}: {'PASS' if p.returncode == 0 else 'FAIL'} (rc={p.returncode})")
    return p.returncode == 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="功率模型标定整链")
    ap.add_argument("--cond", required=True, help="工况名,如 spin300/steady300")
    ap.add_argument("--seconds", type=int, default=20)
    ap.add_argument("--power", type=float, required=True, help="该稳态点的实测功率(W)")
    ap.add_argument("--csv", default="steady.csv", help="累计稳态 CSV(默认 steady.csv)")
    ap.add_argument("--log", default=None, help="已有日志路径(--skip-capture 时)")
    ap.add_argument("--skip-capture", action="store_true")
    ap.add_argument("--skip-append", action="store_true")
    ap.add_argument("--skip-fit", action="store_true")
    args = ap.parse_args(argv)

    log = Path(args.log) if args.log else ROOT / "log" / f"{args.cond}_{args.seconds}s.log"
    csv = ROOT / args.csv

    if not args.skip_capture:
        if not step("采点", [PY, "tools/rtt_capture_point.py",
                             "--cond", args.cond, "--seconds", str(args.seconds)]):
            return 1
    if not log.exists():
        print(f"FAIL: 日志不存在: {log}", file=sys.stderr)
        return 2

    if not args.skip_append:
        if not step("转 CSV", [PY, "tools/rtt_pwr_to_csv.py", str(log),
                               "--power", str(args.power), "--append", str(csv)]):
            return 1
    if not csv.exists():
        print(f"FAIL: CSV 不存在: {csv}", file=sys.stderr)
        return 2

    if not args.skip_fit:
        if not step("拟合", [PY, "tools/fit_power_model.py", str(csv)]):
            return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
