#include "telemetry.h"
#include <stdio.h>
/**
 * @file    telemetry.c
 * @brief   遥测环形缓冲实现(接口说明见telemetry.h)
 * @author  JIT CR
 * @date    2026-10-27
 */

/* 三个全局通道实例:tag为静态字符串,其余零初始化.
 * 通道语义(接入点见gimbal.c/robot_cmd.c):
 *  yaw/pitch:  ref=云台目标角(degree)  fb=IMU姿态/总角(degree)  out=最终环输出
 *              extra=对应轴角速度(degree/s)  mode=gimbal_mode_e
 *  chassis_cmd: ref=vx(mm/s) fb=预留(速度估算未实现,恒0) out=wz(degree/s)
 *              extra=offset_angle(degree) mode=chassis_mode_e */
Telemetry_Ring_s telemetry_yaw = {.tag = "yaw"};
Telemetry_Ring_s telemetry_pitch = {.tag = "pit"};
Telemetry_Ring_s telemetry_chassis_cmd = {.tag = "chs"};
/* 底盘反馈通道:ref=vx指令(mm/s) fb=逆解vx(mm/s) out=逆解vy(mm/s) extra=逆解wz(deg/s) mode=chassis_mode
 * 与 chassis_cmd 分开的原因:那条环的单写者是 RobotCMDTask(命令侧),逆解在 ChassisTask,写同一条会破单写者。 */
Telemetry_Ring_s telemetry_chassis_fb = {.tag = "chf"};
/* 发射通道:ref=摩擦轮目标RPM fb=左轮RPM(带符号) out=右轮RPM(带符号) extra=拨盘RPM mode=friction_mode
 * RPM 与到速判定同源换算(speed_aps/RPM_2_ANGLE_PER_SEC,转子=轮,拆减速箱驱动)。 */
Telemetry_Ring_s telemetry_shoot = {.tag = "sho"};

/* 单写者无锁:先填样本内容,最后推进write_idx;
 * 读侧(Ozone/日志)可能读到新旧混合的单条样本,调试数据可容忍. */
void TelemetryPush(Telemetry_Ring_s *ring, float ref, float fb, float out, float extra, uint8_t mode)
{
    uint32_t tick = DWT_GetTimeline_ms();
    Telemetry_Sample_s *s = &ring->samples[ring->write_idx];
    s->tick_ms = tick;
    s->ref = ref;
    s->fb = fb;
    s->out = out;
    s->extra = extra;
    s->mode = mode;
    ring->write_idx = (uint16_t)((ring->write_idx + 1) % TELEMETRY_RING_SIZE);
    ++ring->count;
#if TELEMETRY_LOG_DIV > 0 && !defined(DISABLE_LOG_SYSTEM)
    // Make/release路径定义DISABLE_LOG_SYSTEM时LOGINFO为空,这里同步跳过Float2Str,
    // 避免每0.5s一次的无意义sprintf(比赛场景CPU零浪费).
    if (ring->count % TELEMETRY_LOG_DIV == 0) // RTT日志不支持%f,浮点经Float2Str转换
    {
        char s_ref[16], s_fb[16], s_out[16], s_ext[16];
        Float2Str(s_ref, ref);
        Float2Str(s_fb, fb);
        Float2Str(s_out, out);
        Float2Str(s_ext, extra);
        LOGINFO("[tel][%s] t=%u ref=%s fb=%s out=%s ext=%s mode=%u",
                ring->tag, tick, s_ref, s_fb, s_out, s_ext, mode);
    }
#endif
}

void TelemetryDump(const Telemetry_Ring_s *ring)
{
    // 从最老的存活样本开始按时间序打印,用于现场回放
    uint16_t start = ring->count >= TELEMETRY_RING_SIZE ? ring->write_idx : 0;
    uint16_t valid = ring->count >= TELEMETRY_RING_SIZE ? TELEMETRY_RING_SIZE : (uint16_t)ring->count;
    for (uint16_t i = 0; i < valid; ++i)
    {
        const Telemetry_Sample_s *s = &ring->samples[(start + i) % TELEMETRY_RING_SIZE];
        char s_ref[16], s_fb[16], s_out[16], s_ext[16];
        Float2Str(s_ref, s->ref);
        Float2Str(s_fb, s->fb);
        Float2Str(s_out, s->out);
        Float2Str(s_ext, s->extra);
        LOGINFO("[tel][%s][%u/%u] t=%u ref=%s fb=%s out=%s ext=%s mode=%u",
                ring->tag, i, valid, s->tick_ms, s_ref, s_fb, s_out, s_ext, s->mode);
    }
}
