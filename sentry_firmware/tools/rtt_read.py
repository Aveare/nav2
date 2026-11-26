#!/usr/bin/env python3
"""独立读 RTT 日志(不依赖 EmberProbe 扩展的 Agent 桥接)

背景:
    固件用 SEGGER RTT(bsp/log/bsp_log.c -> SEGGER_RTT_vprintf, BUFFER_INDEX=0)。
    OpenOCD 0.12.0-dev 的 `rtt server start` 在本机实测**能连上但不转发数据**
    (buffer 里 WrOff 已涨到 1021,RdOff 仍是 0,而 TCP 端口收到 0 字节)。
    所以本脚本不用 rtt server,改成:
        1) 用 `rtt setup` 让 OpenOCD 搜出控制块地址(它会打印 "Control block found at 0x...")
        2) dump 控制块 -> 解析 pBuffer / SizeOfBuffer / WrOff / RdOff
        3) dump 环形缓冲区 -> 按 [RdOff, WrOff) 切片后输出

用法:
    python tools/rtt_read.py            # 默认读一次快照
    python tools/rtt_read.py 5          # 间隔 1 秒连续读 5 次(看滚动日志)

前置:
    1. 探针已插好、目标板已上电、固件已启动(否则搜不到控制块)
    2. 不要与 VS Code 的 EmberProbe 同时占用探针

退出码:
    0 = 读到数据
    2 = 控制块在但没有新数据(WrOff == RdOff)
    1 = 探针/端口异常
"""

import os
import re
import struct
import subprocess
import sys
import time

OPENOCD = r"C:\Toolchain\xpack-openocd-0.12.0-7\bin\openocd.exe"
SEARCH_ADDR = "0x20000000"
SEARCH_SIZE = "0x20000"
CB_ID = "SEGGER RTT"
# SEGGER_RTT_CB 里 aUp[0] 的偏移(acID[16] + 2 个 int + 3 个指针/长度前的字段)
OFF_PBUF = 0x1C
OFF_SIZE = 0x20
OFF_WROFF = 0x24
OFF_RDOFF = 0x28


def ocd_cmd(cmds, log_path):
    """跑一次 OpenOCD,返回合并后的输出文本。"""
    args = [
        OPENOCD,
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f4x.cfg",
        "-c", "transport select swd",
        "-c", "adapter speed 1000",
    ]
    for c in cmds:
        args += ["-c", c]
    log = open(log_path, "wb")
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    proc.wait(timeout=40)
    log.close()
    with open(log_path, "rb") as f:
        return f.read().decode("utf-8", "replace")


def find_control_block(log_path):
    out = ocd_cmd(["init", "rtt setup %s %s \"%s\"" % (SEARCH_ADDR, SEARCH_SIZE, CB_ID),
                   "shutdown"], log_path)
    m = re.search(r"Control block found at (0x[0-9a-fA-F]+)", out)
    if not m:
        return None, out
    return int(m.group(1), 16), out


def dump(addr, size, out_path, log_path):
    out = ocd_cmd(["init",
                   "dump_image \"%s\" 0x%X %d" % (out_path, addr, size),
                   "shutdown"], log_path)
    if not os.path.exists(out_path) or os.path.getsize(out_path) < size:
        return None, out
    with open(out_path, "rb") as f:
        return f.read(), out


def read_once(root, cb_addr):
    log_path = os.path.join(root, "build", "rtt_openocd.log")
    ctrl_path = os.path.join(root, "build", "rtt_ctrl.bin")
    buf_path = os.path.join(root, "build", "rtt_buf.bin")

    ctrl, out = dump(cb_addr, 64, ctrl_path, log_path)
    if ctrl is None:
        return None, out

    if ctrl[0:10] != b"SEGGER RTT":
        return None, "控制块内容不是 SEGGER RTT(地址可能变了)"

    pbuf = struct.unpack_from("<I", ctrl, OFF_PBUF)[0]
    size = struct.unpack_from("<I", ctrl, OFF_SIZE)[0]
    wr = struct.unpack_from("<I", ctrl, OFF_WROFF)[0]
    rd = struct.unpack_from("<I", ctrl, OFF_RDOFF)[0]

    if size == 0 or size > 65536 or pbuf < 0x20000000 or pbuf > 0x20020000:
        return None, "控制块字段异常: pbuf=0x%X size=%d" % (pbuf, size)

    if wr == rd:
        return b"", None

    raw, out = dump(pbuf, size, buf_path, log_path)
    if raw is None:
        return None, out

    if wr > rd:
        data = raw[rd:wr]
    else:
        data = raw[rd:] + raw[:wr]
    return data, None


def main():
    times = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if not os.path.exists(OPENOCD):
        print("[错误] 找不到 openocd: %s" % OPENOCD)
        return 1

    log_path = os.path.join(root, "build", "rtt_openocd.log")
    print("[1/2] 搜索 RTT 控制块 ...")
    cb, out = find_control_block(log_path)
    if cb is None:
        print("[错误] 没搜到控制块。OpenOCD 末尾输出:")
        for line in out.splitlines()[-10:]:
            print("   " + line)
        print("排查: 固件是否已启动? 探针是否被 IDE 占用?")
        return 1
    print("[2/2] 控制块 0x%X,开始读取(共 %d 次)\n" % (cb, times))

    last_len = 0
    for i in range(times):
        data, err = read_once(root, cb)
        if err:
            print("[失败] %s" % err)
            return 1
        if data is None:
            return 1
        if not data:
            if i == 0:
                print("--- 控制块在,但 WrOff == RdOff,暂无新数据 ---")
            return 2
        text = data.decode("utf-8", "replace")
        sys.stdout.write(text)
        if not text.endswith("\n"):
            sys.stdout.write("\n")
        last_len += len(data)
        if i + 1 < times:
            time.sleep(1.0)

    print("\n--- 共 %d 字节 ---" % last_len)
    return 0


if __name__ == "__main__":
    sys.exit(main())
