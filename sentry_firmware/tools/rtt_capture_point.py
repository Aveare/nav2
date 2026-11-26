#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""采一个 20s 稳态工况并追加到 steady_raw.csv(不做分析)。

用法:
    python tools/rtt_capture_point.py --cond spin300 --seconds 20
    python tools/rtt_capture_point.py --cond static  --seconds 20

行为:
    - 先追平 RTT 缓冲(丢弃窗口前数据),再按 1s 间隔采集 `--seconds` 秒;
    - 原始文本存 log/<cond>_<seconds>s.log;
    - 解析 [pwri]/[pwr],取后 50% 样本平均,追加一行到 steady_raw.csv;
    - p 列留空(真实功率由操作手从客户端/台架电源读,后面统一补);
    - 只打印样本数,不打印/分析数值。

前置:RTT 已 running(node .agents/skills/mcu-rtt/scripts/rtt.js --start)。
"""
import argparse
import csv
import json
import re
import subprocess
import sys
import time
from pathlib import Path

RTT_JS = ".agents/skills/mcu-rtt/scripts/rtt.js"
RE_I = re.compile(r"\[pwri\]\s*i=([-\d.,]+)\s+w=([-\d.,]+)")
RE_P = re.compile(r"\[pwr\]\s*p=([-\d.]+)\s+s=([-\d.]+)\s+lim=([-\d.]+)\s+be=([-\d.]+)")


def rtt_read(gen, cur):
    cmd = ["node", RTT_JS, "--workspace", ".", "--read", "--channel", "0", "--max-bytes", "16384"]
    if gen is not None:
        cmd += ["--generation", str(gen), "--cursor", str(cur)]
    p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    try:
        j = json.loads(p.stdout)
    except json.JSONDecodeError:
        raise SystemExit(f"RTT read failed: {p.stdout[:300]} {p.stderr[:300]}")
    if "generation" not in j:
        raise SystemExit(f"RTT error: {j}")
    return j


def parse(text):
    iw, ps = [], []
    for line in text.splitlines():
        m = RE_I.search(line)
        if m:
            i = [float(x) for x in m.group(1).split(",") if x.strip()]
            w = [float(x) for x in m.group(2).split(",") if x.strip()]
            if len(i) == 4 and len(w) == 4:  # 丢掉被缓冲截断的短行
                iw.append((i, w))
            continue
        m = RE_P.search(line)
        if m:
            ps.append([float(x) for x in m.groups()])
    return iw, ps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cond", required=True, help="工况名,如 static/spin300/trans1000")
    ap.add_argument("--seconds", type=int, default=20)
    ap.add_argument("--append", default="steady_raw.csv")
    ap.add_argument("--no-append", action="store_true",
                    help="只存日志不写 CSV(平移这类要按 vx 分段时用)")
    args = ap.parse_args()

    gen = cur = None
    for _ in range(400):  # 追平缓冲
        j = rtt_read(gen, cur)
        gen, cur = j["generation"], j["nextCursor"]
        if not j.get("more"):
            break

    chunks = []
    for _ in range(args.seconds):
        j = rtt_read(gen, cur)
        chunks.append(j.get("text", ""))
        gen, cur = j["generation"], j["nextCursor"]
        time.sleep(1.0)
    text = "".join(chunks)
    Path("log").mkdir(exist_ok=True)
    log_path = Path(f"log/{args.cond}_{args.seconds}s.log")
    log_path.write_text(text, encoding="utf-8")

    iw, ps = parse(text)
    n = min(len(iw), len(ps))
    if n == 0:
        raise SystemExit(f"{log_path} 没有有效 [pwri]/[pwr] 对")
    iw, ps = iw[-n:], ps[-n:]
    k = max(1, int(round(n * 0.5)))
    iw, ps = iw[-k:], ps[-k:]
    i_avg = [sum(x[0][j] for x in iw) / k for j in range(4)]
    w_avg = [sum(x[1][j] for x in iw) / k for j in range(4)]
    s_avg = sum(x[1] for x in ps) / k

    if args.no_append:
        print(f"saved {log_path}: samples={n} used={k} (not appended)")
        return

    row = [args.cond] + [f"{x:.1f}" for x in i_avg] + [f"{x:.1f}" for x in w_avg] + \
          ["", f"{s_avg:.3f}"]
    out = Path(args.append)
    new = not out.exists()
    with out.open("a", encoding="utf-8", newline="") as fh:
        wr = csv.writer(fh)
        if new:
            wr.writerow(["cond", "i0", "i1", "i2", "i3", "w0", "w1", "w2", "w3", "p", "s"])
        wr.writerow(row)
    print(f"saved {log_path}: samples={n} used={k}; appended {out}")


if __name__ == "__main__":
    raise SystemExit(main())
