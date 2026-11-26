/**
 * @file    chassis_remote_test.c
 * @brief   主机测试:CMD 层仲裁与底盘/云台/发射命令断言
 * @layer   TESTS
 * @author  JIT CR
 * @date    2026-10-27
 */
/* 编译真实CMD应用、DBUS遥控、消息中心与守护线程,仅模拟串口收发.
 * 仲裁输出通过订阅话题断言,不依赖底盘/电机. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "robot_def.h"
#include "general_def.h" // MATH_ABS
#include "dji_motor.h" // DJI_ECD_TO_DEG:云台反馈桩换算用,避免再抄一个魔数
#include "robot_cmd.h"
#include "robot_cmd_internal.h"
#include "remote_control.h"
#include "msg.h"
#include "daemon.h"
#include "telemetry.h"
#include "usart_port.h"
#include "ax650_uart1.h"

UART_HandleTypeDef huart3; // main.h桩中声明,此处定义
static usart_t remote_usart;
static msg_sub_t chassis_sub, gimbal_sub, shoot_sub;
static chassis_ctrl_cmd_t chassis_cmd_ut;
static gimbal_ctrl_cmd_t gimbal_cmd_ut;
static shoot_ctrl_cmd_t shoot_cmd_ut;
static ax650_uart1_state_t ax650_state;
static uint8_t ax650_tx_mode;

static uint32_t fake_ms = 1000;
float dwt_get_delta_t(uint32_t *count)
{
    *count += 1;
    return 0.001f;
}
uint32_t dwt_get_timeline_ms(void)
{
    return ++fake_ms; // 每1ms
}
int kernel_is_in_isr(void) { return 0; } /* msg_publish ISR 守卫桩:主机恒否 */

void ax650_uart1_set_mode(uint8_t mode)
{
    ax650_tx_mode = mode;
}
void ax650_uart1_get_state(ax650_uart1_state_t *out)
{
    *out = ax650_state;
}

/* log_float_to_str桩:与bsp_log.c相同语义(整数.3位小数,带符号),供telemetry链接 */
void log_float_to_str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = MATH_ABS(head);
    point = MATH_ABS(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}

usart_t *usart_register(usart_init_config_t *config)
{
    assert(config->usart_handle == &huart3);
    assert(config->recv_buff_size == 18);
    remote_usart.usart_handle = config->usart_handle;
    remote_usart.recv_buff_size = config->recv_buff_size;
    remote_usart.module_callback = config->module_callback;
    return &remote_usart;
}
void usart_service_init(usart_t *instance)
{
    (void)instance;
}

/* DJI DBUS 18字节帧:4路11位摇杆(ch0右横,ch1右纵,ch2左横,ch3左纵),
 * byte5低2位右拨杆/高2位左拨杆(1上2下3中),byte16-17拨轮11位. */
static void RcFrame(int r_h, int r_v, int l_h, int l_v, unsigned right, unsigned left, int dial,
                    uint16_t len)
{
    uint16_t ch[5] = {(uint16_t)(r_h + 1024), (uint16_t)(r_v + 1024), (uint16_t)(l_h + 1024),
                      (uint16_t)(l_v + 1024), (uint16_t)(dial + 1024)};
    memset(remote_usart.recv_buff, 0, sizeof(remote_usart.recv_buff));
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned b = 0; b < 11; ++b)
            if (ch[c] & (1u << b))
                remote_usart.recv_buff[(c * 11 + b) / 8] |= (uint8_t)(1u << ((c * 11 + b) % 8));
    remote_usart.recv_buff[5] |= (uint8_t)((right & 3u) << 4);
    remote_usart.recv_buff[5] |= (uint8_t)((left & 3u) << 6);
    remote_usart.recv_buff[16] = (uint8_t)(ch[4] & 0xff);
    remote_usart.recv_buff[17] = (uint8_t)(ch[4] >> 8);
    remote_usart.recv_len = len;
    remote_usart.module_callback();
}

