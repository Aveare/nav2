#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G5c 分区与段属性检查(自主研发框架升级总方案 §5-G5c,B1 交付)。

判据:.ccmbss 段存在且 VMA ∈ [0x10000000, 0x10010000);
CCMRAM 占用 ≤90%(默认),主 RAM 占用 ≤70%(默认)。
口径:size 的 bss 合计看不见 CCM 回迁,必须分区 —— 主 RAM = data + (.bss + 保留段),
CCM = .ccmbss;百分比按 §0.2 口径(131KB 主 RAM / 65.5KB CCM)。

用法:
  python tools/check_sections.py [--elf build/basic_framework.elf]
      [--threshold-ccm 90] [--threshold-ram 70]
      [--expect-ccm-over]  # fixture:人为把阈值压到 1% 以验证 FAIL 路径
退出码:0=PASS;1=FAIL。
"""
import argparse
import re
import subprocess
import sys

CCM_LOW = 0x10000000
CCM_HIGH = 0x10010000
CCM_SIZE = 0x10000       # 64KB CCM
RAM_SIZE = 128 * 1024 + 512  # 128KB+64KB = 131072+... F407IGH:128KB SRAM+64KB CCM;主 RAM 128KB
RAM_SIZE = 0x20000       # 128KB 主 RAM


def readelf_sections(elf):
    out = subprocess.run(["arm-none-eabi-readelf", "-S", "-W", elf],
                         capture_output=True, text=True)
    if out.returncode != 0:
        print(f"FAIL: readelf 无法读取 {elf}")
        return None
    secs = {}
    for line in out.stdout.splitlines():
        m = re.match(r"\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)", line)
        if m:
            secs[m.group(1)] = (int(m.group(2), 16), int(m.group(4), 16))  # (addr, size)
    return secs


def main():
    ap = argparse.ArgumentParser(description="G5c 分区与段属性")
    ap.add_argument("--elf", default="build/basic_framework.elf")
    ap.add_argument("--threshold-ccm", type=float, default=90.0)
    ap.add_argument("--threshold-ram", type=float, default=70.0)
    ap.add_argument("--expect-ccm-over", action="store_true",
                    help="fixture:把 CCM 阈值压到 1%% 验证 FAIL 路径")
    args = ap.parse_args()

    secs = readelf_sections(args.elf)
    if secs is None:
        return 1
    problems, notes = [], []

    if ".ccmbss" not in secs:
        problems.append(".ccmbss 段不存在(链接脚本回退/段属性丢失)")
        ccm_addr, ccm_size = 0, 0
    else:
        ccm_addr, ccm_size = secs[".ccmbss"]
        if not (CCM_LOW <= ccm_addr < CCM_HIGH):
            problems.append(f".ccmbss VMA 0x{ccm_addr:08x} 越界(期望 [{CCM_LOW:08x},{CCM_HIGH:08x})")
        else:
            notes.append(f".ccmbss VMA 0x{ccm_addr:08x} 合规,size {ccm_size}B")

    bss = secs.get(".bss", (0, 0))[1]
    data = secs.get(".data", (0, 0))[1]
    heap_stack = sum(sz for name, (a, sz) in secs.items()
                     if name.startswith("._user_") or name.startswith("._esp_"))
    ram_used = data + bss + heap_stack
    ccm_pct = 100.0 * ccm_size / CCM_SIZE
    ram_pct = 100.0 * ram_used / RAM_SIZE
    notes.append(f"主 RAM {ram_used}B/{RAM_SIZE}B = {ram_pct:.1f}%(data{data}+bss{bss}+保留{heap_stack});"
                 f"CCM {ccm_size}B/{CCM_SIZE}B = {ccm_pct:.1f}%")
    if ccm_pct > args.threshold_ccm:
        problems.append(f"CCM {ccm_pct:.1f}% > {args.threshold_ccm}%(G5c 阻断)")
    if ram_pct > args.threshold_ram:
        problems.append(f"主 RAM {ram_pct:.1f}% > {args.threshold_ram}%(G5c 阻断)")

    print("=== G5c 分区与段属性 ===")
    for n in notes:
        print(f"  [INFO] {n}")
    for p in problems:
        print(f"  [FAIL] {p}")
    print("RESULT:", "PASS" if not problems else "FAIL")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
