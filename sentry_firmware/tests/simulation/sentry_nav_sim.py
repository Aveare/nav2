"""
Sentry NV v1 navigation simulation (host, no hardware).

Checks the chain: NUC command -> cmd gating -> nav ramp -> mecanum inverse ->
wheel rotor speed / 7000 rpm limit -> stop on timeout. The plant is kinematic
(ramp + ideal wheel tracking), not a tyre/force model; use it to catch sign,
scale and limit errors before the bench.

Run: python tests/simulation/sentry_nav_sim.py
"""
from __future__ import annotations

import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
D2R = math.pi / 180.0
DT = 0.005


def macro(path: Path, name: str) -> float:
    text = path.read_text(encoding="utf-8")
    m = re.search(r"^#define\s+" + re.escape(name) + r"\s+([^\s/]+)", text, re.M)
    if not m:
        raise SystemExit(f"macro {name} not found in {path}")
    return float(m.group(1).rstrip("fFuU"))


CFG_C = ROOT / "config/sentry_2026/config_chassis.h"
CFG_N = ROOT / "config/sentry_2026/config_autonomy.h"

WHEEL_BASE = macro(CFG_C, "CHASSIS_WHEEL_BASE")
TRACK = macro(CFG_C, "CHASSIS_TRACK_WIDTH")
WHEEL_R = macro(CFG_C, "CHASSIS_WHEEL_RADIUS")
REDUCTION = macro(CFG_C, "CHASSIS_WHEEL_REDUCTION")
MAX_RPM = macro(CFG_C, "CHASSIS_MAX_MOTOR_RPM")
MAX_SPEED = macro(CFG_C, "CHASSIS_MAX_SPEED")
NAV_ACCEL = macro(CFG_C, "CHASSIS_VX_RAMP")
NAV_LIMIT_M_S = macro(CFG_N, "SENTRY_NAVIGATION_MAX_SPEED_M_S")
NAV_WZ_LIMIT = macro(CFG_N, "SENTRY_NAVIGATION_MAX_WZ_RAD_S")
NAV_RAMP = macro(CFG_N, "SENTRY_NAVIGATION_ACCEL_MM_S2")

WHEEL_RATIO = 360.0 * REDUCTION / (2.0 * math.pi * WHEEL_R)  # rotor deg per mm
HALF = (WHEEL_BASE / 2.0 + TRACK / 2.0) * D2R                   # mm per (deg/s)


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def ramp(cur, target, accel, dt):
    step = accel * dt
    d = target - cur
    return cur + clamp(d, -step, step)


def wheel_speeds(vx, vy, wz_dps):
    """Sentry mecanum inverse; returns rotor deg/s LF/RF/LB/RB."""
    lf = (-vx - vy - wz_dps * HALF) * 0.70710678 * WHEEL_RATIO
    rf = (vx - vy - wz_dps * HALF) * 0.70710678 * WHEEL_RATIO
    lb = (-vx + vy - wz_dps * HALF) * 0.70710678 * WHEEL_RATIO
    rb = (vx + vy - wz_dps * HALF) * 0.70710678 * WHEEL_RATIO
    return lf, rf, lb, rb


def simulate(name, vx_m_s, vy_m_s, wz_rad_s, duration=2.5, cmd_until=1.0,
             gate=True):
    # ROS base_link -> STM32 chassis frame (see autonomy.c)
    vx_t = vx_m_s * 1000.0 if gate else 0.0
    vy_t = -vy_m_s * 1000.0 if gate else 0.0
    wz_t = wz_rad_s / D2R if gate else 0.0

    vx = vy = wz = 0.0
    peak_rpm = 0.0
    peak_vx = peak_vy = peak_wz = 0.0
    t90 = -1.0
    stop_t = -1.0
    n = int(duration / DT)
    for k in range(n):
        t = k * DT
        active = gate and t < cmd_until
        vx = ramp(vx, vx_t if active else 0.0, NAV_RAMP, DT)
        vy = ramp(vy, vy_t if active else 0.0, NAV_RAMP, DT)
        wz = ramp(wz, wz_t if active else 0.0, NAV_RAMP, DT)
        w = wheel_speeds(vx, vy, wz)
        rpm = [abs(x) / 6.0 for x in w]
        peak_rpm = max(peak_rpm, max(rpm))
        peak_vx = max(peak_vx, abs(vx)); peak_vy = max(peak_vy, abs(vy)); peak_wz = max(peak_wz, abs(wz))
        # equal-scale limit, same as chassis_limit_output
        if max(rpm) > MAX_RPM:
            s = MAX_RPM / max(rpm)
            w = tuple(x * s for x in w)
            vx *= s; vy *= s; wz *= s
        speed = math.hypot(vx, vy)
        target = math.hypot(vx_t, vy_t)
        if t90 < 0.0 and target > 1.0 and speed >= 0.9 * target:
            t90 = t
        if not active and stop_t < 0.0 and speed < 1.0 and abs(wz) < 1.0:
            stop_t = t
    delay = (stop_t - cmd_until) if stop_t >= 0.0 else -1.0
    print(f"{name:34s} peakVx={peak_vx:7.1f} peakVy={peak_vy:7.1f} peakWz={peak_wz:6.1f}dps "
          f"peakRPM={peak_rpm:6.0f} t90={t90:4.2f}s stopDelay={delay:4.2f}s")
    return peak_rpm, t90, stop_t


def main():
    print(f"Sentry nav sim: wheelbase={WHEEL_BASE:.0f} track={TRACK:.0f} "
          f"wheelR={WHEEL_R:.0f} ratio={WHEEL_RATIO:.3f} deg/mm "
          f"maxRPM={MAX_RPM:.0f} navAccel={NAV_RAMP:.0f}mm/s^2")
    assert NAV_LIMIT_M_S <= MAX_SPEED / 1000.0
    assert NAV_WZ_LIMIT <= 1.0
    simulate("NUC forward 0.35 m/s", 0.35, 0.0, 0.0)
    simulate("NUC left 0.35 m/s", 0.0, 0.35, 0.0)
    simulate("NUC ccw 0.5 rad/s", 0.0, 0.0, 0.5)
    simulate("NUC diagonal 0.35/0.35", 0.35, 0.35, 0.0)
    simulate("NUC max + spin", NAV_LIMIT_M_S, NAV_LIMIT_M_S, NAV_WZ_LIMIT)
    simulate("NUC timeout stop", 0.35, 0.0, 0.0, cmd_until=0.8)
    simulate("gate closed (manual)", 0.35, 0.0, 0.0, gate=False)

    # sign checks: forward should make RF/RB positive and LF/LB negative in this frame
    wf = wheel_speeds(350.0, 0.0, 0.0)
    wl = wheel_speeds(0.0, -350.0, 0.0)  # STM32 +vy = right; ROS left -> -vy
    wc = wheel_speeds(0.0, 0.0, 28.65)   # ccw positive
    print("sign forward LF,RF,LB,RB:", [round(x / 6.0) for x in wf])
    print("sign left    LF,RF,LB,RB:", [round(x / 6.0) for x in wl])
    print("sign ccw     LF,RF,LB,RB:", [round(x / 6.0) for x in wc])
    print("note: peakRPM must stay < 7000; timeout stop should be < 0.2 s after command loss.")


if __name__ == "__main__":
    main()
