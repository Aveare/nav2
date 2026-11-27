/**
 * @file    msg.h
 * @brief   哨兵 USB 双协议主机测试消息总线桩
 * @layer   TESTS
 * @author  小猫大王
 */
#pragma once

#include <stddef.h>

typedef void *msg_sub_t;

msg_sub_t msg_sub(const char *name, size_t size);
int msg_recv(msg_sub_t sub, void *output);
