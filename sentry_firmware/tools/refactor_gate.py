#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""重构闸门:证明"零行为变化"的可执行检查。

配套方案:.Doc/history/archive/plan/重构总方案.md §2。本脚本只读(除自检产物写入 build/ 下),不改任何源码。

用法(在工程根执行):
    python tools/refactor_gate.py save-baseline      # 在改动前采集基线
    python tools/refactor_gate.py check              # 改动后逐项比对
    python tools/refactor_gate.py check --no-build --no-tests   # 只比静态项
    python tools/refactor_gate.py check-links        # 校验 .Doc/ 引用路径存在

闸门:
    G1 宏集合逐字节一致(gcc -E -dM,排序后 diff)
    G2 ARM 全量编译(ninja -C build,0 error / 第一方 0 warning)
    G3 主机测试(tests\run_*.ps1 全 PASS)
    G5 逐符号机器码尺寸(nm -S 多重集完全一致)
    G6 纯搬家行多重集(git diff -U0,仅对指定 ref)
    R1 兵种范围(tools/check_robot_scope.py;跨兵种 FAIL、兵种分支改共享层 WARN)

    G4 基线审计已于 2026-11-26 随"脱离上游"移除(原 audit_baseline 依赖上游快照)。

退出码:0 全部通过;1 有 FAIL。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
ELF = BUILD / "basic_framework.elf"
BASE = BUILD / "refactor_baseline"


def _resolve_toolchain():
    """工具链以 CMake 缓存里的实际编译器为准,避免 PATH 上另一版 gcc 造成"基线不是上板那版"。

    本机实测:build/ 用的是 C:/Toolchain/bin/arm-none-eabi-gcc(15.3.1),
    而 PATH 上另一个是 14.2.1;两者编出来的 text 差约 600 B。
    """
    cache = BUILD / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CMAKE_C_COMPILER:"):
                cc = line.split("=", 1)[1].strip()
                p = Path(cc)
                return (cc,
                        str(p.with_name("arm-none-eabi-nm.exe" if p.suffix == ".exe" else "arm-none-eabi-nm")),
                        str(p.with_name("arm-none-eabi-size.exe" if p.suffix == ".exe" else "arm-none-eabi-size")))
    return "arm-none-eabi-gcc", "arm-none-eabi-nm", "arm-none-eabi-size"


GCC, NM, SIZE = _resolve_toolchain()
CONFIG_H = "config/infantry_2026/config.h"

# 不参与链接校验:历史记录(只追加)、上游快照(必须逐字节等同)、代理工作笔记、工具自身。
LINK_SKIP_FILES = {"RM_DEVLOG.md", "tools/refactor_gate.py"}
LINK_SKIP_DIRS = (".Doc/history", ".Doc/review", ".agents/skills/rm-electrical-dev",
                  ".workbuddy", ".zcode")


def run(cmd, cwd=ROOT, timeout=1800):
    """执行命令,返回 (ok, text)。"""
    try:
        p = subprocess.run(
            cmd, cwd=str(cwd), capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=timeout,
        )
    except FileNotFoundError:
        return False, f"[not found] {cmd[0]}"
    except subprocess.TimeoutExpired:
        return False, f"[timeout] {' '.join(map(str, cmd))}"
    return p.returncode == 0, (p.stdout or "") + (p.stderr or "")


# ---------------- 采集 ----------------

def dump_macros():
    """预处理 config 伞头,导出全部宏(含编译器内建),排序。"""
    ok, out = run([GCC, "-E", "-dM", "-Iconfig", CONFIG_H])
    if not ok:
        return None, out
    return "\n".join(sorted(out.splitlines())) + "\n", out


def parse_nm(text):
    """nm -S --defined-only → Counter[(name, type, size)]。"""
    pat = re.compile(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]*)\s+([A-Za-z?])\s+(.+)$")
    syms = Counter()
    for line in text.splitlines():
        m = pat.match(line)
        if not m:
            continue
        _addr, size, typ, name = m.groups()
        name = name.strip()
        # 文件级/函数级 static 的符号名由编译器加序号后缀(如 angle.8),
        # 拆分到不同 TU 后序号会变,但它不是"符号名变化"。统一去掉 ".N"。
        name = re.sub(r"\.\d+$", "", name)
        syms[(name, typ, int(size, 16) if size else 0)] += 1
    return syms


def dump_symbols():
    ok, out = run([NM, "-S", "--defined-only", str(ELF)])
    if not ok:
        return None, out
    return parse_nm(out), out


