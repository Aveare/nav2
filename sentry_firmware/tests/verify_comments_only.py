#!/usr/bin/env python3
"""注释重写安全闸门。

验证"只改注释、不改代码"与"注释信息零丢失"。配套文档:.Doc/注释重写规划.md

用法:
    python tests/verify_comments_only.py --snapshot   # 建立代码快照基线(改注释前执行)
    python tests/verify_comments_only.py --check      # 闸门1:代码是否零改动
    python tests/verify_comments_only.py --tokens     # 闸门2:注释 token 是否丢失(对比 git HEAD)

退出码:0 通过,1 不通过。
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

ROOTS = ["application", "modules", "bsp", "config"]
EXTS = (".c", ".h")
SNAPSHOT = os.path.join(".workbuddy", "tmp", "comment_baseline.json")


def strip_comments(text):
    """按 C 语法剥离注释,返回 (代码片段, 注释片段)。正确处理字符串与字符常量。"""
    code, cmts = [], []
    i, n = 0, len(text)
    state = "normal"
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "normal":
            if c == "/" and nxt == "*":
                state, i = "block", i + 2
                cmts.append(" ")
                continue
            if c == "/" and nxt == "/":
                state, i = "line", i + 2
                cmts.append(" ")
                continue
            if c == '"':
                state = "string"
            elif c == "'":
                state = "char"
            code.append(c)
            i += 1
        elif state == "line":
            if c == "\n":
                state = "normal"
                code.append(c)
            else:
                cmts.append(c)
            i += 1
        elif state == "block":
            if c == "*" and nxt == "/":
                state, i = "normal", i + 2
                cmts.append(" ")
                continue
            cmts.append(c)
            i += 1
        else:  # string / char
            if c == "\\" and i + 1 < n:
                code.append(c)
                code.append(nxt)
                i += 2
                continue
            if (state == "string" and c == '"') or (state == "char" and c == "'"):
                state = "normal"
            code.append(c)
            i += 1
    return "".join(code), "".join(cmts)


def normalize(code):
    """去掉注释删除后的空行与行尾空白差异,字符串内容不受影响。"""
    lines = [ln.rstrip() for ln in code.splitlines()]
    return "\n".join(ln for ln in lines if ln.strip())


def collect():
    files = []
    for root in ROOTS:
        for dp, _, fns in os.walk(root):
            for fn in sorted(fns):
                if fn.endswith(EXTS):
                    files.append(os.path.join(dp, fn).replace("\\", "/"))
    return files


def digest(s):
    return hashlib.sha256(s.encode("utf-8", "ignore")).hexdigest()[:16]


TOKEN_PATTERNS = [
    r"DEV-\d{8}-\d{2}",                 # 决策编号
    r"0x[0-9A-Fa-f]{4}",                # 协议 ID
    r"\b\d+(?:\.\d+)?\b",               # 数值
    r"\b(?:deg/s|mm/s|RPM|ms|Hz)\b",    # 单位(不含符号类)
    r"%",                               # 百分号:符号不能用 \b 界定
    r"\b[A-Z][A-Z0-9_]{2,}\b",          # 宏/术语
]


def tokens(text):
    out = set()
    for pat in TOKEN_PATTERNS:
        for m in re.findall(pat, text):
            out.add(m)
    return out


def cmd_snapshot():
    data = {}
    for f in collect():
        text = open(f, encoding="utf-8", errors="ignore").read()
        code, _ = strip_comments(text)
        data[f] = digest(normalize(code))
    os.makedirs(os.path.dirname(SNAPSHOT), exist_ok=True)
    with open(SNAPSHOT, "w", encoding="utf-8") as fp:
        json.dump(data, fp, ensure_ascii=False, indent=1)
    print(f"[snapshot] {len(data)} 个文件,基线写入 {SNAPSHOT}")
    return 0


def cmd_check():
    if not os.path.exists(SNAPSHOT):
        print("[check] 未找到基线,请先执行 --snapshot")
        return 1
    base = json.load(open(SNAPSHOT, encoding="utf-8"))
    changed, missing = [], []
    for f, h in base.items():
        if not os.path.exists(f):
            missing.append(f)
            continue
        code, _ = strip_comments(open(f, encoding="utf-8", errors="ignore").read())
        if digest(normalize(code)) != h:
            changed.append(f)
    if missing:
        print(f"[check] FAIL 文件缺失 {len(missing)} 个:")
        for f in missing:
            print("   -", f)
    if changed:
        print(f"[check] FAIL 代码被改动 {len(changed)} 个文件(只允许改注释):")
        for f in changed:
            print("   -", f)
        return 1
    if missing:
        return 1
    print(f"[check] PASS 全部 {len(base)} 个文件代码零改动")
    return 0


def is_numeric_covered(tok, new_tokens):
    """纯数字 token 若被更精确的数值包含(如 51 -> 51.43、52 -> 2.52),视为未丢失。

    汉字紧邻数字时正则会切出数字碎片(如 "阈值2.52" 切出 "52"),新文本加了空格后
    得到完整数值,属于精度提升而非信息丢失。
    """
    if not re.fullmatch(r"\d+(?:\.\d+)?", tok):
        return False
    for nt in new_tokens:
        if nt != tok and re.fullmatch(r"\d+(?:\.\d+)?", nt) and (nt.startswith(tok) or nt.endswith(tok)):
            return True
    return False


def cmd_tokens():
    """对比 git HEAD 版本与工作区版本的注释 token,要求旧 token 全部保留。"""
    files = collect()
    lost_total, checked = {}, 0
    for f in files:
        try:
            old = subprocess.run(["git", "show", f"HEAD:{f}"], capture_output=True,
                                 check=True).stdout.decode("utf-8", "ignore")
        except subprocess.CalledProcessError:
            continue
        _, old_c = strip_comments(old)
        _, new_c = strip_comments(open(f, encoding="utf-8", errors="ignore").read())
        new_tokens = tokens(new_c)
        lost = {t for t in (tokens(old_c) - new_tokens) if not is_numeric_covered(t, new_tokens)}
        if lost:
            lost_total[f] = sorted(lost)
        checked += 1
    if lost_total:
        print(f"[tokens] FAIL {len(lost_total)} 个文件丢失注释信息:")
        for f, ts in lost_total.items():
            print(f"   - {f}: {', '.join(ts[:12])}")
        return 1
    print(f"[tokens] PASS {checked} 个文件注释 token 无丢失")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--snapshot", action="store_true", help="建立代码快照基线")
    ap.add_argument("--check", action="store_true", help="验证代码零改动")
    ap.add_argument("--tokens", action="store_true", help="验证注释信息未丢失")
    a = ap.parse_args()
    if a.snapshot:
        return cmd_snapshot()
    if a.check:
        return cmd_check()
    if a.tokens:
        return cmd_tokens()
    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
