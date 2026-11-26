#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T11 ABI golden 检查:tools/abi_dump.c 的 sizeof/offsetof 快照比对。

用法:
  python tools/check_abi_golden.py            # 主机编译+运行+比对
  python tools/check_abi_golden.py --update   # 布局有意变更后重建基线
退出码:0=PASS;1=FAIL。
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "tools" / "abi_dump.c"
EXE = ROOT / "tests" / "build" / "abi_dump.exe"
GOLDEN = ROOT / ".Doc/gate/arch-baseline/abi-host.txt"

INCLUDES = [
    "-Iapps/common/app",
    "-Iconfig/infantry_2026",
    "-Itests/native_stubs",
    "-Iframework",
    "-Icomponents/imu",
]


def run_dump(gcc: str) -> list:
    EXE.parent.mkdir(parents=True, exist_ok=True)
    build = subprocess.run([gcc, "-std=gnu11", "-Wall", "-Werror", *INCLUDES,
                            str(SRC), "-o", str(EXE)],
                           capture_output=True, text=True)
    if build.returncode != 0:
        print(f"FAIL: abi_dump 编译失败\n{build.stderr[-2000:]}")
        raise SystemExit(1)
    run = subprocess.run([str(EXE)], capture_output=True, text=True)
    if run.returncode != 0:
        print(f"FAIL: abi_dump 运行失败\n{run.stderr[-500:]}")
        raise SystemExit(1)
    return sorted(run.stdout.splitlines())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true")
    ap.add_argument("--gcc", default="gcc")
    args = ap.parse_args()

    lines = run_dump(args.gcc)
    if args.update:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        GOLDEN.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"[update] ABI 基线重建 {GOLDEN}:{len(lines)} 条")
        return 0

    if not GOLDEN.is_file():
        print(f"FAIL: 基线不存在,先跑 --update 生成 {GOLDEN}")
        return 1
    golden = sorted(GOLDEN.read_text(encoding="utf-8").splitlines())
    if lines == golden:
        print(f"=== T11 ABI 快照 ===\n{len(lines)} 条 sizeof/offsetof 逐条一致")
        print("RESULT: PASS")
        return 0
    print("[FAIL] ABI 布局漂移(协议合同变更必须走双板同步流程):")
    only_golden = sorted(set(golden) - set(lines))
    only_current = sorted(set(lines) - set(golden))
    for l in only_golden[:15]:
        print(f"  - golden 独有: {l}")
    for l in only_current[:15]:
        print(f"  + 当前独有: {l}")
    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
