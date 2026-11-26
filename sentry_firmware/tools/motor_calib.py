#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""电机速度环离线标定:从 trace CSV 拟合一阶模型并给 PID/前馈初值。

方法(公开实践:系统辨识的一阶响应法 + 稳态增益法,详见
  .Doc/guide/调参诊断手册.md §标定):
  输入列:t(可选), target(速度目标), speed(实测速度), u(输出:电流码或 N·m)
  1) 稳态增益 K  = Δspeed / Δu,取"目标恒定且速度变化 < 容差"的稳态段中位数;
  2) 时间常数 τ:阶跃后 speed 首次达到 63.2%·Δspeed 的时间;
  3) 死区/噪声:静止段速度标准差 σ 与输出波动带;
  4) 输出建议:Kp ≈ 1/K·阻尼系数(默认 0.7,仅量级参考),Ki ≈ Kp/τ·0.5,
     并附 [标定脚本] 证据标记 —— 是否采纳仍以实车单变量验证为准。

用法:
  python tools/motor_calib.py build/trace.csv --target vt_lf --speed motor_lf->measure.speed_aps --u motor_lf->measure.real_current
列名用 --list 先看表头。退出码:0=给出结果;2=无法拟合。
"""
import argparse
import csv
import statistics
import sys


def load(path):
    with open(path, encoding="utf-8", errors="replace", newline="") as f:
        rows = [r for r in csv.reader(f) if r]
    if len(rows) < 3:
        raise SystemExit("FAIL: 样本不足")
    header = rows[0]
    data = []
    for r in rows[1:]:
        try:
            data.append([float(x) for x in r])
        except ValueError:
            continue  # 跳过 label 列所在的行
    return header, data


def col(header, data, name):
    if name not in header:
        print(f"FAIL: 无列 {name}(表头: {header})")
        raise SystemExit(2)
    i = header.index(name)
    return [r[i] for r in data]


def steady_gain(target, speed, u, tol):
    """取目标恒定段(Δtarget<tol)的 Δspeed/Δu 中位数"""
    gains = []
    n = len(target)
    for i in range(1, n):
        if abs(target[i] - target[i - 1]) < tol and abs(u[i] - u[i - 1]) > 1e-6:
            g = (speed[i] - speed[i - 1]) / (u[i] - u[i - 1])
            gains.append(g)
    if not gains:
        return None
    return statistics.median(gains)


def time_constant(target, speed, tol):
    """找最大阶跃的 63.2% 响应时间(采样间隔按行距近似)"""
    best = None
    for i in range(1, len(target)):
        if abs(target[i] - target[i - 1]) >= tol:
            step_t, step_s = target[i], speed[i]
            final = None
            for j in range(i, len(target)):
                if abs(target[j] - step_t) > tol:
                    break
                final = speed[j]
            if final is None:
                continue
            delta = abs(final - step_s)
            if delta < 1e-6:
                continue
            thresh = step_s + 0.632 * (final - step_s)
            for j in range(i, len(target)):
                if (speed[j] - thresh) * (final - step_s) >= 0:
                    best = (j - i, step_t, delta)
                    break
            break
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--target", required=True)
    ap.add_argument("--speed", required=True)
    ap.add_argument("--u", required=True, help="输出列:电流码或 N·m(需与增益方向一致)")
    ap.add_argument("--step-tol", type=float, default=1.0, help="目标判稳阈差(单位同列)")
    ap.add_argument("--damping", type=float, default=0.7, help="期望阻尼比(0.6~0.9)")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    header, data = load(args.csv)
    if args.list:
        print("表头:", header)
        return 0

    tgt = col(header, data, args.target)
    spd = col(header, data, args.speed)
    u = col(header, data, args.u)

    K = steady_gain(tgt, spd, u, args.step_tol)
    if K is None or abs(K) < 1e-9:
        print("FAIL: 未找到有效的稳态段(检查 --step-tol 或加大激励)")
        return 2
    tc = time_constant(tgt, spd, args.step_tol)

    # 静止段噪声:目标≈0 段的速度标准差
    quiet = [s for s, t in zip(spd, tgt) if abs(t) < args.step_tol]
    noise = statistics.pstdev(quiet) if len(quiet) > 5 else None

    print("=== 电机速度环离线标定 ===")
    print(f"样本 {len(data)} 行;稳态增益 K = {K:.4f} (speed单位/u单位)")
    if tc:
        n, step, delta = tc
        print(f"时间常数 τ ≈ {n} 个采样周期(阶跃目标 {step:.2f},Δspeed {delta:.2f})")
    else:
        print("时间常数 τ:未捕获完整阶跃(τ 建议先用 3×采样周期试)")
    if noise is not None:
        print(f"静止噪声 σ ≈ {noise:.4f}(死区建议 ≥ 2σ)")

    kp = args.damping / K if K > 0 else -args.damping / K
    tau_est = max(tc[0], 3) if tc else 3
    ki = kp / tau_est * 0.5
    print("\n建议初值([标定脚本],需实车单变量复核):")
    print(f"  speed_Kp ≈ {kp:.4g}")
    print(f"  speed_Ki ≈ {ki:.4g}")
    print("  积分限幅:先按 0.5×MaxOut,实车回调")
    return 0


if __name__ == "__main__":
    sys.exit(main())
