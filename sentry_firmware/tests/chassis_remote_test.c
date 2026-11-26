/**
 * @file    chassis_remote_test.c
 * @brief   主机测试:CMD 层仲裁与底盘/云台/发射命令断言
 * @author  JIT CR
 * @date    2026-10-27
 */
/* 编译真实CMD应用、DBUS遥控、消息中心与守护线程,仅模拟串口收发.
 * 仲裁输出通过订阅话题断言,不依赖底盘/电机. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "robot_def.h"
#include "dji_motor.h" // ECD_ANGLE_COEF_DJI:云台反馈桩换算用,避免再抄一个魔数
#include "robot_cmd.h"
#include "remote_control.h"
#include "message_center.h"
#include "daemon.h"
#include "telemetry.h"
#include "bsp_usart.h"
#include "autonomy.h"
#include "ax650_usb.h"

int autonomy_get_command(sentry_auto_cmd_t *out)
{
    if (out)
        memset(out, 0, sizeof(*out));
    return 0;
}
void ax650_usb_set_mode(uint8_t mode) { (void)mode; }

UART_HandleTypeDef huart3; // main.h桩中声明,此处定义
static USARTInstance remote_usart;
static Subscriber_t *chassis_sub, *gimbal_sub, *shoot_sub;
static Chassis_Ctrl_Cmd_s chassis_cmd;
static Gimbal_Ctrl_Cmd_s gimbal_cmd;
static Shoot_Ctrl_Cmd_s shoot_cmd;

static uint32_t fake_ms = 1000;
float DWT_GetDeltaT(uint32_t *count)
{
    *count += 1;
    return 0.001f;
}
uint32_t DWT_GetTimeline_ms(void)
{
    return ++fake_ms; // 每1ms
}

/* Float2Str桩:与bsp_log.c相同语义(整数.3位小数,带符号),供telemetry链接 */
void Float2Str(char *str, float va)
{
    int flag = va < 0;
    int head = (int)va;
    int point = (int)((va - head) * 1000);
    head = abs(head);
    point = abs(point);
    if (flag)
        sprintf(str, "-%d.%d", head, point);
    else
        sprintf(str, "%d.%d", head, point);
}

USARTInstance *USARTRegister(USART_Init_Config_s *config)
{
    assert(config->usart_handle == &huart3);
    assert(config->recv_buff_size == 18);
    remote_usart.usart_handle = config->usart_handle;
    remote_usart.recv_buff_size = config->recv_buff_size;
    remote_usart.module_callback = config->module_callback;
    return &remote_usart;
}
void USARTServiceInit(USARTInstance *instance) { (void)instance; }

/* DJI DBUS 18字节帧:4路11位摇杆(ch0右横,ch1右纵,ch2左横,ch3左纵),
 * byte5低2位右拨杆/高2位左拨杆(1上2下3中),byte16-17拨轮11位. */
static void RcFrame(int r_h, int r_v, int l_h, int l_v,
                    unsigned right, unsigned left, int dial, uint16_t len)
{
    uint16_t ch[5] = {(uint16_t)(r_h + 1024), (uint16_t)(r_v + 1024),
                      (uint16_t)(l_h + 1024), (uint16_t)(l_v + 1024), (uint16_t)(dial + 1024)};
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
    RobotCMDTask();
    assert(SubGetMessage(chassis_sub, &chassis_cmd) == 1);
    assert(SubGetMessage(gimbal_sub, &gimbal_cmd) == 1);
    assert(SubGetMessage(shoot_sub, &shoot_cmd) == 1);
}

static void DummyLost(void *id) { (void)id; }

static void ExpectSafe(void)
{
    assert(chassis_cmd.chassis_mode == CHASSIS_ZERO_FORCE);
    assert(gimbal_cmd.gimbal_mode == GIMBAL_ZERO_FORCE);
    assert(shoot_cmd.shoot_mode == SHOOT_OFF);
    assert(shoot_cmd.friction_mode == FRICTION_OFF);
    assert(shoot_cmd.load_mode == LOAD_STOP);
}

/* 原解锁流程:右上+左中+全回中;解锁当周期仍保持零力 */
static void Unlock(void)
{
    RcFrame(0, 0, 0, 0, RC_SW_UP, RC_SW_MID, 0, 18);
    Pump();
    ExpectSafe();
}

/* 解锁后右中+左下:检查底盘模式与两路命令 */
static void ArmFollow(void)
{
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.chassis_mode ==
           (CHASSIS_TUNING_ONLY ? CHASSIS_RC_CONTROL : CHASSIS_FOLLOW_GIMBAL_YAW));
    assert(chassis_cmd.offset_angle == 0.0f);
    assert(gimbal_cmd.gimbal_mode == GIMBAL_GYRO_MODE);
    assert(shoot_cmd.shoot_mode == SHOOT_ON);
    assert(shoot_cmd.friction_mode == FRICTION_OFF); // 摩擦轮由拨轮扳机驱动,左下挡不再开关
    assert(shoot_cmd.load_mode == LOAD_STOP);
}