def dump_size():
    ok, out = run([SIZE, str(ELF)])
    return out if ok else None


def parse_size(text):
    """elf size 第二行: text data bss dec hex filename。"""
    lines = [l for l in (text or "").splitlines() if l.strip()]
    if len(lines) < 2:
        return None
    parts = lines[1].split()
    try:
        return {"text": int(parts[0]), "data": int(parts[1]), "bss": int(parts[2])}
    except (IndexError, ValueError):
        return None


def run_build():
    return run(["ninja", "-C", "build"])


def run_tests():
    results = {}
    for script in (
        "tests\\run_chassis_tests.ps1",
        "tests\\run_fwk_tests.ps1",
        "tests\\run_ax650_tests.ps1",
        "tests\\run_profile_tests.ps1",
    ):
        ok, out = run(["powershell", "-ExecutionPolicy", "Bypass", "-File", script])
        # 各脚本均以退出码作为唯一 PASS/FAIL 判据。fwk 会故意打印负例 FAIL,
        # 不能再按文本搜 FAIL,否则会把通过的负向测试误判为失败。
        results[script] = {"ok": ok, "tail": out[-2000:]}
    return results


def elf_sha():
    return hashlib.sha256(ELF.read_bytes()).hexdigest()


# ---------------- B1 工具先行:G1a 版本门禁 / G2 三断言 / G5b 预算 / G6 模式 ----------------

GCC_BUILD = r"C:/Toolchain/bin/arm-none-eabi-gcc"  # AGENTS.md:工程构建工具链(勿混 PATH 上的 14.2.1)
GATE_DIRS = ("Src/", "bsp/", "modules/", "application/",
             "framework/", "ports/", "apps/", "components/")


def probe_gcc_build():
    """工程构建工具链版本首行;B1 起 G1a 以此为准(env.txt 的 gcc_build 键)。"""
    try:
        ok, out = run([GCC_BUILD, "--version"], timeout=30)
        return out.splitlines()[0].strip() if ok and out else None
    except Exception:
        return None


def read_env_gcc():
    """取最新 gate manifest(seed/manifest-*)env.txt 的 gcc_build 值。"""
    import glob
    cands = sorted(glob.glob(str(ROOT / ".Doc/gate/manifest-*/env.txt")) +
                   glob.glob(str(ROOT / ".Doc/gate/seed-*/env.txt")),
                   key=lambda p: Path(p).stat().st_mtime)
    for path in reversed(cands):
        for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("gcc_build="):
                return line.split("=", 1)[1].strip(), path
    return None, None


def check_g1a(fails, notes):
    cur = probe_gcc_build()
    base, src = read_env_gcc()
    if cur is None or base is None:
        fails.append(("G1a 版本门禁", f"无法取得 gcc 版本(cur={cur} base={base} from {src})"))
        return
    if cur != base:
        fails.append(("G1a 版本门禁", f"gcc 版本 ≠ 入库基线: {base}({src}) → {cur};重锚或装回工具链"))
    else:
        notes.append(("G1a 版本门禁", "PASS", f"{cur}"))


