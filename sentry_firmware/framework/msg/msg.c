/**
 * @file    msg.c
 * @brief   发布订阅消息中心:深度 1 只保最新
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    结构:message_center 的链表机制原样保留(多订阅者勿删),槽位改
*          静态池 + 上限 8 实施检查(替换死宏 MAX_TOPIC_COUNT)。
*          深度=1:queue 只有 1 槽,发布即覆盖(recv 未取也覆盖,语义=只保最新)。
*          单写者无锁:与 message_center/遥测环同纪律(先填后推)。
*          **订阅先于发布合法**(B7):旧 SubRegister 内调 PubRegister 自动建话题,
*          robot_cmd.c 的 feed_sub 注册早于 gimbal_init 的 pub —— msg_sub 同样自动建。
 */
#include "msg.h"
#include "kernel.h"
#include <string.h>

typedef struct msg_topic
{
    char name[MSG_NAME_MAX];
    uint8_t data_len;
    struct msg_sub_s *first_sub; /* 订阅者链表头(多订阅者机制,勿删) */
} msg_topic_t;

struct msg_sub_s
{
    msg_topic_t *topic;
    uint8_t slot[MSG_DATA_MAX]; /* 深度=1:只保最新 */
    uint8_t fresh;              /* 1=有未取新帧 */
    struct msg_sub_s *next;     /* 同话题下一订阅者 */
};

struct msg_pub_s
{
    msg_topic_t *topic;
};

static msg_topic_t topics[MSG_TOPIC_MAX];
static struct msg_pub_s pubs[MSG_TOPIC_MAX];
static struct msg_sub_s subs[MSG_TOPIC_MAX * 2]; /* 每话题至少可 2 订阅 */
static unsigned topic_n;
static unsigned sub_n;
static unsigned pub_n;
static unsigned drop_cnt;

static msg_topic_t *msg_topic_find(const char *name)
{
    for (unsigned i = 0; i < topic_n; ++i)
        if (strcmp(topics[i].name, name) == 0)
            return &topics[i];
    return NULL;
}

static int msg_name_ok(const char *name)
{
    return name != NULL && name[0] != '\0' && strlen(name) < MSG_NAME_MAX;
}

/* 查找话题;不存在则自动创建(订阅先于发布合法);长度冲突返回 NULL */
static msg_topic_t *msg_topic_get_or_create(const char *name, uint8_t data_len)
{
    msg_topic_t *t = msg_topic_find(name);
    if (t != NULL)
    {
        if (t->data_len != data_len)
            return NULL; /* 长度冲突:拒绝(CheckLen 挂死的修正) */
        return t;
    }
    if (topic_n >= MSG_TOPIC_MAX)
        return NULL; /* 上限 8:实施检查(替换死宏) */
    t = &topics[topic_n++];
    strncpy(t->name, name, MSG_NAME_MAX - 1);
    t->name[MSG_NAME_MAX - 1] = '\0';
    t->data_len = data_len;
    t->first_sub = NULL;
    return t;
}

msg_pub_t msg_pub(const char *name, uint8_t data_len)
{
    /* data_len 为 uint8_t(天然 ≤ MSG_DATA_MAX),超长在上层类型层面已不可能 */
    if (!msg_name_ok(name) || data_len == 0 || pub_n >= MSG_TOPIC_MAX)
        return NULL;
    msg_topic_t *t = msg_topic_get_or_create(name, data_len);
    if (t == NULL)
        return NULL;
    pubs[pub_n].topic = t;
    return &pubs[pub_n++];
}

msg_sub_t msg_sub(const char *name, uint8_t data_len)
{
    if (!msg_name_ok(name) || data_len == 0 || sub_n >= MSG_TOPIC_MAX * 2)
        return NULL;
    msg_topic_t *t = msg_topic_get_or_create(name, data_len);
    if (t == NULL)
        return NULL;
    struct msg_sub_s *s = &subs[sub_n++];
    s->topic = t;
    s->fresh = 0;
    s->next = t->first_sub; /* 头插:多订阅者链表(机制保留) */
    t->first_sub = s;
    return s;
}

int msg_recv(msg_sub_t sub, void *out)
{
    if (sub == NULL || out == NULL || !sub->fresh)
        return 0;
    memcpy(out, sub->slot, sub->topic->data_len);
    sub->fresh = 0;
    return 1;
}

uint8_t msg_publish(msg_pub_t pub, const void *data)
{
    if (pub == NULL || data == NULL)
        return 0;
    if (kernel_is_in_isr())
    {
        drop_cnt++; /* 方案 §4:msg_publish 非 ISR;ISR 调用丢弃并计数 */
        return 0;
    }
    uint8_t delivered = 0;
    for (struct msg_sub_s *s = pub->topic->first_sub; s != NULL; s = s->next)
    {
        memcpy(s->slot, data, pub->topic->data_len); /* 深度 1:直接覆盖=只保最新 */
        s->fresh = 1;
        delivered++;
    }
    return delivered;
}

unsigned msg_topic_count(void) { return topic_n; }
unsigned msg_drop_count(void) { return drop_cnt; }