/* 跑一个CMD周期,并从话题取回三路发布值 */
static void Pump(void)
{
    cmd_task();
    assert(msg_recv(chassis_sub, &chassis_cmd_ut) == 1);
    assert(msg_recv(gimbal_sub, &gimbal_cmd_ut) == 1);
    assert(msg_recv(shoot_sub, &shoot_cmd_ut) == 1);
}

static void DummyLost(void *id)
{
    (void)id;
}

static void ExpectSafe(void)
{
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(gimbal_cmd_ut.gimbal_mode == GIMBAL_ZERO_FORCE);
    assert(shoot_cmd_ut.shoot_mode == SHOOT_OFF);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF);
    assert(shoot_cmd_ut.load_mode == LOADER_STOP);
}

/* 原解锁流程:右上+左中+全回中;解锁当周期仍保持零力 */
static void Unlock(void)
{
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_UP, REMOTE_RC_SW_MID, 0, 18);
    Pump();
    ExpectSafe();
}

/* 解锁后右中+左下:跟随模式+陀螺闭环，摩擦轮保持默认关闭 */
static void ArmFollow(void)
{
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
#else
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_FOLLOW_GIMBAL_YAW);
#endif
    assert(gimbal_cmd_ut.gimbal_mode == GIMBAL_GYRO_MODE);
    assert(shoot_cmd_ut.shoot_mode == SHOOT_ON);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF);
    assert(shoot_cmd_ut.load_mode == LOADER_STOP);
}

int main(void)
{
    cmd_init();
    chassis_sub = msg_sub("chassis_cmd", sizeof(chassis_ctrl_cmd_t));
    gimbal_sub = msg_sub("gimbal_cmd", sizeof(gimbal_ctrl_cmd_t));
    shoot_sub = msg_sub("shoot_cmd", sizeof(shoot_ctrl_cmd_t));
    assert(!remote_control_is_online());

    /* 云台反馈桩:让 yaw 单圈角=对准零位 -> offset≈0,解锁回中立即结束。
     * 对应实车"解锁时云台已对准前方"的状态;消息队列保留最新,推一次即长期生效。 */
    {
        msg_pub_t feed_pub = msg_pub("gimbal_feed", sizeof(gimbal_upload_t));
        gimbal_upload_t feed = {0};
        /* 必须跟 config 的 YAW_CHASSIS_ALIGN_ECD 联动。这里原先是写死的 3564,
         * 2026-11-15 把宏重标成 5892 后,桩喂进来的角度就与宏差 +104°,
         * 解锁回中去追这个假目标 -> gimbal_cmd_ut.yaw != 0,后面第 4 步断言假失败。
         * 用宏现算,标定值怎么改测试都自洽。 */
        feed.yaw_motor_single_round_angle = YAW_CHASSIS_ALIGN_ECD * DJI_ECD_TO_DEG;
        msg_publish(feed_pub, (void *)&feed);
    }

    // 1.上电失联:零力(来源RC_LOST,输出与安全态一致)
    Pump();
    ExpectSafe();

    // 2.上电即锁存:运行挡位也不能自动恢复输出
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();

    // 3.原解锁流程解锁后,遥控目标正常穿透
    Unlock();
    ArmFollow();

    // 4.右杆前推660 -> vx=CHASSIS_MAX_SPEED,vy=0;左杆不动云台目标不变
    RcFrame(0, 660, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(chassis_cmd_ut.vx == 0 && chassis_cmd_ut.vy == 0 && chassis_cmd_ut.wz == 0);
#else
    assert(chassis_cmd_ut.vx == CHASSIS_MAX_SPEED && chassis_cmd_ut.vy == 0);
#endif
    assert(gimbal_cmd_ut.yaw == 0 && gimbal_cmd_ut.pitch == 0);

    // 4a.左中挡切 AX650 自瞄，只采用 0x0503 的目标角，协议 shoot 不得触发发射
    ax650_state.command.yaw = 12.5f;
    ax650_state.command.pitch = -4.0f;
    ax650_state.command.shoot = 3u;
    ax650_state.command.target_id = 3u;
    ax650_state.command_count = 1u;
    ax650_state.command_valid = 1u;
    yaw_recentering = 1u; // 模拟解锁/历史切挡残留，自瞄必须主动取消回中
    recenter_cnt = 17u;
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, 0, 18);
    Pump();
#if AX650_UART1_ENABLE
    assert(ax650_tx_mode == AX650_AUTO_AIM_MODE); // 自瞄挡把 ProgramMode 上报给 AX650
#else
    /* USART1 让给底盘 VOFA 时 AX650 链路不初始化,不上报模式;
     * 控制侧的选目标逻辑仍要照常走,故只跳过这一条上报断言。 */
    assert(ax650_tx_mode == AX650_TX_MODE);
#endif
    assert(gimbal_cmd_ut.gimbal_mode == GIMBAL_AUTO_AIM_MODE);
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
#else
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW); // 自瞄右中:不跟随 yaw 6020
#endif
    assert(yaw_recentering == 0u && recenter_cnt == 0u);
    assert(gimbal_cmd_ut.yaw == 0.0f && gimbal_cmd_ut.pitch == -4.0f); // 从当前实际角平滑接入，不阶跃到12.5°
    assert(cmd_get_ax650_raw_yaw() == 12.5f);
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
    assert(shoot_cmd_ut.shoot_mode == SHOOT_ON);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF && shoot_cmd_ut.load_mode == LOADER_STOP);
