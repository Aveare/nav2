"""
Infantry chassis power-control simulation (host, no hardware).

Reproduces the exact controller arithmetic from apps/infantry/chassis/chassis_power.c
and config/infantry_2026/config_chassis.h:
  P_motor = K0 + K1|i| + K2|w| + K3|i||w| + K4 i^2 + K5 w^2
  P_total = SCALE * sum(motoring motors) + BIAS   (braking wheels excluded)
  limit   = min(referee 120, LIMIT_MAX) - MARGIN, then buffer attenuation
  s_raw   = larger root of S2 s^2 + S1 s + (S0 - limit) = 0, clamped [0,1]
  s       = fast-down / slow-up (LP), then max(SCALE_MIN, MIN_SCALE)

DEV-20261127-200745: braking wheels (i and w opposite signs) no longer contribute
to the predicted power. Section [5] compares the legacy |i||w| behaviour with the
new motoring-only behaviour and asserts the controller invariants.

The plant (tyre/chassis) is NOT modelled: the sweep answers "does the controller
hold the limit for a given (current command, wheel speed) pair", not "how fast
does the robot accelerate". Use it to find the floor-bind region before bench.

Run: python tests/simulation/chassis_power_sim.py
"""
from __future__ import annotations

import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CFG = ROOT / "config/infantry_2026/config_chassis.h"


def macro(name: str) -> float:
    text = CFG.read_text(encoding="utf-8")
    m = re.search(r"^#define\s+" + re.escape(name) + r"\s+([^\s/]+)", text, re.M)
    if not m:
        raise SystemExit(f"macro {name} not found in {CFG}")
    raw = m.group(1).strip().strip("()").rstrip("fFuU")
    return float(raw)


K0, K1, K2, K3, K4, K5 = (macro(f"CHASSIS_POWER_MODEL_K{k}") for k in range(6))
SCALE = macro("CHASSIS_POWER_MODEL_SCALE")
BIAS = macro("CHASSIS_POWER_MODEL_BIAS")
IDIV = macro("CHASSIS_POWER_CURRENT_DIV")
LIMIT_MAX = macro("CHASSIS_POWER_LIMIT_MAX")
MARGIN = macro("CHASSIS_POWER_MARGIN_W")
MIN_SCALE = macro("CHASSIS_POWER_MIN_SCALE")
MODEL_FLOOR = macro("CHASSIS_POWER_SCALE_MIN")
SPEED_MAXOUT = macro("CHASSIS_SPEED_PID_MAXOUT")
CLIMIT = min(120.0, LIMIT_MAX) - MARGIN  # referee 120W clamped, no buffer attenuation
MAX_RPM = macro("CHASSIS_MAX_MOTOR_RPM")
WHEEL_RPM_MAX = macro("CHASSIS_SPIN_WHEEL_RPM_MAX")
VMAX = macro("CHASSIS_MAX_SPEED")
SPIN_MAX = macro("CHASSIS_ROTATE_SPEED")
SPIN_HIGH = macro("CHASSIS_SPIN_HIGH_DPS")
TRANS_PER_MM = 10.26  # rotor deg/s per mm/s (45 deg mecanum)
SPIN_PER_DPS = 86.0   # rotor deg/s per deg/s


def coeffs(i_raw: float, w_dps: float) -> tuple[float, float, float]:
    i = abs(i_raw) / IDIV
    w = abs(w_dps) / 6.0
    c0 = K0 + K2 * w + K5 * w * w
    c1 = (K1 + K3 * w) * i
    c2 = K4 * i * i
    return c0, c1, c2


