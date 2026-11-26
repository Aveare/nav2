/**
 * @file    telemetry.c
 * @brief   遥测环形缓冲:推入/转储(Ozone 观测)
 * @layer   FRAMEWORK
 * @author  JIT CR
 * @author  modified by 小猫大王
* @date    2026-10-27
 */
#include "telemetry.h"
#include <stdio.h>
#include <string.h>
/* 五个全局通道实例,整体放 CCMRAM(.ccmbss NOLOAD,主 RAM 零占用,DEV-20261122-02)。
 * 该段启动不拷贝不清零 → **不能带静态初始化值**(tag 等改由 telemetry_init 运行期赋值),
 * 否则上电是垃圾值。通道语义(接入点见gimbal.c/robot_cmd.c):
 *  yaw/pitch:  ref=云台目标角(degree)  fb=IMU姿态/总角(degree)  out=最终环输出
 *              extra=对应轴角速度(degree/s)  mode=gimbal_mode_e
 *  chassis_cmd: ref=vx(mm/s) fb=预留(速度估算未实现,恒0) out=wz(degree/s)
 *              extra=offset_angle(degree) mode=chassis_mode_e */
/* B6b arena 注入(方案 §4 diag):五环对象由 ports/stm32f4/telemetry_arena.c 定义
 * (.ccmbss 段属性是平台细节,framework 零段宏);本文件仅 extern 消费。 */
extern telemetry_ring_t telemetry_yaw;
extern telemetry_ring_t telemetry_pitch;
extern telemetry_ring_t telemetry_chassis_cmd;
/* 底盘反馈通道:ref=vx指令(mm/s) fb=逆解vx(mm/s) out=逆解vy(mm/s) extra=逆解wz(deg/s)
 * mode=chassis_mode 与 chassis_cmd 分开的原因:那条环的单写者是 cmd_task(命令侧),逆解在
 * chassis_task,写同一条会破单写者。 */
extern telemetry_ring_t telemetry_chassis_fb;
extern telemetry_ring_t telemetry_shoot;
/* (五环对象/通道注释迁 ports/stm32f4/telemetry_arena.c) */

void telemetry_init(void)
{
    /* 整环 memset(5 环共 ~27KB,168MHz 下 <0.2ms,启动期一次,可接受):
     * CCM 段上电是随机值,write_idx 垃圾会让首次写入越界,必须全清 */
    telemetry_ring_t *const rings[] = {&telemetry_yaw, &telemetry_pitch, &telemetry_chassis_cmd,
                                       &telemetry_chassis_fb, &telemetry_shoot};
    for (size_t i = 0; i < sizeof(rings) / sizeof(rings[0]); ++i)
    {
        memset(rings[i], 0, sizeof(telemetry_ring_t));
    }
    diag_arena_assign_tags(); /* tag 赋值随五环对象留 port(B6b arena 注入) */
}

/* 单写者无锁:先填样本内容,最后推进write_idx;
 * 读侧(Ozone/日志)可能读到新旧混合的单条样本,调试数据可容忍. */
void telemetry_push(telemetry_ring_t *ring, float ref, float fb, float out, float extra,
                   uint8_t mode)
{
    uint32_t tick = dwt_get_timeline_ms();
    telemetry_sample_t *s = &ring->samples[ring->write_idx];
    s->tick_ms = tick;
    s->ref = ref;
    s->fb = fb;
    s->out = out;
    s->extra = extra;
    s->mode = mode;
    ring->write_idx = (uint16_t)((ring->write_idx + 1) % TELEMETRY_RING_SIZE);
    ++ring->count;
#if TELEMETRY_LOG_DIV > 0 && !defined(LOG_DISABLE_SYSTEM)
    // Make/release路径定义LOG_DISABLE_SYSTEM时LOG_INFO为空,这里同步跳过log_float_to_str,
    // 避免每0.5s一次的无意义sprintf(比赛场景CPU零浪费).
    if (ring->count % TELEMETRY_LOG_DIV == 0) // RTT日志不支持%f,浮点经log_float_to_str转换
    {
        char s_ref[16], s_fb[16], s_out[16], s_ext[16];
        log_float_to_str(s_ref, ref);
        log_float_to_str(s_fb, fb);
        log_float_to_str(s_out, out);
        log_float_to_str(s_ext, extra);
        LOG_INFO("[tel][%s] t=%u ref=%s fb=%s out=%s ext=%s mode=%u", ring->tag, tick, s_ref, s_fb,
                s_out, s_ext, mode);
    }
#endif
}

void telemetry_dump(const telemetry_ring_t *ring)
{
    // 从最老的存活样本开始按时间序打印,用于现场回放
    uint16_t start = ring->count >= TELEMETRY_RING_SIZE ? ring->write_idx : 0;
    uint16_t valid =
        ring->count >= TELEMETRY_RING_SIZE ? TELEMETRY_RING_SIZE : (uint16_t)ring->count;
    for (uint16_t i = 0; i < valid; ++i)
    {
        const telemetry_sample_t *s = &ring->samples[(start + i) % TELEMETRY_RING_SIZE];
        char s_ref[16], s_fb[16], s_out[16], s_ext[16];
        log_float_to_str(s_ref, s->ref);
        log_float_to_str(s_fb, s->fb);
        log_float_to_str(s_out, s->out);
        log_float_to_str(s_ext, s->extra);
        LOG_INFO("[tel][%s][%u/%u] t=%u ref=%s fb=%s out=%s ext=%s mode=%u", ring->tag, i, valid,
                s->tick_ms, s_ref, s_fb, s_out, s_ext, s->mode);
    }
}
