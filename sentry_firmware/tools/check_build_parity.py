#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""双构建 parity 检查:Makefile 与 CMake 的编译期 -D 宏集合必须一致。

背景(升级总方案 §8 风险册"双构建漂移"):Makefile 曾带 -DDISABLE_LOG_SYSTEM 而
CMake 不带,两个产物日志行为不同。LOG 宏改名后该开关静默失效,更说明
"parity 靠人记"不可靠 —— 本脚本把集合比对机器化。

判据:
  - Makefile C_DEFS 与 CMake add_definitions 的非工具链宏集合完全一致;
  - 废弃宏黑名单在两个构建文件中都不得出现(改名后残留 = 静默失效)。
退出码:0=PASS;1=FAIL。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# 与固件语义无关的工具链/芯片宏不参与比对(两个构建体系各自的提供方式不同)
IGNORE = {"USE_HAL_DRIVER", "STM32F407xx", "ARM_MATH_CM4"}

# 改名后残留的废弃宏:出现即 FAIL(无论哪套构建)
OBSOLETE = {"DISABLE_LOG_SYSTEM", "TRUE", "FALSE", "PI", "PI2", "TASK_UI_ENABLE",
            "MatAdd", "MatSubtract", "MatMultiply", "MatTranspose", "MatInverse",
            "msin", "mcos", "VAL_MIN", "VAL_MAX", "user_malloc", "BUFFER_INDEX",
            "LOGINFO", "LOGWARNING", "LOGERROR", "LOG", "Float2Str",
            "ECD_ANGLE_COEF_DJI", "SPEED_SMOOTH_COEF", "GYRO2GIMBAL_DIR_YAW",
            "GYRO2GIMBAL_DIR_PITCH", "GYRO2GIMBAL_DIR_ROLL", "MX_CAN_FILTER_CNT",
            "MX_CAN_COMM_COUNT", "DAEMON_MX_CNT", "DEVICE_CAN_CNT", "DEVICE_USART_CNT"}


def makefile_defs() -> set:
    text = (ROOT / "Makefile").read_text(encoding="utf-8", errors="replace")
    m = re.search(r"^C_DEFS\s*=\s*\\\n(.*?)(?=^\s*$)", text, re.M | re.S)
    if not m:
        raise SystemExit("FAIL: Makefile 里找不到 C_DEFS")
    defs = set(re.findall(r"-D([A-Za-z_][A-Za-z0-9_]*)", m.group(1)))
    return defs, m.group(1)


def cmake_defs():
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")
    defs = set()
    blocks = []
    for m in re.finditer(r"add_definitions\(([^)]*)\)", text):
        defs |= set(re.findall(r"-D([A-Za-z_][A-Za-z0-9_]*)", m.group(1)))
        blocks.append(m.group(1))
    for m in re.finditer(r"add_compile_definitions\(([^)]*)\)", text):
        defs |= set(re.findall(r"([A-Za-z_][A-Za-z0-9_]*)", m.group(1)))
        blocks.append(m.group(1))
    return defs, "\n".join(blocks)


def main() -> int:
    mk, mk_text = makefile_defs()
    cm, cm_text = cmake_defs()
    ok = True

    for f, block in (("Makefile C_DEFS", mk_text), ("CMake add_definitions", cm_text)):
        hits = [o for o in OBSOLETE if re.search(rf"\b{re.escape(o)}\b", block)]
        if hits:
            ok = False
            print(f"[FAIL] {f} 含废弃宏(改名后残留=静默失效): {', '.join(sorted(hits))}")

    mk_cmp, cm_cmp = mk - IGNORE, cm - IGNORE
    if mk_cmp != cm_cmp:
        ok = False
        only_mk = sorted(mk_cmp - cm_cmp)
        only_cm = sorted(cm_cmp - cm_cmp)
        if only_mk:
            print(f"[FAIL] 仅 Makefile 有: {only_mk}")
        if only_cm:
            print(f"[FAIL] 仅 CMake 有: {only_cm}")
    else:
        print(f"[PASS] 双构建 -D 集合一致(忽略工具链宏 {len(IGNORE)}): {sorted(mk_cmp)}")

    print("RESULT: PASS" if ok else "RESULT: FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
