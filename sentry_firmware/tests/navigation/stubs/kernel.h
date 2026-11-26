#pragma once
#include <stdint.h>
uint32_t kernel_tick_ms(void);
uint32_t kernel_enter_critical(void);
void kernel_exit_critical(uint32_t saved);
