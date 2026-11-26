#!/usr/bin/env python3
"""把 RTT 日志连续落盘成 log/ 下的会话文件。

作什么:
    常驻一个 OpenOCD,通过它的 telnet 命令口周期性读 RTT 控制块与环形缓冲,
    把新增字节追加写入 `log/<时间戳>.log`。与会话节奏对齐:
        **起点** = 固件打出解锁标记 `[CMD] reinstate, robot ready`(上电解锁后)
        **终点** = 目标下电(连续读失败)或 Ctrl-C
    不落在起点之前的启动日志,避免每次开机十几行噪音污染文件头。

为什么不用 tools/rtt_read.py 的方式:
    rtt_read.py 每读一次就 spawn 一个 OpenOCD(启动 + attach + shutdown)。
    单机实测这个来回在无线 CMSIS-DAP 上不可忽略,而 RTT 上行缓冲只有 1024 B:
    当前三条日志约 370 B/s,缓冲只够撑 2.7 s,轮询一慢就开始覆盖丢数据。
    本脚本只 attach 一次,之后每次轮询走 telnet 文本命令(数毫秒),
    单轮成本从"秒级"降到"毫秒级",丢数据的窗口几乎不存在。

为什么不用 `rtt server start`:
    OpenOCD 0.12.0-dev 在本机的 rtt server 能连上但不转发数据
    (WrOff 已涨到 1021、RdOff 仍为 0,TCP 端口收 0 字节)——见 rtt_read.py 文件头。
    所以这里不用 rtt server,直接读内存。

用法:
    python tools/rtt_log.py                        # 等到解锁后开始记,下电自动收尾
    python tools/rtt_log.py --from-boot            # 从开机第一行开始记
    python tools/rtt_log.py --interval 0.2         # 加快轮询(默认 0.2 s)
    python tools/rtt_log.py --marker "[CMD] reinstate"
    python tools/rtt_log.py --raw                  # 保留 ANSI 颜色码
    python tools/rtt_log.py --max-min 30           # 安全上限,防止忘关

    # 自动启动用这个:常驻,一次会话结束后回到等待,等下次上电解锁
    python tools/rtt_log.py --loop

自动启动(Windows):
    双击 `tools/capture_rtt.bat`,或把它放进启动项:
        Win+R → shell:startup → 放快捷方式
    命令行注册登录自启(把路径换成实际路径):
        schtasks /create /tn "RM-RTT-Logger" /tr "<工程>\\tools\\capture_rtt.bat" /sc onlogon /f

输出:
    log/YYYYMMDD_HHMMSS.log   —— 纯文本,已剥掉 ANSI 颜色码
    终端                       —— 每 5 s 一行状态(字节数 / 丢弃数 / 当前轮询耗时)

退出码:
    0 = 正常结束(下电或 Ctrl-C,文件已收尾)
    1 = 探针/OpenOCD 异常,或在启动阶段失败
    3 = 达到 --max-min 上限
    --loop 下不返回,除非 Ctrl-C。

前置:
    - 探针空闲:VS Code 的 EmberProbe 扩展、Ozone、RTT Viewer 都不能同时占着探针
    - 目标板上电、固件已启动(否则搜不到控制块)
"""

import argparse
import datetime
import os
import re
import socket
import struct
import subprocess
import sys
import time

OPENOCD = r"C:\Toolchain\xpack-openocd-0.12.0-7\bin\openocd.exe"
SEARCH_ADDR = "0x20000000"
SEARCH_SIZE = "0x20000"
CB_ID = "SEGGER RTT"

# SEGGER_RTT_CB 里 aUp[0] 的字段偏移,与 tools/rtt_read.py 保持一致
OFF_PBUF = 0x1C
OFF_SIZE = 0x20
OFF_WROFF = 0x24
OFF_RDOFF = 0x28
CTRL_BYTES = 64

TELNET_PORT = 4444
LOCK_PORT = 4456  # 单实例互斥用的本地端口(只 bind 不通信)
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
DEFAULT_MARKER = "[CMD] reinstate, robot ready"


def log(msg):
    print(msg, flush=True)


class NoTarget(Exception):
    """常驻模式下:探针或目标暂时不可用,应回到等待态而不是退出。"""


# ---------------------------------------------------------------- 一次性 OpenOCD

def ocd_once(args, cmds, log_path):
    """spawn 一个 OpenOCD 跑完给定命令后退出,返回合并输出。"""
    cmd = [OPENOCD] + args
    for c in cmds:
        cmd += ["-c", c]
    with open(log_path, "wb") as f:
        proc = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT)
        try:
            proc.wait(timeout=40)
        except subprocess.TimeoutExpired:
            proc.kill()
    with open(log_path, "rb") as f:
        return f.read().decode("utf-8", "replace")


