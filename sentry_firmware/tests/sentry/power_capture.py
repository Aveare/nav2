#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""哨兵功率标定采集工具:一次一条命令,秒级体检,采集即成功。

背景(2026-11-27 哨兵台架实测定性,见 tools/lib/capture_guard.js):
    采集"又慢又总出问题"的根因是前置状态不对——ELF 被别的兵种覆盖、板上固件
    与磁盘 ELF 不同源、RTT 没起、固件没带标定日志。本工具把这些检查全部前置
    到秒级 doctor,采集本体复用共享工具链(不改 tools/)。

用法(仓库根目录运行):
    python tests/sentry/power_capture.py doctor              # 体检:ELF 身份/固件一致/RTT/[pwri] 流
    python tests/sentry/power_capture.py capture spin300     # doctor 通过后采 20s 存 log + steady_raw.csv
    python tests/sentry/power_capture.py capture spin300 --seconds 15
    python tests/sentry/power_capture.py selftest            # 无硬件:验证日志格式与两个解析器兼容

标定协议(步兵实车沉淀,哨兵沿用):
    一次一工况、默认 20s、开采有提示、先存 CSV 不分析、全采完再统一拟合。
    真实功率机器人拿不到:操作手从裁判客户端/台架电源读数,事后补进 CSV 的 p 列。

拟合与回填:
    python tools/fit_power_model.py steady_raw.csv --out build/power_model_snippet.h
    拟合值回填 config/sentry_2026/config_chassis.h 的 CHASSIS_POWER_MODEL_K0..K5;
    注意同时核对 LIMIT_MAX/LIMIT_DEFAULT/BUFFER_MIN_SCALE(只改 K 不改上限会误判)。
