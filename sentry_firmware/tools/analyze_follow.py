"""Follow-loop ripple analysis for build/follow_*.csv (read-only)."""
import sys
import numpy as np
import pandas as pd

p = sys.argv[1] if len(sys.argv) > 1 else r"build\follow_osc_20261120_2216.csv"
df = pd.read_csv(p)
t = df["t_ms"].to_numpy(float) / 1000.0
dt = np.diff(t)
rate = 1.0 / np.median(dt)
print(f"rows {len(df)} span {t[-1]:.1f}s dt median {np.median(dt)*1000:.1f}ms (~{rate:.0f} Hz)")

cmd = df["cmd_wz"].to_numpy(float)
est = df["est_wz"].to_numpy(float)
off = df["off"].to_numpy(float)
gz = df["gz_dps"].to_numpy(float)  # already deg/s in follow_trace.csv

ff = 0.9 * gz
print(f"\n{'signal':8s} {'min':>8s} {'max':>8s} {'mean':>8s} {'std':>8s} {'p2p':>8s}")
for name, s in (("cmd_wz", cmd), ("est_wz", est), ("off", off), ("gz_deg", gz), ("ff0.9gz", ff)):
    print(f"{name:8s} {s.min():8.1f} {s.max():8.1f} {s.mean():8.1f} {s.std():8.1f} {s.max()-s.min():8.1f}")

# plateau = steady rotation: |cmd| above half of its peak
thr = 0.5 * np.abs(cmd).max()
m = np.abs(cmd) > thr
print(f"\nplateau |cmd|>{thr:.0f}: {m.sum()} pts")

def ripple(x, mask, label):
    seg = x[mask]
    if len(seg) < 32:
        print(f"  {label}: too short")
        return
    seg = seg - seg.mean()
    win = np.hanning(len(seg))
    spec = np.abs(np.fft.rfft(seg * win))
    freqs = np.fft.rfftfreq(len(seg), d=np.median(dt))
    k = np.argmax(spec[1:]) + 1
    amp = 2 * spec[k] / win.sum() * 2
    print(f"  {label}: mean {x[mask].mean():7.1f}  std {x[mask].std():6.1f}  "
          f"p2p {np.ptp(x[mask]):6.1f}  dominant {freqs[k]:5.2f} Hz (amp~{amp:.1f})")

print("\nripple on plateau (dominant frequency of the detrended segment):")
for label, s in (("cmd_wz", cmd), ("est_wz", est), ("off", off), ("gz_deg", gz)):
    ripple(s, m, label)

# how much of cmd is the ff term?
print(f"\ncorr(ff, cmd) = {np.corrcoef(ff[m], cmd[m])[0,1]:.3f}   "
      f"mean|cmd-ff| = {np.mean(np.abs(cmd[m]-ff[m])):.1f} deg/s")

# excerpt: 2 s around the plateau middle, 40 ms decimation
idx = np.where(m)[0]
if len(idx) > 0:
    a = idx[len(idx)//2]
    sel = np.arange(a, min(a + int(2.0/np.median(dt)), len(df)), 8)
    print("\nexcerpt (t, cmd, est, off, gz_deg):")
    for i in sel:
        print(f"  {t[i]:6.2f}  {cmd[i]:7.1f}  {est[i]:7.1f}  {off[i]:7.2f}  {gz[i]:7.1f}")

