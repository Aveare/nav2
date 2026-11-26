#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T13 工具符号契约检查(自主研发框架升级总方案 §13/B0 交付;入 G3)。

《工具符号冻结清单》 = tools/ 下 9 个调试/采样脚本(chassis_trace/doctor/follow_trace/
gimbal_axis_probe/gimbal_trace/odom/pitch_calib/pitch_trace + lib/preflight)依赖的
**固件变量路径**(形如 `root->member->member[0]`,即 EmberProbe variables.read 的
路径字符串)。这些路径是 B5/B6 迁移批"搬家即改名"的**隐性 API**——改名脚本里
的变量而不改工具,采样工具会静默失效(T13 红旗)。

清单范围:完整符号路径;`variables.read` 等 EmberProbe API 名、*.csv/*.elf 文件名、
RCC.* SVD 寄存器路径、注释散文里的 `Gyro[0]` 片段**不在符号契约内**(前两者是
EmberProbe 固定接口,后两者不是 ELF 符号)。

用法:
  python tools/check_symbol_contract.py --update   # 重扫 tools/ 重写清单(改工具后跑)
  python tools/check_symbol_contract.py            # 校验:清单与源码一致 + 根符号 nm 100% 命中
退出码:0=PASS;1=FAIL(漂移或根符号未命中)。

判据(方案 T13):nm 命中率 100% + 路径数 = 清单声明的 unique 集。
成员链的 DWARF 全路径解析(不含探针)留待 B1 工具升级,当前 = 根符号 nm 解析。
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

TOOLS = sorted(p.as_posix() for p in Path("tools").glob("*.js")) + ["tools/lib/preflight.js"]
TOOLS = [t for t in TOOLS if Path(t).exists()]
LEDGER = Path(".Doc/gate/tool-symbols.json")
ELF = "build/basic_framework.elf"

# 路径字符串:引号内 root + (->|. + member + 可选 [n])+;排除 EmberProbe API/文件名
PATH_RE = re.compile(r"[\"']([A-Za-z_][A-Za-z0-9_]*(?:(?:->|\.)[A-Za-z_][A-Za-z0-9_]*(?:\[\d+\])?)+)[\"']")
API_EXCLUDE = re.compile(
    r"^(debug\.|variables\.|memory\.|peripherals\.|rtt\.|bakesheep\.|RCC\.)|(\.csv$)|(\.elf$)|(\.json$)|(\.h$)")
# 注释行(// 或 *)里的散文片段不算读路径
COMMENT_RE = re.compile(r"(?m)^\s*(?://|\*|/\*)")


def scan_sources():
    """重扫工具源码,返回 {path: [files]}(只取非注释行中的引号路径)。"""
    uniq = {}
    for f in TOOLS:
        for i, line in enumerate(Path(f).read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if COMMENT_RE.match(line):
                continue
            for m in PATH_RE.finditer(line):
                p = m.group(1)
                if API_EXCLUDE.search(p):
                    continue
                uniq.setdefault(p, set()).add(f)
    return {p: sorted(fs) for p, fs in sorted(uniq.items())}


def nm_roots():
    out = subprocess.run(["arm-none-eabi-nm", ELF], capture_output=True, text=True)
    if out.returncode != 0:
        print(f"FAIL: nm 无法读取 {ELF}(先编译)")
        return None
    syms = set()
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            syms.add(parts[2])
    return syms


def main():
    ap = argparse.ArgumentParser(description="T13 工具符号契约检查")
    ap.add_argument("--update", action="store_true", help="重扫 tools/ 重写冻结清单")
    args = ap.parse_args()

    current = scan_sources()
    if args.update:
        LEDGER.parent.mkdir(parents=True, exist_ok=True)
        LEDGER.write_text(json.dumps(current, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"[update] 清单已重写 {LEDGER}:{len(current)} 条路径")
        return 0

    if not LEDGER.exists():
        print(f"FAIL: 清单缺失 {LEDGER}(先 --update)")
        return 1
    frozen = json.loads(LEDGER.read_text(encoding="utf-8"))

    problems = []
    for p in sorted(set(current) - set(frozen)):
        problems.append(f"未登记路径: {p} <- {current[p]}")
    for p in sorted(set(frozen) - set(current)):
        problems.append(f"清单失效(源码已无): {p}")

    syms = nm_roots()
    if syms is None:
        return 1
    roots = sorted({re.split(r"->|\.", p)[0] for p in list(current) + list(frozen)})
    missed = [r for r in roots if r not in syms]
    hit = len(roots) - len(missed)
    print(f"=== T13 工具符号契约 ===")
    print(f"工具脚本: {len(TOOLS)} 个;清单路径: {len(frozen)};源码实测: {len(current)};根符号: {len(roots)}")
    for p, fs in list(frozen.items())[:3]:
        print(f"  例: {p} <- {fs}")
    print(f"nm 根符号命中: {hit}/{len(roots)}")
    for m in missed:
        print(f"  [根未命中] {m}")
    for pr in problems:
        print(f"  [漂移] {pr}")
    ok = not missed and not problems
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
