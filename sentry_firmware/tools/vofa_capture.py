#!/usr/bin/env python3
"""VOFA JustFloat 原始字节录制:把底盘 25 通道帧原样存成 .bin。

用法:
    pip install pyserial
    python tools/vofa_capture.py COM5 90
    python tools/vofa_capture.py COM5 90 --output power_run1.bin

参数:
    port    串口号(Windows 如 COM5,Linux 如 /dev/ttyUSB0)
    seconds 录制时长,s(默认 60)

说明:
- 只负责收字节,不做解析;帧格式与通道顺序见 tools/fit_power_model.py。
- 921600 8N1,底盘 200Hz 输出 25 通道 JustFloat,60s 约 1.25MB。
- 录完直接把这个 .bin 交给 fit_power_model.py。
"""

import argparse
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("需要 pyserial: pip install pyserial")
    sys.exit(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="串口号,如 COM5")
    ap.add_argument("seconds", nargs="?", type=float, default=60.0, help="录制时长,s")
    ap.add_argument("--output", default="power_capture.bin", help="输出文件")
    ap.add_argument("--baud", type=int, default=921600, help="波特率")
    args = ap.parse_args()

    out = Path(args.output)
    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    print("recording %s for %.0fs -> %s" % (args.port, args.seconds, out))

    total = 0
    t0 = time.time()
    try:
        with out.open("wb") as f:
            while time.time() - t0 < args.seconds:
                chunk = ser.read(4096)
                if chunk:
                    f.write(chunk)
                    total += len(chunk)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

    dt = time.time() - t0
    frames = total // 104
    print("saved %.2f MB, %d bytes, ~%d frames, %.1fs" % (total / 1e6, total, frames, dt))
    if total < 20000:
        print("数据太少:检查串口/波特率/是否烧的是 power_calib.elf")
    elif frames < 1000:
        print("帧数偏少:确认底盘 200Hz 在跑、VOFA 通道数为 25")


if __name__ == "__main__":
    main()
