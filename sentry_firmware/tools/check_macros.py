#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""T14/G1b 宏检查(自主研发框架升级总方案 §5-G1b/§T14/附D,B1 交付)。

判据(§T14 验收口径):
  - #if 系使用点口径:`^\s*#\s*(if|ifdef|ifndef|elif)\b`,排除 include guard
    (首个 `#ifndef <同名文件宏>` 到对应 `#endif` 之间不计);
  - 治理目标 = **消除 config 内跨子文件 #if**:config/infantry_2026/*.h
    子头里的条件编译,其条件宏若定义于**另一个**子头 → 违例(已知反例:
    ax650.h #if 依赖 chassis.h)。违例白名单 = 账本内登记项(只减不增);
  - 账本 = `.Doc/gate/macro-ledger.csv`(name,file,class,value;五类
    A 参数/B 开关/C 平台/D 函数式/E 容量预算;初版由本脚本启发式生成,人工复核)。

用法:
  python tools/check_macros.py --update   # 重生成账本骨架(新宏入库)
  python tools/check_macros.py            # 校验:违例 ⊆ 账本登记 + guard 外 #if 点数
退出码:0=PASS;1=FAIL。
"""
import argparse
import csv
import re
import sys
from pathlib import Path

UMBRELLA = Path("config/infantry_2026/config.h")
SUB_DIR = Path("config/infantry_2026")
LEDGER = Path(".Doc/gate/macro-ledger.csv")

IF_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif)\b(.*)")
DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\b(.*)", re.M)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
GUARD_NAME_RE = re.compile(r"^_?[A-Z0-9_]*_H_?_?$|^_?[A-Z0-9_]*_H$")

CLASS_RULES = (
    ("C", re.compile(r"ONE_BOARD|CHASSIS_BOARD|GIMBAL_BOARD|STM32|USE_|HAL_")),
    ("B", re.compile(r"(_ENABLE|_DISABLE|_OPTION|_MODE_SWITCH|_BYPASS|_REVERSE)$|(ENABLE|BYPASS)_")),
    ("E", re.compile(r"(_SIZE|_CNT|_MAX|_LEN|_DEPTH|_CAPACITY|_LIMIT)$|RING|POOL")),
    ("D", re.compile(r"\(")),  # 函数式:定义带形参括号
)


def classify(name, value):
    for cls, pat in CLASS_RULES:
        if pat.search(name) or (cls == "D" and pat.search(value)):
            return cls
    return "A"


def guard_ranges(text):
    """include guard 区间(首ifndef同名宏→文件尾);guard 内 #if 不计使用点。"""
    m = re.search(r"^\s*#\s*ifndef\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", text, re.M)
    if not m:
        return None, 0
    name = m.group(1)
    if not GUARD_NAME_RE.match(name):
        return None, 0
    return name, m.start()


def active_if_points(path):
    """返回 [(line_no, condition_text)](排除 guard 区间)。"""
    text = path.read_text(encoding="utf-8", errors="replace")
    guard, gpos = guard_ranges(text)
    points = []
    for i, line in enumerate(text.splitlines(), 1):
        m = IF_RE.match(line)
        if not m:
            continue
        if guard and "ifndef" in m.group(1) and m.group(2).strip() == guard:
            continue  # guard 本身
        points.append((i, m.group(2).strip()))
    return points, text


def defined_macros(text):
    out = {}
    for m in DEFINE_RE.finditer(text):
        name = m.group(1)
        value = m.group(2).strip()
        out.setdefault(name, value)
    return out


def main():
    ap = argparse.ArgumentParser(description="T14/G1b 宏检查")
    ap.add_argument("--update", action="store_true", help="重生成账本骨架")
    args = ap.parse_args()

    if not UMBRELLA.exists() or not SUB_DIR.exists():
        print("FAIL: config 伞头/子头目录缺失")
        return 1

    files = [UMBRELLA] + sorted(SUB_DIR.glob("*.h"))
    defines_by_file = {f.as_posix(): defined_macros(f.read_text(encoding="utf-8", errors="replace")) for f in files}

    # 跨子文件 #if 依赖:子头条件宏定义于另一子头
    violations = []
    for f in files:
        if f == UMBRELLA:
            continue
        points, _ = active_if_points(f)
        for line_no, cond in points:
            for word in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", cond):
                if word in ("defined", "if", "ifdef", "ifndef", "elif"):
                    continue
                owner = [
                    fp for fp, defs in defines_by_file.items()
                    if fp != f.as_posix() and word in defs
                ]
                other_sub = [fp for fp in owner if fp != UMBRELLA.as_posix()]
                if other_sub:
                    violations.append((f.as_posix(), line_no, word, other_sub[0]))

    if args.update:
        LEDGER.parent.mkdir(parents=True, exist_ok=True)
        with open(LEDGER, "w", encoding="utf-8", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["name", "file", "class", "value"])
            for fp in sorted(defines_by_file):
                for name, value in sorted(defines_by_file[fp].items()):
                    w.writerow([name, fp, classify(name, value), value[:60]])
        print(f"[update] 宏账本骨架已重写 {LEDGER}:{sum(len(v) for v in defines_by_file.values())} 宏")
        return 0

    ledger_names = set()
    if LEDGER.exists():
        for row in csv.DictReader(open(LEDGER, encoding="utf-8")):
            ledger_names.add((row["name"], row["file"]))
    ledger_files = {fp for _, fp in ledger_names}

    problems = []
    new_viol = [(f, ln, w, o) for f, ln, w, o in violations if (w, o) not in ledger_names]
    if new_viol:
        for f, ln, w, o in new_viol:
            problems.append(f"跨子文件 #if 新违例: {f}:{ln} 条件宏 {w} 定义于 {o}(登记进账本或消除)")
    else:
        known = len(violations)
        print(f"[PASS] 跨子文件 #if 违例 {known} 处,全部在账本登记(只减不增;治理目标=消除)")

    gone = ledger_files - {f.as_posix() for f in files}
    if gone:
        problems.append(f"账本引用的文件已不存在: {sorted(gone)}(子头改名/删除未同步账本)")

    total_if = sum(len(active_if_points(f)[0]) for f in files)
    print(f"=== T14/G1b 宏检查 ===")
    print(f"文件 {len(files)};guard 外 #if 使用点 {total_if};跨子文件违例 {len(violations)}")
    for p in problems:
        print(f"  [FAIL] {p}")
    print("RESULT:", "PASS" if not problems else "FAIL")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