#else
    assert(shoot_cmd_ut.shoot_mode == SHOOT_OFF);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF && shoot_cmd_ut.load_mode == LOADER_STOP);
#endif

    // 4a-1.同一目标的新视觉帧经过卡尔曼滤波，不对1°测量跳变直接全量阶跃
    ax650_state.command.yaw = 13.5f;
    ax650_state.command_count = 2u;
    Pump();
    assert(gimbal_cmd_ut.yaw > 0.0f && gimbal_cmd_ut.yaw < 13.5f);
    assert(cmd_get_ax650_raw_yaw() == 13.5f);

    // 4a-2.自瞄右下:小陀螺下仍可平移；拨回右中后直接无跟随，不进入底盘回中
    RcFrame(0, 660, 0, 0, REMOTE_RC_SW_DOWN, REMOTE_RC_SW_MID, 0, 18);
    Pump();
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(chassis_cmd_ut.vx == 0 && chassis_cmd_ut.vy == 0 && chassis_cmd_ut.wz == 0);
#else
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ROTATE);
    assert(chassis_cmd_ut.vx == CHASSIS_MAX_SPEED && chassis_cmd_ut.vy == 0);
#endif
    RcFrame(660, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, 0, 18);
    Pump();
#if CHASSIS_M3508_FORCE_DISABLE
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(chassis_cmd_ut.vx == 0 && chassis_cmd_ut.vy == 0 && chassis_cmd_ut.wz == 0);
#else
    assert(chassis_cmd_ut.chassis_mode == CHASSIS_NO_FOLLOW);
    assert(chassis_cmd_ut.vx == 0 && chassis_cmd_ut.vy == CHASSIS_MAX_SPEED);
#endif

    // 4b.视觉挡空闲时摩擦轮关闭；拨轮压入后开启并只发一个单发脉冲，保持不重复，回中后重装
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, -300, 18);
    Pump();
#if SHOOT_AUTO_AIM_REMOTE_ENABLE
    assert(shoot_cmd_ut.shoot_mode == SHOOT_ON);
    assert(shoot_cmd_ut.friction_mode == FRICTION_ON && shoot_cmd_ut.load_mode == LOADER_SINGLE);
    Pump();
    assert(shoot_cmd_ut.friction_mode == FRICTION_ON && shoot_cmd_ut.load_mode == LOADER_STOP);
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, 0, 18);
    Pump();
    assert(shoot_cmd_ut.friction_mode == FRICTION_ON && shoot_cmd_ut.load_mode == LOADER_STOP);
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, -300, 18);
    Pump();
    assert(shoot_cmd_ut.load_mode == LOADER_SINGLE);
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, 0, 18);
    fake_ms += (uint32_t)SHOOT_DIAL_FRICTION_KEEP_MS + 1u;
    Pump();
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF && shoot_cmd_ut.load_mode == LOADER_STOP);
#else
    assert(shoot_cmd_ut.shoot_mode == SHOOT_OFF);
    assert(shoot_cmd_ut.friction_mode == FRICTION_OFF && shoot_cmd_ut.load_mode == LOADER_STOP);
