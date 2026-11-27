/**
 * @file    msg.h
 * @brief   消息接口:发布/订阅/接收
 * @layer   FRAMEWORK
 * @author  小猫大王
* @note    与 message_center(旧)的语义对照:.Doc/history/archive/gate/msg-semantic-map.md
*          (T04 验收 = 对照表逐项 PASS)。关键继承语义:
*          - 每订阅者深度 = 1:**只保最新一帧**,新帧直接覆盖未取走的旧帧;
*            适合状态型数据(指令/反馈),事件型需求是"深度>1"新能力(B3+ 再议,
*            本版不实现 —— 方案 T04:深度>1 才是新能力,不进零行为声明);
*          - 多订阅者:同一话题按注册链表逐个投递(message_center 链表机制保留);
*          - 单写者约定:一个话题只应有一个发布任务(文档约定,运行期不查)。
*          关键修正(对照表标注):CheckLen 挂死 → 返回 NULL;PubPushMessage
*          "头文件写数量、实现恒返 1" → msg_publish 返回真实投递数,适配头保留 1。
*
* @note    话题上限 **8**(实施检查;替换 message_center 的死宏 MAX_TOPIC_COUNT=12)。
*          data_len ≤255(uint8 口径不变,超长拒绝而非截断)。
*          **msg_publish 非 ISR**:ISR 调用直接丢弃并计 drop(方案 §4;调试构建
*          由 kernel_is_in_isr 的 port 实现保证)。
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MSG_TOPIC_MAX 8   /* 话题上限(实施检查;方案 §4) */
#define MSG_NAME_MAX  32  /* 话题名最大长度(含 '\0') */
#define MSG_DATA_MAX  255 /* 单条消息上限(uint8 口径继承) */

    typedef struct msg_pub_s *msg_pub_t; /* 不透明句柄 */
    typedef struct msg_sub_s *msg_sub_t;

    /* 注册发布者;失败(话题满/重名不同长/无名/超长)返回 NULL,不挂死 */
    msg_pub_t msg_pub(const char *name, uint8_t data_len);

    /* 订阅话题;同一话题可多订阅者;失败同上 */
    msg_sub_t msg_sub(const char *name, uint8_t data_len);

    /* 取走一帧:1=新帧 0=无(与 SubGetMessage 返回语义一致) */
    int msg_recv(msg_sub_t sub, void *out);

    /* 向该话题全部订阅者推送(覆盖未取走旧帧);返回实际投递到的订阅者数;
     * ISR 内调用:丢弃该帧、计 drop、返回 0(方案 §4 非 ISR 断言口径) */
    uint8_t msg_publish(msg_pub_t pub, const void *data);

    /* 观测 */
    unsigned msg_topic_count(void); /* 当前话题数(≤MSG_TOPIC_MAX) */
    unsigned msg_drop_count(void);  /* ISR 丢弃累计 */

#ifdef __cplusplus
}
#endif
