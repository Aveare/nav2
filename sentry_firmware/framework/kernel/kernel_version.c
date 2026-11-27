/**
 * @file    kernel_version.c
 * @brief   框架版本符号(链接锚点)
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    零引用符号会被 --gc-sections 回收:锚点常量放 .fw_anchor 段,
*          链接脚本 KEEP(STM32F407IGHx_FLASH.ld .rodata 块)。
 */
#include "kernel.h"

const uint32_t fw_abi_anchor __attribute__((section(".fw_anchor"), used)) = 0xB10C0DEu;

const char *fw_version(void)
{
    return "fw 0.1.0-b1 (rm-jit-cr)";
}