#endif

    // 4c.左下挡退出自瞄，恢复摇杆云台，并把上报模式恢复为默认值
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
    assert(ax650_tx_mode == AX650_TX_MODE);
    assert(gimbal_cmd_ut.gimbal_mode == GIMBAL_GYRO_MODE);

    // 5.坏帧立即零力;数据恢复后仍锁存,须重新解锁
    // (DEV-20261121-06:流式装配下"长度≠18"不再等于坏帧 —— 半帧由 bsp 拼装,
    //  故改为注入非法档位的完整帧来验证坏帧判据)
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
    remote_usart.recv_buff[5] = 0; // 左右开关档位全 0 = 非法帧
    remote_usart.recv_len = 18;
    remote_usart.module_callback();
    Pump();
    assert(!remote_control_is_online());
    ExpectSafe();
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 6.左上挡急停:立即零力并锁存;回中后仍锁存,须解锁
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_UP, 400, 18);
    Pump();
    ExpectSafe();
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_MID, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 7.拨轮急停:down>300触发;拨轮回中后仍锁存,须解锁
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 400, 18);
    Pump();
    ExpectSafe();
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 8.失联+左上挡同时:输出同为安全态,来源取更高优先级的RC_LOST(靠日志区分)
    // (DEV-20261121-06:流式装配下"长度≠18"表达不了坏帧,改用非法档位的完整帧)
    RcFrame(0, 0, 0, 0, REMOTE_RC_SW_MID, REMOTE_RC_SW_UP, 400, 18);
    remote_usart.recv_buff[5] = 0; // 开关档位非法 → 坏帧
    remote_usart.recv_len = 18;
    remote_usart.module_callback();
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 9.100ms超时失联与物理断链等效
    for (unsigned i = 0; i < 11; ++i)
        daemon_task();
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 10.云台软限位:左纵杆持续推满,pitch被夹在PITCH_MAX_ANGLE
    RcFrame(0, 0, 0, 660, REMOTE_RC_SW_MID, REMOTE_RC_SW_DOWN, 0, 18);
    for (unsigned i = 0; i < 400; ++i)
        Pump();
    assert(gimbal_cmd_ut.pitch == PITCH_MAX_ANGLE);

    // 11.daemon init_count上线等待窗口生效(修复前被reload_count覆盖)
    daemon_init_config_t lost_conf = {.reload_count = 5, .init_count = 12, .callback = DummyLost};
    daemon_t *d = daemon_register(&lost_conf);
    assert(d->temp_count == 12); // init_count生效
    for (unsigned i = 0; i < 7; ++i)
        daemon_task();
    assert(daemon_is_online(d)); // 12-7=5>0;修复前第5拍即超时
    for (unsigned i = 0; i < 5; ++i)
        daemon_task();
    assert(!daemon_is_online(d));

    // 12.未填init_count沿用reload_count(与遥控器等现有用户行为一致)
    daemon_init_config_t plain_conf = {.reload_count = 8, .init_count = 0, .callback = DummyLost};
    daemon_t *d2 = daemon_register(&plain_conf);
    assert(d2->temp_count == 8);

    // 13.遥测环形缓冲——覆盖后保留最新256条,write_idx/count正确
    for (unsigned i = 0; i < 300; ++i)
        telemetry_push(&telemetry_yaw, (float)i, (float)(i * 2), 1.0f * i, 0.5f * i, (uint8_t)i);
    assert(telemetry_yaw.count == 300);
    assert(telemetry_yaw.write_idx == 300 % TELEMETRY_RING_SIZE); // 44
    assert(telemetry_yaw.samples[44].tick_ms > 0);
    assert((unsigned)telemetry_yaw.samples[44].ref == 44);  // 最老存活样本
    assert((unsigned)telemetry_yaw.samples[43].ref == 299); // 最新样本
    assert(telemetry_yaw.samples[43].fb == 598.0f);         // i*2
    telemetry_dump(&telemetry_yaw);                          // 回放打印(stub日志no-op),不应崩溃

    // 14.通道独立性 + cmd环接入点验证(随前面场景的Pump已累计)
    assert(telemetry_chassis_cmd.count > 0); // cmd_task接入点已产生数据
    assert(telemetry_pitch.count == 0);      // pitch环无独立写入
    uint32_t chs_before = telemetry_chassis_cmd.count;
    telemetry_push(&telemetry_pitch, 1.0f, 2.0f, 3.0f, 4.0f, 1);
    assert(telemetry_pitch.count == 1);
    Pump(); // 此刻rc已被case11的daemon_task耗尽为离线:仲裁输出零力
    assert(telemetry_chassis_cmd.count == chs_before + 1);
    uint16_t last = (uint16_t)((telemetry_chassis_cmd.write_idx + TELEMETRY_RING_SIZE - 1) %
                               TELEMETRY_RING_SIZE);
    assert((unsigned)telemetry_chassis_cmd.samples[last].ref == 0);
    // 遥测推送位于cmd_task末尾,记录的是仲裁后实际下发的命令:离线→ZERO_FORCE(0)
    assert(telemetry_chassis_cmd.samples[last].mode == 0);

