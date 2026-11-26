/**
 * @file    message_center.h
 * @brief   轻量发布-订阅:按话题名解耦发布者与订阅者
 * @layer   MODULE
 * @note    **不是真正的队列**:QUEUE_SIZE 为 1,每个订阅者只保留最新一帧,
 *          新消息直接覆盖尚未取走的旧帧。因此本机制适合"状态型"数据(指令、反馈),
 *          不适合"事件型"数据 —— 需要"发一次必被处理一次"的命令,必须保证
 *          发布者与消费者在同一拍内先后执行(本工程 cmd → 执行单元即为此模式)。
 * @note    单写者约定:一个话题只应有一个发布任务,多写者时帧序无法保证。
 * @note    data_len 为 uint8_t,单条消息不得超过 255 字节,超长会被截断且不报错。
 * @note    源自跃鹿 basic_framework(作者 NeoZeng,上游版本 0.1 / 2022-11-30)。
 */

#ifndef PUBSUB_H
#define PUBSUB_H

#include "stdint.h"

#define MAX_TOPIC_NAME_LEN 32 // 话题名字符串最大长度
#define MAX_TOPIC_COUNT 12    // 同时存在的话题数量上限
#define QUEUE_SIZE 1          // 每个订阅者的消息槽数:1 = 只保最新一帧

typedef struct mqt
{
    /* 用数组模拟 FIFO 队列 */
    void *queue[QUEUE_SIZE];
    uint8_t data_len;
    uint8_t front_idx;
    uint8_t back_idx;
    uint8_t temp_size; // 当前队列长度

    /* 指向下一个订阅了相同话题的订阅者,使发布者能遍历全部订阅者 */
    struct mqt *next_subs_queue;
} Subscriber_t;

/**
 * @brief 发布者实例。持有话题名与订阅者链表头,通过链表访问所有订阅者。
 */
typedef struct ent
{
    /* 话题名称 */
    char topic_name[MAX_TOPIC_NAME_LEN + 1]; // 额外 1 个字节存放字符串结束符 '\0'
    uint8_t data_len;                        // 该话题的数据长度,字节
    /* 指向第一个订阅了该话题的订阅者 */
    Subscriber_t *first_subs;
    /* 指向下一个 Publisher 的指针 */
    struct ent *next_topic_node;
    uint8_t pub_registered_flag; // 该发布者是否已注册
} Publisher_t;

/**
 * @brief  订阅指定话题
 * @param  name     话题名称
 * @param  data_len 消息长度,用 sizeof() 获取
 * @return 订阅者实例;注册失败(话题数超限/内存不足/无名话题)返回 NULL
 * @note   同一话题可被多个模块订阅,发布时按链表逐个投递。
 */
Subscriber_t *SubRegister(char *name, uint8_t data_len);

/**
 * @brief  注册为消息发布者
 * @param  name     发布的话题名称
 * @param  data_len 消息长度,必须与订阅方一致,否则 memcpy 越界
 * @return 发布者实例;注册失败返回 NULL
 */
Publisher_t *PubRegister(char *name, uint8_t data_len);

/**
 * @brief  取走一帧消息
 * @param  sub      订阅者实例
 * @param  data_ptr 接收缓冲,长度需不小于 data_len
 * @return 1 = 取到新消息;0 = 队列为空(本次没发布过,或已被取走)
 * @note   取走即出队;因 QUEUE_SIZE 为 1,两帧之间只取一次会漏掉前一帧。
 */
uint8_t SubGetMessage(Subscriber_t *sub, void *data_ptr);

/**
 * @brief  向所有订阅者推送一帧消息
 * @param  pub      发布者实例
 * @param  data_ptr 待发布数据指针
 * @return 实际投递成功的订阅者数量
 * @note   订阅者未取走旧帧时直接覆盖,不阻塞、不报错。
 */
uint8_t PubPushMessage(Publisher_t *pub, void *data_ptr);

#endif // !PUBSUB_H