#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G4 自基线执行器(自主研发框架升级总方案 §5-G4 / §13,B0a ⑤ 交付)。

与已退役的 audit_baseline.py 的关系:红旗扫描规则镜像其口径(历史来源;HANDLE_PATTERN/
WHILE_ONE_PATTERN/作用域),但基线不再是上游 manifest,而是本仓库 git 树的
「文件清单+sha256」快照(种子 seed-arch-before-upgrade / 各批收工 manifest)。
文件比对完全走 git 对象(cat-file blob → sha256),不扫工作树、不受未提交内容干扰。

用法:
  python tools/g4_baseline.py snapshot --ref arch-before-upgrade \
         --out .Doc/gate/seed-arch-before-upgrade
      从 <ref> 树生成快照:files.sha256 + git_rev + env.txt + redflags_scan.json
      (seed 与各批收工 manifest 同一入口;排除集 = G4 扫描集,见 EXCLUDE_PREFIXES)

  python tools/g4_baseline.py check --ref <tag|HEAD> [--baseline <dir>] [--approve <file>]
      将 --ref 树与基线快照比对:moved/added/deleted/changed;
      红旗 = 现树扫描结果对比基线 redflags_scan.json 的「新增站点」(计数增加)
           + .Doc/gate/redflags 中 verdict=待裁决 的开放项;
      PASS 当且仅当 新增红旗站点 = 0;--approve 提供批准清单时逐项核对。
退出码:0=PASS,1=FAIL,2=用法/环境错误。

红旗口径:
  - 句柄 token(hcan/htim/hadc/husart/huart/hspi/hi2c/hdma + 序号)
    在 apps/ components/ framework/ Src/ Inc/ 下计红(ports/ 是平台落地例外);
  - RTOS 头/调用(cmsis_os/FreeRTOS/task/osDelay/osThread*/vTaskDelete/
    taskENTER_CRITICAL/taskEXIT_CRITICAL)在 apps/ components/ framework/ 下计红;
  - while(1) 在第一方 .c/.h 计红,tests/ 排除;
  - 站点键 = (文件, token),计数增加视为新增站点(行号漂移不算新增)。

env.txt 约定: gcc_build=工程构建工具链(AGENTS.md: C:/Toolchain/bin,15.3.1),
gcc_path=PATH 上的另一版(14.2.1,勿混用)——G1a 版本门禁以 gcc_build 为准。
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from datetime import datetime

# G4 扫描集排除(方案 §5-G4 行):gate 产物自身不计入 diff
EXCLUDE_PREFIXES = ("build/", ".Doc/gate/", ".workbuddy/", ".zcode/", ".Doc/history/archive/review/")

HANDLE_PATTERN = re.compile(r"\b(?:hcan\d+|htim\d+|hadc\d+|husart\d+|huart\d+|hspi\d+|hi2c\d+|hdma\d+)\b")
RTOS_PATTERN = re.compile(
    r"(?:#include\s*[<\"](?:cmsis_os|FreeRTOS|task)\.h[>\"]|"
    r"\bosDelay\s*\(|\bosThreadCreate\s*\(|"
    r"\bosThreadDef\s*\(|\bvTaskDelete\s*\(|\btaskENTER_CRITICAL\s*\(|"
    r"\btaskEXIT_CRITICAL\s*\()"
)
WHILE_ONE_PATTERN = re.compile(r"while\s*\(\s*1\s*\)")
HANDLE_SCOPE_DIRS = ("apps/", "components/", "framework/", "Src/", "Inc/")
RTOS_SCOPE_DIRS = ("apps/", "components/", "framework/")
WHILE1_EXCLUDE_DIRS = ("tests/",)

REDFLAGS_FILE = os.path.join(".Doc", "gate", "redflags")


def run_git(args):
    res = subprocess.run(["git"] + args, capture_output=True)
    if res.returncode != 0:
        sys.stderr.write(res.stderr.decode("utf-8", "replace"))
        raise SystemExit(2)
    return res.stdout