#if CHASSIS_SPIN_VARY_EN
    // 15.键鼠 C 沿切换小陀螺档位:状态随 chassis_cmd.spin_vary 下发(DEV-20261125-08)
    {
        assert(chassis_cmd_ut.spin_vary == 1u); // 默认变速
        /* 模拟图传键鼠:按住 W 切入键鼠源(切入帧只同步沿基线,不补触发) */
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = (uint16_t)(1u << REMOTE_KEY_W);
        Pump();
        assert(input_source == INPUT_KBM);
        /* C 沿:变速 → 匀速 → 变速。此刻仲裁为失联零力,只断言档位本身 */
        rc_data[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS][REMOTE_KEY_C]++;
        Pump();
        assert(chassis_cmd_ut.spin_vary == 0u);
        rc_data[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS][REMOTE_KEY_C]++;
        Pump();
        assert(chassis_cmd_ut.spin_vary == 1u);
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = 0;
    }
#endif

    // 16.小陀螺锁存期间不因键鼠超时回落遥控(DEV-20261125-10)
    {
        rc_data[RC_SLOT_CURRENT].mouse.x = 0;
        rc_data[RC_SLOT_CURRENT].mouse.y = 0;
        rc_data[RC_SLOT_CURRENT].mouse.press_l = 0;
        rc_data[RC_SLOT_CURRENT].mouse.press_r = 0;
        /* 按住 W 保持键鼠源,再按 Q 开小陀螺 */
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = (uint16_t)(1u << REMOTE_KEY_W);
        Pump();
        rc_data[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q]++;
        Pump();
        /* 松手静置超过 KBM_TIMEOUT_MS:小陀螺锁存应让输入源留在键鼠 */
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = 0;
        fake_ms += (uint32_t)KBM_TIMEOUT_MS + 10u;
        Pump();
        assert(input_source == INPUT_KBM);
        /* 按 Q 关小陀螺后再静置:应恢复超时回落 */
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = (uint16_t)(1u << REMOTE_KEY_W);
        rc_data[RC_SLOT_CURRENT].key_count[REMOTE_KEY_PRESS][REMOTE_KEY_Q]++;
        Pump();
        rc_data[RC_SLOT_CURRENT].key[REMOTE_KEY_PRESS].keys = 0;
        fake_ms += (uint32_t)KBM_TIMEOUT_MS + 10u;
        Pump();
        assert(input_source == INPUT_RC);
    }

    puts("PASS: authority arbitration (rc-lost, kill-switch, dial, latch, unlock, passthrough, "
         "pitch-limit, daemon-init, telemetry-ring, spin-vary-toggle, spin-idle-hold).");
    return 0;
}