def predict(i_list: list[float], w_list: list[float], limit: float,
            include_braking: bool = False):
    s0 = s1 = s2 = 0.0
    for i_raw, w_dps in zip(i_list, w_list):
        # 制动轮(i 与 w 反号)回馈功率为负,不占电池预算;include_braking=True
        # 复刻 DEV-20261127-200745 之前的 |i||w| 口径,仅供对照。
        if not include_braking and i_raw * w_dps < 0.0:
            continue
        c0, c1, c2 = coeffs(i_raw, w_dps)
        s0 += c0
        s1 += c1
        s2 += c2
    s0 = SCALE * s0 + BIAS
    s1 *= SCALE
    s2 *= SCALE
    p_full = s0 + s1 + s2
    if p_full <= limit:
        return p_full, 1.0, p_full
    a, b, c = s2, s1, s0 - limit
    if abs(a) < 1e-9:
        s_raw = 0.0 if abs(b) < 1e-9 else max(0.0, min(1.0, -c / b))
    else:
        disc = b * b - 4.0 * a * c
        if disc < 0.0:
            s_raw = 0.0
        else:
            sq = math.sqrt(disc)
            roots = [(-b - sq) / (2 * a), (-b + sq) / (2 * a)]
            roots = [r for r in roots if 0.0 <= r <= 1.0]
            s_raw = max(roots) if roots else 0.0
    s = max(s_raw, MODEL_FLOOR, MIN_SCALE)
    p_post = s0 + s1 * s + s2 * s * s
    return p_full, s, p_post


def wheels(v: float, wz: float, diagonal: bool = False) -> list[float]:
    """Four rotor speeds (deg/s) for pure translation + rotation."""
    if diagonal:
        trans = (abs(v) * 0.7071 + abs(v) * 0.7071) * 0.7071 * 14.515
    else:
        trans = abs(v) * TRANS_PER_MM
    spin = abs(wz) * SPIN_PER_DPS
    return [trans + spin, trans - spin, trans - spin, trans + spin]


def report_point(name: str, i_cmd: float, w_list: list[float], limit: float = CLIMIT):
    p_full, s, p_post = predict([i_cmd] * 4, w_list, limit)
    flag = "FLOOR" if s <= MIN_SCALE + 1e-9 and p_full > limit else ""
    print(
        f"  {name:<34} i={i_cmd:6.0f} w={max(abs(x) for x in w_list):6.0f}dps "
        f"P_full={p_full:7.1f}W s={s:5.3f} P_post={p_post:7.1f}W {flag}"
    )
    return p_full, s, p_post


def floor_boundary(w_dps: float, limit: float = CLIMIT) -> float | None:
    """Lowest i_cmd whose required scale falls to the floor."""
    lo, hi = 0.0, SPEED_MAXOUT
    if predict([hi] * 4, [w_dps] * 4, limit)[1] > MIN_SCALE:
        return None
    for _ in range(40):
        mid = (lo + hi) / 2
        if predict([mid] * 4, [w_dps] * 4, limit)[1] > MIN_SCALE:
            lo = mid
        else:
            hi = mid
    return hi


