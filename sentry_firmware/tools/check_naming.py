#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""命名规范机器检查(2026-11-23 工程统一化收官)。

判据见 .Doc/spec/命名规范.md:
  - 文件名 snake_case;
  - 文件头 @file 与文件名一致、@layer 合法、@author 存在;
  - 函数/文件级变量 snake_case;
  - 类型 _t / 枚举 _e,且 snake_case;
  - 宏全大写,禁止单字母宏;必须带域前缀(平台/第三方前缀在白名单)。

只读。退出码 0=PASS,1=FAIL。
"""
import os
import re
import sys

ROOTS = ("apps", "components", "framework", "ports", "config")
LAYERS = {"APP", "COMPONENT", "FRAMEWORK", "PORTS", "CONFIG", "TESTS"}

# 第三方/平台/ABI 例外:不按本工程命名规范要求
MACRO_PREFIXES = (
    "HAL_", "ARM_", "STM32", "USE_", "MX_", "CS1_", "ADDR_FLASH_SECTOR_",
    "ONE_BOARD", "CHASSIS_BOARD", "GIMBAL_BOARD", "__",
    "APP_", "AX650_", "BOARD_", "BMI088_", "BUZZER_", "CALIB_", "CAN_", "CAN_COMM_",
    "CAN_FILTER_", "CHASSIS_", "CMD_", "CONFIG_", "CRC", "DAEMON_", "DG_TEST_", "DJI_",
    "DM_", "DWT_", "EKF_", "ERR_", "FAULT_", "FLASH_", "FW_", "GIMBAL_", "HEAT_", "HT_",
    "HT_MOTOR_", "IMU_", "INS_", "IWDG_", "KALMAN_", "KBM_", "KERNEL_", "LOADER_", "LOG_",
    "MATH_", "MEM_", "MSG_", "MOTOR_", "PID_", "PITCH_", "PROFILE_", "PROTECT_", "PWM_",
    "REFEREE_", "REMOTE_", "ROBOT_", "SENTRY_", "SERVO_MOTOR_", "SHOOT_", "SUPER_CAP_",
    "TASK_", "TELEMETRY_", "UART_", "USART_", "VISION_", "YAW_", "RC_", "DIAG_",
)
MACRO_ALLOW = {"NULL", "true", "false"}
TYPE_ALLOW = {"rc", "mouse", "mat"}
TYPE_SKIP = {"break", "continue", "return", "case", "default", "if", "else", "do", "while", "for", "switch"}
FUNC_ALLOW = {
    "main", "fw_assert_fail", "vApplicationStackOverflowHook",
    "vApplicationMallocFailedHook", "vApplicationIdleHook", "vApplicationTickHook",
}
FUNC_PREFIX_SKIP = ("HAL_", "os", "xTask", "vTask", "pvPort", "uxTask", "MX_", "System",
                    "NVIC_", "DMA_", "SPI_", "I2C_", "TIM_", "USART_", "CAN_", "GPIO_",
                    "RCC_", "FLASH_", "PWR_", "ADC_", "pcTask")
VAR_ALLOW = {"rc", "mouse"}

BLOCK = re.compile(r"/\*.*?\*/", re.S)
LINE = re.compile(r"//[^\n]*")
FUNC = re.compile(
    r"^[ \t]*(?:static[ \t]+)?(?:inline[ \t]+)?(?:const[ \t]+)?"
    r"(?:unsigned[ \t]+|signed[ \t]+)?"
    r"(?:void|int|float|double|char|short|long|uint8_t|uint16_t|uint32_t|uint64_t|"
    r"int8_t|int16_t|int32_t|int64_t|size_t|bool|"
    r"[A-Za-z_][A-Za-z0-9_]*_t|[A-Za-z_][A-Za-z0-9_]*_e|[A-Za-z_][A-Za-z0-9_]*_s|"
    r"[A-Za-z_][A-Za-z0-9_]*)[ \t\*]+"
    r"([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(",
    re.M,
)
SKIP_FUNC = {"if", "for", "while", "switch", "return", "sizeof", "else", "do", "case",
             "defined", "typeof", "__attribute__", "asm"}
DEF = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)", re.M)
TYPE_TAIL = re.compile(r"}\s*([A-Za-z_]\w*)\s*;")
TYPE_ONE = re.compile(r"typedef\s+[^;{}]+?\b([A-Za-z_]\w*)\s*;")
VAR = re.compile(
    r"^(?:static\s+)?(?:const\s+)?(?:volatile\s+)?(?:unsigned\s+|signed\s+)?"
    r"(?:struct\s+\w+\s+)?[A-Za-z_]\w*(?:\s*\*+)?\s+([A-Za-z_]\w*)\s*"
    r"(?:\[[^\]]*\])?\s*(?:=|;)"
)


def clean(text):
    text = BLOCK.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return LINE.sub("", text)


def main():
    errors = []
    counts = {"file": 0, "func": 0, "type": 0, "macro": 0, "var": 0}
    for root in ROOTS:
        for dp, dns, fns in os.walk(root):
            for fn in fns:
                if not fn.endswith((".c", ".h")):
                    continue
                path = os.path.join(dp, fn).replace("\\", "/")
                counts["file"] += 1
                base = fn[:-2]
                if not re.fullmatch(r"[a-z0-9_]+", base):
                    errors.append((path, 0, f"文件名不是 snake_case: {fn}"))
                raw = open(path, encoding="utf-8", errors="replace").read()
                head = "\n".join(raw.splitlines()[:30])
                m = re.search(r"@file\s+(\S+)", head)
                if not m:
                    errors.append((path, 0, "文件头缺 @file"))
                elif m.group(1) != fn:
                    errors.append((path, 0, f"@file 错配: {m.group(1)} != {fn}"))
                lm = re.search(r"@layer\s+(\S+)", head)
                if not lm or not re.match(r"(APP|COMPONENT|FRAMEWORK|PORTS|CONFIG|TESTS)",
                                          lm.group(1)):
                    errors.append((path, 0, "@layer 缺失或非法"))
                if "@author" not in head:
                    errors.append((path, 0, "文件头缺 @author"))

                code = clean(raw)
                for mm in FUNC.finditer(code):
                    name = mm.group(1)
                    if name in SKIP_FUNC or name.isupper():
                        continue
                    line_start = code.rfind("\n", 0, mm.start()) + 1
                    line = code[line_start:code.find("\n", mm.end())]
                    if "=" in line.split("(")[0]:
                        continue
                    counts["func"] += 1
                    if name in FUNC_ALLOW or name.startswith(FUNC_PREFIX_SKIP):
                        continue
                    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
                        ln = raw[:mm.start()].count("\n") + 1
                        errors.append((path, ln, f"函数名非 snake_case: {name}"))

                for mm in TYPE_TAIL.finditer(code):
                    name = mm.group(1)
                    counts["type"] += 1
                    if name in TYPE_ALLOW or name in TYPE_SKIP:
                        continue
                    if not re.fullmatch(r"[a-z][a-z0-9_]*_[te]", name):
                        ln = raw[:mm.start()].count("\n") + 1
                        errors.append((path, ln, f"类型名不合规: {name}"))
                for mm in TYPE_ONE.finditer(code):
                    name = mm.group(1)
                    if name in TYPE_ALLOW:
                        continue
                    if not re.fullmatch(r"[a-z][a-z0-9_]*_[te]", name):
                        ln = raw[:mm.start()].count("\n") + 1
                        errors.append((path, ln, f"typedef 名不合规: {name}"))

                for mm in DEF.finditer(code):
                    name = mm.group(1)
                    if re.fullmatch(r"_?[A-Z0-9_]+_H_?", name):
                        continue
                    counts["macro"] += 1
                    if name in MACRO_ALLOW:
                        continue
                    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name):
                        ln = raw[:mm.start()].count("\n") + 1
                        errors.append((path, ln, f"宏名非全大写: {name}"))
                    elif len(name) == 1:
                        errors.append((path, ln, f"单字母宏: {name}"))
                    elif not name.startswith(MACRO_PREFIXES):
                        ln = raw[:mm.start()].count("\n") + 1
                        errors.append((path, ln, f"宏缺域前缀: {name}"))

                if path.endswith(".c"):
                    for i, line in enumerate(code.splitlines(), 1):
                        if not line or line[0].isspace() or line.startswith(("#", "/*", "*")):
                            continue
                        vm = VAR.match(line)
                        if not vm:
                            continue
                        name = vm.group(1)
                        counts["var"] += 1
                        if name in VAR_ALLOW:
                            continue
                        if re.fullmatch(r"[A-Z][A-Za-z0-9_]*", name):
                            continue  # 类型/宏引导的声明,交给上面的规则
                        if not re.fullmatch(r"[a-z_][a-z0-9_]*", name):
                            errors.append((path, i, f"文件级变量非 snake_case: {name}"))

    print("=== 命名检查 ===")
    print("文件 {file} | 函数 {func} | 类型 {type} | 宏 {macro} | 文件级变量 {var}".format(**counts))
    if not errors:
        print("RESULT: PASS")
        return 0
    print(f"违例 {len(errors)} 处:")
    for path, line, msg in errors[:80]:
        print(f"  {path}:{line}: {msg}")
    if len(errors) > 80:
        print(f"  ... 其余 {len(errors) - 80} 处")
    print("RESULT: FAIL")
    return 1


if __name__ == "__main__":
    sys.exit(main())