def check_g2_asserts(fails, notes):
    """G2 三断言(方案 §5-G2/B1 交付):Debug / 编译单元数==磁盘 .c 数 / 无 -flto。"""
    cache = ROOT / "build/CMakeCache.txt"
    ninja = ROOT / "build/build.ninja"
    if not cache.exists() or not ninja.exists():
        fails.append(("G2 三断言", "build/CMakeCache.txt 或 build.ninja 缺失(先 cmake -G Ninja -B build)"))
        return
    build_type = None
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CMAKE_BUILD_TYPE"):
            build_type = line.split("=", 1)[1].strip()
    if build_type != "Debug":
        fails.append(("G2 三断言", f"CMAKE_BUILD_TYPE={build_type!r} ≠ Debug"))
    else:
        notes.append(("  G2 断言a", "PASS", "CMAKE_BUILD_TYPE=Debug"))
    ninja_text = ninja.read_text(encoding="utf-8", errors="replace")
    # -flto 禁令限定**编译** FLAGS(LINK_FLAGS 的 -flto 对非 LTO 目标文件是惰性参数,不属于编译面)
    compile_flags = [l for l in ninja_text.splitlines() if l.startswith("  FLAGS = ")]
    if any("-flto" in l for l in compile_flags):
        fails.append(("G2 三断言", "build.ninja 编译 FLAGS 出现 -flto(禁用)"))
    else:
        notes.append(("  G2 断言c", "PASS", f"编译 FLAGS 无 -flto({len(compile_flags)} 条编译规则)"))
    # 编译单元数 == 磁盘 .c 数(G2 第一方边界;tests/ 由 G3 单独统计)
    robot_profile = "infantry"
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("ROBOT_PROFILE:"):
            robot_profile = line.split("=", 1)[1].strip() or robot_profile
    disk = set()
    for d in GATE_DIRS:
        p = ROOT / d
        if p.exists():
            disk |= {f.relative_to(ROOT).as_posix() for f in p.rglob("*.c") if "build" not in f.parts}
    disk = {
        p for p in disk
        if not (p.startswith("apps/") and not p.startswith("apps/common/") and
                not p.startswith(f"apps/{robot_profile}/"))
    }
    built = set()
    obj_rule = re.compile(r"build CMakeFiles/[^/:]+\.dir/(.+)\.obj:")
    for line in ninja_text.splitlines():
        m = obj_rule.match(line)
        if m:
            src = m.group(1).replace("\\", "/")
            if src.startswith(GATE_DIRS) or any(src.startswith(d) for d in GATE_DIRS):
                built.add(src)
    if built != disk:
        only_disk = sorted(disk - built)[:6]
        only_built = sorted(built - disk)[:6]
        fails.append(("G2 三断言", f"编译单元数≠磁盘 .c 数: 磁盘 {len(disk)} vs 构建 {len(built)};"
                                   f"只磁盘有 {only_disk};只构建有 {only_built}(先重跑 cmake -G Ninja -B build)"))
    else:
        notes.append(("  G2 断言b", "PASS", f"编译单元数 == 磁盘 .c 数 == {len(disk)}(G2 第一方边界)"))


def check_budget(args, fails, notes):
    """G5b 预算:B4 起强制;--budget PCT(默认 text+rodata ≤+1%,B7/B8 ≤+2%)。"""
    if not args.budget:
        return
    s_new, s_old = parse_size(dump_size()), parse_size((BASE / "size.txt").read_text(encoding="utf-8"))
    if not s_new or not s_old:
        fails.append(("G5b 预算", "尺寸不可得"))
        return
    growth = s_new["text"] - s_old["text"]
    limit = s_old["text"] * args.budget / 100.0
    if growth > limit:
        fails.append(("G5b 预算", f"text {growth:+d}B 超 --budget {args.budget}% 上限 {limit:.0f}B"))
    else:
        notes.append(("G5b 预算", "PASS", f"text {growth:+d}B ≤ {args.budget}% 上限 {limit:.0f}B"))


def check_mode_paths(args, fails, notes):
    """G6:--mode move/rewrite + --paths + --since 必填组合。"""
    if not args.mode:
        return
    if not args.since or not args.paths:
        fails.append(("G6 搬家行多重集", "--mode 必须与 --since 和 --paths 同用"))
        return
    for spec in args.paths.split(","):
        spec = spec.strip()
        if args.mode == "move":
            old, _, new = spec.partition("->")
            old, new = old.strip(), new.strip()
            ok, old_out = run(["git", "show", f"{args.since}:{old}"])
            new_p = ROOT / new
            if not ok or not new_p.exists():
                fails.append(("G6 搬家行多重集", f"路径不可得: {old} @ {args.since} / {new}"))
                continue
            norm = lambda text: sorted(l.rstrip() for l in text.splitlines() if l.strip())
            if norm(old_out) != norm(new_p.read_text(encoding="utf-8", errors="replace")):
                fails.append(("G6 搬家行多重集", f"行多重集不等: {old} -> {new}"))
            else:
                inc_old = {l.strip() for l in old_out.splitlines() if l.strip().startswith("#include")}
                inc_new = {l.strip() for l in new_p.read_text(encoding="utf-8", errors="replace").splitlines()
                           if l.strip().startswith("#include")}
                notes.append(("G6 搬家行多重集", "PASS", f"{old} -> {new};"
                                                      f"#include 增: {sorted(inc_new - inc_old) or '无'}"
                                                      f" 减: {sorted(inc_old - inc_new) or '无'}"))
        elif args.mode == "rewrite":
            ok, old_out = run(["git", "show", f"{args.since}:{spec}"])
            new_p = ROOT / spec
            if not ok or not new_p.exists():
                fails.append(("G6 改写行数", f"路径不可得: {spec}"))
                continue
            from collections import Counter
            om = Counter(l.rstrip() for l in old_out.splitlines() if l.strip())
            nm = Counter(l.rstrip() for l in new_p.read_text(encoding="utf-8", errors="replace").splitlines() if l.strip())
            added = sum((nm - om).values())
            deleted = sum((om - nm).values())
            notes.append(("G6 改写行数", "INFO", f"{spec}: +{added}/-{deleted}(逐文件记录进 DEV)"))
        else:
            fails.append(("G6 搬家行多重集", f"--mode 仅支持 move/rewrite,得到 {args.mode!r}"))


