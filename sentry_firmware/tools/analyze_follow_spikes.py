"""Look for spikes / causal order in the follow loop capture (read-only)."""
import sys
import numpy as np
import pandas as pd

p = sys.argv[1] if len(sys.argv) > 1 else r"build\follow_osc_20261120_2216.csv"
df = pd.read_csv(p)
t = df["t_ms"].to_numpy(float) / 1000.0
cmd = df["cmd_wz"].to_numpy(float)
est = df["est_wz"].to_numpy(float)
off = df["off"].to_numpy(float)
gz = df["gz_dps"].to_numpy(float)
dt = np.median(np.diff(t))

plateau = np.abs(cmd) > 0.5 * np.abs(cmd).max()
print(f"plateau {plateau.sum()} pts of {len(df)}, dt {dt*1000:.1f} ms")

# high-pass each channel (remove mean) and look at extreme deviations
def spikes(x, name, k=3.0):
    seg = x[plateau]
    med = np.median(seg)
    dev = seg - med
    mad = np.median(np.abs(dev)) * 1.4826
    thr = k * mad if mad > 0 else 1e9
    idx = np.where(np.abs(dev) > max(thr, 3 * np.std(seg)))[0]
    print(f"{name:7s} median {med:7.1f}  MAD-sigma {mad:5.2f}  std {np.std(seg):5.2f}  "
          f"outliers(>max(3σ,3*MAD)) {len(idx)}")
    return dev

dg = spikes(gz, "gz")
dc = spikes(cmd, "cmd")
de = spikes(est, "est")
do = spikes(off, "off")

# cross-correlation lag between channels on the plateau (cmd vs gz, off vs cmd)
def xcorr_lag(a, b, maxlag=30):
    a = a - a.mean()
    b = b - b.mean()
    best, bl = -1e18, 0
    for lag in range(-maxlag, maxlag + 1):
        aa = a[max(0, lag):len(a) + min(0, lag)]
        bb = b[max(0, -lag):len(b) - max(0, lag)]
        n = min(len(aa), len(bb))
        if n < 50:
            continue
        v = np.dot(aa[:n], bb[:n]) / n
        if v > best:
            best, bl = v, lag
    return bl * dt

a_cmd = cmd[plateau]
a_gz = gz[plateau]
a_off = off[plateau]
a_est = est[plateau]
print(f"\nlag(cmd→gz)  {xcorr_lag(a_cmd, a_gz)*1000:+6.0f} ms  (负=cmd 领先)")
print(f"lag(off→cmd) {xcorr_lag(a_off, a_cmd)*1000:+6.0f} ms  (负=off 领先)")
print(f"lag(cmd→est) {xcorr_lag(a_cmd, a_est)*1000:+6.0f} ms  (负=cmd 领先)")

# amplitude of the 3 Hz component per channel
def band_amp(x, f0=3.16, dt=dt):
    n = len(x)
    seg = x - x.mean()
    freqs = np.fft.rfftfreq(n, d=dt)
    spec = np.abs(np.fft.rfft(seg)) * 2 / n
    band = (freqs > f0 * 0.7) & (freqs < f0 * 1.4)
    return spec[band].max() if band.any() else 0

print(f"\n3.16Hz component amplitude: cmd {band_amp(a_cmd):5.1f}  gz {band_amp(a_gz):5.1f}  "
      f"off {band_amp(a_off):5.2f}  est {band_amp(a_est):5.1f} deg(/s)")

# ratio: how much does 1 deg/s of gz ripple become cmd?
print(f"\ngz->cmd gain needed to explain cmd ripple by ff: 0.9 (FF). observed cmd ripple "
      f"{np.std(a_cmd):.1f} vs 0.9*gz ripple {0.9*np.std(a_gz):.1f}")