def ocd_args():
    return ["-f", "interface/cmsis-dap.cfg",
            "-f", "target/stm32f4x.cfg",
            "-c", "transport select swd",
            "-c", "adapter speed 1000"]


def find_control_block(log_path):
    """扫 RAM 找 SEGGER RTT 控制块地址。用一次性实例做,复用 rtt_read.py 已验证的路子。"""
    out = ocd_once(ocd_args(),
                   ["init",
                    'rtt setup %s %s "%s"' % (SEARCH_ADDR, SEARCH_SIZE, CB_ID),
                    "shutdown"],
                   log_path)
    m = re.search(r"Control block found at (0x[0-9a-fA-F]+)", out)
    if not m:
        return None, out
    return int(m.group(1), 16), out


# ---------------------------------------------------------------- 常驻 OpenOCD

class Daemon:
    """常驻 OpenOCD + telnet 命令通道。"""

    def __init__(self, log_path):
        self.log_path = log_path
        self.proc = None

    def start(self, timeout=20.0):
        log_f = open(self.log_path, "wb")
        kwargs = {}
        if os.name == "nt":
            kwargs["creationflags"] = 0x08000000  # CREATE_NO_WINDOW
        self.proc = subprocess.Popen([OPENOCD] + ocd_args() + ["-c", "init"],
                                     stdout=log_f, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, **kwargs)
        log_f.close()

        # 等 telnet 端口就绪:OpenOCD 起来了才会打印 Listening on port 4444
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                with open(self.log_path, "rb") as f:
                    if b"Listening on port %d" % TELNET_PORT in f.read():
                        return True
            except OSError:
                pass
            if self.proc.poll() is not None:
                return False
            time.sleep(0.2)
        return False

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.kill()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass


class Cli:
    """OpenOCD telnet 文本命令通道。"""

    def __init__(self, port=TELNET_PORT, timeout=5.0):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b""
        self._drain_banner()

    def _drain_banner(self):
        # 连接后 OpenOCD 会先甩一段横幅和一个提示符,读掉
        deadline = time.time() + 2.0
        while time.time() < deadline:
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                break
            if not chunk:
                break
            self.buf += chunk
            if self.buf.rstrip().endswith(b">"):
                self.buf = b""
                break

    def cmd(self, text):
        self.sock.sendall(text.encode() + b"\n")
        deadline = time.time() + 5.0
        while time.time() < deadline:
            if self.buf.rstrip().endswith(b">"):
                break
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                raise ConnectionError("telnet 断开")
            self.buf += chunk
        raw = self.buf.decode("utf-8", "replace")
        self.buf = b""
        # 去掉回显的第一行与结尾提示符
        lines = raw.replace("\r", "").split("\n")
        if lines and lines[0].strip().endswith(text.strip()):
            lines = lines[1:]
        out = "\n".join(lines).rstrip()
        if out.endswith(">"):
            out = out[:-1].rstrip()
        return out

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def parse_mdw(text):
    """解析 `mdw` 输出里的数值。格式:`0x20000000: 4c4c4449 4c50534f ...`"""
    vals = []
    for line in text.splitlines():
        line = line.strip()
        if ":" not in line:
            continue
        head, _, rest = line.partition(":")
        if not re.fullmatch(r"0x[0-9a-fA-F]{8}", head.strip()):
            continue
        for tok in rest.split():
            if re.fullmatch(r"[0-9a-fA-F]{8}", tok):
                vals.append(int(tok, 16))
    return vals


def read_words(cli, addr, count):
    text = cli.cmd("mdw 0x%X 0x%X" % (addr, count))
    vals = parse_mdw(text)
    if len(vals) < count:
        raise ConnectionError("mdw 读数不足: 期望 %d 实际 %d\n%s" % (count, len(vals), text[:200]))
    return vals[:count]


