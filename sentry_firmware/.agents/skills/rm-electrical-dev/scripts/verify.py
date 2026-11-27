#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""一条命令跑常用验证:兵种守卫 + 文档链接(+ 可选主机测试/构建)。

只封装现有 tools/,不复制实现。默认不编译、不烧录。

用法(工程根或任意目录均可):
  python .agents/skills/rm-electrical-dev/scripts/verify.py
  python .agents/skills/rm-electrical-dev/scripts/verify.py --tests    # 追加 run_fwk_tests.ps1
  python .agents/skills/rm-electrical-dev/scripts/verify.py --build sentry
退出码:0=全 PASS;1=有 FAIL;2=环境不可用。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]


def run(label: str, cmd: list[str]) -> bool:
    print(f"== {label}: {' '.join(cmd)}")
    p = subprocess.run(cmd, cwd=str(ROOT), text=True, encoding="utf-8", errors="replace")
    ok = p.returncode == 0
    print(f"-- {label}: {'PASS' if ok else 'FAIL'} (rc={p.returncode})")
    return ok


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="工程验证聚合")
    ap.add_argument("--tests", action="store_true", help="追加 tests/run_fwk_tests.ps1")
    ap.add_argument("--build", default=None, metavar="ROBOT", help="追加 build.ps1 <ROBOT> -Current")
    args = ap.parse_args(argv)

    if not (ROOT / "AGENTS.md").exists():
        print(f"FAIL: 找不到工程根(推导为 {ROOT})", file=sys.stderr)
        return 2

    py = sys.executable
    checks = [
        ("兵种守卫", [py, "tools/check_robot_scope.py", "--gate"]),
        ("文档链接", [py, "tools/refactor_gate.py", "check-links"]),
    ]
    if args.tests:
        checks.append(("主机测试", ["powershell", "-ExecutionPolicy", "Bypass", "-File",
                                    "tests/run_fwk_tests.ps1"]))
    if args.build:
        checks.append((f"构建 {args.build}", ["powershell", "-ExecutionPolicy", "Bypass", "-File",
                                              "build.ps1", args.build, "-Current"]))

    results = []
    for label, cmd in checks:
        try:
            results.append((label, run(label, cmd)))
        except FileNotFoundError:
            print(f"-- {label}: SKIP(命令不存在)", file=sys.stderr)
            results.append((label, None))

    failed = [label for label, ok in results if ok is False]
    print("=" * 60)
    print("RESULT:", "ALL PASS" if not failed else f"FAIL: {failed}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
