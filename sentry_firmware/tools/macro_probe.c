/**
 * @file    macro_probe.c
 * @brief   G1b 宏探针:预处理器层面的宏守恒快照(-E -dM)
 * @author  小猫大王
 * @layer   TESTS
 *
 * @note    与 tools/check_macros.py 的账本互补:账本查源文件里的定义,
 *          探针查"包含这些头之后预处理器实际看到了什么"——能抓到
 *          跨头重定义、条件编译分支差异这类账本看不到的问题。
 * @note    本文件只 include config 伞头与 framework 公开头(纯逻辑,无 HAL);
 *          组件公开头多依赖 main.h/HAL,由 G2 编译本身覆盖。
 */
#include "config.h"

#include "general_def.h"
#include "types.h"
#include "kernel/kernel.h"
#include "msg/msg.h"
#include "err/err.h"
#include "mem/mem.h"
#include "diag/diag.h"
#include "diag/telemetry.h"

/* 探针本身不产生任何符号/行为;存在意义是让 -E 有一个稳定的翻译单元 */
typedef int macro_probe_anchor_t;
