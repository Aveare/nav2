#!/usr/bin/env python3
"""底盘功率模型拟合:稳态点 CSV(主路径)或 VOFA JustFloat(旧路径)-> K0..K5。

模型(与 chassis_power.c 完全一致,取绝对值):
    P_i = K0 + K1*|i| + K2*|w| + K3*|i|*|w| + K4*i^2 + K5*w^2
    i = 电流指令原始码 / 1000
    w = 转子转速 deg/s / 6 (rpm)
四轮总功率:
    P_total = 4*K0 + K1*sum|i| + K2*sum|w| + K3*sum|i|*|w|
              + K4*sum(i^2) + K5*sum(w^2)

用法:
    python tools/fit_power_model.py steady.csv
    python tools/fit_power_model.py vofa.bin --vofa
    python tools/fit_power_model.py steady.csv --out build/power_model_snippet.h

稳态点 CSV(推荐):
    单电机台架: i,w,p
    整车四轮:   i0,w0,i1,w1,i2,w2,i3,w3,p
    可选列:     s  —— 功率衰减系数;s<0.99 的行丢弃(限幅生效时指令电流 != 实际电流)
    单位:       i=电流指令原始码,w=转子 deg/s,p=真实功率 W
    真实功率来源:裁判客户端显示的底盘功率,或台架电源/功率计。
    一个稳态工况一行:保持 5~10s,对 [pwri]/[pwr] 日志取平均,再填客户端功率。

VOFA 旧路径:
    25 通道 JustFloat .bin 或 25 列 CSV,通道顺序见 tools/vofa_capture.py。
    2026 协议已删除 0x0202 功率反馈,通道 22 恒 0,该路径只能用于旧固件;
    新固件请用稳态点 CSV。

注意:
- 拟合时功率上限应足够高(或关闭功率控制),保证 s=1,否则样本会被丢弃。
- 拟合结果只对同型号电机、同供电/采样链路有效;换车/换电调要重录。
- 纯标准库,不依赖 numpy;--plot 才需要 matplotlib。
"""

import argparse
import csv
import math
import struct
import sys
from pathlib import Path

FRAME_MAGIC = b"\x00\x00\x80\x7f"
CHANNELS = 25
FRAME_BYTES = CHANNELS * 4 + len(FRAME_MAGIC)

CURRENT_DIV = 1000.0   # 与 chassis_power.c 的 CHASSIS_POWER_CURRENT_DIV 一致
DEG_S_PER_RPM = 6.0    # deg/s -> rpm
COEF_NAMES = ["K0", "K1", "K2", "K3", "K4", "K5"]


def norm_key(k):
    return k.strip().lower().replace(" ", "_")


def pick(row, names):
    for n in names:
        if n in row:
            return row[n]
    return None


def read_bin(path):
    raw = Path(path).read_bytes()
    rows = []
    i = 0
    while i + FRAME_BYTES <= len(raw):
        if raw[i + CHANNELS * 4 : i + FRAME_BYTES] != FRAME_MAGIC:
            i += 1
            continue
        rows.append(list(struct.unpack_from("<%df" % CHANNELS, raw, i)))
        i += FRAME_BYTES
    if not rows:
        raise SystemExit("没有解析到 JustFloat 帧:确认 VOFA 是 JustFloat 模式、25 通道")
    return rows


def read_vofa_csv(path):
    rows = []
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.replace(";", ",").split(",")
            if len(parts) < CHANNELS:
                continue
            try:
                vals = [float(x) for x in parts[:CHANNELS]]
            except ValueError:
                continue  # 表头
            rows.append(vals)
    if not rows:
        raise SystemExit("CSV 没有有效数据行:确认至少 25 列数值")
    return rows


def group_by_power(rows, min_samples, power_eps):
    """VOFA 路径:按 chassis_power 变化分组,组内平均。"""
    groups = []
    current = []
    last_power = None
    for r in rows:
        p = r[22]
        if not math.isfinite(p) or p <= 0.0:
            if len(current) >= min_samples:
                groups.append(current)
            current = []
            last_power = None
            continue
        if last_power is None or abs(p - last_power) > power_eps:
            if len(current) >= min_samples:
                groups.append(current)
            current = [r]
            last_power = p
        else:
            current.append(r)
    if len(current) >= min_samples:
        groups.append(current)
    return groups