int main(void)
{
    RobotCMDInit();
    chassis_sub = SubRegister("chassis_cmd", sizeof(Chassis_Ctrl_Cmd_s));
    gimbal_sub = SubRegister("gimbal_cmd", sizeof(Gimbal_Ctrl_Cmd_s));
    shoot_sub = SubRegister("shoot_cmd", sizeof(Shoot_Ctrl_Cmd_s));
    assert(!RemoteControlIsOnline());

    /* 云台反馈桩:让 yaw 单圈角=对准零位 -> offset≈0,解锁回中立即结束。
     * 对应实车"解锁时云台已对准前方"的状态;消息队列保留最新,推一次即长期生效。 */
    {
        Publisher_t *feed_pub = PubRegister("gimbal_feed", sizeof(Gimbal_Upload_Data_s));
        Gimbal_Upload_Data_s feed = {0};
        /* 必须跟 config 的 YAW_CHASSIS_ALIGN_ECD 联动。这里原先是写死的 3564,
         * 2026-11-15 把宏重标成 5892 后,桩喂进来的角度就与宏差 +104°,
         * 解锁回中去追这个假目标 -> gimbal_cmd.yaw != 0,后面第 4 步断言假失败。
         * 用宏现算,标定值怎么改测试都自洽。 */
        if (!CHASSIS_TUNING_ONLY)
        {
            feed.yaw_motor_single_round_angle = YAW_CHASSIS_ALIGN_ECD * ECD_ANGLE_COEF_DJI;
            PubPushMessage(feed_pub, (void *)&feed);
        }
    }

    // 1.上电失联:零力(来源RC_LOST,输出与安全态一致)
    Pump();
    ExpectSafe();

    // 2.上电即锁存:运行挡位也不能自动恢复输出
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();

    // 3.原解锁流程解锁后,遥控目标正常穿透
    Unlock();
    ArmFollow();

    // 4.实发DBUS帧验证本次前后、左右反向；通道+660对应正向底盘命令。
    RcFrame(0, 660, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == CHASSIS_MAX_SPEED && chassis_cmd.vy == 0);
    assert(gimbal_cmd.yaw == 0 && gimbal_cmd.pitch == 0);
    RcFrame(0, -660, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == -CHASSIS_MAX_SPEED && chassis_cmd.vy == 0);
    RcFrame(660, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == 0 && chassis_cmd.vy == CHASSIS_MAX_SPEED);
    RcFrame(-660, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == 0 && chassis_cmd.vy == -CHASSIS_MAX_SPEED);
    RcFrame(660, 660, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx > 0 && chassis_cmd.vy > 0);
    assert(fabsf(hypotf(chassis_cmd.vx, chassis_cmd.vy) - CHASSIS_MAX_SPEED) < 0.01f);
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == 0 && chassis_cmd.vy == 0);
    RcFrame(29, 29, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(chassis_cmd.vx == 0 && chassis_cmd.vy == 0); // 回中后带内不重新起动

    RcFrame(0, 0, 0, 0, RC_SW_DOWN, RC_SW_MID, 0, 18);
    Pump();
    assert(chassis_cmd.chassis_mode == CHASSIS_ROTATE);
    assert(chassis_cmd.offset_angle == 0.0f);

    // 5.失联立即零力;数据恢复后仍锁存,须重新解锁(原语义)
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 17);
    Pump();
    assert(!RemoteControlIsOnline());
    ExpectSafe();
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 6.左上挡急停:立即零力并锁存;回中后仍锁存,须解锁
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_UP, 400, 18);
    Pump();
    ExpectSafe();
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_MID, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 7.拨轮急停:down>300触发;拨轮回中后仍锁存,须解锁
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 400, 18);
    Pump();
    ExpectSafe();
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 8.失联+左上挡同时:输出同为安全态,来源取更高优先级的RC_LOST(靠日志区分)
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_UP, 400, 17);
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 9.100ms超时失联与物理断链等效
    for (unsigned i = 0; i < 11; ++i)
        DaemonTask();
    Pump();
    ExpectSafe();
    Unlock();
    ArmFollow();

    // 10.云台软限位按当前调试行程，不能沿用旧整车角度上限。
    RcFrame(0, 0, 0, 660, RC_SW_MID, RC_SW_DOWN, 0, 18);
    for (unsigned i = 0; i < 600; ++i)
        Pump();
    assert(gimbal_cmd.pitch == PITCH_COMMAND_MAX_DEG);

    // 11.daemon init_count上线等待窗口生效(修复前被reload_count覆盖)
    Daemon_Init_Config_s lost_conf = {.reload_count = 5, .init_count = 12, .callback = DummyLost};
    DaemonInstance *d = DaemonRegister(&lost_conf);
    assert(d->temp_count == 12); // init_count生效
    for (unsigned i = 0; i < 7; ++i)
        DaemonTask();
    assert(DaemonIsOnline(d)); // 12-7=5>0;修复前第5拍即超时
    for (unsigned i = 0; i < 5; ++i)
        DaemonTask();
    assert(!DaemonIsOnline(d));

    // 12.未填init_count沿用reload_count(与遥控器等现有用户行为一致)
    Daemon_Init_Config_s plain_conf = {.reload_count = 8, .init_count = 0, .callback = DummyLost};
    DaemonInstance *d2 = DaemonRegister(&plain_conf);
    assert(d2->temp_count == 8);

    // 13.遥测环形缓冲——覆盖后保留最新256条,write_idx/count正确
    for (unsigned i = 0; i < 300; ++i)
        TelemetryPush(&telemetry_yaw, (float)i, (float)(i * 2), 1.0f * i, 0.5f * i, (uint8_t)i);
    assert(telemetry_yaw.count == 300);
    assert(telemetry_yaw.write_idx == 300 % TELEMETRY_RING_SIZE); // 44
    assert(telemetry_yaw.samples[44].tick_ms > 0);
    assert((unsigned)telemetry_yaw.samples[44].ref == 44);        // 最老存活样本
    assert((unsigned)telemetry_yaw.samples[43].ref == 299);       // 最新样本
    assert(telemetry_yaw.samples[43].fb == 598.0f);               // i*2
    TelemetryDump(&telemetry_yaw);                                // 回放打印(stub日志no-op),不应崩溃

    // 14.通道独立性 + cmd环接入点验证(随前面场景的Pump已累计)
    assert(telemetry_chassis_cmd.count > 0);            // RobotCMDTask接入点已产生数据
    assert(telemetry_pitch.count == 0);                 // pitch环无独立写入
    uint32_t chs_before = telemetry_chassis_cmd.count;
    TelemetryPush(&telemetry_pitch, 1.0f, 2.0f, 3.0f, 4.0f, 1);
    assert(telemetry_pitch.count == 1);
    Pump(); // 此刻rc已被case11的DaemonTask耗尽为离线:仲裁输出零力
    assert(telemetry_chassis_cmd.count == chs_before + 1);
    uint16_t last = (uint16_t)((telemetry_chassis_cmd.write_idx + TELEMETRY_RING_SIZE - 1) % TELEMETRY_RING_SIZE);
    assert((unsigned)telemetry_chassis_cmd.samples[last].ref == 0);
    // 遥测推送位于RobotCMDTask末尾,记录的是仲裁后实际下发的命令:离线→ZERO_FORCE(0)
    assert(telemetry_chassis_cmd.samples[last].mode == 0);

