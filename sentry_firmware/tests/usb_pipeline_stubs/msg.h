#pragma once
#include <stddef.h>
typedef void *msg_sub_t;
msg_sub_t msg_sub(const char *name, size_t size);
int msg_recv(msg_sub_t sub, void *output);
