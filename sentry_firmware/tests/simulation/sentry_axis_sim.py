"""
Sentry 3-axis + chassis deep simulation (host, no hardware).

Purpose: check whether pitch/small-yaw can track an autoaim world target while
the big yaw and chassis move, using the gains currently in
config/sentry_2026/config_gimbal.h. The plant is a lumped-inertia model with
Coulomb/viscous friction, torque saturation and axis coupling; it is NOT a
validated mechanical model. Treat the numbers as a sanity/robustness screen,
then confirm on the bench.

Run: python tests/simulation/sentry_axis_sim.py
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

D2R = math.pi / 180.0
R2D = 180.0 / math.pi
DT_CTRL = 0.005
SMALL_TRIM_LP = 0.03  # mirrors SENTRY_AUTO_AIM_SMALL_TRIM_LP
DT_PLANT = 0.0005
SUBSTEPS = int(round(DT_CTRL / DT_PLANT))


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def wrap180(deg: float) -> float:
    while deg > 180.0:
        deg -= 360.0
    while deg < -180.0:
        deg += 360.0
    return deg


@dataclass
class PID:
    kp: float
    ki: float = 0.0
    kd: float = 0.0
    maxout: float = 1.0e9
    deadband: float = 0.0
    integral_limit: float = 1.0e9
    output_rc: float = 0.0
    d_on_measure: bool = False
    d_limit: float = 0.0
    iout: float = 0.0
    last_err: float = 0.0
    last_meas: float = 0.0
    last_out: float = 0.0

    def clear(self) -> None:
        self.iout = 0.0
        self.last_err = 0.0
        self.last_out = 0.0

    def step(self, meas: float, ref: float, dt: float) -> float:
        err = ref - meas
        if abs(err) > self.deadband:
            pout = self.kp * err
            iterm = self.ki * err * dt
            dout = self.kd * (self.last_meas - meas) / dt if self.d_on_measure else self.kd * (err - self.last_err) / dt
            if self.d_limit > 0.0:
                dout = clamp(dout, -self.d_limit, self.d_limit)
            # PID_Integral_Limit: drop the new I term if output saturates in the same direction
            if abs(pout + self.iout + dout) > self.maxout and err * self.iout > 0.0:
                iterm = 0.0
            self.iout = clamp(self.iout + iterm, -self.integral_limit, self.integral_limit)
            out = pout + self.iout + dout
            if self.output_rc > 0.0:
                out = out * dt / (self.output_rc + dt) + self.last_out * self.output_rc / (self.output_rc + dt)
            out = clamp(out, -self.maxout, self.maxout)
            self.last_out = out
        else:
            out = 0.0
            iterm = 0.0
        self.last_err = err
        self.last_meas = meas
        return out


@dataclass
class Axis:
    angle: PID
    speed: PID
    kd_near: float
    kd_far: float
    blend_lo: float
    blend_hi: float
    ff_gain: float
    ff_lpf: float
    torque_lpf_rc: float
    last_target: float = 0.0
    ff: float = 0.0
    torque_lpf: float = 0.0
    torque: float = 0.0

    def step(self, angle: float, speed: float, target: float, dt: float) -> float:
        err = abs(target - angle)
        span = self.blend_hi - self.blend_lo
        blend = clamp((err - self.blend_lo) / span, 0.0, 1.0) if span > 0.0 else 1.0
        self.angle.kd = self.kd_near + blend * (self.kd_far - self.kd_near)
        delta = target - self.last_target
        rate = delta / dt if abs(delta) <= self.angle.maxout * dt * 1.1 else 0.0
        self.ff += self.ff_lpf * (rate * self.ff_gain - self.ff)
        angle_out = self.angle.step(angle, target, dt)
        speed_target = clamp(angle_out + self.ff, -self.angle.maxout, self.angle.maxout)
        torque = self.speed.step(speed, speed_target, dt)
        if self.torque_lpf_rc > 0.0:
            self.torque_lpf += dt / (self.torque_lpf_rc + dt) * (torque - self.torque_lpf)
            torque = self.torque_lpf
        self.last_target = target
        self.torque = torque
        return torque


@dataclass
class SmallYaw:
    angle_kp: float
    max_speed: float
    deadband: float
    speed: PID
    code_to_torque: float = 2.4 / 25000.0

    def step(self, joint_deg: float, speed_deg_s: float, target_deg: float) -> float:
        err = target_deg - joint_deg
        err_eff = 0.0 if abs(err) <= self.deadband else err - math.copysign(self.deadband, err)
        ref = clamp(self.angle_kp * err_eff, -self.max_speed, self.max_speed)
        code = self.speed.step(speed_deg_s, ref, DT_CTRL)
        return code * self.code_to_torque


@dataclass
class Smooth:
    """Mirror of cmd_yaw_auto_smooth_update: 2nd-order target shaping."""
    angle: float = 0.0
    rate: float = 0.0
    initialized: bool = False

    def step(self, target: float, initial_angle: float, dt: float) -> float:
        if not self.initialized:
            self.angle = initial_angle
            self.rate = 0.0
            self.initialized = True
            return initial_angle
        err = wrap180(target - self.angle)
        continuous = self.angle + err
        desired = clamp(20.0 * err, -300.0, 300.0)
        dr = clamp(desired - self.rate, -3000.0 * dt, 3000.0 * dt)
        self.rate += dr
        prev = err
        self.angle += self.rate * dt
        err = continuous - self.angle
        if prev * err <= 0.0:
            self.angle = continuous
            self.rate = 0.0
        return self.angle


@dataclass
class Plant:
    # inertias (kg m^2) - lumped estimates, not measured
    j_chassis: float = 1.5
    j_big: float = 0.60
    j_small: float = 0.010
    j_pitch: float = 0.015
    j_couple_bp: float = 0.010
    # friction
    b_big: float = 0.05
    c_big: float = 0.30
    b_small: float = 0.006
    c_small: float = 0.03
    b_pitch: float = 0.020
    c_pitch: float = 0.08
    mgl: float = 0.45  # gravity torque amplitude on pitch, N*m
    torque_scale: float = 1.0
    friction_scale: float = 1.0

    chassis_yaw: float = 0.0
    chassis_rate: float = 0.0
    big: float = 0.0
    big_rate: float = 0.0
    small: float = 0.0
    small_rate: float = 0.0
    pitch: float = 0.0
    pitch_rate: float = 0.0
    platform_pitch: float = 0.0
    platform_rate: float = 0.0

    def step(self, tau_big: float, tau_small: float, tau_pitch: float,
             chassis_rate_cmd: float, platform_pitch: float, platform_rate: float, dt: float) -> None:
        # chassis follows the commanded rate with a first-order lag (drive/tyre compliance)
        alpha_c = clamp((chassis_rate_cmd - self.chassis_rate) / 0.25, -30.0, 30.0)
        self.chassis_rate += alpha_c * dt
        self.chassis_yaw += self.chassis_rate * dt

        self.platform_pitch = platform_pitch
        self.platform_rate = platform_rate

        # DM4310 back-EMF / voltage ceiling: torque falls as joint speed rises.
        tau_big_eff = clamp(tau_big * self.torque_scale - 0.3 * self.big_rate, -10.0, 10.0)
        alpha_b = (tau_big_eff - self.b_big * self.big_rate - self.c_big * self.friction_scale * math.tanh(self.big_rate / 0.02)
                   - self.j_big * alpha_c) / self.j_big
        self.big_rate += alpha_b * dt
        self.big += self.big_rate * dt

        alpha_s = (tau_small - self.b_small * self.small_rate - self.c_small * self.friction_scale * math.tanh(self.small_rate / 0.02)
                   - self.j_small * (alpha_c + alpha_b)) / self.j_small
        self.small_rate += alpha_s * dt
        self.small += self.small_rate * dt

        tau_pitch_eff = clamp(tau_pitch * self.torque_scale - 0.05 * self.pitch_rate, -1.0, 1.0)
        alpha_p = (tau_pitch_eff - self.b_pitch * self.pitch_rate - self.c_pitch * self.friction_scale * math.tanh(self.pitch_rate / 0.02)
                   - self.mgl * math.cos(self.platform_pitch + self.pitch)
                   - self.j_couple_bp * alpha_b) / self.j_pitch
        self.pitch_rate += alpha_p * dt
        self.pitch += self.pitch_rate * dt


def make_axes():
    big = Axis(
        angle=PID(kp=20.0, maxout=150.0 * D2R, deadband=0.02 * D2R, d_on_measure=True, d_limit=30.0 * D2R),
        speed=PID(kp=4.2, ki=0.8, maxout=10.0, integral_limit=3.0, output_rc=0.012),
        kd_near=0.8, kd_far=0.2, blend_lo=4.0 * D2R, blend_hi=8.0 * D2R,
        ff_gain=1.0, ff_lpf=0.3, torque_lpf_rc=0.03)
    pitch = Axis(
        angle=PID(kp=8.0, maxout=150.0 * D2R, deadband=0.3 * D2R, d_on_measure=True, d_limit=30.0 * D2R),
        speed=PID(kp=0.6, ki=0.2, maxout=1.5, integral_limit=1.0, output_rc=0.012),
        kd_near=0.0, kd_far=0.0, blend_lo=1.0, blend_hi=2.0,
        ff_gain=1.0, ff_lpf=0.3, torque_lpf_rc=0.0)
    small = SmallYaw(
        angle_kp=25.0, max_speed=180.0, deadband=0.3,
        speed=PID(kp=50.0, ki=0.0, maxout=16000.0, deadband=3.0, integral_limit=2000.0, output_rc=0.005))
    return big, pitch, small


def run_scenario(name: str, seconds: float, *, chassis_spin_at: float = -1.0,
                 yaw_target_fn=lambda t: 0.0, pitch_world_fn=lambda t: 0.0,
                 platform_fn=lambda t: 0.0, pitch_rate_limit_deg_s: float = 120.0,
                 gravity_ff: bool = False, use_smooth: bool = False,
                 latency_s: float = 0.0, quiet: bool = False, plant_kwargs=None,
                 chassis_rate_fn=None):
    big, pitch_axis, small = make_axes()
    plant = Plant(**(plant_kwargs or {}))
    smooth = Smooth()
    n = int(seconds / DT_CTRL)
    pitch_target_slew = 0.0
    small_trim = 0.0
    yaw_err_max = 0.0
    yaw_err_sum = 0.0
    true_err_max = 0.0
    true_err_sum = 0.0
    pitch_err_max = 0.0
    pitch_err_sum = 0.0
    small_err_max = 0.0
    big_torque_max = 0.0
    pitch_torque_max = 0.0
    small_torque_max = 0.0
    settle_start = None
    world_err_at_end = 0.0
    for k in range(n):
        t = k * DT_CTRL
        spin_cmd = (chassis_rate_fn(t) if chassis_rate_fn is not None else
                    150.0 * D2R if (chassis_spin_at >= 0.0 and t >= chassis_spin_at) else 0.0)
        platform = platform_fn(t) * D2R
        platform_rate = (platform_fn(t + DT_CTRL) - platform_fn(t)) / DT_CTRL * D2R

        # --- controller -------------------------------------------------
        # big yaw controller runs in rad (angle P output is rad/s); allocation in deg.
        big_world_rad = plant.chassis_yaw + plant.big
        big_world = big_world_rad * R2D
        big_world_rate = (plant.chassis_rate + plant.big_rate) * D2R
        gun_world = (plant.chassis_yaw + plant.big + plant.small) * R2D
        raw_target = yaw_target_fn(max(0.0, t - latency_s))
        yaw_target = smooth.step(raw_target, big_world, DT_CTRL) if use_smooth else raw_target
        err = wrap180(yaw_target - big_world)
        # small yaw is a slow trim: it assists transients, big yaw owns the fast loop.
        small_trim += SMALL_TRIM_LP * (clamp(err, -115.0, 115.0) - small_trim)
        small_target = small_trim
        big_target = big_world + wrap180(yaw_target - small_target - big_world)
        tau_big = big.step(big_world_rad, big_world_rate, big_target * D2R, DT_CTRL)

        tau_small = small.step(plant.small * R2D, plant.small_rate * R2D, small_target)

        pitch_world_target = pitch_world_fn(t)
        # joint angle is relative to the horizontal zero (motor position - SENTRY_PITCH_ZERO_RAD)
        pitch_joint_target = (pitch_world_target - platform * R2D) * D2R
        pitch_joint_target = clamp(pitch_joint_target, -20.0 * D2R, 20.0 * D2R)
        # target slew, same 60 deg/s cap as sentry_pitch_task
        step = pitch_rate_limit_deg_s * D2R * DT_CTRL
        pitch_target_slew += clamp(pitch_joint_target - pitch_target_slew, -step, step)
        tau_pitch = pitch_axis.step(plant.pitch, plant.pitch_rate, pitch_target_slew, DT_CTRL)
        if gravity_ff:
            tau_pitch += plant.mgl * math.cos(plant.platform_pitch + plant.pitch)

        # --- plant ------------------------------------------------------
        for _ in range(SUBSTEPS):
            plant.step(tau_big, tau_small, tau_pitch, spin_cmd, platform, platform_rate, DT_PLANT)

        # --- metrics ----------------------------------------------------
        world_err = abs(wrap180((raw_target if (use_smooth or latency_s > 0.0) else yaw_target) - gun_world))
        true_err = abs(wrap180(yaw_target_fn(t) - gun_world)) if latency_s > 0.0 else world_err
        pitch_err = abs(pitch_world_target - (platform * R2D + plant.pitch * R2D))
        small_err = abs(small_target - plant.small * R2D)
        yaw_err_max = max(yaw_err_max, world_err)
        yaw_err_sum += world_err * world_err
        true_err_max = max(true_err_max, true_err)
        true_err_sum += true_err * true_err
        pitch_err_max = max(pitch_err_max, pitch_err)
        pitch_err_sum += pitch_err * pitch_err
        small_err_max = max(small_err_max, small_err)
        big_torque_max = max(big_torque_max, abs(tau_big))
        pitch_torque_max = max(pitch_torque_max, abs(tau_pitch))
        small_torque_max = max(small_torque_max, abs(tau_small))
        if world_err < 1.0 and settle_start is None:
            settle_start = t
        elif world_err >= 1.0:
            settle_start = None
        world_err_at_end = world_err

    if not quiet:
            print(f"{name:34s} yawMax={yaw_err_max:6.2f}° trueMax={true_err_max:5.2f}° yawRMS={math.sqrt(yaw_err_sum/n):5.2f}° "
              f"pitchMax={pitch_err_max:5.2f}° pitchRMS={math.sqrt(pitch_err_sum/n):4.2f}° "
              f"smallMax={small_err_max:5.2f}° Tbig={big_torque_max:5.2f}Nm "
              f"Tpitch={pitch_torque_max:4.2f}Nm Tsmall={small_torque_max:4.2f}Nm "
              f"settle<1°={settle_start if settle_start is not None else -1:5.2f}s endErr={world_err_at_end:4.2f}°")
    return {
        "yaw_max": yaw_err_max, "true_max": true_err_max, "true_rms": math.sqrt(true_err_sum/n),
        "pitch_max": pitch_err_max, "small_max": small_err_max,
        "big_torque": big_torque_max, "pitch_torque": pitch_torque_max,
        "yaw_rms": math.sqrt(yaw_err_sum/n), "true_max": true_err_max,
        "true_rms": math.sqrt(true_err_sum/n), "pitch_rms": math.sqrt(pitch_err_sum/n),
        "small_torque": small_torque_max, "end_err": world_err_at_end,
    }


def main() -> None:
    print("Sentry axis simulation (lumped plant; gains from config/sentry_2026)")
    run_scenario("pitch step 0->10deg", 2.0, pitch_world_fn=lambda t: 10.0 if t > 0.2 else 0.0)
    run_scenario("platform 3deg 1Hz, pitch=0", 3.0, platform_fn=lambda t: 3.0 * math.sin(2 * math.pi * 1.0 * t))
    run_scenario("platform 3deg 1Hz + gravity FF", 3.0, platform_fn=lambda t: 3.0 * math.sin(2 * math.pi * 1.0 * t), gravity_ff=True)
    run_scenario("small yaw step 0->60deg", 2.0, yaw_target_fn=lambda t: 60.0 if t > 0.2 else 0.0)
    run_scenario("autoaim step 0->140deg", 2.0, yaw_target_fn=lambda t: 140.0 if t > 0.2 else 0.0, use_smooth=True, latency_s=0.02)
    run_scenario("spin 150dps, hold 0deg", 3.0, chassis_spin_at=0.2)
    run_scenario("spin + yaw 0.2Hz 20deg", 5.0, chassis_spin_at=0.2,
                 yaw_target_fn=lambda t: 20.0 * math.sin(2 * math.pi * 0.2 * max(0.0, t - 0.2)),
                 use_smooth=True, latency_s=0.02)
    run_scenario("spin + yaw sine 30deg 1Hz", 4.0, chassis_spin_at=0.2,
                 yaw_target_fn=lambda t: 30.0 * math.sin(2 * math.pi * 1.0 * max(0.0, t - 0.2)),
                 use_smooth=True, latency_s=0.02)
    run_scenario("nav wz 0.5rad/s + yaw 20deg", 4.0,
                 chassis_rate_fn=lambda t: 0.5 if t > 0.2 else 0.0,
                 yaw_target_fn=lambda t: 20.0 if t > 0.5 else 0.0,
                 use_smooth=True, latency_s=0.02)
    run_scenario("combined spin+yaw+pitch", 4.0, chassis_spin_at=0.2,
                 yaw_target_fn=lambda t: 40.0 * math.sin(2 * math.pi * 0.8 * max(0.0, t - 0.2)),
                 pitch_world_fn=lambda t: 8.0 if t > 0.5 else 0.0,
                 platform_fn=lambda t: 2.0 * math.sin(2 * math.pi * 1.2 * t),
                 use_smooth=True, latency_s=0.02)


    print("envelope: J_big=0.6, Tmax=10Nm -> 16.7 rad/s^2; 1Hz/30deg needs 20.7 -> torque-limited; 0.2Hz/20deg passes.")
    print("pitch: gravity FF is off by default; bench-calibrate SENTRY_PITCH_GRAVITY_A/B.")
    sweep()

def sweep() -> None:
    """Coarse robustness grid: inertia / torque / friction are estimates, not measured."""
    print("Sensitivity (J_big x torque_scale x friction_scale):")
    for jb in (0.4, 0.6, 0.8):
        for tq in (0.8, 1.0, 1.2):
            for fr in (0.7, 1.0, 1.5):
                kw = {"j_big": jb, "torque_scale": tq, "friction_scale": fr}
                y = run_scenario("spin+yaw20", 4.0, chassis_spin_at=0.2,
                                 yaw_target_fn=lambda t: 20.0 * math.sin(2 * math.pi * 0.2 * max(0.0, t - 0.2)),
                                 use_smooth=True, latency_s=0.02, quiet=True, plant_kwargs=kw)
                p = run_scenario("pitch plat", 3.0,
                                 platform_fn=lambda t: 3.0 * math.sin(2 * math.pi * 1.0 * t),
                                 quiet=True, plant_kwargs=kw)
                print(f"  J={jb:.1f} T={tq:.1f} F={fr:.1f}: "
                      f"yawMax={y['yaw_max']:5.1f} yawRMS={y['yaw_rms']:4.1f} "
                      f"pitchMax={p['pitch_max']:4.1f} pitchRMS={p['pitch_rms']:3.1f} "
                      f"Tbig={y['big_torque']:4.1f}")
    print("  note: pass if spin+0.2Hz20 yawMax < 8 deg and pitch platform max < 6 deg across the grid.")


if __name__ == "__main__":
    main()
