#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 RTT 日志里的 [pwri]/[pwr] 行转成一行功率标定 CSV。

背景:
    固件在 CHASSIS_CALIB_LOG=1 时每 0.5s 输出:
        [pwri] i=i0,i1,i2,i3 w=w0,w1,w2,w3
        [pwr] p=预限幅预测功率 s=总衰减系数 [po=限幅后预测功率] lim=上限 be=缓冲
    DEV-20261126-33 起 s 是所有模式的总 scale,po 是缩放后模型功率(可选字段)。
    日志用 tools/rtt_log.py 落盘(log/<时间戳>.log)。
    真实功率机器人拿不到,必须由操作手从裁判客户端/台架电源读,再用 --power 传入。

用法:
    # 单个稳态点:取日志最后 50% 样本平均,写入/追加到 steady.csv
    python tools/rtt_pwr_to_csv.py log/2026-11-26_12-00-00.log --power 84.6 --append steady.csv

    # 先看平均结果,不写文件
    python tools/rtt_pwr_to_csv.py log/xxx.log

    # 只用最后 30% 样本(前面加速段不要)
    python tools/rtt_pwr_to_csv.py log/xxx.log --power 84.6 --tail 0.3

输出列:
    i0,w0,i1,w1,i2,w2,i3,w3,p,s
    i=电流指令原始码,w=转子 deg/s,p=真实功率 W,s=平均衰减系数

注意:
    - 平均 s < 0.99 说明限幅在生效,该点不能用于拟合(工具会警告,拟合脚本也会丢弃)。
    - 每个稳态点单独录一段日志最稳;不要在一段日志里跨多个工况。
"""
import argparse
import csv
import re
import sys
from pathlib import Path

RE_PWRI = re.compile(
    r"\[pwri\]\s*i=([-\d.,]+)\s+w=([-\d.,]+)"
)
RE_PWR = re.compile(
    r"\[pwr\]\s*p=([-\d.]+)\s+s=([-\d.]+)(?:\s+po=[-\d.]+)?\s+lim=([-\d.]+)\s+be=([-\d.]+)"
)


def parse_floats(s, n, what):
    parts = [x for x in s.split(",") if x.strip() != ""]
    if len(parts) != n:
        raise ValueError(f"{what} 期望 {n} 个数,实际 {len(parts)}: {s}")
    return [float(x) for x in parts]


def load(log_path):
    iw = []
    ps = []
    text = Path(log_path).read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        m = RE_PWRI.search(line)
        if m:
            try:
                i = parse_floats(m.group(1), 4, "i")
                w = parse_floats(m.group(2), 4, "w")
            except ValueError as exc:
                print(f"WARN: 跳过一行: {exc}", file=sys.stderr)
                continue
            iw.append((i, w))
            continue
        m = RE_PWR.search(line)
        if m:
            ps.append((float(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))))
    n = min(len(iw), len(ps))
    if n == 0:
        raise SystemExit("没有解析到 [pwri]/[pwr] 行:确认固件 CHASSIS_CALIB_LOG=1 且日志完整")
    if len(iw) != len(ps):
        print(f"WARN: [pwri] {len(iw)} 行 / [pwr] {len(ps)} 行,按较短的 {n} 行配对", file=sys.stderr)
    return iw[-n:], ps[-n:]


def main():
    ap = argparse.ArgumentParser(description="RTT [pwri]/[pwr] -> 功率标定 CSV")
    ap.add_argument("log", help="RTT 日志文件")
    ap.add_argument("--power", type=float, default=None,
                    help="该稳态点的真实功率 W(裁判客户端/台架电源读数);不传只打印平均")
    ap.add_argument("--tail", type=float, default=0.5,
                    help="只用最后这一比例的样本(默认 0.5,丢掉起步瞬态)")
    ap.add_argument("--append", default=None,
                    help="追加到该 CSV;文件不存在时自动写表头")
    args = ap.parse_args()

    if not (0.0 < args.tail <= 1.0):
        raise SystemExit("--tail 必须在 (0,1]")

    iw, ps = load(args.log)
    n_total = len(iw)
    n_use = max(1, int(round(n_total * args.tail)))
    iw = iw[-n_use:]
    ps = ps[-n_use:]

    i_avg = [sum(x[0][k] for x in iw) / n_use for k in range(4)]
    w_avg = [sum(x[1][k] for x in iw) / n_use for k in range(4)]
    p_avg = sum(x[0] for x in ps) / n_use
    s_avg = sum(x[1] for x in ps) / n_use
    lim_avg = sum(x[2] for x in ps) / n_use
    be_avg = sum(x[3] for x in ps) / n_use

    print(f"样本 {n_use}/{n_total} 条(tail={args.tail})")
    print("  i = " + ", ".join(f"{x:8.1f}" for x in i_avg))
    print("  w = " + ", ".join(f"{x:8.1f}" for x in w_avg))
    print(f"  p_pred={p_avg:7.2f} W  s={s_avg:.3f}  lim={lim_avg:.1f} W  be={be_avg:.1f} J")

    if s_avg < 0.99:
        print("WARN: 平均 s<0.99,功率限幅在生效;该点不能用于拟合。"
              "标定时应让限幅不生效(临时 CHASSIS_POWER_LIMIT_MAX=200,或 CTRL_EN=0)。",
              file=sys.stderr)
    if 0.0 <= be_avg < 10.0:
        print("WARN: 缓冲能量偏低,先让缓冲回充再采下一个点。", file=sys.stderr)

    if args.power is None:
        print("\n未给 --power,不写文件。补上真实功率后重跑,例如:")
        print(f"  python tools/rtt_pwr_to_csv.py {args.log} --power <W> --append steady.csv")
        return 0

    row = [f"{x:.1f}" for x in i_avg] + [f"{x:.1f}" for x in w_avg] + [f"{args.power:.2f}", f"{s_avg:.3f}"]
    header = ["i0", "i1", "i2", "i3", "w0", "w1", "w2", "w3", "p", "s"]
    out = Path(args.append) if args.append else None
    if out:
        new_file = not out.exists()
        with out.open("a", encoding="utf-8", newline="") as fh:
            wr = csv.writer(fh)
            if new_file:
                wr.writerow(header)
            wr.writerow(row)
        print(f"\n已追加到 {out}")
    print("CSV 行: " + ",".join(row))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
