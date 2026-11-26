#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""B0-⓪ while(1)/for(;;)/malloc 逐站点台账(自主研发框架升级总方案 §6-B0 / §13)。

口径:--ref(默认 84d51d7)的 git 全树 .c/.h,**先剥注释再匹配**(ist8310.c:93
这类"raw 命中但在注释里"的站点自动消失);行号 = 该 ref 的行号,删除后不重算。
**必须先于 B0 删除提交生成**(否则删掉 bsp_tools/spi/iic 后站点集合对不上)。

分类规则(按序命中;扫描范围 = FIRST_PARTY 第一方目录,Drivers/Middlewares/
与 .agents 上游快照副本不在 T02/T07 口径内):
  1 tests/                       → 主机测试(合法豁免)
  2 前三行内有 NVIC_SystemReset  → 复位后不可达(不进 T02 台账,只进 G4 redflags)
  3 Inc/FreeRTOSConfig.h         → configASSERT 宏(grep 盲区,合法)
  4 Src/                         → CubeMX 生成区(合法豁免)
  5 #define 行                   → 宏定义(附D.1/D.4 收口,B1)
  6 B0 删除集文件                → 随删除(B0;两提交顺序见 DEL_SET 注释)
  7 T02 启动期白名单 10 点(file+line 精确命中)→ 白名单锚注(B3);未命中 → FAIL
  8 modules/algorithm/kalman_filter.c → 启动期一次性 malloc 豁免(B6a 按调用点登记)
  9 modules/message_center/      → 随删除(B7)
 10 function 含 Task 且在 application/ → 任务主循环(合法豁免)
 11 function 含 Init/Register/Config/Create → 注册/初始化(B3 转换候选)
 12 默认                        → 运行期待分类(B3 按台账处置)

B0 删除集与两提交顺序(DEV-20261122-06 记录的偏差:原方案①bsp 三件套→②modules,
但 ist8310.h/tfminiplus.h include bsp_iic.h/bsp_gpio.h,先删 bsp 会断编译,
故执行顺序对调:提交①=modules 死件,提交②=bsp 三件套+bsp_tools):
  提交① modules/oled/ modules/ist8310/ modules/bluetooth/ modules/TFminiPlus/
         modules/motor/LKmotor/(motor_task.c:9 include + Makefile 同步)
  提交② bsp/gpio/ bsp/iic/ bsp/spi/ bsp/bsp_tools.c/h(仅 Makefile 同步)
  保留: modules/standard_cmd/ modules/unicomm/(0 字节占位,ARCH-DEC-006,B12 裁决)

用法:
  python tools/site_ledger.py --ref 84d51d7 --out .Doc/gate/site-ledger.csv