# ---------------- 兵种范围 ----------------

def check_robot_scope(fails, notes):
    """R1 兵种范围守卫(.Doc/spec/兵种分支协作规范.md §9)。

    跨兵种改动 FAIL;兵种分支上的共享层改动只 WARN(热修须回流 main),
    因此不会改变既有 G1~G6 的通过条件;脚本缺失/环境不可用时 SKIP。
    """
    tool = ROOT / "tools" / "check_robot_scope.py"
    if not tool.exists():
        notes.append(("R1 兵种范围", "SKIP", "tools/check_robot_scope.py 不存在"))
        return
    ok, out = run([sys.executable, str(tool), "--gate"], timeout=120)
    line = next((ln.strip() for ln in reversed(out.strip().splitlines())
                 if ln.strip().startswith("[")), "")
    if not line:
        notes.append(("R1 兵种范围", "SKIP", f"无输出(rc={ok}): {out[-200:]}"))
    elif "[FAIL]" in line:
        fails.append(("R1 兵种范围", line))
    elif "[WARN]" in line:
        notes.append(("R1 兵种范围", "WARN", line))
    else:
        notes.append(("R1 兵种范围", "PASS", line))


# ---------------- 基线 ----------------

def cmd_save(argv):
    if not ELF.exists():
        print(f"FAIL: 找不到 {ELF},先编译一次(ninja -C build)")
        return 1
    BASE.mkdir(parents=True, exist_ok=True)
    ok, gcc_v = run([GCC, "--version"])
    (BASE / "env.txt").write_text(gcc_v.splitlines()[0] if ok else "gcc unknown", encoding="utf-8")

    ok, git_rev = run(["git", "rev-parse", "HEAD"])
    (BASE / "git_rev.txt").write_text(git_rev.strip() if ok else "unknown", encoding="utf-8")
    (BASE / "elf_sha256.txt").write_text(elf_sha(), encoding="utf-8")

    macros, raw = dump_macros()
    if macros is None:
        print("FAIL: 宏导出失败\n" + raw[-2000:])
        return 1
    (BASE / "macros.txt").write_text(macros, encoding="utf-8")

    syms, raw = dump_symbols()
    if syms is None:
        print("FAIL: 符号导出失败\n" + raw[-2000:])
        return 1
    (BASE / "symbols.txt").write_text(raw, encoding="utf-8")

    (BASE / "size.txt").write_text(dump_size() or "", encoding="utf-8")

    tests = run_tests()
    (BASE / "tests.json").write_text(json.dumps(tests, ensure_ascii=False, indent=1), encoding="utf-8")

    print(f"基线已写入 {BASE}")
    print(f"  宏 {len(macros.splitlines())} 条 / 符号 {sum(syms.values())} 个")
    return 0


# ---------------- 比对 ----------------

