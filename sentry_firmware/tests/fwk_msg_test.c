/**
 * @file    fwk_msg_test.c
 * @brief   framework msg 主机用例(方案 §13"T04 主机单测(含 pthread 双线程用例)"
 *          → 本机 MinGW 无 pthread,Win32 线程同语义,DEV-07 偏差沿用)
 * @author  小猫大王
 * @layer   TESTS
 *
 * @note    覆盖 .Doc/history/archive/gate/msg-semantic-map.md 全部 12 行:一致项逐项断言,修正项断言新行为;
 *          双线程用例:发布线程 1..N 连发(深度 1),join 后订阅者 drain 必取到
 *          最后一帧(只保最新语义在并发下成立,结果确定)。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "msg.h"
#include "kernel_posix.h"

typedef struct
{
    float vx;
    int32_t mode;
} test_frame_t;

static void test_roundtrip_and_overwrite(void)
{
    msg_pub_t p = msg_pub("t1", sizeof(test_frame_t));
    assert(p != NULL);
    msg_sub_t s = msg_sub("t1", sizeof(test_frame_t));
    assert(s != NULL);

    test_frame_t f = {1.0f, 10}, g = {2.0f, 20};
    assert(msg_publish(p, &f) == 1u); /* 单订阅 → 投递 1 */
    assert(msg_publish(p, &g) == 1u); /* 覆盖未取走的第一帧(只保最新) */
    test_frame_t rx = {0};
    assert(msg_recv(s, &rx) == 1);
    assert(rx.vx == 2.0f && rx.mode == 20); /* 拿到的是第二帧 */
    assert(msg_recv(s, &rx) == 0);          /* 取走即空 */
    printf("PASS: roundtrip + overwrite-to-latest\n");
}

static void test_multi_subscriber(void)
{
    msg_pub_t p = msg_pub("t2", sizeof(test_frame_t));
    msg_sub_t s1 = msg_sub("t2", sizeof(test_frame_t));
    msg_sub_t s2 = msg_sub("t2", sizeof(test_frame_t));
    assert(p && s1 && s2);
    test_frame_t f = {3.0f, 30};
    assert(msg_publish(p, &f) == 2u); /* 真实投递数 = 订阅者数(对照表 #4 新口径) */
    test_frame_t a = {0}, b = {0};
    assert(msg_recv(s1, &a) == 1 && msg_recv(s2, &b) == 1);
    assert(a.mode == 30 && b.mode == 30);
    printf("PASS: multi-subscriber (both delivered, publish returns true count)\n");
}

static void test_topic_cap_and_len(void)
{
    /* 此时已用 3 话题(t1/t2/thr2):剩余 5 槽;第 9 个话题 = NULL(对照表 #6) */
    for (unsigned i = 0; i < MSG_TOPIC_MAX - 3; ++i)
    {
        char name[MSG_NAME_MAX];
        snprintf(name, sizeof(name), "cap%u", i);
        assert(msg_pub(name, 4) != NULL);
    }
    assert(msg_topic_count() == MSG_TOPIC_MAX);
    assert(msg_pub("overflow", 4) == NULL); /* 上限实施检查 */

    assert(msg_pub(NULL, 4) == NULL); /* 无名(对照表 #9) */
    assert(msg_pub("", 4) == NULL);
    assert(msg_pub("lenx", 0) == NULL); /* 零长 */
    assert(msg_pub("t1", 16) == NULL);  /* 与已存在话题长度冲突 → NULL 不挂死(对照表 #5) */
    printf("PASS: topic cap 8 / name & len guards (no hang)\n");
}

static test_frame_t latest;
static msg_pub_t g_thr_pub;
static unsigned publisher_thread(void *arg)
{
    (void)arg;
    for (uint32_t i = 1; i <= 1000; ++i)
    {
        latest.vx = (float)i;
        latest.mode = (int32_t)i;
        msg_publish(g_thr_pub, &latest);
    }
    return 0u;
}

static void test_threads_latest_wins(void)
{
    g_thr_pub = msg_pub("thr2", sizeof(test_frame_t)); /* 先 pub(注册序契约) */
    assert(g_thr_pub != NULL);
    msg_sub_t s = msg_sub("thr2", sizeof(test_frame_t));
    assert(s != NULL);
    unsigned h = kernel_posix_spawn(publisher_thread, NULL);
    kernel_posix_join(h);
    test_frame_t rx = {0};
    assert(msg_recv(s, &rx) == 1);
    assert(rx.mode == 1000); /* 深度 1:并发下必取最后一帧(确定性) */
    printf("PASS: concurrent publish, depth-1 keeps latest (1000)\n");
}

int main(void)
{
    test_roundtrip_and_overwrite();
    test_multi_subscriber();
    test_threads_latest_wins();
    test_topic_cap_and_len(); /* 最后:占满话题表 */
    puts("PASS: fwk_msg_test ALL");
    return 0;
}
