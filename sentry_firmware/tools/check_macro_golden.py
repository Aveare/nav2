#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G1b 宏守恒探针检查(tools/macro_probe.c 的 -E -dM 快照比对)。

判据(升级总方案 §5-G1b):
  - 宏删除 → FAIL(新框架公共宏不允许静默消失);
  - 同名宏定义值变化 → FAIL;
  - 新增宏 → 允许(打印提示,--update 后入库成为新基线)。

用法:
  python tools/check_macro_golden.py            # 比对(默认 infantry_2026)
  python tools/check_macro_golden.py --update   # 重建基线(宏集合有预期变化时)
退出码:0=PASS;1=FAIL。
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

# 复用命名闸门的域前缀白名单:宏守恒只看**项目自有宏**;
# 编译器/系统头噪声宏(MinGW DUMMY* 等,DEV-20261123-12 实测 96 个)不参与比对,
# 使基线与主机环境无关(另一设备换 gcc 曾因此误报 FAIL)。
sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_naming import MACRO_PREFIXES, MACRO_ALLOW  # noqa: E402


def project_macro(name: str) -> bool:
    if name in MACRO_ALLOW:
        return True
    if re.fullmatch(r"_?[A-Z0-9]+_H_?", name):  # include guard
        return False
    return name.startswith(MACRO_PREFIXES)

ROOT = Path(__file__).resolve().parent.parent
PROBE = ROOT / "tools" / "macro_probe.c"
GOLDEN = ROOT / ".Doc/gate/arch-baseline/macros-probe.txt"

INCLUDES = [
    "-Iconfig/infantry_2026",
    "-Iframework",
    "-Iframework/kernel",
    "-Iframework/msg",
    "-Iframework/err",
    "-Iframework/mem",
    "-Iframework/diag",
    # telemetry.h → log_port.h/dwt_port.h;主机用 stub 的 main.h,避免拖 HAL 头链
    "-Itests/native_stubs",
    "-Iports/stm32f4",
    "-IMiddlewares/Third_Party/SEGGER/RTT",
    "-IMiddlewares/Third_Party/SEGGER/Config",
]

# 内建/环境差异宏不参与守恒(随 gcc 版本与命令行变化)
SKIP_PREFIXES = ("__", "_GNU", "_WIN", "_MSC", "__STDC")


def probe_defines(gcc: str) -> dict:
    cmd = [gcc, "-E", "-dM", *INCLUDES, str(PROBE)]
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        print(f"FAIL: 探针预处理失败\n{out.stderr[-2000:]}")
        raise SystemExit(1)
    defs = {}
    for line in out.stdout.splitlines():
        if not line.startswith("#define "):
            continue
        parts = line[len("#define "):].split(None, 1)
        name = parts[0]
        value = parts[1] if len(parts) > 1 else ""
        if name.startswith(SKIP_PREFIXES):
            continue
        if not project_macro(name):
            continue
        defs[name] = value
    return defs


def load_golden() -> dict:
    if not GOLDEN.is_file():
        return {}
    defs = {}
    for line in GOLDEN.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, value = line.partition(" ")
        if not project_macro(name):
            continue
        defs[name] = value
    return defs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true", help="重建基线")
    ap.add_argument("--gcc", default="gcc", help="用于 -E -dM 的编译器")
    args = ap.parse_args()

    current = probe_defines(args.gcc)
    print(f"[INFO] 编译器环境:{subprocess.run([args.gcc,'-dumpversion'],capture_output=True,text=True).stdout.strip() or 'unknown'}(基线已按项目宏过滤,环境无关)")
    if args.update:
        GOLDEN.parent.mkdir(parents=True, exist_ok=True)
        lines = [f"# G1b 宏探针基线(probe={args.gcc};重建用 --update)"]
        lines += [f"{k} {v}" for k, v in sorted(current.items())]
        GOLDEN.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"[update] 基线重建 {GOLDEN}:{len(current)} 宏")
        return 0

    golden = load_golden()
    if not golden:
        print(f"FAIL: 基线不存在,先跑 --update 生成 {GOLDEN}")
        return 1

    deleted = sorted(set(golden) - set(current))
    changed = sorted(k for k in set(golden) & set(current) if golden[k] != current[k])
    added = sorted(set(current) - set(golden))

    ok = True
    if deleted:
        ok = False
        print(f"[FAIL] 宏删除 {len(deleted)} 个(不允许静默消失):")
        for k in deleted[:20]:
            print(f"  - {k}")
    if changed:
        ok = False
        print(f"[FAIL] 宏值变化 {len(changed)} 个:")
        for k in changed[:20]:
            print(f"  ~ {k}: {golden[k]!r} -> {current[k]!r}")
    if added:
        print(f"[INFO] 新增宏 {len(added)} 个(允许;--update 入库): {', '.join(added[:8])}...")

    print(f"=== G1b 宏探针 ===\n当前 {len(current)} / 基线 {len(golden)}")
    print("RESULT: PASS" if ok else "RESULT: FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