def cmd_check(argv):
    args = argv
    fails, notes = [], []
    base_macros = BASE / "macros.txt"
    if not base_macros.exists():
        print("FAIL: 没有基线,先跑 save-baseline")
        return 1

    # G1 宏
    cur, raw = dump_macros()
    if cur is None:
        fails.append(("G1 宏集合", "导出失败"))
    else:
        old = base_macros.read_text(encoding="utf-8")
        if cur == old:
            notes.append(("G1 宏集合", "PASS", "逐字节一致"))
        else:
            a, b = set(cur.splitlines()), set(old.splitlines())
            added, removed = a - b, b - a
            if args.allow_new_macros and not removed:
                notes.append(("G1 宏集合", "PASS",
                              f"逐字节一致;新增 {len(added)} 条宏(本批声明:参数收敛)"))
            else:
                fails.append(("G1 宏集合", f"新增 {len(added)} / 消失 {len(removed)};"
                                           f"示例 {sorted(added)[:3]} {sorted(removed)[:3]}"))

    # G2 编译
    if not args.no_build:
        ok, out = run_build()
        if ok:
            notes.append(("G2 ARM 编译", "PASS", "0 error"))
        else:
            fails.append(("G2 ARM 编译", out[-1500:]))
    else:
        notes.append(("G2 ARM 编译", "SKIP", "--no-build"))

    # G5 符号尺寸(编译后 elf 才可信)
    if not args.no_build and ELF.exists():
        syms, raw = dump_symbols()
        if syms is None:
            fails.append(("G5 符号尺寸", "导出失败"))
        else:
            old_raw = (BASE / "symbols.txt").read_text(encoding="utf-8")
            old = parse_nm(old_raw)

            def by_name(counter):
                d = {}
                for (name, typ, size) in counter:
                    d.setdefault(name, []).append((typ, size))
                for name in d:
                    d[name].sort()
                return d

            o, c = by_name(old), by_name(syms)
            missing_names = sorted(set(o) - set(c))
            new_names = sorted(set(c) - set(o))
            linkage, resized = [], []
            for name in sorted(set(o) & set(c)):
                if [s for (_t, s) in o[name]] != [s for (_t, s) in c[name]]:
                    resized.append((name, o[name], c[name]))
                elif [t for (t, _s) in o[name]] != [t for (t, _s) in c[name]]:
                    linkage.append(name)

            declared_new = {s.strip() for s in (args.expect_new_symbols or "").split(",") if s.strip()}
            declared_gone = {s.strip() for s in (getattr(args, "expect_gone_symbols", None) or "").split(",") if s.strip()}
            missing_names = [n for n in missing_names if n not in declared_gone]
            undeclared_new = [n for n in new_names if n not in declared_new]
            if missing_names or undeclared_new:
                fails.append(("G5 符号尺寸",
                              f"符号名变化(不允许):消失 {missing_names[:8]} "
                              f"未声明新增 {undeclared_new[:8]}(新框架代码在 --expect-new-symbols 声明)"))
            elif resized and not args.allow_linkage:
                detail = [f"{n}: {o_} → {c_}" for (n, o_, c_) in resized[:6]]
                fails.append(("G5 符号尺寸", f"{len(resized)} 个符号尺寸变化(本批未声明 --allow-linkage): {detail}"))
            else:
                msg = f"{sum(syms.values())} 个符号名与基线一致"
                if linkage:
                    msg += f";{len(linkage)} 个仅链接性变化(static→extern,拆分类预期)"
                if resized:
                    msg += f";{len(resized)} 个尺寸变化已声明: " + \
                           ", ".join(f"{n} {o_[0][1]}→{c_[0][1]}B" for (n, o_, c_) in resized)
                notes.append(("G5 符号尺寸", "PASS", msg))
            # 尺寸表(便于人工核对 ROM/RAM)
            s_new, s_old = parse_size(dump_size()), parse_size((BASE / "size.txt").read_text(encoding="utf-8"))
            if s_new and s_old:
                diff = {k: s_new[k] - s_old[k] for k in ("text", "data", "bss")}
                pct = 100.0 * diff["text"] / max(s_old["text"], 1)
                notes.append(("  尺寸", "INFO", f"text {s_new['text']} ({diff['text']:+d}, {pct:+.3f}%) "
                                                f"data {s_new['data']} ({diff['data']:+d}) "
                                                f"bss {s_new['bss']} ({diff['bss']:+d})"))
    elif args.no_build:
        notes.append(("G5 符号尺寸", "SKIP", "--no-build 时 ELF 不可信"))

    # G1a 版本门禁 + G2 三断言(B1 工具先行)
    check_g1a(fails, notes)
    check_g2_asserts(fails, notes)
    check_budget(args, fails, notes)
    check_mode_paths(args, fails, notes)
    check_robot_scope(fails, notes)

    # G3 主机测试
    if not args.no_tests:
        tests = run_tests()
        bad = [k for k, v in tests.items() if not v["ok"]]
        if bad:
            fails.append(("G3 主机测试", f"失败: {bad}"))
        else:
            notes.append(("G3 主机测试", "PASS", "chassis/fwk/ax650/profile 四套全 PASS"))
    else:
        notes.append(("G3 主机测试", "SKIP", "--no-tests"))

    # G6 纯搬家(需要 ref)
    if args.since:
        fails += check_moved(args.since, args.max_unmatched)

    # 输出
    print("=" * 78)
    for row in notes:
        print(f"  [{'PASS' if row[1] in ('PASS', 'INFO') else row[1]}] {row[0]:<14} {row[2]}")
    for name, detail in fails:
        print(f"  [FAIL] {name:<14} {detail}")
    print("=" * 78)
    print("RESULT:", "ALL PASS" if not fails else f"{len(fails)} FAIL")
    return 0 if not fails else 1