def solve6(a, b):
    """6x6 高斯消元(部分主元),失败返回 None。"""
    n = 6
    m = [list(a[i]) + [b[i]] for i in range(n)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(m[r][col]))
        if abs(m[piv][col]) < 1e-12:
            return None
        m[col], m[piv] = m[piv], m[col]
        for r in range(col + 1, n):
            f = m[r][col] / m[col][col]
            for c in range(col, n + 1):
                m[r][c] -= f * m[col][c]
    x = [0.0] * n
    for r in range(n - 1, -1, -1):
        s = m[r][n] - sum(m[r][c] * x[c] for c in range(r + 1, n))
        x[r] = s / m[r][r]
    return x


def features_from_four(i_raw, w_dps):
    """四轮 -> 总功率特征向量 [1(×4), sum|i|, sum|w|, sum|i||w|, sum i^2, sum w^2]。"""
    i = [abs(x) / CURRENT_DIV for x in i_raw]
    w = [abs(x) / DEG_S_PER_RPM for x in w_dps]
    return [
        4.0,
        sum(i),
        sum(w),
        sum(i[k] * w[k] for k in range(4)),
        sum(x * x for x in i),
        sum(x * x for x in w),
    ]


def read_steady_csv(path):
    """稳态点 CSV: 一行一个工况(单电机 i,w,p 或四轮 i0..w3,p)。"""
    samples = []
    skipped_s = 0
    with open(path, "r", encoding="utf-8-sig", newline="") as fh:
        reader = csv.DictReader(fh)
        if reader.fieldnames is None:
            raise SystemExit("CSV 没有表头")
        reader.fieldnames = [norm_key(k) for k in reader.fieldnames]
        for line_no, raw in enumerate(reader, 2):
            row = {norm_key(k): v for k, v in raw.items() if k is not None}
            if any(v is None or str(v).strip() == "" for v in row.values()):
                continue
            s_raw = pick(row, ["s", "scale"])
            if s_raw is not None and float(s_raw) < 0.99:
                skipped_s += 1
                continue
            p_raw = pick(row, ["p", "power", "power_w"])
            if p_raw is None:
                raise SystemExit(f"第 {line_no} 行缺少功率列 p")
            power = float(p_raw)
            if all(f"i{k}" in row and f"w{k}" in row for k in range(4)):
                feat = features_from_four(
                    [float(row[f"i{k}"]) for k in range(4)],
                    [float(row[f"w{k}"]) for k in range(4)],
                )
            else:
                i_raw = pick(row, ["i", "current", "current_raw"])
                w_raw = pick(row, ["w", "speed", "speed_dps"])
                if i_raw is None or w_raw is None:
                    raise SystemExit(f"第 {line_no} 行需要 i/w 或 i0..i3/w0..w3")
                # 单电机台架:四轮同型,系数仍按单电机口径(K0 只乘 1)
                i_m = abs(float(i_raw)) / CURRENT_DIV
                w_m = abs(float(w_raw)) / DEG_S_PER_RPM
                feat = [1.0, i_m, w_m, i_m * w_m, i_m * i_m, w_m * w_m]
            samples.append((feat, power))
    return samples, skipped_s


def build_vofa_samples(groups):
    samples = []
    for g in groups:
        n = float(len(g))
        f = [0.0] * 6
        for r in g:
            i_raw = [r[18 + k] for k in range(4)]
            w_dps = [r[14 + k] for k in range(4)]
            ff = features_from_four(i_raw, w_dps)
            for a in range(6):
                f[a] += ff[a]
        samples.append(([x / n for x in f], sum(r[22] for r in g) / n))
    return samples


