#ifndef AUTONOMY_H
#define AUTONOMY_H

#include <stdint.h>

typedef struct
{
    float vx;      /* mm/s, chassis forward positive */
    float vy;      /* mm/s, chassis right positive */
    float wz;      /* deg/s, chassis CCW positive */
    uint8_t valid; /* 1=fresh valid navigation command */
} sentry_auto_cmd_t;

void autonomy_init(void);
void autonomy_task(void);
int autonomy_get_command(sentry_auto_cmd_t *command);

#endif
