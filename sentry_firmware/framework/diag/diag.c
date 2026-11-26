/**
 * @file    diag.c
 * @brief   诊断:心跳监督注册/推进/喂狗判定
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    监督对象 = **任务级**心跳(必活 4 任务);模块级离线检测仍是 daemon
*          (回调语义,与监督分权:daemon 报警/自愈,diag 断 IWDG)。
*          拍推进在 daemon_task(100Hz):超时单位=拍数不变量(方案 §4)。
*          G9 注入:hang 标志使 kick 失效——任务本身没挂也能测出"IWDG 停喂→复位"链路。
 */
#include "diag.h"
#include <string.h>

#define DIAG_WATCH_MAX 12
#define DIAG_NAME_MAX 16

typedef struct
{
    char name[DIAG_NAME_MAX];
    uint32_t remaining;   /* 剩余拍数(拍源 daemon_task 100Hz) */
    uint32_t timeout;     /* 超时拍数 */
    uint8_t required;     /* 1=必活(进掩码) 0=可选 */
    uint8_t alive;        /* 最近一次 tick 时的存活判定 */
    uint8_t hang;         /* G9 注入:kick 失效 */
    uint8_t used;
} diag_entry_t;

static diag_entry_t entries[DIAG_WATCH_MAX];
static unsigned entry_n;

static diag_entry_t *diag_find(const char *name)
{
    for (unsigned i = 0; i < entry_n; ++i)
        if (entries[i].used && strcmp(entries[i].name, name) == 0)
            return &entries[i];
    return NULL;
}

static diag_watch_t reg(const char *name, uint32_t timeout, uint8_t required)
{
    diag_entry_t *e = NULL;
    if (name == NULL)
        return NULL;
    /* 先复用注销留下的空槽:unregister 是标记清除,槽位地址必须保持稳定,
     * 否则外部持有的 diag_watch_t 句柄会指向被挪动的数据(2026-11-23 修)。 */
    for (unsigned i = 0; i < entry_n; ++i)
    {
        if (!entries[i].used)
        {
            e = &entries[i];
            break;
        }
    }
    if (e == NULL)
    {
        if (entry_n >= DIAG_WATCH_MAX)
            return NULL;
        e = &entries[entry_n++];
    }
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, DIAG_NAME_MAX - 1);
    e->timeout = timeout == 0 ? 1 : timeout;
    e->remaining = e->timeout;
    e->required = required;
    e->alive = 1;
    e->used = 1;
    return (diag_watch_t)e;
}

diag_watch_t diag_watch_register(const char *name, uint32_t timeout_ticks)
{
    return reg(name, timeout_ticks, 1);
}

diag_watch_t diag_watch_register_optional(const char *name, uint32_t timeout_ticks)
{
    return reg(name, timeout_ticks, 0);
}

void diag_watch_kick(diag_watch_t w)
{
    diag_entry_t *e = (diag_entry_t *)w;
    if (e == NULL || !e->used || e->hang)
        return; /* hang 注入:kick 失效(G9) */
    e->remaining = e->timeout;
}

void diag_watch_unregister(diag_watch_t w)
{
    diag_entry_t *e = (diag_entry_t *)w;
    if (e == NULL || !e->used)
        return;
    /* 标记清除而非 memmove 紧缩:紧缩会把其他在用条目整体前移,外部持有的
     * diag_watch_t 立即悬空(2026-11-23 修);空槽由 reg() 复用,
     * required 清零保证空槽不参与必活判定。 */
    e->used = 0;
    e->required = 0;
    e->hang = 0;
}

void diag_watch_tick(void)
{
    for (unsigned i = 0; i < entry_n; ++i)
    {
        diag_entry_t *e = &entries[i];
        if (e->remaining > 0)
            e->remaining--;
        e->alive = (e->remaining > 0) ? 1u : 0u;
    }
}

int diag_watch_required_alive(void)
{
    for (unsigned i = 0; i < entry_n; ++i)
        if (entries[i].required && !entries[i].alive)
            return 0;
    return 1;
}

unsigned diag_watch_dead_names(char *out, unsigned cap)
{
    unsigned n = 0;
    for (unsigned i = 0; i < entry_n && n + 1 < cap; ++i)
    {
        if (entries[i].required && !entries[i].alive)
        {
            unsigned len = strlen(entries[i].name);
            if (n + len + 2 >= cap)
                break;
            memcpy(out + n, entries[i].name, len);
            n += len;
            out[n++] = ' ';
        }
    }
    if (n)
        out[n - 1] = '\0';
    else if (cap)
        out[0] = '\0';
    return n;
}

void diag_watch_simulate_hang(const char *name)
{
    diag_entry_t *e = diag_find(name);
    if (e != NULL)
        e->hang = 1;
}
