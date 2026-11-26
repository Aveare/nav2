#!/usr/bin/env python3
"""电机状态监测:读四轮 + 云台的转速/电流/温度/在线状态

原理:
    用 OpenOCD 起 gdb server(3333),再用 arm-none-eabi-gdb 按符号名直接读结构体字段。
    比手算结构体偏移可靠 —— 结构体一改,偏移就错,符号名不会。

    在线判据用 feed_cnt(反馈帧计数)的增量:
        halt 时外设中断不执行,feed_cnt 不会涨。所以采样必须"halt 读一次 -> 放它跑 ->
        再 halt 读一次",比较两次差值。1 秒内差值 > 10 判为在线。

用法:
    python tools/monitor_motors.py          # 采样一次(内部自动取两次算增量)
    python tools/monitor_motors.py 10       # 每 2 秒刷新,共 10 次

前置:
    - 探针空闲(不要与 VS Code 的 EmberProbe 同时用)
    - 目标已上电、固件已跑起来

注意:
    halt 会让车短暂停住(每次约几十毫秒),静止测试无影响;不要在车高速运转时用。
"""

import os
import re
import subprocess
import sys
import time

OPENOCD = r"C:\Toolchain\xpack-openocd-0.12.0-7\bin\openocd.exe"
GDB = r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin\arm-none-eabi-gdb.exe"
GDB_PORT = 3333

# 显示名 -> 符号名
MOTORS = [
    ("LF", "motor_lf"),
    ("RF", "motor_rf"),
    ("LB", "motor_lb"),
    ("RB", "motor_rb"),
    ("yaw", "yaw_motor"),
    ("pitch", "pitch_motor"),
]


def build_gdb_script(tag):
    lines = [
        "set pagination off",
        "set confirm off",
        "set width 0",
        "target extended-remote :%d" % GDB_PORT,
        "monitor halt",
    ]
    for disp, sym in MOTORS:
        lines.append(
            'printf "%s|%s|%%u|%%.1f|%%d|%%d\\n", %s->feed_cnt, %s->measure.speed_aps, '
            '%s->measure.real_current, %s->measure.temperature'
            % (tag, disp, sym, sym, sym, sym)
        )
    lines += [
        'printf "%s|EST|%%.1f|%%.1f|%%.2f\\n", est_vx, est_vy, est_wz' % tag,
        'printf "%s|STATE|%%d\\n", robot_state' % tag,
        "detach",
        "quit",
    ]
    return "\n".join(lines) + "\n"


def read_sample(root, tag):
    """跑一次 gdb,返回 {key: value} 字典。"""
    script_path = os.path.join(root, "build", "monitor.gdb")
    log_path = os.path.join(root, "build", "monitor_gdb.log")
    with open(script_path, "w", encoding="ascii") as f:
        f.write(build_gdb_script(tag))

    with open(log_path, "wb") as log:
        subprocess.run(
            [GDB, "-batch", "-x", script_path, os.path.join(root, "build", "basic_framework.elf")],
            stdout=log, stderr=subprocess.STDOUT, timeout=40,
        )

    result = {}
    with open(log_path, "rb") as f:
        text = f.read().decode("utf-8", "replace")
    for line in text.splitlines():
        m = re.match(r"%s\|(.+)$" % tag, line.strip())
        if not m:
            continue
        parts = m.group(1).split("|")
        result[parts[0]] = parts[1:]
    return result, text


def fmt_row(disp, s1, s2):
    if disp not in s1 or disp not in s2:
        return "  %-6s  <读不到,符号可能被优化掉>" % disp
    try:
        f1 = int(s1[disp][0]); f2 = int(s2[disp][0])
        spd = float(s2[disp][1]); cur = int(s2[disp][2]); tmp = int(s2[disp][3])
    except (ValueError, IndexError):
        return "  %-6s  <数据格式异常>" % disp
    delta = f2 - f1
    if f2 < f1:          # 计数器回绕/复位
        delta = f2
    online = "在线" if delta > 10 else ("离线" if delta == 0 else "极慢")
    return "  %-6s  feed=%6d/s  spd=%9.1f  cur=%7d  tmp=%3d℃  %s" % (
        disp, delta, spd, cur, tmp, online)


def main():
    rounds = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if not os.path.exists(OPENOCD):
        print("[错误] 找不到 openocd: %s" % OPENOCD); return 1
    if not os.path.exists(GDB):
        print("[错误] 找不到 gdb: %s" % GDB); return 1

    log = open(os.path.join(root, "build", "monitor_ocd.log"), "wb")
    proc = subprocess.Popen([
        OPENOCD,
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f4x.cfg",
        "-c", "transport select swd",
        "-c", "adapter speed 1000",
        "-c", "init",
    ], stdout=log, stderr=subprocess.STDOUT)

    try:
        time.sleep(4)
        if proc.poll() is not None:
            print("[错误] OpenOCD 启动失败,末尾输出:")
            log.flush()
            with open(os.path.join(root, "build", "monitor_ocd.log"), "rb") as f:
                for line in f.read().decode("utf-8", "replace").splitlines()[-8:]:
                    print("   " + line)
            return 1

        for r in range(rounds):
            s1, err = read_sample(root, "S1")
            if not s1:
                print("[错误] gdb 读不到数据,末尾输出:")
                for line in err.splitlines()[-8:]:
                    print("   " + line)
                return 1
            time.sleep(1.0)
            s2, _ = read_sample(root, "S2")

            print("=" * 68)
            if rounds > 1:
                print("第 %d/%d 次采样" % (r + 1, rounds))
            for disp, _sym in MOTORS:
                print(fmt_row(disp, s1, s2))
            if "EST" in s2:
                e = s2["EST"]
                print("  底盘估计  vx=%s mm/s  vy=%s mm/s  wz=%s deg/s" % (e[0], e[1], e[2]))
            if "STATE" in s2:
                print("  robot_state = %s" % s2["STATE"][0])
            if r + 1 < rounds:
                time.sleep(1.0)
        return 0
    finally:
        proc.terminate()
        time.sleep(0.5)
        try:
            proc.kill()
        except Exception:
            pass
        log.close()


if __name__ == "__main__":
    sys.exit(main())
