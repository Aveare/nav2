#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""R1 兵种范围守卫:兵种分支上不得改动其他兵种的代码。

判据来源:.Doc/spec/兵种分支协作规范.md §3.3(路径归属)、§9(守卫判据)。
本脚本只读 git 元数据与工作区状态,不改任何文件。

规则:
  ①当前分支可推断出兵种 R(infantry/sentry/heavy,含 feat/<robot>/… 短命分支)→
   改动集出现其他兵种路径 = FAIL;共享层改动 = WARN(热修,须回流 main);
  ②一次改动同时覆盖 ≥2 个兵种 = FAIL(拆分提交);
  ③main 或共享短命分支上改单个兵种目录 = WARN(应落到兵种分支);
  ④detached HEAD / 非 git 仓库 = 无法核对,给 WARN/退出码 2。

兵种路径:apps/<robot>/、config/<robot>_20xx/、tests/*<robot>*、.Doc/robots/<robot>/
(文档分库规则见 .Doc/spec/文档存放与管理规则.md)。

用法(工程根):
  python tools/check_robot_scope.py                        # 工作区+暂存(提交前)
  python tools/check_robot_scope.py --base main            # 相对 main 的全部未提交改动
  python tools/check_robot_scope.py --range origin/main..HEAD
  python tools/check_robot_scope.py --strict-shared        # 共享改动也判 FAIL
  python tools/check_robot_scope.py --gate                 # 供 refactor_gate 调用,单行结论
退出码:0=PASS 或仅 WARN;1=FAIL;2=环境不可用。
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ROBOTS = ("infantry", "sentry", "heavy")

# 构建产物/依赖目录不参与归属判定
_SKIP_RE = re.compile(r"^(build[^/]*|cmake-build[^/]*|firmware|\.git|\.vscode)/")

# 共享层(兵种分支上改动 → WARN;兵种目录之外的其它路径默认也按共享处理)
_SHARED_PREFIXES = (
    "apps/common/",
    "components/",
    "framework/",
    "ports/",
    "tools/",
    "tests/",
    ".agents/",
    ".Doc/",
    "Src/",
    "Inc/",
    "Drivers/",
    "Middlewares/",
)


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args], cwd=str(ROOT), capture_output=True, text=True,
        encoding="utf-8", errors="replace",
    )


def current_branch() -> str:
    p = git("rev-parse", "--abbrev-ref", "HEAD")
    return p.stdout.strip() if p.returncode == 0 else ""


def branch_robot(branch: str) -> str | None:
    """从分支名推断兵种:infantry / feat/infantry/xxx / fix/sentry-... 均可。"""
    if not branch or branch == "HEAD":
        return None
    if branch in ROBOTS:
        return branch
    for seg in re.split(r"[/_-]", branch):
        if seg in ROBOTS:
            return seg
    return None


def owner(path: str) -> str:
    """返回路径归属:'infantry'/'sentry'/'heavy' 或 'shared'。"""
    p = path.replace("\\", "/")
    while p.startswith("./"):
        p = p[2:]
    if p.startswith("apps/"):
        head = p.split("/")[1] if "/" in p else ""
        return head if head in ROBOTS else "shared"
    if p.startswith("config/"):
        m = re.match(r"config/([a-z0-9]+)_\d{4}/", p)
        if m and m.group(1) in ROBOTS:
            return m.group(1)
        return "shared"
    if p.startswith("tests/"):
        rest = p.split("/", 1)[1] if "/" in p else ""
        head = rest.split("/")[0]
        if head in ROBOTS:
            return head
        name = Path(p).name
        for robot in ROBOTS:
            if re.search(rf"(^|[_.\-]){robot}([_.\-]|$)", name):
                return robot
        return "shared"
    # 兵种专属文档库(.Doc/spec/文档存放与管理规则.md):目录即归属
    m = re.match(r"\.Doc/robots/([a-z0-9]+)/", p)
    if m and m.group(1) in ROBOTS:
        return m.group(1)
    return "shared"


def _paths_from_z(out: str, rename_aware: bool) -> list[str]:
    """解析带 -z 的 git 输出(porcelain 或 --name-only)。"""
    entries = [e for e in out.split("\0") if e]
    if not rename_aware:
        return entries
    paths: list[str] = []
    i = 0
    while i < len(entries):
        entry = entries[i]
        code, path = entry[:2], entry[3:]
        paths.append(path)
        if code and (code[0] in "RC" or code[1] in "RC"):
            i += 1
            if i < len(entries):
                paths.append(entries[i])
        i += 1
    return paths


def worktree_paths() -> list[str]:
    p = git("status", "--porcelain", "-z", "-uall")
    if p.returncode != 0:
        raise RuntimeError(p.stderr.strip() or "git status 失败")
    return _paths_from_z(p.stdout, rename_aware=True)


def diff_paths(spec: str) -> list[str]:
    p = git("diff", "--name-only", "-z", "--no-renames", spec)
    if p.returncode != 0:
        raise RuntimeError(p.stderr.strip() or f"git diff {spec} 失败")
    return _paths_from_z(p.stdout, rename_aware=False)


def collect(args) -> tuple[list[str], str]:
    """返回 (改动路径列表, 采集口径说明)。"""
    if args.range:
        return diff_paths(args.range), f"range {args.range}"
    if args.base:
        paths = diff_paths(args.base) + worktree_paths()
        return paths, f"工作区 vs {args.base}"
    return worktree_paths(), "工作区+暂存"


def evaluate(branch: str, paths: list[str], strict_shared: bool):
    robot = branch_robot(branch)
    touched: dict[str, list[str]] = {r: [] for r in ROBOTS}
    shared: list[str] = []
    for path in sorted(set(paths)):
        if _SKIP_RE.match(path.replace("\\", "/")):
            continue
        who = owner(path)
        if who == "shared":
            shared.append(path)
        else:
            touched[who].append(path)

    fails: list[str] = []
    warns: list[str] = []
    multi = [r for r in ROBOTS if touched[r]]

    if robot is None:
        if branch == "HEAD":
            warns.append("detached HEAD:无法推断兵种,只做混合改动检查")
        if len(multi) >= 2:
            fails.append(f"一次改动同时覆盖 {len(multi)} 个兵种 {multi}(必须拆分)")
        elif len(multi) == 1:
            warns.append(f"非兵种分支 {branch or '(detached)'} 改动 {multi[0]} 专属路径,"
                         f"应落到 {multi[0]} 分支")
    else:
        others = [r for r in multi if r != robot]
        for r in others:
            sample = touched[r][:4]
            fails.append(f"{branch} 是 {robot} 分支,却改动 {r} 专属路径:"
                         f"{sample}{' …' if len(touched[r]) > 4 else ''}")
        if shared:
            msg = (f"{robot} 分支改动共享层 {len(shared)} 个文件(应走 fix/shared-* → main):"
                   f"{shared[:4]}{' …' if len(shared) > 4 else ''}")
            (fails if strict_shared else warns).append(msg)

    return robot, touched, shared, fails, warns


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="R1 兵种范围守卫")
    ap.add_argument("--range", default=None, help="git 区间,如 origin/main..HEAD")
    ap.add_argument("--base", default=None, help="对照 ref 的未提交改动,如 main")
    ap.add_argument("--strict-shared", action="store_true", help="共享层改动也判 FAIL")
    ap.add_argument("--gate", action="store_true", help="单行结论(refactor_gate 用)")
    ap.add_argument("--json", action="store_true", help="JSON 输出")
    args = ap.parse_args(argv)

    if args.range and args.base:
        print("FAIL: --range 与 --base 只能二选一", file=sys.stderr)
        return 2
    if git("rev-parse", "--git-dir").returncode != 0:
        print("FAIL: 不在 git 仓库内", file=sys.stderr)
        return 2

    branch = current_branch()
    try:
        paths, source = collect(args)
    except RuntimeError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 2

    robot, touched, shared, fails, warns = evaluate(branch, paths, args.strict_shared)
    n_robot = sum(len(v) for v in touched.values())

    if args.json:
        print(json.dumps({
            "branch": branch, "robot": robot, "source": source,
            "touched": {r: v for r, v in touched.items() if v},
            "shared": shared, "fails": fails, "warns": warns,
        }, ensure_ascii=False, indent=2))
        return 1 if fails else 0

    verdict = "FAIL" if fails else ("WARN" if warns else "PASS")
    summary = (f"{branch or '(detached)'} | 兵种={robot or '-'} | {source} | "
               f"兵种文件 {n_robot} / 共享 {len(shared)}")
    if args.gate:
        detail = fails[0] if fails else (warns[0] if warns else "")
        print(f"[{verdict}] 兵种范围 {summary}" + (f" | {detail}" if detail else ""))
        return 1 if fails else 0

    print(f"[{verdict}] 兵种范围守卫  {summary}")
    for r in ROBOTS:
        if touched[r]:
            print(f"  {r:<9} {len(touched[r]):>2} 文件  {touched[r][:6]}")
    for path in shared[:8]:
        print(f"  {'shared':<9}       {path}")
    if len(shared) > 8:
        print(f"  {'shared':<9}  … 共 {len(shared)} 个")
    for msg in warns:
        print(f"  [WARN] {msg}")
    for msg in fails:
        print(f"  [FAIL] {msg}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
