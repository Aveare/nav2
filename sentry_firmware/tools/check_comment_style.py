#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""T16/注释规范机器检查(tools/check_comment_style.py)。

判据(.Doc/spec/注释规范.md):
  - 文件头 @brief 非空(空 @brief = 模板占位符);
  - 全仓禁 `you@domain.com` 等占位邮箱;
  - 自研文件(@author 小猫大王/modified by)禁 @retval/@attention/@warning/@date/@version/
    @copyright —— 上游衍生文件保留原样,不检查;
  - TODO 必须是 `TODO(YYYY-MM-DD): ...` 格式,裸 TODO/@todo FAIL;
  - 禁 `//=====` 类装饰分隔线。
退出码:0=PASS;1=FAIL。
"""
import os
import re
import sys

ROOTS = ("apps", "components", "framework", "ports", "config")
OWN_MARKERS = ("小猫大王", "modified by")
FORBIDDEN_TAGS = ("@retval", "@attention", "@warning", "@date", "@version", "@copyright")
OWN_TAG_FORBIDDEN = ("@retval", "@attention", "@warning")
HEADER_TAGS_FORBIDDEN_OWN = ("@date", "@version", "@copyright")


def is_own_author(head: str) -> bool:
    """自研文件 = 本队新写;上游衍生(modified by)保留原样,不查标签风格。"""
    if "modified by" in head:
        return False
    return "小猫大王" in head


def check_file(path: str):
    errors = []
    text = open(path, encoding="utf-8", errors="replace").read()
    lines = text.splitlines()
    head = "\n".join(lines[:40])

    if "you@domain.com" in text:
        errors.append("占位邮箱 you@domain.com")
    if is_own_author(head) and re.search(r"@brief\s*(\*?/?\s*)$", head, re.M):
        errors.append("文件头 @brief 为空")

    for i, line in enumerate(lines, 1):
        if re.search(r"//\s*=+\s*$", line) or re.search(r"//\s*=\+=", line):
            errors.append(f"{i}: 装饰分隔线 //===== ")

    if is_own_author(head):
        for i, line in enumerate(lines, 1):
            if re.search(r"\bTODO\b(?!-)", line) and not re.search(r"TODO\(\d{4}-\d{2}-\d{2}\)", line):
                errors.append(f"{i}: 裸 TODO(需 TODO(YYYY-MM-DD): 三要素)")
            if re.search(r"@todo\b", line, re.I):
                errors.append(f"{i}: 使用 @todo(应写 TODO(日期):)")

        for i, line in enumerate(lines, 1):
            for tag in OWN_TAG_FORBIDDEN:
                if re.search(rf"{tag}\b", line):
                    errors.append(f"{i}: 自研文件使用禁用标签 {tag}")
        for tag in HEADER_TAGS_FORBIDDEN_OWN:
            if re.search(rf"{tag}\b", head):
                errors.append(f"文件头禁用 {tag}(靠 git 溯源)")
    return errors


def main() -> int:
    errors = []
    files = 0
    for root in ROOTS:
        for dp, dns, fns in os.walk(root):
            dns[:] = [d for d in dns if d not in ("build", "__pycache__")]
            for fn in fns:
                if not fn.endswith((".c", ".h")):
                    continue
                path = os.path.join(dp, fn).replace("\\", "/")
                files += 1
                errors += [(f"{path}:{e}" if ":" not in str(e).split(":")[0] else f"{e}")
                           for e in check_file(path)]

    print(f"=== 注释风格检查 ===\n文件 {files} | 违例 {len(errors)}")
    if errors:
        for e in errors[:40]:
            print(f"  {e}")
        if len(errors) > 40:
            print(f"  ... 其余 {len(errors) - 40} 处")
        print("RESULT: FAIL")
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