def fit(samples):
    xtx = [[0.0] * 6 for _ in range(6)]
    xty = [0.0] * 6
    for f, y in samples:
        for a in range(6):
            xty[a] += f[a] * y
            for b in range(6):
                xtx[a][b] += f[a] * f[b]
    k = solve6(xtx, xty)
    if k is None:
        raise SystemExit("正规方程奇异:数据覆盖不足(需要不同转速/电流组合,含带负载点)")
    ys = [y for _, y in samples]
    pred = [sum(k[a] * f[a] for a in range(6)) for f, _ in samples]
    ss_res = sum((y - p) ** 2 for y, p in zip(ys, pred))
    mean_y = sum(ys) / len(ys)
    ss_tot = sum((y - mean_y) ** 2 for y in ys)
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
    rmse = math.sqrt(ss_res / len(ys))
    max_err = max(abs(y - p) for y, p in zip(ys, pred))
    return k, ys, pred, r2, rmse, max_err


def main():
    ap = argparse.ArgumentParser(description="拟合 M3508 功率模型 K0..K5")
    ap.add_argument("input", help="稳态点 CSV,或 VOFA .bin/.csv(加 --vofa)")
    ap.add_argument("--vofa", action="store_true", help="按 VOFA 25 通道旧格式解析")
    ap.add_argument("--min-samples", type=int, default=5,
                    help="VOFA 每个功率平台至少多少个 200Hz 样本(默认 5)")
    ap.add_argument("--power-eps", type=float, default=0.5,
                    help="VOFA 判定功率平台变化的最小差值,W")
    ap.add_argument("--out", default=None, help="把 C 宏片段写到该文件")
    ap.add_argument("--plot", action="store_true", help="画预测 vs 实测散点")
    args = ap.parse_args()

    path = Path(args.input)
    if not path.is_file():
        raise SystemExit(f"文件不存在: {path}")

    skipped_s = 0
    if args.vofa or path.suffix.lower() == ".bin":
        rows = read_bin(path)
        groups = group_by_power(rows, args.min_samples, args.power_eps)
        if len(groups) < 7:
            raise SystemExit("有效功率平台太少(%d 组);2026 协议通道 22 恒 0,"
                             "请改用稳态点 CSV(i/w/p)" % len(groups))
        samples = build_vofa_samples(groups)
    else:
        samples, skipped_s = read_steady_csv(path)

    if len(samples) < 7:
        raise SystemExit(f"有效样本 {len(samples)} 条,至少需要 7 条(6 个系数 + 1)")

    k, y, pred, r2, rmse, max_err = fit(samples)
    print(f"样本 {len(samples)} 条(丢弃 s<0.99 的 {skipped_s} 条)")
    print("K0..K5 = %s" % ", ".join("%.8g" % v for v in k))
    print("R^2 = %.4f, RMSE = %.3f W, max|err| = %.3f W" % (r2, rmse, max_err))
    if k[4] <= 0.0:
        print("WARN: K4<=0 不符合铜损物理,样本可能缺少大电流点", file=sys.stderr)
    if k[5] <= 0.0:
        print("WARN: K5<=0 不符合转速损耗物理,样本可能缺少高转速点", file=sys.stderr)

    worst = sorted(range(len(y)), key=lambda i: -abs(y[i] - pred[i]))[:5]
    print("残差最大的 5 点(实际 / 预测 / 误差):")
    for idx in worst:
        print("  %8.2f / %8.2f / %+7.2f W" % (y[idx], pred[idx], y[idx] - pred[idx]))

    snippet = "\n".join(
        "#define CHASSIS_POWER_MODEL_%s %.8gf" % (name, val) for name, val in zip(COEF_NAMES, k)
    )
    print("\n填入 config/infantry_2026/config_chassis.h:\n" + snippet)
    if args.out:
        Path(args.out).write_text(snippet + "\n", encoding="utf-8")
        print("\n已写入 %s" % args.out)

    if args.plot:
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            print("缺少 matplotlib,跳过绘图")
            return
        plt.figure()
        plt.scatter(y, pred, s=8)
        lim = [min(min(y), min(pred)), max(max(y), max(pred))]
        plt.plot(lim, lim, "r--")
        plt.xlabel("measured power / W")
        plt.ylabel("predicted power / W")
        plt.title("R2=%.3f RMSE=%.1fW" % (r2, rmse))
        plt.grid(True)
        plt.show()


if __name__ == "__main__":
    main()
