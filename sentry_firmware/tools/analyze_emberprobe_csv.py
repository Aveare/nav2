"""Summarize an EmberProbe live CSV (chart export).

Usage: python tools/analyze_emberprobe_csv.py <csv> [--rows N]
Read-only analysis; writes nothing.
"""
import sys, statistics
import pandas as pd


def main() -> int:
    path = sys.argv[1]
    df = pd.read_csv(path)
    t = pd.to_datetime(df.iloc[:, 0], utc=True)
    dt = t.diff().dt.total_seconds().dropna()
    print(f"file      : {path}")
    print(f"rows      : {len(df)}   columns: {len(df.columns)}")
    print(f"span      : {t.iloc[0]} .. {t.iloc[-1]}  ({dt.sum():.2f} s)")
    print(f"dt median : {dt.median()*1000:.1f} ms  -> ~{1/dt.median():.1f} Hz   "
          f"(min {dt.min()*1000:.1f} / max {dt.max()*1000:.1f} ms)")
    print()
    print(f"{'column':48s} {'min':>12s} {'max':>12s} {'mean':>12s} {'std':>10s}")
    for c in df.columns[1:]:
        s = pd.to_numeric(df[c], errors="coerce").dropna()
        if s.empty:
            print(f"{c:48s} {'(non-numeric)':>12s}")
            continue
        print(f"{c:48s} {s.min():12.3f} {s.max():12.3f} {s.mean():12.3f} {s.std():10.3f}")
    print()

    # How much of the capture has a real command?
    vx = pd.to_numeric(df.get("chassis_cmd_recv.vx [f32]"), errors="coerce")
    wz = pd.to_numeric(df.get("chassis_cmd_recv.wz [f32]"), errors="coerce")
    if vx is not None:
        moving = (vx.abs() > 50).sum()
        print(f"vx |.|>50 samples: {moving}/{len(df)}   peak |vx| = {vx.abs().max():.1f} mm/s")
    if wz is not None:
        print(f"wz peak |.|      : {wz.abs().max():.2f} deg/s")
    if "authority_source [u8]" in df:
        print("authority_source values:", sorted(df['authority_source [u8]'].unique().tolist()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
