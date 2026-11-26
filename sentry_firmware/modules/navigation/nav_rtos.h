#ifndef NAV_RTOS_H
#define NAV_RTOS_H

#include <stdint.h>

uint32_t nav_tick_ms(void);
uint32_t nav_enter_critical(void);
void nav_exit_critical(uint32_t saved);

#endif
