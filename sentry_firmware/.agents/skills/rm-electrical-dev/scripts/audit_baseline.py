#!/usr/bin/env python3
"""Read-only baseline audit for rm-electrical-dev (Python twin of audit_baseline.ps1).

Compares a working project against the skill's pinned YueLu snapshot manifest
(assets/yuelu-basic-framework.manifest.json, per-file SHA-256) and reports:
  - Unchanged : byte-identical to the pinned commit
  - Changed   : content differs (first-party .c/.h get a heuristic symbol delta)
  - Missing   : in baseline, absent in project
  - Added     : in project, absent in baseline (needs manual classification)

Self-verifies the snapshot against the manifest first; exits 3 if the snapshot
itself is corrupted (all diff conclusions would be unreliable).

Never modifies the project or the snapshot. Writes a file only with --report.
!!! 孪生脚本: audit_baseline.ps1 与本文件逻辑必须保持一致, 修改任一端需同步另一端 !!!
Usage:
  python audit_baseline.py --project "<project_root>" [--skill "<skill_root>"]
      [--ignore-eol] [--json] [--report "<path>"]
"""

import argparse
import fnmatch
import hashlib
import json
import os
import re
import sys

TEXT_EXTENSIONS = {
    ".c", ".h", ".md", ".s", ".ld", ".ioc", ".txt", ".json",
    ".yml", ".yaml", ".clangd", ".cfg", ".gitignore",
}
SCAN_DIRS = ["application", "bsp", "config", "modules", "Src", "Inc", "tests"]
SKIP_DIRS = {".git", "build", "CMakeFiles", ".cache", "__pycache__", "node_modules"}
ROOT_PATTERNS = [
    "*.md", "LICENSE*", "Makefile*", "CMakeLists.txt", "*.ioc", "*.ld",
    "*.s", "*.cfg", "*.svd", "*.jflash", "*.ps1", ".clangd", ".mxproject", ".gitignore",
]
FUNC_KEYWORDS = {
    "if", "for", "while", "switch", "else", "do", "return", "sizeof",
    "struct", "union", "enum", "typedef", "_",
}
# Red-flag patterns (cf. references/ai-constraints.md / evolution-roadmap.md):
#  - direct HAL handle refs in logic layers (application/, Src/, Inc/)
#  - new while(1) as error handling anywhere in first-party .c/.h
HANDLE_PATTERN = re.compile(
    r"\b(?:hcan\d+|htim\d+|hadc\d+|husart\d+|huart\d+|hspi\d+|hi2c\d+|hdma\d+)\b"
)
WHILE_ONE_PATTERN = re.compile(r"while\s*\(\s*1\s*\)")
HANDLE_SCOPE_DIRS = ("application/", "Src/", "Inc/")
# while(1)红旗排除tests/目录: 测试主循环使用while(1)合法(见coding-contract修改检查)
WHILE1_EXCLUDE_DIRS = ("tests/",)
FUNC_PATTERN = re.compile(
    r"(?ms)^[ \t]*(?:(?:static|inline|const|extern|[A-Za-z_][A-Za-z0-9_]*)[ \t\*]+)+"
    r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\([^;{}]*\)[ \t\r\n]*\{"
)
DEFINE_PATTERN = re.compile(r"(?m)^[ \t]*#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)")
TYPE_PATTERN_BRACE = re.compile(r"(?s)\}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;")
TYPE_PATTERN_SIMPLE = re.compile(r"(?m)^[ \t]*typedef[ \t]+[^;{}()]*?([A-Za-z_][A-Za-z0-9_]*)[ \t]*;")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_hash(path: str, normalize_eol: bool) -> str:
    with open(path, "rb") as f:
        data = f.read()
    if normalize_eol:
        data = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return sha256_bytes(data)


def read_text(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def pattern_tokens(text: str) -> set:
    toks = {m.group(0) for m in HANDLE_PATTERN.finditer(text)}
    if WHILE_ONE_PATTERN.search(text):
        toks.add("while(1)")
    return toks


def new_red_flags(rel: str, base_text: str, proj_text: str) -> list:
    """Return red-flag tokens newly introduced in rel vs the baseline text."""
    new_toks = pattern_tokens(proj_text) - pattern_tokens(base_text)
    flags = []
    for tok in sorted(new_toks):
        if tok == "while(1)":
            if not rel.startswith(WHILE1_EXCLUDE_DIRS):
                flags.append(f"{rel}: {tok}")
        elif rel.startswith(HANDLE_SCOPE_DIRS):
            flags.append(f"{rel}: {tok}")
    return flags


def extract_symbols(path: str) -> dict:
    """Heuristic symbol extraction: function defs, #defines, typedef names."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    funcs = {
        m.group(1)
        for m in FUNC_PATTERN.finditer(text)
        if m.group(1) not in FUNC_KEYWORDS
    }
    defines = {m.group(1) for m in DEFINE_PATTERN.finditer(text)}
    types = {m.group(1) for m in TYPE_PATTERN_BRACE.finditer(text)}
    types |= {m.group(1) for m in TYPE_PATTERN_SIMPLE.finditer(text)}
    return {"funcs": funcs, "defines": defines, "types": types}


def diff_symbols(base: dict, proj: dict) -> dict:
    added_f = sorted(proj["funcs"] - base["funcs"])
    removed_f = sorted(base["funcs"] - proj["funcs"])
    added_d = sorted(proj["defines"] - base["defines"])
    removed_d = sorted(base["defines"] - proj["defines"])
    added_t = sorted(proj["types"] - base["types"])[:20]
    removed_t = sorted(base["types"] - proj["types"])[:20]
    if not any([added_f, removed_f, added_d, removed_d, added_t, removed_t]):
        return None
    return {
        "added_functions": added_f,
        "removed_functions": removed_f,
        "added_defines": added_d,
        "removed_defines": removed_d,
        "added_types": added_t,
        "removed_types": removed_t,
    }


def scan_added(project: str, manifest_paths: set) -> list:
    added = set()
    for d in SCAN_DIRS:
        root = os.path.join(project, d)
        if not os.path.isdir(root):
            continue
        for cur, dirs, files in os.walk(root):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS and not x.startswith("cmake-build-")]
            for name in files:
                rel = os.path.relpath(os.path.join(cur, name), project).replace(os.sep, "/")
                if rel not in manifest_paths:
                    added.add(rel)
    try:
        root_files = os.listdir(project)
    except OSError:
        root_files = []
    for name in root_files:
        full = os.path.join(project, name)
        if not os.path.isfile(full):
            continue
        if any(fnmatch.fnmatch(name, pat) for pat in ROOT_PATTERNS) and name not in manifest_paths:
            added.add(name)
    return sorted(added)


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only baseline audit against the pinned YueLu snapshot.")
    parser.add_argument("--project", required=True, help="Working project root directory")
    parser.add_argument("--skill", default="", help="Skill root (default: two levels above this script)")
    parser.add_argument("--ignore-eol", action="store_true", help="Normalize CRLF/LF before hashing text files")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    parser.add_argument("--report", default="", help="Optional path to also write the report (otherwise stdout only)")
    args = parser.parse_args()

    project = os.path.abspath(args.project)
    if not os.path.isdir(project):
        print(f"error: ProjectRoot is not a directory: {project}", file=sys.stderr)
        return 2

    skill = os.path.abspath(args.skill) if args.skill else os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))
    manifest_path = os.path.join(skill, "assets", "yuelu-basic-framework.manifest.json")
    if not os.path.isfile(manifest_path):
        print(f"error: manifest not found: {manifest_path}", file=sys.stderr)
        return 2
    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    snapshot_root = os.path.join(skill, "assets", "yuelu-basic-framework")

    # 快照自检:基线必须与manifest一致,否则后续所有比对结论失真(P0盲区修复)
    snapshot_bad = []
    for entry in manifest["files"]:
        snap_file = os.path.join(snapshot_root, *entry["path"].split("/"))
        if not os.path.isfile(snap_file):
            snapshot_bad.append(("MISSING", entry["path"]))
            continue
        with open(snap_file, "rb") as f:
            if hashlib.sha256(f.read()).hexdigest() != entry["sha256"]:
                snapshot_bad.append(("HASH", entry["path"]))
    if snapshot_bad:
        print(f"error: snapshot corrupted ({len(snapshot_bad)} files) - baseline audit unreliable:", file=sys.stderr)
        for kind, p in snapshot_bad[:10]:
            print(f"  {kind} {p}", file=sys.stderr)
        return 3

    changed, missing = [], []
    unchanged = 0
    for entry in manifest["files"]:
        rel = entry["path"]
        proj_file = os.path.join(project, *rel.split("/"))
        if not os.path.isfile(proj_file):
            missing.append(rel)
            continue
        ext = os.path.splitext(rel)[1].lower()
        normalize = args.ignore_eol and ext in TEXT_EXTENSIONS
        if file_hash(proj_file, normalize) == entry["sha256"]:
            unchanged += 1
            continue
        item = {"path": rel}
        snap_file = os.path.join(snapshot_root, *rel.split("/"))
        if ext in (".c", ".h"):
            if os.path.isfile(snap_file):
                delta = diff_symbols(extract_symbols(snap_file), extract_symbols(proj_file))
                base_text = read_text(snap_file)
            else:
                delta, base_text = None, ""
            if delta:
                item["symbol_delta"] = delta
            red = new_red_flags(rel, base_text, read_text(proj_file))
            if red:
                item["red_flags"] = red
        changed.append(item)

    manifest_paths = {e["path"] for e in manifest["files"]}
    added = scan_added(project, manifest_paths)

    red_flags = []
    for item in changed:
        red_flags.extend(item.get("red_flags", []))
    for rel in added:
        if rel.endswith((".c", ".h")):
            p = os.path.join(project, *rel.split("/"))
            red_flags.extend(new_red_flags(rel, "", read_text(p)))

    if args.json:
        result = {
            "project": project,
            "baseline_commit": manifest.get("source_revision", ""),
            "unchanged_count": unchanged,
            "changed": changed,
            "missing": missing,
            "added": added,
            "red_flags": red_flags,
        }
        out = json.dumps(result, ensure_ascii=False, indent=2)
    else:
        lines = [
            "=== Baseline Audit (read-only) ===",
            f"Project  : {project}",
            f"Baseline : {manifest.get('upstream_repository', '')} @ {manifest.get('source_revision', '')}",
            f"Collected: {manifest.get('collected_at', '')}",
            f"Mode     : {'EOL-normalized (CRLF -> LF) for text files' if args.ignore_eol else 'raw bytes'}",
            "",
            f"Unchanged: {unchanged} / {len(manifest['files'])}",
            f"Changed  : {len(changed)}",
            f"Missing  : {len(missing)}",
            f"Added    : {len(added)}",
            f"RedFlags : {len(red_flags)}",
        ]
        if red_flags:
            lines += ["", "--- Red flags (new vs baseline; see references/ai-constraints.md) ---"]
            lines += [f"[!] {f}" for f in red_flags]
        if changed:
            lines += ["", "--- Changed files (+ heuristic symbol delta) ---"]
            for item in changed:
                lines.append(f"[M] {item['path']}")
                d = item.get("symbol_delta")
                if d:
                    for key, tag in (("added_functions", "+ funcs   "), ("removed_functions", "- funcs   "),
                                     ("added_defines", "+ defines "), ("removed_defines", "- defines "),
                                     ("added_types", "+ types   "), ("removed_types", "- types   ")):
                        if d[key]:
                            lines.append(f"    {tag}: {', '.join(d[key])}")
        if added:
            lines += ["", "--- Added files (not in pinned baseline; classify per baseline-diff.md) ---"]
            lines += [f"[A] {a}" for a in added]
        if missing:
            lines += ["", "--- Missing files (in baseline, absent in project) ---"]
            lines += [f"[X] {m}" for m in missing]
        lines += [
            "",
            "Note: symbol delta is heuristic; confirm with a real diff before acting.",
            "Note: Added is not automatically a violation; classify per references/baseline-diff.md.",
        ]
        out = "\n".join(lines)

    print(out)
    if args.report:
        with open(args.report, "w", encoding="utf-8", newline="\n") as f:
            f.write(out + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
