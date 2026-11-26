"""Summarize build/chassis_*.csv produced by tools/chassis_trace.js (read-only)."""
import sys
import pandas as pd

p = sys.argv[1] if len(sys.argv) > 1 else r"build\chassis_drive_20261120_2150.csv"
df = pd.read_csv(p)
t = df["t_ms"] / 1000.0
dt = t.diff().dropna()
print(f"rows {len(df)}  span {t.iloc[-1]-t.iloc[0]:.1f} s  dt median {dt.median()*1000:.0f} ms"
      f"  (~{1/dt.median():.0f} Hz)")

cols = ["est_vx", "est_vy", "est_wz", "spd_lf", "spd_rf", "spd_lb", "spd_rb"]
for c in cols:
    s = df[c]
    print(f"  {c:7s} min {s.min():9.1f}  max {s.max():9.1f}  mean {s.mean():8.1f}  std {s.std():8.1f}")

moving = (df[["est_vx", "est_vy", "est_wz"]].abs().max(axis=1) > 20) | \
         (df[["spd_lf", "spd_rf", "spd_lb", "spd_rb"]].abs().max(axis=1) > 200)
print(f"\nmoving samples {int(moving.sum())}/{len(df)}  "
      f"first at t={t[moving].iloc[0]:.2f}s last at t={t[moving].iloc[-1]:.2f}s" if moving.any() else "\nno motion")
print("state values:", sorted(df["state"].unique().tolist()), " auth:", sorted(df["auth"].unique().tolist()))

# wheel consistency during motion (pure spin: all four same sign & magnitude)
if moving.any():
    m = df[moving]
    sp = m[["spd_lf", "spd_rf", "spd_lb", "spd_rb"]]
    mean_abs = sp.abs().mean(axis=1)
    rel = ((sp.abs().max(axis=1) - sp.abs().min(axis=1)) / mean_abs.replace(0, pd.NA)).dropna()
    print(f"wheel |fb| spread during motion: median {rel.median()*100:.1f}%  p95 {rel.quantile(0.95)*100:.1f}%")
    # sign agreement (spin => all same sign)
    signs = sp.apply(lambda r: len(set(int(v > 0) for v in r)), axis=1)
    print(f"rows with mixed wheel signs: {int((signs > 1).sum())}/{len(m)}")
    # est_wz vs mean wheel speed relation
    ew = m["est_wz"].abs()
    if ew.max() > 30:
        sel = ew > 30
        print(f"est_wz vs mean|spd|: est {ew[sel].mean():.0f} deg/s, wheels {mean_abs[sel].mean():.0f} deg/s,"
              f" ratio {mean_abs[sel].mean()/ew[sel].mean():.1f}")