#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
    // 15.左下挡:左横杆改控**小 yaw**,底盘不再由左横杆转向;大yaw世界目标不受影响。
    Unlock();
    ArmFollow();
    const float old_yaw = gimbal_cmd.yaw;
    RcFrame(0, 0, 660, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    Pump();
    assert(gimbal_cmd.yaw == old_yaw);
    assert(chassis_cmd.chassis_mode == CHASSIS_RC_CONTROL);
    assert(chassis_cmd.offset_angle == 0 && chassis_cmd.wz == 0.0f);
    assert(gimbal_cmd.small_yaw_rate_deg_s != 0.0f);

    // 15b.左中挡:左横杆仍作底盘转向,小 yaw 指令归零(两轴互斥)。
    RcFrame(0, 0, 660, 0, RC_SW_MID, RC_SW_MID, 0, 18);
    Pump();
    assert(chassis_cmd.wz == -CHASSIS_ROTATE_SPEED);
    assert(gimbal_cmd.small_yaw_rate_deg_s == 0.0f);

    // 16.拨轮扳机:压下即开摩擦轮(与拨弹盘同源启动),短拨松手沿登记单发。
    //    必须放在鼠标用例之前:键鼠一旦激活会保持 3s,期间 RemoteControlSet 不被调用。
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_MID, -660, 18);
    Pump();
    assert(shoot_cmd.friction_mode == FRICTION_ON); // 进区即开轮
    assert(shoot_cmd.load_mode == LOAD_STOP);       // 首拍只开轮,不拨弹
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_MID, 0, 18);
    Pump();
    assert(shoot_cmd.friction_mode == FRICTION_OFF);  // 松手关轮(shoot 侧再延时 1s)
    assert(shoot_cmd.load_mode == LOAD_1_BULLET);     // 短拨松手沿 = 单发

    // 17.鼠标横移接管底盘转向，不改变锁定正前方的大yaw目标。
    RcFrame(0, 0, 0, 0, RC_SW_MID, RC_SW_DOWN, 0, 18);
    remote_usart.recv_buff[6] = 100; // DBUS mouse.x低字节
    remote_usart.module_callback();
    Pump();
    assert(chassis_cmd.chassis_mode == CHASSIS_RC_CONTROL);
    assert(chassis_cmd.wz < 0 && chassis_cmd.wz >= -SENTRY_CHASSIS_MOUSE_RATE_DEG_S);
    assert(gimbal_cmd.small_yaw_rate_deg_s == 0.0f); // 键鼠不控小 yaw
    assert(gimbal_cmd.yaw == old_yaw);
#endif
    puts("PASS: authority/RC-loss/kill/latch/unlock, pitch-limit, relative-yaw-lock/independent-chassis-steering, telemetry.");
    return 0;
}