def main() -> None:
    print("=" * 78)
    print(f"config: limit = min(120, {LIMIT_MAX:g}) - {MARGIN:g} = {CLIMIT:g} W")
    print(f"        floor = max({MODEL_FLOOR:g}, {MIN_SCALE:g}) = {max(MODEL_FLOOR, MIN_SCALE):g}")
    print(f"        wheel caps: hard {MAX_RPM:g} rpm, spin {WHEEL_RPM_MAX:g} rpm")
    print(f"        targets: pure v {VMAX:g} mm/s, spin {SPIN_MAX:g} deg/s, "
          f"spin band high {SPIN_HIGH:g}")
    print("=" * 78)

    print("\n[1] steady operating points (i_ss from the measured-point inversion)")
    # model unit i = code/1000: 0.80 (800 code) @1500 mm/s ~= 32W measured;
    # 0.43 (430 code) @390 deg/s ~= 52.5W measured.
    report_point("pure translation 2000", 800, wheels(VMAX, 0))
    report_point("pure translation 2000 (2x i)", 1600, wheels(VMAX, 0))
    report_point("pure spin 520", 430, wheels(0, SPIN_MAX))
    report_point("pure spin 520 (2x i)", 860, wheels(0, SPIN_MAX))
    report_point("combined 2000 + 285", 800, wheels(VMAX, 285))
    report_point("combined 1500 + 344", 800, wheels(1500, 344))
    report_point("combined 2000 + 285 diagonal", 800, wheels(VMAX, 285, True))

    print("\n[2] saturated-current stress (worst case: speed loop at MaxOut)")
    for w_rpm in (0, 500, 1000, 2000, 3000, 4000, 5590, 7453):
        report_point(f"i=10000 @ {w_rpm} rpm", SPEED_MAXOUT, [w_rpm * 6.0] * 4)
    for i in (4000, 6000, 8000, 10000):
        report_point(f"i={i} @ 7453 rpm (spin 520)", i, [7453 * 6.0] * 4)

    print("\n[3] floor-bind boundary (lowest i_cmd that hits MIN_SCALE)")
    print("  wheel rpm :  i_cmd at floor")
    for w_rpm in range(0, 7600, 500):
        b = floor_boundary(w_rpm * 6.0)
        print(f"  {w_rpm:5d}     :  {'never' if b is None else f'{b:7.0f}'}")

    print("\n[4] 88W vs 85W target at the same transient points")
    for i, w_rpm in ((10000, 2000), (10000, 3000), (8000, 7453), (6000, 3000)):
        _, s88, p88 = predict([i] * 4, [w_rpm * 6.0] * 4, CLIMIT)
        _, s85, p85 = predict([i] * 4, [w_rpm * 6.0] * 4, 85.0)
        print(f"  i={i:5.0f} @ {w_rpm:5.0f}rpm: 88W s={s88:.3f} po={p88:6.1f} | "
              f"85W s={s85:.3f} po={p85:6.1f}")

    print("\n[5] braking exclusion (DEV-20261127-200745)")
    print("      v mm/s    rpm | legacy s   legacy P | new s    new P")
    fails: list[str] = []
    for v in (500, 1000, 1500, 2000, 2500, 3000):
        w = v * 1.71 * 6.0
        i_brake = [-SPEED_MAXOUT] * 4
        p_old, s_old, _ = predict(i_brake, [w] * 4, CLIMIT, include_braking=True)
        p_new, s_new, _ = predict(i_brake, [w] * 4, CLIMIT)
        print(f"  {v:9d} {v * 1.71:6.0f} | {s_old:8.3f} {p_old:10.1f} | "
              f"{s_new:6.3f} {p_new:8.1f}")
        if abs(s_new - 1.0) > 1e-6:
            fails.append(f"pure brake {v}mm/s: s={s_new:.4f} != 1.0")
        if p_new > 1e-6:
            fails.append(f"pure brake {v}mm/s: P={p_new:.2f} != 0")

    # 混合:刹车带转向,两轮电动(满指令)、两轮制动 @ 2000rpm。
    # 电动子集应解出 0.2~1.0 的 scale 且限幅后不超 88W;旧口径会把制动轮也算进去。
    i_mix = [SPEED_MAXOUT, SPEED_MAXOUT, -SPEED_MAXOUT, -SPEED_MAXOUT]
    w_mix = [2000 * 6.0] * 4
    p_old, s_old, _ = predict(i_mix, w_mix, CLIMIT, include_braking=True)
    p_new, s_new, p_post = predict(i_mix, w_mix, CLIMIT)
    print(f"  mixed 2mot+2brk @2000rpm: legacy s={s_old:.3f} P={p_old:6.1f} | "
          f"new s={s_new:.3f} P={p_new:6.1f} post={p_post:6.1f}")
    if p_post > CLIMIT + 1e-6:
        fails.append(f"mixed: p_post={p_post:.2f} > {CLIMIT}")
    if s_new < s_old - 1e-6:
        fails.append("mixed: new scale must be >= legacy")
    if not (MIN_SCALE < s_new < 1.0):
        fails.append(f"mixed: s={s_new:.3f} not in ({MIN_SCALE}, 1.0)")

    # 电动工况不能变:2000mm/s 平移 @800 码仍为基准 43.0W / s=1.0
    p_tr, s_tr, _ = predict([800.0] * 4, wheels(VMAX, 0), CLIMIT)
    print(f"  motoring 2000mm/s i=800: P={p_tr:.1f} s={s_tr:.3f}")
    if abs(p_tr - 43.0) > 0.2 or abs(s_tr - 1.0) > 1e-6:
        fails.append(f"motoring baseline changed: P={p_tr:.1f} s={s_tr:.3f}")

    if fails:
        print("[5] FAIL")
        for reason in fails:
            print("  - " + reason)
        raise SystemExit(1)
    print("[5] PASS: 全轮制动 scale=1、混合不超限、电动基准不变")


if __name__ == "__main__":
    main()
