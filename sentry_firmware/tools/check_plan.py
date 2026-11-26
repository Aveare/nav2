#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""方案自检(自主研发框架升级总方案 §13 `check_plan.py` 行,B1 交付;G.2 验证口径的机器复核版)。

按章节解析正文,复核 §10/G.2/§13 的机器可查断言。§13 声明:以「附G 起点」为排除界,
行内 grep 命令按原样执行 —— 判据是「命中数 = 声明数且全部位于本表」,不是 0 命中。

用法:python tools/check_plan.py [--plan .Doc/plan/自主研发框架升级总方案.md]
退出码:0=PASS;1=FAIL。
"""
import argparse
import re
import sys
from pathlib import Path

DEFAULT = ".Doc/plan/自主研发框架升级总方案.md"


def main():
    ap = argparse.ArgumentParser(description="方案自检(机器复核)")
    ap.add_argument("--plan", default=DEFAULT)
    args = ap.parse_args()
    text = Path(args.plan).read_text(encoding="utf-8")
    lines = text.splitlines()
    problems = []

    # ---- §10 ARCH-DEC:每条单独一行、行首 **ARCH-DEC-nnn**,rg -c = 6 ----
    arch_lines = [l for l in lines if re.match(r"^\*\*ARCH-DEC-00", l)]
    if len(arch_lines) != 6:
        problems.append(f"§10 ARCH-DEC 行首计数 = {len(arch_lines)} ≠ 6(每行恰一条)")
    else:
        print(f"[PASS] §10 ARCH-DEC 行首 6 条(001~006)")

    # ---- G.2 验证口径:'B4c 收尾后删除' 恰 1 行且位于附G ----
    g2_start = text.find("### G.2")
    hits = [i for i, l in enumerate(lines) if "B4c 收尾后删除" in l]
    in_g2 = [i for i in hits if text[: sum(len(l) + 1 for l in lines[:i])].find("### G.2") >= 0]
    if len(hits) != 1 or not in_g2:
        problems.append(f"'B4c 收尾后删除' 命中 {len(hits)} 行(应 1 且位于 G.2)")
    else:
        print("[PASS] G.2 'B4c 收尾后删除' 恰 1 行且位于附G(旧口径唯一存留点)")

    # ---- kernel 平台边界:'后端唯一落点 ports/freertos' ≥2 ----
    n = len(re.findall(r"后端唯一落点 ports/freertos", text))
    if n < 2:
        problems.append(f"'后端唯一落点 ports/freertos' 命中 {n} < 2(T05/§4)")
    else:
        print(f"[PASS] '后端唯一落点 ports/freertos' ×{n} ≥ 2")

    # ---- 终态里程碑:档二=M3′ ----
    for key, least in (("M3′", 4), ("档二结项验收", 2)):
        n = len(re.findall(re.escape(key), text))
        if n < least:
            problems.append(f"'{key}' 命中 {n} < {least}(终态统一漂移?)")
        else:
            print(f"[PASS] '{key}' ×{n} ≥ {least}")

    # ---- §13 必备交付物(B1 工具先行 + B0 产物)----
    for key in ("check_sections.py", "check_tags.py", "check_plan.py", "run_fwk_tests",
                "check_macros.py", "tool-symbols.json", "site-ledger"):
        if key not in text:
            problems.append(f"§13 缺交付物关键词: {key}")
    if not problems:
        print("[PASS] §13 交付物关键词齐全")

    # ---- G.2 验证口径声明行 ----
    if "命中数 = 表内声明的个数" in text:
        print("[PASS] G.2 验证口径声明行存在")
    else:
        problems.append("G.2 验证口径声明行缺失")

    # ---- §7.3 断路器与 §0.1 状态 ----
    if "T_season" not in text:
        problems.append("§7.3 T_season 断路器缺失")
    if "B 已批准待启动" not in text or "C 升级期" not in text:
        problems.append("§0.1 状态机行缺失")

    print("=== 方案自检 ===")
    for p in problems:
        print(f"  [FAIL] {p}")
    print("RESULT:", "PASS" if not problems else "FAIL")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