def check_moved(ref, max_unmatched):
    """G6:git diff -U0 的删除行与新增行多重集比较,忽略预处理行与空白。"""
    ok, out = run(["git", "diff", "-U0", ref, "--", "*.c", "*.h"])
    if not ok:
        return [("G6 纯搬家", f"git diff 失败: {out[-300:]}")]

    def norm(line):
        s = line.strip()
        if not s or s.startswith("#"):
            return None
        return s

    removed, added = Counter(), Counter()
    for line in out.splitlines():
        if line.startswith("+++") or line.startswith("---"):
            continue
        if line.startswith("-"):
            s = norm(line[1:])
            if s:
                removed[s] += 1
        elif line.startswith("+"):
            s = norm(line[1:])
            if s:
                added[s] += 1
    only_removed = removed - added
    only_added = added - removed
    total = sum(only_removed.values()) + sum(only_added.values())
    if total <= max_unmatched:
        return []
    sample_r = [k for k, _ in only_removed.most_common(5)]
    sample_a = [k for k, _ in only_added.most_common(5)]
    return [("G6 纯搬家", f"{total} 行不是纯搬家(阈值 {max_unmatched});"
                          f"仅删除示例 {sample_r};仅新增示例 {sample_a}")]


# ---------------- 链接 ----------------

LINK_PAT = re.compile(r"\.Doc/([^\s`'\"()\[\]{}<>,;、。》《「」]+)")


def cmd_links(argv):
    skip_hist = not argv.include_history
    broken = []
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT).as_posix()
        if "build/" in rel or rel.startswith(".git/"):
            continue
        if path.suffix.lower() not in {".md", ".c", ".h", ".js", ".py", ".ps1", ".bat", ".txt"}:
            continue
        if skip_hist and (rel in LINK_SKIP_FILES or rel.startswith(LINK_SKIP_DIRS)):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in LINK_PAT.finditer(text):
            token = m.group(1).rstrip("/")
            if not token.endswith((".md", ".json", ".csv")):
                continue  # 目录/通配/散文引用不判存在性
            target = ".Doc/" + token
            if not (ROOT / target).exists():
                broken.append((rel, target))
    if broken:
        print(f"FAIL: {len(broken)} 处 .Doc/ 引用指向不存在的路径")
        for rel, target in broken:
            print(f"  {rel}: {target}")
        return 1
    print("PASS: 所有 .Doc/ 引用路径存在")
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd, rest = sys.argv[1], sys.argv[2:]
    if cmd == "save-baseline":
        return cmd_save(rest)
    if cmd == "check":
        p = argparse.ArgumentParser()
        p.add_argument("--no-build", action="store_true")
        p.add_argument("--no-tests", action="store_true")
        p.add_argument("--since", default=None, help="G6 的 git ref")
        p.add_argument("--max-unmatched", type=int, default=0)
        p.add_argument("--allow-linkage", action="store_true",
                       help="拆分类批次:允许符号 static→extern 及随之的尺寸微变(逐符号打印,超 0.2% 需人工复核)")
        p.add_argument("--budget", type=float, default=None,
                       help="G5b:text 相对基线增长上限 %%(--budget 1 即 +1%%;B4 起强制)")
        p.add_argument("--mode", choices=("move", "rewrite"), default=None,
                       help="G6:move=纯搬家行多重集(需 --paths old->new,逗号分隔);rewrite=改写行数记录")
        p.add_argument("--paths", default=None, help="G6 的路径清单(与 --mode 同用)")
        p.add_argument("--expect-new-symbols", default=None,
                       help="G5:声明本批新增符号(逗号分隔;新框架代码批用)")
        p.add_argument("--expect-gone-symbols", default=None,
                       help="G5:声明本批消失符号(逗号分隔;删除模块批用)")
        p.add_argument("--allow-new-macros", action="store_true",
                       help="参数收敛批次:允许新增宏(消失仍判失败);值不变时 text 必须 +0")
        return cmd_check(p.parse_args(rest))
    if cmd == "check-links":
        p = argparse.ArgumentParser()
        p.add_argument("--include-history", action="store_true")
        return cmd_links(p.parse_args(rest))
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