def ls_tree(ref):
    """返回 {path: blob_sha1}(排除集已剔除;中文路径不转义)。"""
    out = run_git(["-c", "core.quotepath=false", "ls-tree", "-r", ref]).decode("utf-8", "replace")
    files = {}
    for line in out.splitlines():
        meta, _, path = line.partition("\t")
        parts = meta.split()
        if len(parts) < 3 or parts[1] != "blob":
            continue
        if any(path.startswith(p) for p in EXCLUDE_PREFIXES):
            continue
        files[path] = parts[2]
    return files


class BatchReader:
    """git cat-file --batch 流式读取器(一次进程遍历全部 blob)。"""

    def __init__(self):
        self.proc = subprocess.Popen(["git", "cat-file", "--batch"],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def read_blob(self, blob_sha):
        self.proc.stdin.write((blob_sha + "\n").encode())
        self.proc.stdin.flush()
        header = b""
        while not header.endswith(b"\n"):
            c = self.proc.stdout.read(1)
            if not c:
                raise SystemExit("cat-file --batch 流提前结束")
            header += c
        hdr = header.decode().strip()
        if hdr.endswith("missing"):
            raise SystemExit(f"blob 缺失: {blob_sha}")
        size = int(hdr.rsplit(" ", 1)[1])
        data = self.proc.stdout.read(size)
        self.proc.stdout.read(1)  # 尾部换行
        return data

    def close(self):
        self.proc.stdin.close()
        self.proc.wait()


def tree_digest(files, want_text=True):
    """一次遍历返回 ({path: sha256}, {path: text})。sha256 与 snapshot 口径一致。"""
    br = BatchReader()
    shas, texts = {}, {}
    for path, blob in sorted(files.items()):
        data = br.read_blob(blob)
        shas[path] = hashlib.sha256(data).hexdigest()
        if want_text and path.endswith((".c", ".h")):
            texts[path] = strip_comments(data.decode("utf-8", "replace"))
    br.close()
    return shas, texts


def strip_comments(text):
    """C 注释剥离(保留换行与行结构;字符串/字符字面量内容保留)。
    与 site_ledger.py 同一实现(该脚本 import 本函数);红旗扫描剥注释后,
    注释里的 while(1) 字样(如 err.h 文档)不再计入站点 —— 站点语义与
    B0 台账一致(§6-B0:剥注释,禁手数)。"""
    out, i, n = [], 0, len(text)
    NORMAL, LINE, BLOCK, STR, CHR = range(5)
    st = NORMAL
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if st == NORMAL:
            if c == "/" and nxt == "/":
                st = LINE; out.append("  "); i += 2; continue
            if c == "/" and nxt == "*":
                st = BLOCK; out.append("  "); i += 2; continue
            if c == '"':
                st = STR; out.append(c); i += 1; continue
            if c == "'":
                st = CHR; out.append(c); i += 1; continue
            out.append(c); i += 1
        elif st == LINE:
            if c == "\n":
                st = NORMAL; out.append("\n")
            else:
                out.append(" ")
            i += 1
        elif st == BLOCK:
            if c == "*" and nxt == "/":
                st = NORMAL; out.append("  "); i += 2; continue
            out.append("\n" if c == "\n" else " "); i += 1
        elif st == STR:
            if c == "\\":
                out.append(c); out.append(nxt); i += 2; continue
            if c == '"':
                st = NORMAL
            out.append(c); i += 1
        else:
            if c == "\\":
                out.append(c); out.append(nxt); i += 2; continue
            if c == "'":
                st = NORMAL
            out.append(c); i += 1
    return "".join(out)


def scan_redflags_by_path(files, texts):
    """镜像已退役 audit_baseline 的红旗口径,返回 {(path, token): count}。"""
    sites = {}
    for path, text in texts.items():
        if path not in files:
            continue
        toks = {}
        if path.startswith(HANDLE_SCOPE_DIRS):
            for m in HANDLE_PATTERN.finditer(text):
                toks[m.group(0)] = toks.get(m.group(0), 0) + 1
        if path.startswith(RTOS_SCOPE_DIRS):
            for m in RTOS_PATTERN.finditer(text):
                token = "rtos:" + m.group(0).strip()
                toks[token] = toks.get(token, 0) + 1
        if not path.startswith(WHILE1_EXCLUDE_DIRS):
            n = len(WHILE_ONE_PATTERN.findall(text))
            if n:
                toks["while(1)"] = toks.get("while(1)", 0) + n
        for tok, cnt in toks.items():
            sites[(path, tok)] = cnt
    return sites


def load_redflag_whitelist():
    """.Doc/gate/redflags 行格式: <path>|<token>|<verdict>|<reason>,verdict ∈ {豁免, 待裁决}"""
    entries = []
    if not os.path.exists(REDFLAGS_FILE):
        return entries
    with open(REDFLAGS_FILE, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 3:
                continue
            entries.append({
                "path": parts[0], "token": parts[1],
                "verdict": parts[2],
                "reason": parts[3] if len(parts) > 3 else "",
            })
    return entries


def tool_versions():
    """工具版本探针。gcc_build=工程构建工具链(AGENTS.md: C:/Toolchain/bin,15.3.1),
    gcc_path=PATH 上的另一版(14.2.1,勿混用)——G1a 版本门禁以 gcc_build 为准。"""
    probes = [
        ("gcc_build", r"C:/Toolchain/bin/arm-none-eabi-gcc"),
        ("gcc_path", "arm-none-eabi-gcc"),
        ("cmake_cube", r"C:/Users/ZhuanZ/AppData/Local/stm32cube/bundles/cmake/4.3.1+st.1/bin/cmake.exe"),
        ("clang-format", "clang-format"),
        ("python", "python"),
    ]
    vers = {}
    for key, tool in probes:
        try:
            res = subprocess.run([tool, "--version"], capture_output=True, timeout=20)
            vers[key] = res.stdout.decode("utf-8", "replace").splitlines()[0].strip()
        except Exception:
            vers[key] = "not-found"
    return vers


def cmd_snapshot(args):
    files = ls_tree(args.ref)
    rev_full = run_git(["rev-parse", args.ref + "^{commit}"]).decode().strip()
    tag_obj = run_git(["rev-parse", args.ref]).decode().strip()
    os.makedirs(args.out, exist_ok=True)
    shas, texts = tree_digest(files)
    with open(os.path.join(args.out, "files.sha256"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(f"{shas[p]}  {p}" for p in sorted(shas)) + "\n")
    with open(os.path.join(args.out, "git_rev"), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"{rev_full}  {args.ref}^{{commit}}\n{tag_obj}  {args.ref} (tag object)\n")
    vers = tool_versions()
    with open(os.path.join(args.out, "env.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"git_commit={rev_full}\ntag_object={tag_obj}\nref={args.ref}\n"
                f"date={datetime.now().isoformat(timespec='seconds')}\n")
        for k, v in vers.items():
            f.write(f"{k}={v}\n")
    sites = scan_redflags_by_path(files, texts)
    with open(os.path.join(args.out, "redflags_scan.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump({f"{p}|{t}": c for (p, t), c in sorted(sites.items())},
                  f, ensure_ascii=False, indent=1)
    print(f"[snapshot] ref={args.ref} rev={rev_full[:12]} files={len(files)} "
          f"redflag_sites={len(sites)} -> {args.out}")
    return 0


def cmd_check(args):
    base_dir = args.baseline
    base_sha_file = os.path.join(base_dir, "files.sha256")
    if not os.path.exists(base_sha_file):
        print(f"FAIL: 基线快照缺失 {base_sha_file}(先 snapshot)")
        return 1
    base_map = {}
    for line in open(base_sha_file, encoding="utf-8"):
        sha, _, path = line.strip().partition("  ")
        if path:
            base_map[path] = sha
    cur_files = ls_tree(args.ref)
    cur_shas, cur_texts = tree_digest(cur_files)
    base_by_sha = {}
    for p, s in base_map.items():
        base_by_sha.setdefault(s, []).append(p)

    added = sorted(p for p in cur_files if p not in base_map)
    deleted = sorted(p for p in base_map if p not in cur_files)
    changed = sorted(p for p in cur_files if p in base_map and base_map[p] != cur_shas[p])

    del_set = set(deleted)
    moved = []
    for p in list(added):
        cands = [q for q in base_by_sha.get(cur_shas[p], []) if q in del_set]
        if cands:
            moved.append((cands[0], p))
            del_set.discard(cands[0])
            added.remove(p)
            deleted = [q for q in deleted if q != cands[0]]

    # 红旗:现树扫描 vs 基线扫描
    cur_sites = scan_redflags_by_path(cur_files, cur_texts)
    rf_json = os.path.join(base_dir, "redflags_scan.json")
    base_sites = {}
    if os.path.exists(rf_json):
        base_sites = {tuple(k.split("|", 1)): v
                      for k, v in json.load(open(rf_json, encoding="utf-8")).items()}
    new_sites = [(k[0], k[1], c - base_sites.get(k, 0))
                 for k, c in cur_sites.items() if c > base_sites.get(k, 0)]

    wl = load_redflag_whitelist()
    wl_exempt = [e for e in wl if e["verdict"] != "待裁决"]
    open_items = [e for e in wl if e["verdict"] == "待裁决"]
    exempt_sites = {(e["path"], e["token"]) for e in wl_exempt}
    unresolved_sites = [s for s in new_sites if (s[0], s[1]) not in exempt_sites]

    print(f"=== G4 自基线比对: baseline={base_dir} vs ref={args.ref} ===")
    print(f"files: baseline={len(base_map)} current={len(cur_files)}")
    print(f"moved={len(moved)}")
    for a, b in moved:
        print(f"  [V] {a} -> {b}")
    print(f"added={len(added)}")
    for p in added:
        print(f"  [+] {p}")
    print(f"deleted={len(deleted)}")
    for p in deleted:
        print(f"  [-] {p}")
    print(f"changed={len(changed)}")
    for p in changed:
        print(f"  [M] {p}")
    print(f"redflags: new_sites={len(new_sites)} 豁免={len(wl_exempt)} 待裁决={len(open_items)}")
    for p, t, d in new_sites:
        tag = "[EXEMPT]" if (p, t) in exempt_sites else "[NEW-RED]"
        print(f"  {tag} {p} | {t} (+{d})")
    for e in open_items:
        print(f"  [OPEN-RED] {e['path']} | {e['token']} | {e['reason']}")

    ok = not unresolved_sites and not open_items
    if args.approve:
        approve = [l.strip() for l in open(args.approve, encoding="utf-8")
                   if l.strip() and not l.startswith("#")]
        missing = []
        for a, b in moved:
            if f"move:{a}->{b}" not in approve:
                missing.append(f"move:{a}->{b}")
        for p in added:
            if f"add:{p}" not in approve:
                missing.append(f"add:{p}")
        for p in changed:
            if f"mod:{p}" not in approve:
                missing.append(f"mod:{p}")
        for p in deleted:
            if f"del:{p}" not in approve:
                missing.append(f"del:{p}")
        print(f"approve: {len(approve)} 条;未在批准清单的变化 {len(missing)} 条")
        for m in missing:
            print(f"  [UNAPPROVED] {m}")
        ok = ok and not missing
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description="G4 自基线执行器(文件清单+sha256+红旗)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p1 = sub.add_parser("snapshot", help="从 git ref 生成快照/manifest")
    p1.add_argument("--ref", required=True)
    p1.add_argument("--out", required=True)
    p2 = sub.add_parser("check", help="基线快照 vs ref 树比对 + 红旗新增检查")
    p2.add_argument("--ref", default="HEAD")
    p2.add_argument("--baseline", default=".Doc/gate/seed-arch-before-upgrade")
    p2.add_argument("--approve", help="本批批准清单文件(可选,行格式 move:a->b / add:p / mod:p / del:p)")
    args = ap.parse_args()
    if args.cmd == "snapshot":
        raise SystemExit(cmd_snapshot(args))
    raise SystemExit(cmd_check(args))


if __name__ == "__main__":
    main()