def read_bytes(cli, addr, n):
    if n <= 0:
        return b""
    words = read_words(cli, addr, (n + 3) // 4)
    raw = b"".join(struct.pack("<I", w) for w in words)
    return raw[:n]


# ---------------------------------------------------------------- 会话文件

def open_session(out_dir, header_lines):
    os.makedirs(out_dir, exist_ok=True)
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    path = os.path.join(out_dir, stamp + ".log")
    n = 2
    while os.path.exists(path):
        path = os.path.join(out_dir, "%s_%d.log" % (stamp, n))
        n += 1
    f = open(path, "w", encoding="utf-8", newline="")
    for line in header_lines:
        f.write("# " + line + "\n")
    f.write("#" + "-" * 68 + "\n")
    f.flush()
    return path, f


# ---------------------------------------------------------------- 主流程

def parse_args():
    ap = argparse.ArgumentParser(description="连续抓 RTT 日志到 log/ 目录")
    ap.add_argument("--out", default=None, help="输出目录,默认 <工程>/log")
    ap.add_argument("--interval", type=float, default=0.2, help="轮询间隔秒,默认 0.2")
    ap.add_argument("--from-boot", action="store_true", help="不等解锁标记,从开机第一行开始记")
    ap.add_argument("--marker", default=DEFAULT_MARKER, help="会话起点标记")
    ap.add_argument("--raw", action="store_true", help="保留 ANSI 颜色码")
    ap.add_argument("--max-min", type=float, default=180.0, help="单次最长记录分钟数,默认 180")
    ap.add_argument("--loop", action="store_true",
                    help="常驻:一次会话结束后回到等待状态,等下一次上电解锁(自动启动用这个)")
    ap.add_argument("--loop-delay", type=float, default=5.0,
                    help="--loop 下两次尝试的间隔秒,默认 5")
    ap.add_argument("--verbose-loop", action="store_true",
                    help="--loop 下每次都打印失败详情(默认只在首次和每 60 s 打一次,避免刷屏)")
    return ap.parse_args()


def run_once(args):
    """跑一个完整会话:找控制块 → 常驻 OpenOCD → 记录到文件 → 收尾。"""
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = args.out or os.path.join(root, "log")
    build_dir = os.path.join(root, "build")
    os.makedirs(build_dir, exist_ok=True)
    ocd_log = os.path.join(build_dir, "rtt_openocd_daemon.log")

    if not os.path.exists(OPENOCD):
        log("[错误] 找不到 openocd: %s" % OPENOCD)
        return 1

    cb, out = find_control_block(os.path.join(build_dir, "rtt_openocd.log"))
    if cb is None:
        if args.loop:
            # 常驻模式下"没目标"不是错误,是等待中的正常状态
            tail = [l for l in out.strip().splitlines() if l.strip()]
            raise NoTarget(tail[-1] if tail else "")
        log("[1/3] 搜索 RTT 控制块 ...")
        log("[错误] 没搜到控制块。OpenOCD 末尾输出:")
        for line in out.splitlines()[-10:]:
            log("   " + line)
        log("排查:固件是否已启动? 探针是否被 IDE/Ozone 占用?")
        return 1
    log("[1/3] 控制块 0x%X" % cb)

    log("[2/3] 启动常驻 OpenOCD 并连接 telnet ...")
    daemon = Daemon(ocd_log)
    if not daemon.start():
        log("[错误] OpenOCD 常驻实例起不来,末尾输出:")
        try:
            with open(ocd_log, "rb") as f:
                for line in f.read().decode("utf-8", "replace").splitlines()[-10:]:
                    log("   " + line)
        except OSError:
            pass
        log("排查:4444 端口是否被别的 OpenOCD 占用。")
        daemon.stop()
        return 1

    cli = None
    fout = None
    try:
        cli = Cli()

        ctrl = read_words(cli, cb, CTRL_BYTES // 4)
        pbuf = ctrl[OFF_PBUF // 4]
        size = ctrl[OFF_SIZE // 4]
        rd_target = ctrl[OFF_RDOFF // 4]
        if size == 0 or size > 65536 or not (0x20000000 <= pbuf <= 0x20020000):
            log("[错误] 控制块字段异常: pbuf=0x%X size=%d" % (pbuf, size))
            return 1
        log("      pbuf=0x%X size=%d B" % (pbuf, size))

        cur = 0 if args.from_boot else rd_target
        started = args.from_boot
        t_start = time.time()
        last_report = 0.0
        total_bytes = 0
        dropped = 0
        fail_streak = 0

        log("[3/3] 开始轮询(间隔 %.2f s)。起点:%s" %
            (args.interval, "开机" if args.from_boot else "标记 " + args.marker))

        while True:
            if (time.time() - t_start) / 60.0 > args.max_min:
                log("[提示] 达到 --max-min 上限,收尾退出")
                break

            try:
                ctrl = read_words(cli, cb, CTRL_BYTES // 4)
                wr = ctrl[OFF_WROFF // 4]
                rd_target = ctrl[OFF_RDOFF // 4]
                fail_streak = 0
            except (ConnectionError, socket.timeout, OSError):
                fail_streak += 1
                if fail_streak >= 3:
                    log("[提示] 连续 %d 次读不到目标 —— 视为下电,收尾" % fail_streak)
                    break
                time.sleep(0.3)
                continue

            # 目标侧 RdOff 归零且 WrOff 很小 = 目标刚重启,游标跟着重置
            # (只重置不回绕,否则 (wr - cur) 会算出天文数字)
            if rd_target == 0 and wr < size and cur > size:
                cur = 0
                dropped = 0

            advance = (wr - cur) % (1 << 32)
            if advance > size:
                # 环形缓冲被写过一整轮:中间那段永久丢失,只能拿最后 size 字节
                dropped += advance - size
                cur = wr - size
                advance = size

            if advance:
                try:
                    if cur < size and cur + advance > size:
                        first = read_bytes(cli, pbuf + cur, size - cur)
                        second = read_bytes(cli, pbuf, advance - (size - cur))
                        data = first + second
                    else:
                        data = read_bytes(cli, pbuf + (cur % size), advance)
                except (ConnectionError, socket.timeout, OSError):
                    fail_streak += 1
                    if fail_streak >= 3:
                        log("[提示] 连续 %d 次读不到数据 —— 视为下电,收尾" % fail_streak)
                        break
                    time.sleep(0.3)
                    continue

                text = data.decode("utf-8", "replace")
                if not args.raw:
                    text = ANSI_RE.sub("", text)

                if not started:
                    idx = text.find(args.marker)
                    if idx >= 0:
                        started = True
                        path, fout = open_session(out_dir, [
                            "RTT session %s" % datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                            "firmware basic_framework.elf, control block 0x%X, buffer %d B" % (cb, size),
                            "起点:解锁标记 %s" % args.marker,
                        ])
                        log("[会话开始] %s" % path)
                        text = text[idx:]
                        total_bytes += len(text.encode("utf-8"))
                        fout.write(text)
                        fout.flush()
                else:
                    total_bytes += len(text.encode("utf-8"))
                    fout.write(text)
                    fout.flush()

                cur = wr
                # 回写 RdOff,否则目标端认为缓冲仍是满的,后续日志会被 SKIP 丢光
                try:
                    cli.cmd("mww 0x%X 0x%X" % (cb + OFF_RDOFF, wr))
                except (ConnectionError, socket.timeout, OSError):
                    pass

            now = time.time()
            if now - last_report >= 5.0:
                last_report = now
                stat = "已写 %d B" % total_bytes if started else "等待解锁标记"
                log("      [%4.0fs] %s | 丢弃 %d B | 环占用 %d B" %
                    (now - t_start, stat, dropped, (wr - cur) % (1 << 32)))

            time.sleep(args.interval)

        dur = time.time() - t_start
        if fout:
            fout.write("#" + "-" * 68 + "\n")
            fout.write("# session end %s (%.1f s, %d B, dropped %d B)\n" %
                      (datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), dur, total_bytes, dropped))
            fout.close()
            log("[会话结束] 时长 %.0f s,共 %d B,丢失 %d B" % (dur, total_bytes, dropped))
        else:
            log("[提示] 本次没有创建文件(未等到解锁标记)")
            if not args.from_boot:
                log("       若要连开机日志一起记,加 --from-boot")

        if (time.time() - t_start) / 60.0 > args.max_min:
            return 3
        return 0

    finally:
        # 中断/异常路径:文件里明确标一句,免得事后把截断的日志当成"下电正常结束"
        if fout is not None and not fout.closed:
            try:
                fout.write("#[未正常收尾:中断或异常]\n")
                fout.close()
            except OSError:
                pass
        if cli:
            cli.close()
        daemon.stop()


def main():
    args = parse_args()
    last_note = 0.0

    # 单实例锁:自动启动 + 手动再开一个时,两个进程会抢 4444 端口互相打死
    # 用占用一个本地端口当互斥量,进程退出即释放,不需要清理锁文件
    lock = None
    try:
        lock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        lock.bind(("127.0.0.1", LOCK_PORT))
        lock.listen(1)
    except OSError:
        log("[错误] 已有一个抓取进程在跑(端口 %d 被占)。本次退出。" % LOCK_PORT)
        return 1

    try:
        while True:
            try:
                rc = run_once(args)
            except NoTarget as e:
                now = time.time()
                # 默认每 60 s 只说一句,免得开机自启在没接车时把控制台刷满
                if args.verbose_loop or (now - last_note) >= 60.0:
                    reason = (": " + str(e)) if str(e) else ""
                    log("[等待] 探针/目标不可用%s —— %.0f s 后重试" % (reason, args.loop_delay))
                    last_note = now
                time.sleep(args.loop_delay)
                continue

            if not args.loop:
                return rc

            log("      [常驻] 本次会话结束,%.0f s 后回到等待状态" % args.loop_delay)
            time.sleep(args.loop_delay)
    finally:
        if lock:
            lock.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # 每轮都 flush 过,已采数据不丢;只是少一行结束标记
        print("\n[提示] Ctrl-C 中断,已写入的数据保留在 log/ 下的会话文件里。")
        sys.exit(0)