退出码:0=台账生成且白名单 10 点全部命中;1=白名单未命中或其他失败。
"""
import argparse
import csv
import re
import sys

from g4_baseline import BatchReader, ls_tree, run_git, strip_comments

# B0 删除集(相对路径前缀;行尾判定用前缀匹配)
DEL_SET = (
    "modules/oled/", "modules/ist8310/", "modules/bluetooth/",
    "modules/TFminiPlus/", "modules/motor/LKmotor/",
    "bsp/gpio/", "bsp/iic/", "bsp/spi/", "bsp/bsp_tools.c", "bsp/bsp_tools.h",
)

# 台账范围 = 第一方代码(上游快照 .agents/、Drivers/、Middlewares/ 不进 T02/T07 口径)
FIRST_PARTY = ("application/", "bsp/", "modules/", "Inc/", "Src/", "tests/")

# T02 启动期白名单 4 类 10 点(方案 T02 行;行号 = 84d51d7 口径,路径为实际仓库路径)
WHITELIST_10 = {
    ("modules/imu/ins_task.c", 111),
    ("modules/motor/DJImotor/dji_motor.c", 81),
    ("modules/motor/DJImotor/dji_motor.c", 116),
    ("modules/motor/DJImotor/dji_motor.c", 144),
    ("modules/motor/DJImotor/dji_motor.c", 162),
    ("modules/motor/DJImotor/dji_motor.c", 170),
    ("bsp/can/bsp_can.c", 141),
    ("bsp/can/bsp_can.c", 148),
    ("modules/message_center/message_center.c", 24),
    ("modules/message_center/message_center.c", 34),
}

WHILE = re.compile(r"\bwhile\s*\(\s*1\s*\)")
FOR = re.compile(r"\bfor\s*\(\s*;\s*;\s*\)")
MALLOC = re.compile(r"\b(?:user_)?malloc\s*\(|\bcalloc\s*\(|\brealloc\s*\(")
FUNC = re.compile(
    r"(?m)^[ \t]*(?:(?:static|inline|const|extern|[A-Za-z_][A-Za-z0-9_]*)[ \t\*]+)+"
    r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\([^;{}]*\)[ \t\r\n]*\{")


def enclosing_function(lines, lineno):
    """hit 所在行往上找最近的函数定义行名;找不到返回 '-'。"""
    for i in range(lineno - 2, max(-1, lineno - 60), -1):
        m = FUNC.match(lines[i])
        if m:
            return m.group(1)
    return "-"


def classify(path, lineno, func, kind, text_lines):
    """返回 (category, disposal, batch)。规则按模块 docstring 的序号。"""
    if path.startswith("tests/"):
        return "主机测试", "合法豁免", "-"
    # 复位后不可达:前三行出现 NVIC_SystemReset
    for j in range(max(0, lineno - 4), lineno - 1):
        if "NVIC_SystemReset" in text_lines[j]:
            return "复位后不可达", "不进 T02 台账(G4 redflags 已豁免)", "-"
    if path == "Inc/FreeRTOSConfig.h":
        return "configASSERT 宏", "合法(grep 盲区)", "-"
    if path.startswith("Src/"):
        return "CubeMX 生成区", "合法豁免", "-"
    if "#define" in text_lines[lineno - 1]:
        return "宏定义", "附D.1/D.4 收口", "B1"
    if any(path == p or path.startswith(p) for p in DEL_SET):
        return "B0 删除集", "随删除", "B0"
    if (path, lineno) in WHITELIST_10:
        return "启动期白名单", "白名单锚注", "B3"
    if path == "modules/algorithm/kalman_filter.c" and kind == "malloc":
        return "启动期一次性", "调用点登记豁免", "B6a"
    if path.startswith("modules/message_center/"):
        return "message_center", "随删除", "B7"
    if func != "-" and "Task" in func and path.startswith("application/"):
        return "任务主循环", "合法豁免", "-"
    if func != "-" and any(k in func for k in ("Init", "Register", "Config", "Create")):
        return "注册/初始化", "B3 转 err_report 候选", "B3"
    return "运行期待分类", "B3 按台账处置", "B3"


def main():
    ap = argparse.ArgumentParser(description="B0-⓪ while(1)/for(;;)/malloc 逐站点台账")
    ap.add_argument("--ref", default="84d51d7")
    ap.add_argument("--out", default=".Doc/gate/site-ledger.csv")
    args = ap.parse_args()

    files = {p: b for p, b in ls_tree(args.ref).items()
             if p.endswith((".c", ".h")) and p.startswith(FIRST_PARTY)}
    br = BatchReader()
    rows, misses = [], []
    raw_total = 0
    kinds_count = {"while(1)": 0, "for(;;)": 0, "malloc": 0}
    for path in sorted(files):
        text = br.read_blob(files[path]).decode("utf-8", "replace")
        stripped = strip_comments(text)
        raw_total += len(WHILE.findall(text)) + len(FOR.findall(text)) + len(MALLOC.findall(text))
        lines = stripped.splitlines()
        raw_lines = text.splitlines()
        for lineno, line in enumerate(lines, start=1):
            for pat, kind in ((WHILE, "while(1)"), (FOR, "for(;;)"), (MALLOC, "malloc")):
                if not pat.search(line):
                    continue
                kinds_count[kind] += 1
                func = enclosing_function(lines, lineno)
                cat, disp, batch = classify(path, lineno, func, kind, raw_lines)
                rows.append([kind, path, lineno, func, cat, disp, batch])
                if (path, lineno) in WHITELIST_10 and cat != "启动期白名单":
                    misses.append((path, lineno, cat))
    br.close()

    with open(args.out, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["kind", "file", "line", "function", "category", "disposal", "batch"])
        w.writerows(rows)

    print(f"[ledger] ref={args.ref} 文件数={len(files)} raw 命中行={raw_total} "
          f"有效站点={len(rows)} (while={kinds_count['while(1)']} for={kinds_count['for(;;)']} "
          f"malloc={kinds_count['malloc']}) -> {args.out}")
    from collections import Counter
    by_cat = Counter(r[4] for r in rows)
    for cat, n in by_cat.most_common():
        print(f"  {cat}: {n}")
    wl_hit = sum(1 for r in rows if r[4] == "启动期白名单")
    print(f"白名单 10 点命中: {wl_hit}/10")
    if misses:
        for m in misses:
            print(f"  [白名单未命中] {m[0]}:{m[1]} 实际分类={m[2]}")
        return 1
    if wl_hit != 10:
        print(f"  [FAIL] 白名单仅命中 {wl_hit}/10(行号口径漂移?)")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
