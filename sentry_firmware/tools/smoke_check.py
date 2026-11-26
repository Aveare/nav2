#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""烧录后 smoke 检查:解析 RTT 日志,断言启动/运行关键标记。

背景:烧录后最容易漏看的是"固件其实没跑起来"或"必活任务已被看门狗复位"。
本脚本把 RTT 日志里的结构化标记变成 PASS/FAIL:
  必须出现(任一):[boot] RST cause / [freeRTOS] ... Task Start
  禁止出现(默认):[diag] required task dead / HardFault / StackOverflow
  可选断言:--expect "子串" 可多次,--forbid "子串" 可多次

用法:
  python tools/smoke_check.py log\\_console.log
  python tools/smoke_check.py log\\_console.log --expect "[heat]" --forbid "[err]"
退出码:0=PASS;1=FAIL;2=文件不存在/无内容。
"""
import argparse
import sys
from pathlib import Path

MUST_ANY = ("[boot] RST cause", "[freeRTOS]")
FORBID_DEFAULT = ("[diag] required task dead", "HardFault", "StackOverflow",
                  "configASSERT", "delta task dead")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", help="RTT 抓取的日志文本(log/_console.log 或导出)")
    ap.add_argument("--expect", action="append", default=[], help="必须出现的子串(可多次)")
    ap.add_argument("--forbid", action="append", default=[], help="禁止出现的子串(可多次)")
    args = ap.parse_args()

    p = Path(args.log)
    if not p.is_file() or p.stat().st_size == 0:
        print(f"FAIL: 日志不存在或为空: {p}")
        return 2
    text = p.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    print(f"=== smoke {p}({len(lines)} 行) ===")

    ok = True
    if not any(m in text for m in MUST_ANY):
        ok = False
        print(f"[FAIL] 未找到启动标记(任一): {MUST_ANY}")
    else:
        hit = next(m for m in MUST_ANY if m in text)
        print(f"[ ok ] 启动标记: {hit}")

    for pat in FORBID_DEFAULT + tuple(args.forbid):
        hits = [ln for ln in lines if pat in ln]
        if hits:
            ok = False
            print(f"[FAIL] 禁止标记命中 {len(hits)} 次: {pat}")
            for h in hits[:3]:
                print(f"       {h.strip()[:120]}")
        else:
            print(f"[ ok ] 无: {pat}")

    for pat in args.expect:
        if pat in text:
            print(f"[ ok ] 期望标记: {pat}")
        else:
            ok = False
            print(f"[FAIL] 缺少期望标记: {pat}")

    print("RESULT: PASS" if ok else "RESULT: FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