"""
import argparse
import csv
import hashlib
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RTT_JS = ROOT / ".agents/skills/mcu-rtt/scripts/rtt.js"
CAPTURE_POINT = ROOT / "tools/rtt_capture_point.py"
LOCK_FILE = ROOT / "build/.capture.lock"

# 与 tools/rtt_capture_point.py、tools/rtt_pwr_to_csv.py 相同的行契约
RE_PWRI = re.compile(r"\[pwri\]\s*i=([-\d.,]+)\s+w=([-\d.,]+)")
RE_PWR = re.compile(r"\[pwr\]\s*p=([-\d.]+)\s+s=([-\d.]+)\s+lim=([-\d.]+)\s+be=([-\d.]+)")

SAMPLE_LOG = """[chcal] vx=12.3 vy=-4.5 wz=299.8 gz=0.4 off=1.2 p=68.5 be=55.0
[pwri] i=1234.5,-2100.0,987.6,-3100.2 w=4200.0,-4100.0,4300.0,-4250.0
[pwr] p=77.3 s=1.00 lim=95.0 be=55.0 po=77.3
[chcal] vx=12.1 vy=-4.4 wz=300.2 gz=0.3 off=1.1 p=68.7 be=54.8
[pwri] i=1240.1,-2088.0,991.0,-3088.4 w=4210.0,-4090.0,4310.0,-4240.0
[pwr] p=77.5 s=1.00 lim=95.0 be=54.6 po=77.5
"""


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def read_current_txt():
    """读 firmware/current.txt 身份卡;缺失或半写返回空字段。"""
    file = ROOT / "firmware" / "current.txt"
    if not file.exists():
        return {}
    out = {}
    for line in file.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" in line:
            k, _, v = line.partition("=")
            out[k.strip()] = v.strip()
    return out


def rtt_read_once():
    """读一次 RTT 缓冲,返回 (ok, text, hint)。"""
    if not RTT_JS.exists():
        return False, "", f"找不到 {RTT_JS.relative_to(ROOT)}"
    p = subprocess.run(
        ["node", str(RTT_JS), "--workspace", ".", "--read", "--channel", "0",
         "--max-bytes", "16384"],
        cwd=str(ROOT), capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=20)
    try:
        import json
        j = json.loads(p.stdout)
    except Exception:
        return False, "", ("RTT 读取失败(探针没连/RTT 没起):先跑 tools/capture_rtt.bat,"
                           "或 EmberProbe mcu-rtt --start;输出: " + (p.stdout[:160] or p.stderr[:160]))
    return True, j.get("text", ""), ""


def check(name, ok, detail=""):
    mark = "PASS" if ok else "FAIL"
    print(f"  [{mark}] {name}" + (f" —— {detail}" if detail else ""))
    return ok


def doctor(need_rtt=True):
    """秒级前置体检。返回全部通过与否;need_rtt=False 时跳过硬件相关项。"""
    print("哨兵功率采集体检:")
    ok = True
    elf = ROOT / "firmware" / "sentry.elf"
    ok &= check("firmware/sentry.elf 存在", elf.exists(),
                "不存在则先 build.ps1 sentry -Current" if not elf.exists() else "")

    cur = read_current_txt()
    ok &= check("current.txt PROFILE=sentry", cur.get("PROFILE") == "sentry",
                f"实际={cur.get('PROFILE')!r}(被别的兵种构建覆盖了)" if cur.get("PROFILE") != "sentry" else "")
    disk_sha = sha256_file(elf) if elf.exists() else ""
    ok &= check("current.txt SHA == 磁盘 sentry.elf", cur.get("SHA256", "").upper() == disk_sha,
                "不一致:ELF 在记录后被重新构建,重烧或重跑 build.ps1 sentry -Current"
                if cur.get("SHA256", "").upper() != disk_sha else "")

    if LOCK_FILE.exists():
        ok &= check("无残留采集锁", False, f"存在 {LOCK_FILE.relative_to(ROOT)},确认没有别的采集进程后删除")
    else:
        ok &= check("无残留采集锁", True)

    if not need_rtt:
        return ok
    rtt_ok, text, hint = rtt_read_once()
    ok &= check("RTT 可读", rtt_ok, hint)
    if rtt_ok:
        has_line = RE_PWRI.search(text) is not None
        ok &= check("固件在发 [pwri] 标定行(板上固件即新哨兵)", has_line,
                    "没有:板上固件是旧的(无 CHASSIS_CALIB_LOG),重烧 firmware/sentry.elf"
                    if not has_line else "")
    return ok


def selftest():
    """无硬件自检:样例行必须同时被 capture_point 与 rtt_pwr_to_csv 两个解析器接受。"""
    print("格式兼容自检(无硬件):")
    sys.path.insert(0, str(ROOT / "tools"))

    import rtt_capture_point as cp
    iw, ps = cp.parse(SAMPLE_LOG)
    ok = check("rtt_capture_point.parse: [pwri]×2 且 [pwr]×2", len(iw) == 2 and len(ps) == 2,
               f"实际 iw={len(iw)} ps={len(ps)}")
    if iw:
        ok &= check("[pwri] 数值完整 4+4", len(iw[0][0]) == 4 and len(iw[0][1]) == 4)

    import rtt_pwr_to_csv as p2c
    tmp = ROOT / "build" / "selftest_pwr.log"
    tmp.parent.mkdir(exist_ok=True)
    tmp.write_text(SAMPLE_LOG, encoding="utf-8")
    iw2, ps2 = p2c.load(tmp)
    ok &= check("rtt_pwr_to_csv.load: [pwri]/[pwr] 配对", len(iw2) == len(ps2) == 2,
                f"实际 iw={len(iw2)} ps={len(ps2)}")
    if ps2:
        p, s, lim, be = ps2[0]
        ok &= check("[pwr] 字段序 p/s/lim/be 解析正确",
                    abs(p - 77.3) < 1e-3 and abs(s - 1.0) < 1e-3 and
                    abs(lim - 95.0) < 1e-3 and abs(be - 55.0) < 1e-3,
                    f"实际 p={p} s={s} lim={lim} be={be}")
    tmp.unlink()
    print("SELFTEST " + ("PASS" if ok else "FAIL"))
    return ok


def capture(cond, seconds):
    """体检 → 采集(复用 tools/rtt_capture_point.py,协议不变:一次一工况)。"""
    if not doctor(need_rtt=True):
        print("\n体检未过,先解决上面的 FAIL 再采(这就是'一次成功'的保证)。")
        return 1
    print(f"\n>>> {seconds}s 后开始:请立即保持工况 <{cond}> 并读稳态功率(裁判客户端/台架电源)! <<<")
    time.sleep(3)
    print(f">>> 采集 <{cond}> {seconds}s,保持工况...")
    cmd = [sys.executable, str(CAPTURE_POINT), "--cond", cond, "--seconds", str(seconds),
           "--append", "steady_raw.csv"]
    r = subprocess.run(cmd, cwd=str(ROOT))
    if r.returncode != 0:
        return r.returncode
    log_path = ROOT / "log" / f"{cond}_{seconds}s.log"
    print(f"""
完成:log={log_path}  累计表=steady_raw.csv(p 列待补)
下一步:
  1) 把刚才读到的稳态功率填进 steady_raw.csv 该行 p 列(或记下来最后统一填);
  2) 下一个工况直接再跑: python tests/sentry/power_capture.py capture <cond>;
  3) 全部采完统一拟合:
     python tools/fit_power_model.py steady_raw.csv --out build/power_model_snippet.h
     (fit 会自动丢弃 s<0.99 的行;当前 CTRL_EN=0 时 s 恒 1,全部有效)""")
    return 0


def main():
    ap = argparse.ArgumentParser(description="哨兵功率标定采集(秒级体检 + 复用共享工具链)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("doctor", help="秒级前置体检(ELF/固件一致/RTT/[pwri] 流)")
    cap = sub.add_parser("capture", help="采一个稳态工况 20s 存 log + steady_raw.csv")
    cap.add_argument("cond", help="工况名,如 static/spin300/spin300_v500")
    cap.add_argument("--seconds", type=int, default=20)
    sub.add_parser("selftest", help="无硬件:验证日志格式与两个解析器兼容")
    args = ap.parse_args()

    if args.cmd == "doctor":
        return 0 if doctor() else 1
    if args.cmd == "selftest":
        return 0 if selftest() else 1
    return capture(args.cond, args.seconds)


if __name__ == "__main__":
    raise SystemExit(main())
