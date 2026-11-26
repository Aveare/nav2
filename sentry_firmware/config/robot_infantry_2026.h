/**
 * @file    robot_infantry_2026.h
 * @brief   2026赛季步兵本车配置——本车参数唯一改动点
 * @author  JIT CR
 * @date    2026-10-29
 *
 * 换车/换赛季:复制本文件改名(如 robot_hero_2027.h),在 robot_def.h 切换 include,
 * 不改任何业务代码。每个参数只保留"当前值+一句话依据";
 * 调参依据与踩坑记录见 .Doc/本车调参结论.md。
 */
#pragma once

/* ----------板型(烧录时注意对应功能;修改后需重新编译,只能存在一个!) ---------- */
#define ONE_BOARD // 单板控制整车
// #define CHASSIS_BOARD //底盘板
// #define GIMBAL_BOARD  //云台板

#ifndef CHASSIS_TUNING_ONLY
#define CHASSIS_TUNING_ONLY 1 // 1=单板底盘调试,0=整车
#endif

#ifndef SENTRY_BIG_YAW_CONTROL_ENABLE
#define SENTRY_BIG_YAW_CONTROL_ENABLE 1u // 大yaw锁定相对底盘的云台正前方，使用编码器双环
#endif
#ifndef SENTRY_PITCH_CONTROL_ENABLE
#define SENTRY_PITCH_CONTROL_ENABLE 1u // 底盘调试模式接入达妙 pitch
#endif
#ifndef SENTRY_SHOOT_CONTROL_ENABLE
#define SENTRY_SHOOT_CONTROL_ENABLE 1u // 底盘调试模式下接入 CAN2 发射(两摩擦轮+拨盘)
#endif

#if CHASSIS_TUNING_ONLY
/* 四轮允许在遥控解锁后输出，急停和断流仍由原控制链处理。 */
#ifndef DJI_MOTOR_CAN1_FORCE_DISABLE
#define DJI_MOTOR_CAN1_FORCE_DISABLE 0
#endif
#ifndef DJI_MOTOR_CAN1_ENABLE_BOOT
#define DJI_MOTOR_CAN1_ENABLE_BOOT 1
#endif
#endif

/* ---------------- 哨兵上部电机接线接口 ---------------- */
/* [用户提供 ID，CAN 总线按工程预留接口映射 2026-11-21] 哨兵上部电机。
 * Snail/C615 摩擦轮为专用 ESC 组合，不作为 M3508/C620 反馈电机注册。
 * 此组供哨兵硬件接入使用，现有步兵 GimbalInit/ShootInit 尚未切换到此组。
 */
#define SENTRY_FRICTION_CAN_BUS 2u
#define SENTRY_FRICTION_1_ESC_ID 1u // DJI Snail 2305 + C615，CAN2
#define SENTRY_FRICTION_2_ESC_ID 2u // DJI Snail 2305 + C615，CAN2
#define SENTRY_LOADER_CAN_BUS 2u
#define SENTRY_LOADER_DJI_ESC_ID 3u // DJI M2006 P36 + C610，CAN2
#define SENTRY_BIG_YAW_CAN_BUS 1u
#define SENTRY_BIG_YAW_DM_CAN_ID 5u // 达妙大 yaw 控制 ID，CAN1（用户最新确认）
#define SENTRY_BIG_YAW_DM_MASTER_ID 0x000u // [用户截图] 大 yaw 反馈 ID 对应电机 Master ID 0x0
#define SENTRY_BIG_YAW_FRONT_ALIGN_VALID 1u // [用户确认摆正并实测] 启用固定正前方参考
#define SENTRY_BIG_YAW_FRONT_ALIGN_RAD (-1.52914f)
/* [实测，用户确认回正通过 2026-11-26] 机械正前方对应 sentry_big_yaw_position_rad=-1.52914rad。
 * 当前方向与同步带传动比均为1，标定值等于达妙输出轴编码器位置。
 * 解锁并切入运行挡后，串级PID按最短角度误差回正；急停和失联仍保持零力。
 * 拆装传动或改变编码器零点后须重新标定。 */
#define SENTRY_BIG_YAW_TRANSMISSION_RATIO 1.0f // [用户确认] 外部同步带：电机输出轴/大yaw轴=1:1
#define SENTRY_BIG_YAW_DIRECTION 1.0f // [待实测] 电机输出轴到大yaw关节的正方向
#define SENTRY_BIG_YAW_IMU_DIRECTION 1.0f // [待实测] IMU航向正向到大yaw关节正向，取+1或-1
#define SENTRY_BIG_YAW_IMU_TIMEOUT_MS 20.0f // [理论] 姿态解算停更20ms后停止世界稳像
#define SENTRY_BIG_YAW_ANGLE_KP 20.0f
/* [待实测] 角度环 Kp,1/s。**70 → 20 是治抖的主要手段**。
 * 70 的量级意味着 2° 误差就要 2.45 rad/s(已顶到 90°/s 限速),速度环再乘 3.6
 * 立刻满力——在同步带虚位下,电机端一动负载没跟上,误差还在,于是继续满力,
 * 表现为到位后高频抖动/嗡嗡。20 时 2° 只要 0.7 rad/s,接近目标时出力柔和。
 * 若变得"回正拖沓"(大角度也走不快):先看是不是被 MAX_SPEED_DEG_S 限住,再小幅回调到 30。 */

#define SENTRY_BIG_YAW_TARGET_BAND_ENTER_DEG 0.5f
/* [待实测] 到位滞回**进入**阈值,deg。误差小于它才判定"已到位"(断力)。
 * 与下面的退出阈值构成施密特滞回:没有滞回时,轴在边界附近一进一出就会反复断力/满力,
 * 这是"到位后一直抖"的第二个来源。 */

#define SENTRY_BIG_YAW_TORQUE_LPF_RC_S 0.03f
/* [理论] 力矩指令一阶低通时间常数,s。虚位带来的冲击是高频的,30ms 低通把它压掉,
 * 对回正速度(秒级)几乎没有影响。调到 0.06 会更绵但拖沓;0.01 以下基本等于没加。 */
#define SENTRY_BIG_YAW_KD_NEAR 0.8f // [用户指定，待实测] 角环测量微分，无量纲
#define SENTRY_BIG_YAW_KD_FAR 0.2f // [用户指定，待实测] 远目标降低阻尼以跟手
#define SENTRY_BIG_YAW_KD_BLEND_LO_DEG 4.0f // [参考方案] deg，近目标阻尼边界
#define SENTRY_BIG_YAW_KD_BLEND_HI_DEG 8.0f // [参考方案] deg，远目标阻尼边界
#define SENTRY_BIG_YAW_SPEED_KP 5.2f // [待实测] Nm/(rad/s)，保留本轮开始时源码中的跟速增益
#define SENTRY_BIG_YAW_SPEED_KI 1.2f // [待实测] Nm/rad，保留本轮开始时源码值，积分限幅2Nm
#define SENTRY_BIG_YAW_INTEGRAL_NM 2.0f // [用户指定，待实测] Nm
#define SENTRY_BIG_YAW_TORQUE_RANGE_NM 10.0f
/* [实测 2026-11-22 上位机截图] 大 yaw 驱动器 TMAX=10(PMAX 12.5 / VMAX 30 / 齿轮系数 1 / 阻尼因子 4)。
 * **本值必须等于驱动器 TMAX**:MIT 帧力矩是 12 位定点数,发送编码与反馈解码都用它。
 * 写成 12.5(手册峰值)而驱动器按 10 解:实际出力 = 指令 × 0.8、反馈力矩被高估 25%
 * ——DEV-23 记的"指令约 -0.65Nm"真实只有约 -0.52Nm,这正是静差调不下来的隐藏原因之一。 */
#define SENTRY_BIG_YAW_TORQUE_NM 10.0f
/* [实测 同上] 控制器力矩限幅,Nm。取 ≤ 映射量程,否则 PID 会在一个编码不出来的值上积分饱和。
 * 注意 10Nm 仍是 J4310(额定约 3.5Nm)的 3 倍,只允许短时冲击;持续顶格会过热降额。 */
#define SENTRY_BIG_YAW_MAX_SPEED_DEG_S 90.0f // [待实测] deg/s，角环+前馈总限速
#define SENTRY_CHASSIS_MOUSE_RATE_DEG_S 60.0f // [待实测] deg/s，鼠标横移对应底盘转向限速
#define DM_STARTUP_WAIT_MS 30u // [参考工程] 使能后非阻塞等待，再发送 MIT 控制帧
#define DM_ENABLE_RETRY_MS 100u // [待实测] 无反馈或失能时重试使能的间隔
#ifndef CAN1_DIAGNOSTIC_CAPTURE_ENABLE
#define CAN1_DIAGNOSTIC_CAPTURE_ENABLE 0 // 诊断构建设为1，接收并记录CAN1全部标准数据帧
#endif

#define CAN2_DIAGNOSTIC_CAPTURE_ENABLE 0
/* [诊断开关] 1 = 接收并记录 CAN2 上全部标准数据帧(未注册 ID 也记录),用于定位
 * "某个电机到底在哪个 ID 回话"。2026-11-22 用它查出小 yaw 在 0x205(拨码 1)而非 0x208,
 * 任务完成后已复位为 0。
 * 置 1 的代价:第一个过滤器被改成全通,未匹配帧也进中断;同一帧可能经全通与 IDLIST 各进一次 FIFO,
 * 已注册实例收到重复帧(解码幂等)。**它是临时诊断手段,不要留在正式固件里。** */
/* ---------------- 小 yaw(DJI GM6020,CAN2) ---------------- */
/* [用户更正 2026-11-22] 小 yaw 是 **DJI GM6020**;接 **CAN2 拨码 1**(实测,见下方 ESC_ID 注释)。
 * 驱动走 modules/motor/DJImotor/dji_motor.c 的 GM6020 分支(与四轮/摩擦轮同一套三环与组帧)。
 * 此前按达妙写的裸 CAN 通道(application/gimbal/sentry_small_yaw_comm.c)已删除:
 * 它不解码/不使能/不闭环,且会与达妙 pitch 争 rx=0 导致启动期 CAN id crash。 */

#define SENTRY_SMALL_YAW_CAN_BUS 2u
/* [用户确认] 挂 CAN2。 */

#define SENTRY_SMALL_YAW_ESC_ID 1u
/* [实测 2026-11-22 CAN2 全 ID 捕获] GM6020 拨码 **1**,不是 4。
 * 证据:全 ID 捕获中只出现 0x000/0x201/0x202/0x203/0x205,其中 0x205 的载荷随手动同步变化,
 * 按本工程解码布局(ecd=byte0<<8|byte1,与 C620 同布局、无 ID 字节)解析为
 * 角度74 / 转速0 / 电流0 / 温度23℃,完全自洽;且系统内不存在 M3508 拨码 5,排除 0x205 的另一来源。
 * 取 1 → 命令帧 **0x1FF**(组3,数据字节 0-1)、反馈 **0x205**。
 * **避让提醒**:GM6020 拨码 1~4 与 M3508/M2006 拨码 5~8 反馈帧同号(1↔5,2↔6,3↔7,4↔8)。
 * 本车 CAN2 上没有拨码 5~8 的 3508/2006,故不冲突;将来加电机时必须避开。 */

#define SENTRY_SMALL_YAW_MODEL "GM6020"
/* [用户更正 2026-11-22] 直流无刷 + 内置编码器,直驱无外置减速比;额定力矩约 1.2N·m。
 * 力矩量级与大 yaw 的达妙 J4310(额定约 3.5N·m)不同,增益不能互相照抄。 */
/* ---------------- 小 yaw:位置保持(手推不动) + 左下挡摇杆控制 ---------------- */
/* [用户要求 2026-11-22 修订,取代上一版"自由阻尼轴"] 小 yaw 必须在**任何情况下保持稳定、
 * 手推也推不动**,并有一条完整控制链路供人主动操作。
 *
 * 旧版只做速度阻尼 + 母体角速度补偿、没有位置目标 → 推走不回位(用户实测"根本不对")。
 * 现在改为**角度环位置保持**:目标角以机械中位为 0 点,摇杆只改目标,不直接给速度。
 *
 * 控制链(角度环在 C 板 200Hz,速度环在驱动 1kHz):
 *   target_deg(相对中位,夹在 ±(LIMIT−MARGIN))
 *     └ 角度 P:err = target − joint → speed_ref = clamp(KP_ang·err, ±MAX_SPEED_DEG_S)
 *         └ 速度环(反馈 = 电机编码器 deg/s) → 电压码 → GM6020
 *   joint 由单圈 ecd 解缠得到:中位 7465 距回绕点仅 727 计数,必须解缠,否则跨零误判。
 *
 * 为什么不做世界系保持:IMU 装在**大 yaw 平台**上,量不到这个轴。小 yaw 世界角
 * = 大 yaw 世界角 + 相对角,而大 yaw 本身锁车体,所以"相对保持"等价于整个上部相对车体固定。
 * 只有给小 yaw 加独立 IMU 才谈得上世界系稳像。 */

#define SENTRY_SMALL_YAW_ENABLE 1u
/* [配置] 是否注册并运行小 yaw。1 = 常开;0 = 不注册(GimbalInit 跳过,不影响大 yaw/pitch)。
 * 前提:ESC 拨码与实际一致(见 SENTRY_SMALL_YAW_ESC_ID),否则 CAN 上多一个"没人应答"的实例。 */

#define SENTRY_SMALL_YAW_ANGLE_KP 25.0f
/* [待实测] 角度环 Kp,1/s。误差 deg → 速度目标 deg/s。
 * **这个值决定"手推推不推得动"**:25 意味着 4° 偏差就要求 100°/s,人手基本推不动;
 * 想让手动微调更轻松 → 降到 12~15;想更硬 → 加到 40,但过高会在到位时来回蹭(配合死区)。 */

#define SENTRY_SMALL_YAW_ANGLE_DEADBAND_DEG 0.3f
/* [待实测] 角度死区,deg。误差小于此值不再修正,避免到位后为编码器量化噪声持续出力(嗡嗡/发热)。
 * 大 → 停得更松、会有可见小偏差;小 → 更容易抖。0.3° 沿用旧 yaw 的同量级经验值。 */

#define SENTRY_SMALL_YAW_MAX_SPEED_DEG_S 180.0f
/* [待实测] 角度环输出的速度上限,deg/s。同时是"解锁回中位"的限速。
 * 大 → 回中/摇杆响应更快,但到位冲击大;小 → 平顺但拖沓。 */

#define SENTRY_SMALL_YAW_STICK_RATE_DEG_S 120.0f
/* [待实测] 左下挡满杆时的目标角速度,deg/s。摇杆量按比例积分到目标角,
 * 所以这是"转多快"而不是"转多少";松手立刻停在当前目标。 */

#define SENTRY_SMALL_YAW_STICK_DIR (-1.0f)
/* [待实测] 摇杆符号,只取 +1 或 -1。左横杆推左而小 yaw 往右转,就把它取反。 */

#define SENTRY_SMALL_YAW_SPEED_KP 50.0f
/* [待实测] 速度环 Kp,输出码/(deg/s)。比上一版(20)提高 2.5 倍:
 * 位置环要靠速度环把误差落到实处,"手推不动"很大程度取决于这个刚度。
 * 出现高频抖动/啸叫就回调;顶住手仍缓慢偏 → 继续加。 */

#define SENTRY_SMALL_YAW_SPEED_KI 0.0f
/* [理论] 速度环 Ki = 0。位置环已承担积分作用,速度环再加积分等于双积分,
 * 到位必过冲并来回摆。 */

#define SENTRY_SMALL_YAW_INTEGRAL_LIMIT 2000
/* [理论] 积分限幅,输出码。Ki=0 时无效,仅在将来把 Ki 调非零时生效。 */

#define SENTRY_SMALL_YAW_MAX_OUT 16000
/* [待实测] 输出限幅,电调原始码。电压帧满量程 25000 ≈ 24V,16000 ≈ 64%。
 * 上一版 8000(32%)是为"手能推动"刻意压低的;现在要求"推不动",必须放开。
 * 若堵转发热明显或到位冲击过大 → 回调到 12000。 */

#define SENTRY_SMALL_YAW_DEADBAND_DEG_S 3.0f
/* [理论 · 待实测] 速度误差死区,deg/s。编码器差分在零速附近有量化抖动,
 * 不加死区会听到静止时持续"滋滋";过大会让轻微随动变钝。 */

#define SENTRY_SMALL_YAW_OUTPUT_RC_S 0.005f
/* [理论] 输出一阶低通时间常数,s。噪声经 Kp 放大后进电机,
 * 5ms 在这个环里几乎不损失动态,却能压掉毛刺。 */

#define SENTRY_SMALL_YAW_CENTER_ECD 27u
/* [用户实测 2026-11-22 21:13] 机械中位对应单圈 ecd = **27**(此前给的 7465 作废)。
 * 同时也是"解锁回中"的目标角 0 点。**注意 27 离回绕点 8192 只有 27 计数(≈1.2°)**,
 * 比 7465 更贴边——正向转过 1.2° 就会跨到 0 再往上数,所以解缠是硬要求,裸比较必错。 */

#define SENTRY_SMALL_YAW_LIMIT_DEG 90.0f
/* [用户给定 2026-11-22 修订] 机构只能转**半圆 180°**,以中位 27 为中心左右各 90°。
 * **后半圆是禁区,绝对不能进**(此前写的 ±100° 作废:那会越过半圆)。
 * 换算(1° = 22.76 计数,半圆 = 4096 计数):
 *   左极限 −90° → 27 − 2048 = −2021 → 回绕 **6171**
 *   右极限 +90° → 27 + 2048 = **2075**
 *   允许区 = [6171..8191] ∪ [0..2075];禁区 = [2075..6171](后半圆,中心 4123)。
 * 允许区跨编码器零点,所以任何比较都必须先解缠,不能直接比 ecd 大小。 */

#define SENTRY_SMALL_YAW_LIMIT_MARGIN_DEG 10.0f
/* [理论 · 待实测] 目标角预留余量,deg。目标角夹在 ±(LIMIT−MARGIN) = **±80°**,
 * 正常操作离半圆端点还有 10° 给惯性冲过头。余量不能太小:目标贴到 90° 时,
 * 任何过冲都会真进禁区。 */

#define SENTRY_SMALL_YAW_RETURN_DEG_S 60.0f
/* [待实测] 硬越界回程速度,deg/s。关节角一旦越过 ±90°(被外力硬推、装配偏置、编码器跳变),
 * 放弃角度环、按固定速度往允许区内拉。20°/s 太软:推过去以后慢悠悠回来,手感像"没限位";
 * 60°/s 才有明显的"顶回来"。 */

#define SENTRY_SMALL_YAW_CARRIED_LIMIT_ENABLE 1u
/* [配置] 是否启用行程保护(目标角夹限 + 硬越界回程)。0 = 只做位置保持、不管行程。 */

#define SENTRY_SMALL_YAW_CURRENT_MODE 0u
/* [配置] 命令帧类型:0 = 电压帧 0x2FF(默认,任何电调都能用);
 * 1 = 电流帧 0x2FE(±16384 ↔ ±3A,力矩与指令成正比,高速抗反电动势更好)。
 * **开电流帧的前提**:该电调在 RoboMaster Assistant 里已打开"电流环",
 * 否则现象是反馈全部正常、就是不出力(本车 2026-11 踩过,见 .Doc/本车调参结论.md §1)。 */

#define SENTRY_PITCH_CAN_BUS 2u
#define SENTRY_PITCH_DM_CAN_ID 5u // [用户确认] CAN2 达妙 DM-J4310-2EC V1.2
#define SENTRY_PITCH_DM_MASTER_ID 0x000u // [用户截图] pitch反馈ID对应电机Master ID 0x0
#define SENTRY_PITCH_ANGLE_KP 8.0f
/* [步兵同值 · 待实测] 角度环 Kp,1/s(电机输出轴 deg 误差 → deg/s 目标)。
 * 原取 4 是为了"轻载调试慢慢看";摇杆速率恢复到 60°/s 后,4 会让目标追不上杆
 * (要 15° 误差才凑出 60°/s),体感发黏。取步兵的 8:7.5° 误差即可满速。
 * 若到位后来回蹭 → 先加大 SENTRY_PITCH_DEADBAND_DEG,再考虑降 Kp。 */
#define SENTRY_PITCH_SPEED_KP 0.6f // [待实测] Nm/(rad/s)，4310输出轴速度反馈
#define SENTRY_PITCH_SPEED_KI 0.2f // [待实测] Nm/rad，小积分补静态负载
#define SENTRY_PITCH_INTEGRAL_NM 0.3f // [待实测] Nm，不复制旧机重力前馈
#define SENTRY_PITCH_TORQUE_NM 1.0f // [待实测] Nm，轻云台调试出力上限
#define SENTRY_PITCH_TORQUE_RANGE_NM 10.0f
/* [配置] pitch 这颗达妙 J4310 的 MIT 力矩映射量程,±Nm,**必须等于驱动器 TMAX 参数**。
 * 依据:DM 出厂默认 TMAX=10(本车前一轮上位机截图同型号大 yaw 也是 P/V/T = 12.5/30/10)。
 * 一致性为什么致命:MIT 帧的力矩是 12 位定点数,编码/解码都用这个量程。
 *   TMAX 写大了 → 实际出力 = 指令 ×(TMAX_驱动/量程),少给力;
 *   量程写大了 → 反馈力矩被按比例高估,看到的"已出力 0.65Nm"其实只有 0.52Nm。
 * 改这里之前先去上位机把 TMAX 改成同一个数,两边不同步等于整条力矩链是错的。 */
#define SENTRY_PITCH_MAX_SPEED_DEG_S 90.0f
/* [待实测] deg/s,角度环输出(速度目标)上限。原 30 会成为硬瓶颈:摇杆给 60°/s,
 * 角度环再怎么算也超不过 30 → 体感"推了但走不快"。取 90 给满杆 60°/s 留 1.5 倍余量。 */
#define SENTRY_BIG_YAW_DEADBAND_DEG 0.02f // [用户指定，待实测] deg，大yaw角环死区

#define SENTRY_BIG_YAW_TARGET_DEADBAND_DEG 2.0f
/* [理论 · 待实测] 到位滞回**退出**阈值,deg。误差超过它才重新接管出力。
 * 与 ENTER(0.5°)构成滞回:进入要 <0.5°,退出要 >2.0°,中间保持上一状态。
 *
 * 为什么需要它:本车大 yaw 有 1.1~1.6deg 消除不掉的静差(DEV-19/21/23),积分必然触顶;
 * 触顶后继续顶着的只是摩擦,不产生位移,代价是持续发热 + 持续"嗡嗡"。
 *
 * **取值约束**:退出阈值必须 ≥ 实测静差,否则永远进不了带内,等于没加。
 * 2.0deg 是"略高于实测上限 1.639deg"的最小可用值。
 * 对照现象:停车后仍长时间嗡嗡/发热 → 阈值仍小于静差,继续加;
 *           回正后肉眼可见偏角 → 阈值偏大,减。 */
#define SENTRY_PITCH_DEADBAND_DEG 0.3f // [待实测] deg，pitch电机轴角环死区
#define SENTRY_AXIS_OUTPUT_RC_S 0.012f // [参考方案] s，速度PI输出低通
#define SENTRY_AXIS_D_LIMIT_DEG_S 30.0f // [待实测] deg/s，角环微分限幅
#define SENTRY_AXIS_FF_GAIN 1.0f // [参考方案] 目标角速度前馈系数
#define SENTRY_AXIS_FF_LPF 0.3f // [参考方案] 前馈一阶低通系数
#define SENTRY_PITCH_DIRECTION 1.0f // [待实测] 左纵杆正增量对应电机正转；连杆实际抬头方向待实测
#define SENTRY_PITCH_TRAVEL_DEG 20.0f // [用户要求放宽，待实测] 4310输出轴相对解锁点各20度，不是连杆实际俯仰角
#define SENTRY_PITCH_RATE_DEG_S 60.0f
/* [步兵同值 · 待实测] 左纵杆满杆时的目标角速度,deg/s。原为 10(调试限速),
 * 与步兵(60)差 6 倍,推杆体感就是"没反应"。改回 60:满杆 0.33s 走完 ±20° 窗口。
 * 同时是任务内目标斜坡的上限,所以目标跟杆不再被二次限速。 */

#define SENTRY_PITCH_MOUSE_RATE_DEG_S 150.0f
/* [步兵同值] 键鼠甩动上限,deg/s(=KBM_PITCH_MAX_RATE)。鼠标路径原与遥控共用 10°/s,
 * 在那个量级下鼠标几乎推不动 pitch。 */
#if CHASSIS_TUNING_ONLY && SENTRY_PITCH_CONTROL_ENABLE
#define PITCH_COMMAND_MIN_DEG (-SENTRY_PITCH_TRAVEL_DEG)
#define PITCH_COMMAND_MAX_DEG SENTRY_PITCH_TRAVEL_DEG
#define PITCH_REMOTE_RATE_DEG_S SENTRY_PITCH_RATE_DEG_S
#define PITCH_MOUSE_RATE_DEG_S SENTRY_PITCH_MOUSE_RATE_DEG_S
#else
#define PITCH_COMMAND_MIN_DEG PITCH_MIN_ANGLE
#define PITCH_COMMAND_MAX_DEG PITCH_MAX_ANGLE
#define PITCH_REMOTE_RATE_DEG_S 60.0f
#define PITCH_MOUSE_RATE_DEG_S KBM_PITCH_MAX_RATE
#endif

#define VISION_USE_VCP // 旧 Seasky 模块的编译选择；AX650 使用下方独立 USART1 链路

#define AX650_USB_ENABLE 1   // 共享 USB CDC 自瞄姿态上报(0x1021)与目标接收(0x0503)
#define AX650_UART1_ENABLE 0 // 自瞄迁移至共享 USB CDC，关闭旧 USART1 AX650
#if AX650_USB_ENABLE && AX650_UART1_ENABLE
#error "AX650 USB CDC and USART1 transports must be mutually exclusive"
#endif
#define AX650_COMMAND_TIMEOUT_MS 100u // [协议] 0x0503 下行命令有效期,ms
#define AX650_AUTO_AIM_MODE 1u        // [协议] 左中挡上报的自瞄模式
#ifndef SENTRY_AUTONOMY_ENABLE
#define SENTRY_AUTONOMY_ENABLE 0      // [安全] 0=只回传角度且导航命令硬清零;1=允许导航接管底盘
#endif
#define SENTRY_NAVIGATION_FEEDBACK_PERIOD_MS 20u  // [配置] 大 yaw 新鲜 CAN 角度回传周期,ms(50 Hz)
#define SENTRY_NAVIGATION_ACCEL_MM_S2 350.0f      // [首测] 导航专用平移斜坡,不改手动参数
#define SENTRY_NAVIGATION_MAX_SPEED_M_S 0.35f     // [首测] 每轴导航限速,m/s
#define SENTRY_NAVIGATION_MAX_WZ_RAD_S 0.5f       // [首测] 导航角速度限幅,rad/s
#define SENTRY_AUTONOMY_COMMAND_TIMEOUT_MS 300.0f // [安全] 自主导航指令看门狗,ms
#define AX650_POSE_PERIOD_MS 10u // [协议] 姿态上报间隔，ms；100 Hz，实机时序待验证
#define AX650_TX_SPEED_MPS 0.0f // [协议] 标定占位弹速，m/s；不表示实测弹速
#define AX650_TX_MODE 0u // [协议] 标定模式占位，无量纲；AX650 正常自瞄枚举为 1

/* ---------------- 云台零位与限位 ---------------- */

#define YAW_CHASSIS_ALIGN_ECD 5892
/* [实测 2026-10-28] yaw 对准底盘正前方时的编码器值,原始刻度(0~8191)。
 * 失效:拆装 yaw 传动(同步带/齿轮)后必须重标,否则跟随律 offset 会带固定偏置,
 *       表现为"底盘永远差几度才追上云台"。
 * 标法:底盘固定,手动把云台转到与底盘正前方重合,读 yaw 电机 ecd。 */

#define PITCH_HORIZON_ECD 47
/* [实测 2026-10-28] pitch 处于水平时的编码器值,原始刻度。
 * 失效:拆装 pitch 传动后重标;标法:pitch 摆到水平基准,读 ecd。 */

#define PITCH_MAX_ANGLE 25.0f
/* [待实测] 俯仰上限,deg(IMU 反馈域)。
 * 注意:它决定云台会不会顶机械限位,当前无实测。本车未做过"推到极限读角度"的标定;
 *       若极限处有堵转声或电机发烫,说明设大了。 */

#define PITCH_MIN_ANGLE (-20.0f)
/* [待实测] 俯仰下限,deg(IMU 反馈域)。与上限同源,均未经机械限位实测。 */

/* ---------------- 发射 ---------------- */

#define NUM_PER_CIRCLE 7
/* [实测] 拨盘一圈装弹量,实机数槽得 7。
 * 失效:换拨盘或改槽数后必须重数,否则单发/三发的角度推进量全部错。 */

#define REDUCTION_RATIO_LOADER 36.0f
/* [规格] M2006 + C610 拨盘减速比,官方规格值。 */

#define ONE_BULLET_DELTA_ANGLE (360.0f / NUM_PER_CIRCLE)
/* [实测 换算] 拨出一发弹丸时拨盘输出轴的转角,输出轴 deg(不是电机轴)。
 * 7 槽均分 = 360/7 ≈ 51.43 deg。原为上游图纸遗留的固定值 36,该值对应上游 8 槽机型
 * 且量纲与本机不符,已实车标定修正(DEVLOG 记录)。
 * 标法:单发一次,量拨盘实际转角;若实测偏离 51.43 明显,说明槽数或标定有误。 */

#define LOADER_ANGLE_TOL_DEG 6.0f
/* [理论] 单发到位容差,输出轴 deg(约 1/8 槽距)。
 * 大 → 提前判定到位,未完全落槽就下一发,可能连拨两发;
 * 小 → 角度环迟迟不到位,靠 LOADER_SINGLE_TIMEOUT_MS 兜底中止。
 * 本车未做容差单变量实验,6° 为按槽距比例取的经验值。 */

#define LOADER_SINGLE_TIMEOUT_MS 1000.0f
/* [理论] 单发起步后的到位超时,ms。超时中止当前队列并告警。
 * 大 → 卡弹时电机硬顶更久,发热/磨弹;
 * 小 → 正常拨弹偶尔被判超时(低速起步或弹丸偏紧时)。
 * 1000ms 为经验兜底,未实测正常单发耗时分布。 */

#define SHOOT_RATE_DEFAULT 6.0f
/* [换算 待实测] 连发射速,发/s。6 发/s ≈ 拨盘 1852RPM(该换算关系确定)。
 * 旧版为固定 2000 RPM ≈ 6.5 发/s,现改为按发/s 下发。
 * 未实测:射频上限、卡弹边界、弹道散布随射频的变化——调高是否卡弹无数据。 */

/* 拨轮扳机式,原始刻度域 [-660, 660]:
 * 压过入口即进入触发区,不分深浅;短拨松手沿 = 单发,按住 ≥ 判别时长 = 连发(松手停)。
 * 进入/退出阈值差构成迟滞带,滤拨轮回中过程中的边界抖动。 */

#define DIAL_TRIGGER_ZONE (-240)
/* [实测-部分] 扳机区入口,原始刻度。dial ≤ 此值进入扳机判定。
 * 更灵敏 → 上移至 -300 方向(DEVLOG 建议值);
 * 更迟钝 → 下移。现有 240 出自实车手感,但未做系统扫描。 */

#define DIAL_TRIGGER_RELEASE (-220)
/* [理论] 扳机区退出阈值,原始刻度,比入口高 20 构成迟滞。
 * 大 → 迟滞带宽,松手判定更迟钝,可能出现"松了还连着";
 * 小 → 带太窄,拨轮边界抖动会重复触发单发。
 * 20 刻度为经验值,未实测拨轮 dither 幅度。 */

#define DIAL_SINGLE_HOLD_MS 250.0f
/* [实测-部分] 按住判别时长,ms。按住 ≥ 此值转连发,否则按短拨处理(松手沿登记单发)。
 * 大 → 连发起动更迟(体感"压了没反应");
 * 小 → 想单发却容易滑进连发。
 * DEVLOG 给出三档:嫌慢降到 150~200,误触发连发加到 350。 */

#define FRICTION_TARGET_RPM 4000.0f
/* [实测] 摩擦轮目标转速,RPM(电机转子)。
 * 大 → 弹速升,但 5000 实测弹速过快、弹道不稳;
 * 小 → 3000 实测弹速不足,明显下坠。
 * 依据:实车对比 3000/4000/5000 三档,4000 为可用区间中点。 */

#define FRICTION_READY_TOL_RPM 500.0f
/* [理论] 双轮到速容差,RPM。两轮都进入 ±此值才允许拨弹(互锁)。
 * 大 → 到速判定宽松,拨弹更早,但弹速离散度变大;
 * 小 → 等待更久,首发延迟增加,极端情况下迟迟不放行。
 * 500 为占空比容差经验值,未测"容差与弹速标准差"的关系。 */

/* 拨盘堵转保护:参考非零 + 转速≈0 + 电流超阈,持续超时 → 停盘锁存,拨轮回中解除。 */

#define LOADER_JAM_CURRENT 1800.0f
/* [待实测] 堵转电流阈值,C610 原始单位(满量程 10000)。
 * 大 → 真卡弹判不出来,继续硬顶会磨弹、烧电机;
 * 小 → 正常拨弹的瞬时冲击就被误判,频繁停盘。
 * 当前 1800 为估值,未做堵转台架实验(应记录堵住后的稳态电流分布)。 */

#define LOADER_JAM_SPEED_RPM 30.0f
/* [待实测] 判"堵住"的转速门限,RPM。低于此值且电流超阈才计入堵转累计。
 * 大 → 低速正常拨弹被误判;
 * 小 → 真堵了但编码器残余抖动被判为在转,永不计入。
 * 30 为估值,无实测。 */

#define LOADER_JAM_HOLD_MS 500.0f
/* [待实测] 堵转持续判定时长,ms。超时才锁存,避免瞬时冲击误判。
 * 大 → 真卡弹后硬顶时间更长;
 * 小 → 拨弹冲击被误判为堵转。
 * 500 为估值,无实测。 */

#define FRICTION_RPM_OFFSET 0.0f
/* [待实测] 摩擦轮转速微调,RPM,叠加在 FRICTION_TARGET_RPM 上(弹速标定 P2 后按挡位填)。
 * 当前为占位,未接入控制(接点:P2 定 k/b 后在 SetRef 处 +OFFSET);默认 0 不改变行为。 */

#define FRICTION_STOP_DELAY_MS 1000.0f
/* [用户要求 2026-11-22] 摩擦轮**延时停**,ms。松开摩擦轮后仍按目标转速继续跑这段时间再归零,
 * 让最后拨出的那发还吃得到轮速(否则松手瞬间轮速掉,末发弹道偏低)。
 * 1000ms 是用户指定值。**急停 / SHOOT_OFF 不享受此延时**:那条路径直接 DJIMotorStop。
 * 副作用:松手后轮子还会响 1 秒,别误判成"没停"。 */

#define LOADER_BACKOFF_EN 0
/* [配置] 拨盘自动退弹开关(发射计划 P4-2)。0 = 现状(堵转锁存等人工解除);
 * 1 = 堵转后反转 1.2 槽距再重试,2 次失败才锁存。启用前必须先做 P4-0 台架实验定阈值,
 * 并定义与待发队列状态机的回退联动;默认 0 不改变行为。 */

#define SHOOT_CALIB_LOG 1
/* [配置] 发射标定日志开关:每 0.5s 一行 [shootcal] fl/fr/ldr/cur(双轮RPM带符号/拨盘RPM/拨盘电流)。
 * 用途:P1-B3 转速稳定性(纹波要用遥测环看,日志只做存档)、P2 弹速标定、P4-0 堵转实验对照。
 * 高频纹波观察用 Ozone 看 telemetry_shoot 环;标定结束后置 0。 */

#define LOADER_WAIT_TIMEOUT_MS 2000.0f
/* [理论] 单发/三发"等待起步"的最长时间,ms。
 * 队列已登记但摩擦轮未到速时,状态机会一直等待(无起步就无超时)。
 * 超时即丢弃队列,防止 pending 长期悬挂、之后在意外时刻补发。
 * 大 → 摩擦轮慢起时更能容忍,但悬挂期更长;
 * 小 → 摩擦轮尚未到速就放弃,正常射击被吞。
 * 依据:摩擦轮 0→4000 RPM 约 0.3~0.5s,取 4 倍余量;本车未做起步时间实测。 */

#define LOADER_FRICTION_OFF_GRACE_MS 100.0f
/* [理论] 摩擦轮关闭到丢弃待发队列的宽限期,ms。
 * 存在理由:按 R/E 与按右键常在同一帧,右键的摩擦轮标志可能比按键沿晚一个周期(5ms),
 * 若一关就清会把刚登记的请求误删,表现为"按了没反应"。
 * 大 → 停火后残余弹数保留更久,误补发风险回升;
 * 小 → 同时按键的容错窗口不足。
 * 100 覆盖人手同时按键的时间差,又远小于一次连射的间隔。 */

/* ---------------- 发射电机接线与 PID(布局参考 RM-JIT-CR-Sentinel) ---------------- */

/* 本车发射机构与参考工程布局一致:两摩擦轮 M3508+C620、拨盘 M2006+C610,全部挂 CAN2。
 * 下方 ID 是**电调拨码号**,不是组帧 ID;组帧由 dji_motor.c 自动分组(id1-4 → 0x200,反馈 0x200+id),
 * can2 用组 3/4/5。与 CAN2 上的达妙 pitch(发 5 / 收 0,MIT 帧)分属不同协议,互不干扰。 */

#define FRICTION_ID_L 1
/* [配置] 左摩擦轮拨码号,CAN2。左轮因新结构安装镜像用 REVERSE(只作用于输出,反馈仍是原始方向)。 */
#define FRICTION_ID_R 2
/* [配置] 右摩擦轮拨码号,CAN2,NORMAL。 */
#define LOADER_ID 3
/* [配置] 拨盘拨码号,CAN2,正转=拨出。 */

#define FRICTION_SPEED_PID_KP 2.4f
/* [参考工程实车] 摩擦轮速度环 Kp,反馈 deg/s(转子)。本车摩擦轮同型号同减速比,直接沿用。 */
#define FRICTION_SPEED_PID_KI 0.5f
/* [参考工程实车] 摩擦轮速度环 Ki。 */
#define FRICTION_SPEED_PID_IL 1500
/* [理论] 摩擦轮速度环积分限幅,C620 原始码(±16384 ↔ ±20A)。 */
#define FRICTION_SPEED_PID_MAXOUT 6000
/* [理论] 摩擦轮速度环输出限幅,C620 原始码。C620 内部电流环已启用,外环电流环保留占位。 */

/* 拨盘增益**不能直接照抄参考工程**:用户 2026-11-22 说明本车拨盘硬件阻尼比参考小很多。
 * 速度环的闭环阻尼 ≈ 对象粘性/库仑摩擦 + Kp。摩擦小了同样的 Kp 就是等效过增益,
 * 表现为低速爬行抖动、连发忽快忽慢、松手后多滑一小段。故速度环 Kp/Ki 取参考值的一半起步,
 * 输出限幅同步下调(起转不再需要那么大顶力,堵转时硬顶也轻,少磨弹)。全部标 [待实测]。 */

#define LOADER_SPEED_PID_KP 3.0f
/* [理论 · 待实测] 拨盘速度环 Kp,反馈 deg/s(转子)。参考工程实车 6.0;阻尼更小,取一半。
 * 判据:连发 6 发/s 听不到"咔咔"爬行、转速纹波平滑 → 合适;抖/啸叫 → 降到 2.0;发软 → 回到 4.0~5.0。 */
#define LOADER_SPEED_PID_KI 0.4f
/* [理论 · 待实测] 拨盘速度环 Ki。阻尼小则静态阻力小,不需要大积分去顶,防止低速积分爬行。
 * 大了 → 低速时积分推着走、发一发后还多转一点;小了 → 载弹量变化后稳态掉速。 */
#define LOADER_SPEED_PID_IL 600
/* [理论] 拨盘速度环积分限幅,C610 原始码(满量程 10000)。 */
#define LOADER_SPEED_PID_MAXOUT 5000
/* [理论 · 待实测] 拨盘速度环输出限幅,C610 原始码,取满量程 50%。
 * 参考工程用 80%(8000)是"40% 载弹量下顶不动"的经验;本车阻尼小,50% 起步即可,
 * 且堵转时硬顶力矩只有一半,对弹丸更友好。 */

/* 单发/三发的角度环:参考工程此处 Kp=Ki=0(等于禁用,单发请求会一直不动直到超时)。
 * 本车要单发可用必须有非零增益;低阻尼对象过冲风险高,取小 P + 小 I + 限速,靠容差收尾。 */

#define LOADER_ANGLE_PID_KP 2.5f
/* [理论 · 待实测] 拨盘角度环 Kp,1/s(误差 deg → 速度目标 deg/s 转子)。
 * 小 → 到槽慢、容易顶到 LOADER_SINGLE_TIMEOUT_MS;大 → 低阻尼下冲过槽位,出现连拨两发。
 * 起步 2.5:发一发(转子约 1851 deg)在 0.4~0.6s 量级走完,留出超时余量。 */
#define LOADER_ANGLE_PID_KI 0.1f
/* [理论 · 待实测] 拨盘角度环 Ki。只为吃掉最后一点静态误差;大了会在容差边界反复蹭。 */
#define LOADER_ANGLE_PID_MAXOUT 6000
/* [理论 · 待实测] 拨盘角度环输出的速度目标上限,deg/s(转子)。6000 deg/s ≈ 输出轴 167 rpm,
 * 单发不会被限速卡住;同时低于连发 6 发/s 对应的 11109 deg/s,避免单发比连发还猛。 */

/* ---------------- 底盘几何 (mm) ---------------- */

#define WHEEL_BASE 600
/* [用户测量] 纵向轮轴中心距,mm。原车测量方法沿用:前后轮沿车长方向最外沿跨度近似轴距。 */

#define TRACK_WIDTH 560
/* [按原车测量方法换算] 横向轮轴中心距,mm。新底盘左右最外侧跨度600mm,
 * 扣除左右麦轮各半个轮宽合计40mm,得轮轴中心距560mm。 */

#define CENTER_GIMBAL_OFFSET_X 0
/* [实测-部分] 云台轴相对底盘几何中心的前后偏心,mm。
 * 本义:描述云台旋转中心偏移;本车实车未量,当前兼作"小陀螺平移漂移"的补偿旋钮。
 * 试过 -20、-40 均无改善(DEV-20261103-02/-03),回退 0;0 时表现为纯自转缓慢向右漂。
 * 若漂移加重:优先用 CHASSIS_TRANSLATE_BIAS_ANGLE 补偿,本参数保持 0。 */

#define CENTER_GIMBAL_OFFSET_Y 0
/* [不适用] 云台轴左右偏心,mm。本车云台居中,保持 0。 */

#define RADIUS_WHEEL 75
/* [待新机械实测] 麦轮有效滚动半径,mm;75 是参考工程旧轮值,新轮需重新测量。
 * 轮半径直接影响平移速度与自转速度换算。优先提供轮轴到辊子接地点的距离,
 * 也可提供轮子承重落地时转10圈的车体行程,我可据此计算。 */

#define REDUCTION_RATIO_WHEEL 19.0f
/* [规格] M3508 内置行星减速比 19:1,官方规格。编码器测的是转子,输出轴 = 转子/19。 */

#define CHASSIS_MAX_MOTOR_RPM 6000.0f
/* [理论] 轮电机转子转速限幅,RPM。3508 空载约 8300 RPM,取 72% 作为安全上限。
 * 大 → 允许更高轮速,但接近空载转速时扭矩骤降、电流环易饱和;
 * 小 → 高速平移被整体削速(四轮等比例缩,不改变方向)。
 * 6000 为保守取值,未测"多少 RPM 开始明显丢力矩"。 */

/* ---------------- 底盘速度 (平移 mm/s,自转 deg/s) ---------------- */

#define CHASSIS_MAX_SPEED 1500.0f
/* [待实测] 底盘平移最大速度,mm/s = 1.5 m/s。
 * 大 → 转场更快,但打滑、过冲、被弹判定面积变化;
 * 小 → 机动性损失。当前 1500 为经验值,未测本车实际可稳定达到的速度。 */

#define CHASSIS_ROTATE_SPEED 150.0f // [诊断对照] 临时半速150deg/s，原基线300deg/s
/* [实测] 小陀螺自转速度,deg/s(物理域)。
 * 大 → 实测上探到 390 时 yaw 电压已到上限,再大也上不去;300~390 之间轮子打滑风险上升;
 * 小 → 未测,无低于 300 的实车数据。
 * 依据:2026-10-30 实车逐档上探,390 触顶,取 300 留 23% 余量。
 * 换算:300 deg/s → 四轮转子约 3359 RPM → 纯自转裁判系统功率约 30 W(正常,非异常低)。 */

#define CHASSIS_TRANSLATE_BIAS_ANGLE (10.0f)
/* [实测-部分] 小陀螺平移方向偏置角,deg。自转时存在固定方向漂移(实测正转向右微漂),
 * 把平移指令预先旋转该角度使轨迹回正。负 = 向右偏,正 = 向左偏;仅 ROTATE 模式生效。
 * 大 → 修正过头,按 W 会向左偏;
 * 小 → 仍向右偏。10 为暂定值,**用户未回报最终效果**,需按下面方法复调。
 * 调法:纯自转看漂移方向 → 按 W 看轨迹 → 逐步加大至轨迹与云台指向重合(DEV-20261103-03)。 */

/* ---------------- 运动平顺 ---------------- */

#define CHASSIS_RC_DEADBAND 30
/* [实测] 摇杆进入运动的阈值,原始刻度 [-660, 660]。
 * 大 → 小杆量被吃掉,轻推不动;
 * 小 → 静止噪声(实测 ±5)让指令在 0 与 1500 之间打摆,即"车震"的主因。
 * 依据:实车观察到小死区导致指令振荡,30 为压在噪声之上、又不影响微操的取值。 */

#define CHASSIS_RC_RELEASE 20
/* [实测] 摇杆退出运动的阈值,原始刻度。与 CHASSIS_RC_DEADBAND 构成迟滞:
 * 进入需 >30、退出需 <20,避免在阈值附近反复开关。
 * 大 → 迟滞带变宽,回中后仍持续运动一小段;
 * 小 → 带太窄,退化为单阈值,边界抖动重现。 */

#define CHASSIS_VX_RAMP 5000.0f
/* [理论] 前进/后退方向的指令加速度上限,mm/s²。
 * 大 → 起步更猛,电流阶跃大,纯 P 速度环下有明显冲击;
 * 小 → 加速绵软,急停/急起响应变慢。
 * 5000 为经验值,未测"多大会产生可见冲击"。注意:旋转指令刻意不加斜坡(会引发振荡)。 */

#define CHASSIS_VY_RAMP 5000.0f
/* [理论] 左右平移的指令加速度上限,mm/s²。与 VX 同值,理由同上。 */

#define CHASSIS_CALIB_LOG 0
/* [配置] 底盘标定日志开关:每 0.5s 一行 `[chcal] vx/vy/wz/gz/be`。
 * **默认置 0**:RTT 上行 buffer 仅 1024 B,四个 *cal 日志同时开会 1.6 s 内溢出、互相淹没。
 * 每次标定只开当前需要的那一个,其余保持 0。
 * 用途 P1 速度估计验证(vx/vy/wz 为轮速逆解;gz 是 IMU 世界系角速度,只在"不跟随+不自转"时等于底盘转速;
 * be 是裁判缓冲能量,占用 0x0202 唯一有效字段)。P2 接入功率后追加 p/scale 两列,标定结束置 0。 */

/* ---------------- 云台跟随 (底盘追云台) ---------------- */

#define CHASSIS_FOLLOW_KP (1.5f)
/* [实测] 跟随律增益,输出 deg/s。律为 wz = KP × eff × |eff|(平方律,eff 为死区外 offset)。
 * 大 → 追得更凶,收敛快但易过冲、出现"画圈"式摆动;
 * 小 → 追得肉,云台转过去后底盘慢半拍,大角度滞后明显。
 * 依据:符号 +1.5 由本车旋转矩阵实测裁决(方向反了会越追越偏)。 */

#define CHASSIS_FOLLOW_DEADBAND 2.0f
/* [实测] 跟随软死区,deg。offset 取自 yaw 电机单圈 ecd,实测残差 ±1.4° 永不归零,
 * 故低于死区时位置项清零,超出后先减死区再进律,保证边界处输出连续无跳变。
 * 大 → 云台与底盘允许更大夹角,小角度不跟随;
 * 小 → 残差区间内持续输出,底盘在静止时轻微抖动。
 * 依据:2.0 = 实测残差上限 1.4 + 0.6 余量。 */

#define CHASSIS_FOLLOW_LIN_RANGE 5.0f
/* [理论] 线性律/平方律的分界,deg。|eff| 小于此值用线性,否则平方律。
 * 目的:平方律在零点附近增益趋零会拖慢收敛,故小区间改线性;5° 处两式增益相等,边界连续。
 * 调大 → 线性区更宽,小角度响应更积极,但大角度反而变钝;调小则相反。 */

#define CHASSIS_FOLLOW_DAMP 0.9f
/* [待实测] 速度阻尼系数,作用于 offset 变化率(低通后),起刹车作用,抑制收敛末端摆动。
 * 大 → 末端更稳,但起步阶段被阻尼拖慢,像"有人拽着";
 * 小 → 摆动收敛慢,可能出现小幅来回。
 * 0.9 为经验值,无实车调参记录。 */

#define CHASSIS_FOLLOW_SPD_MIN 3.0f
/* [理论] 速度项噪声门限,deg/s。低于此值的阻尼量直接忽略,用于滤掉 ecd 差分的量化噪声。
 * 大 → 小速度阻尼全被吃掉,末端摆动抑制失效;
 * 小 → 静止时噪声被放大成持续输出,底盘原地微抖。
 * 3.0 由 ecd 量化台阶与 200Hz 差分估算,未实测噪声谱。 */

#define CHASSIS_FOLLOW_FF 0.9f
/* [实测-部分] 云台角速度前馈系数。直接用 INS.Gyro[2] 驱动底盘,云台一动底盘即刻同速转,
 * 不等位置误差积累。
 * 1.0 → 前馈吃满,但会因 gyro 零偏累积成稳态偏移(已试过,故回退到 0.9);
 * 0.9 → 留 10% 给位置项收敛。
 * 符号反了会越转越偏,改负值即可。 */

/* ---------------- 云台解锁回中 ---------------- */

#if CHASSIS_TUNING_ONLY && SENTRY_BIG_YAW_CONTROL_ENABLE
#define GIMBAL_RECENTER_ENABLE 0 // 大yaw由编码器保持正前方，不使用旧IMU世界角回中
#else
#define GIMBAL_RECENTER_ENABLE 1
#endif
/* [配置] 解锁回中功能开关。关掉后 F 键回正与解锁自动回中均失效,云台保持当前角度。 */

#define GIMBAL_RECENTER_SPEED 90.0f
/* [理论] 回中角速度上限,deg/s。180° 约 2s,压住"甩"的观感。
 * 大 → 回中快但会甩,解锁瞬间易过冲;
 * 小 → 回中拖沓,中途被打断的概率上升。
 * 90 由"180° 两秒"反推,未做体感标定。 */

#define GIMBAL_RECENTER_DEADBAND 2.0f
/* [理论] 回中完成判据,deg。|offset| 小于此值即视为已回中。
 * 与跟随死区同值沿用;调大可提前结束回中,但会留下肉眼可见的残余角。 */

#define GIMBAL_RECENTER_DIR (+1.0f)
/* [不适用] 回中方向符号。若出现"越回越偏",改为 -1,不需改逻辑。 */

#define GIMBAL_RECENTER_TIMEOUT 600
/* [理论] 回中超时,单位控制周期。200Hz × 600 = 3s。
 * 大 → 卡住时占位更久;小 → 正常回中被误判超时后放弃。 */

/* ---------------- yaw 角度环 Kd 按 |err| 调度 ---------------- */

/* 远离目标用低 Kd 保跟手,接近目标用全 Kd 防松杆回弹,区间内线性过渡。
 * 依据:阻尼项 = -Kd × 角速度,大 Kd 在起步段会明显拖慢云台(DEV-79)。 */

#define YAW_KD_NEAR (5.8f)
/* [实测] 近目标区阻尼增益。|err| ≤ BLEND_LO(即 YAW_KD_BLEND_LO)时取此值(全阻尼)。
 * 大 → 收敛末端更稳、不来回摆;
 * 小 → 松杆后回弹明显。5.8 为 2026-10-30 整定定稿值。 */

#define YAW_KD_FAR (1.5f)
/* [实测] 远目标区阻尼增益。|err| ≥ BLEND_HI(即 YAW_KD_BLEND_HI)时取此值(低阻尼)。
 * 大 → 起步被拖慢,大角度转身发肉;
 * 小 → 跟手但末端易冲。**加大此值实测更糟,勿动。** */

#define YAW_KD_BLEND_LO (4.0f)
/* [理论] 调度下界,deg。|err| 低于此值进入全阻尼区。 */
#define YAW_KD_BLEND_HI (8.0f)
/* [理论] 调度上界,deg。|err| 高于此值进入低阻尼区,两者之间线性过渡,保证增益连续。 */

/* ---------------- yaw 速度环 Kp 按相对转速调度 ---------------- */

/* 小陀螺推杆时相对转速可达 -460 deg/s,反电动势侵蚀可用扭矩,故高速区线性提高 Kp 补扭矩(DEV-116)。
 * **电流模式下此问题由电调解决**:电流环直接吃力矩,电压不足时电调自动顶高电压抵消反电动势,
 * 原"高速提 Kp"失去补偿对象,退化为常数 20(LOW==HIGH 时 300-450 过渡区自动失效)。 */

#define YAW_SPEED_KP_LOW 20.0f
/* [实测] 低速区速度环 Kp。全部历史稳定数据均在此时段取得。 */

#define YAW_SPEED_KP_HIGH (YAW_ESC_CURRENT_MODE ? 20.0f : 40.0f)
/* [理论→待实测] 高速区速度环满增益,随命令帧类型派生。
 * 电压模式取 40:[实测-部分] 补反电动势扭矩,低了小陀螺推杆明显"软脚"。
 * 电流模式取 20:[理论] 无需补反电动势,40 在高速区是纯过增益(噪声/过冲/啸叫);
 * 待实车回归——高速推杆发飘/啸叫即验证此值正确;若反而"软脚"说明摩擦负载比预期大,再上调。 */

#define YAW_SPEED_KP_BLEND_LO 300.0f
/* [实测-部分] 调度起点,相对转速 deg/s,取稳态自转保持点附近。 */
#define YAW_SPEED_KP_BLEND_HI 450.0f
/* [实测-部分] 调度终点,相对转速 deg/s,此处达到满增益。区间内线性过渡。 */

/* ---------------- yaw 控制域与前馈 ---------------- */

#define YAW_ESC_CURRENT_MODE 1
/* [待实测] yaw GM6020 的 CAN 命令帧类型开关。
 * 1 = 电流帧 0x1FE/0x2FE(±16384 ↔ ±3A);0 = 电压帧 0x1FF/0x2FF(±25000 ↔ ±24V)。
 * **前提:电调固件须支持电流帧。** 本车 2026-10 曾下发电流帧实测零扭矩,
 * 结论记在 .Doc/本车调参结论.md §1(需固件升级 + RoboMaster Assistant 侧打开)。
 * 失败现象:置 1 而固件不支持 → 使能后 yaw 完全不出力,但反馈(角度/转速/温度)一切正常,
 * 极易被误判成"PI 参数调坏了",先看这一位。
 * 回退:置 0 重新编译;下方 OUT_MAX 与 DOMAIN_SCALE 由本开关自动派生,不用手改两处。 */

#define YAW_ESC_OUT_MAX (YAW_ESC_CURRENT_MODE ? 16384.0f : 25000.0f)
/* [规格] yaw 速度环输出限幅 = 对应命令帧的协议满量程(电流 16384 / 电压 25000),单位=电调原始码。
 * 超量程由电调侧静默截断,不报错,表现为"推到底也加不动"。 */

#define YAW_CURRENT_DOMAIN_SCALE (YAW_ESC_CURRENT_MODE ? 0.6554f : 1.0f)
/* [换算] 输出域换算系数 = 16384/25000。电压模式下 PID 输出直接是电压码,
 * 电流模式下同一 PID 输出要当作电流码,故凡是以"输出单位"计的增益
 * (速度环 Kp/Ki、惯量前馈 GAIN、摩擦前馈 B)统一乘此系数,切换前后整环等效增益不变。
 * 角度环在输入侧(deg 误差 → deg/s 目标),不受影响。
 * 漏乘的后果:Ki 漏乘时积分作用相对增强 1.5 倍,表现为残余过冲/小幅极限环。 */

#define YAW_REF_FF_GAIN 1.0f
/* [理论] yaw 目标角速度前馈增益。前馈量取"本周期 ref 增量 / dt",
 * 使小陀螺推杆时不必先积累位置误差就能出力(DEV-93)。1.0 为满前馈。 */

#define YAW_REF_FF_MAX_RATE 450.0f
/* [实测] 前馈认可的最大目标角速度,deg/s(左横杆与键鼠甩枪共用)。
 * **陷阱:MAX_RATE 必须 ≥ KBM_YAW_MAX_RATE。** 前馈按"单周期增量 > 阈值"判目标跳变并清零,
 * 若本值小于键鼠甩枪上限,大甩时前馈会被静默清零,手感变肉且无任何报错
 * (DEV-20261106-01 已由 180 提到 450)。 */

#define YAW_REF_FF_MAX_DELTA (YAW_REF_FF_MAX_RATE * 0.005f * 1.1f)
/* [实测 派生] 单周期合法增量上限,deg。0.005s 为 200Hz 控制周期,1.1 为余量。
 * 超限即判定为目标重锚定级跳变,本周期前馈清零。基准周期的选取曾出错(DEVLOG 记录)。 */

/* yaw 惯量前馈 τ = Jα:α 取 Gyro[2] 差分后低通,加速时助力、减速时刹车,稳态归零(DEV-101)。 */

#define YAW_INERTIA_FF_GAIN 1.875f
/* [理论] 惯量前馈增益,电压/(deg/s²)。取理论值 K=3.0 的 62.5% 作为保守起步,
 * 避免前馈过强引入振荡。 */
#define YAW_INERTIA_FF_LP 0.2f
/* [理论] α 低通系数。200Hz 下截止约 3.5Hz,滤掉差分噪声,避免前馈抖动。 */
#define YAW_INERTIA_FF_ALPHA_MAX 1000.0f
/* [理论] α 限幅,deg/s²。防止陀螺仪尖峰经差分放大后打出异常前馈。 */

#define YAW_INERTIA_FF_SRC 0
/* [理论] 惯量前馈的 α 来源。0 = IMU 世界系实测差分(现行,已实车验证);1 = 参考轨迹 α_ref。
 * **0 与 1 的主语都是世界系 α**(§3.1 定案):底盘加减速时关节系建模需补 +J·α_chassis,
 * 本车不采用关节系记法,故不得用关节 α。
 * 置 1 的动机:实测 α 有差分噪声与低通滞后,模型逆前馈理论上该用参考量;代价是参考来自
 * DBUS 14ms 阶梯,需经"零阶保持 + 一阶低通"再造连续量,**会引入额外相位滞后**,[待实测] 评估。 */

#define YAW_VREF_LP 0.3f
/* [理论] 仅 `YAW_INERTIA_FF_SRC=1` 时使用:参考速度副本的一阶低通系数。
 * 200Hz 下等效时间常数约 14ms,刚好抹平 DBUS 帧阶梯;调小更平滑但滞后更大。 */

#define YAW_FF_CALIB_LOG 0
/* [配置] P2 标定专用日志开关(0.5s 一行)。关掉后无任何输出,也不影响控制。
 * **默认置 0**:RTT buffer 1024 B,四个 *cal 同开会互相淹没;做 yaw 标定时再单独置 1。
 * 打开原因:摩擦/惯量标定需要 out(码)、关节转速、实测电流、前馈分量四列同时可读,
 * 而 telemetry 通道只有五个字段。标定结束后与 `telemetry_yaw.extra` 一起恢复。 */

/* yaw 摩擦/反电动势前馈:ff = B × 相对转速,补偿"世界系反馈看不见相对转速"造成的稳态残差(DEV-125)。 */

/* ---------------- pitch 目标角速度前馈 ---------------- */

/* 治纯 P 恒速跟踪滞后:满杆 60 deg/s 时滞后 7.5°(= 速度 / Kp),松杆后还会再滑一下。
 * 两条教训:①跳变阈值必须按 DBUS 帧周期 14ms 算,按控制周期算会把正常推杆判成跳变、
 * 前馈恒为 0;②前馈需一阶低通抹掉 70Hz 帧率带来的阶跃纹波。 */

#define PITCH_REF_FF_GAIN 1.0f
/* [实测-部分] pitch 目标角速度前馈增益。环路稳定后重新引入(DEV-20261030-11),
 * 满前馈 1.0;实车回归项见 DEVLOG,若仍有细微纹波则改低通而非增益。 */

#define PITCH_REF_FF_MAX_RATE 60.0f
/* [规格] 前馈认可的最大目标角速度,deg/s。与 cmd 层杆量映射一致(满杆 60 deg/s)。
 * 若改杆量映射,此处必须同步,否则前馈在大杆量时被判跳变清零。 */

#define PITCH_REF_FF_MAX_DELTA (PITCH_REF_FF_MAX_RATE * 0.014f * 3.0f)
/* [实测 派生] 单帧合法增量上限,deg。0.014s 为 DBUS 帧周期,×3.0 容纳帧间抖动,
 * 用于拦截"重锚定"级跳变。**陷阱:若误按 0.005s 控制周期算,正常推杆会被判跳变,
 * 前馈静默归零(DEV-04 踩过)。** */

#define PITCH_REF_FF_LP 0.3f
/* [理论] 前馈一阶低通系数。200Hz 下截止约 11Hz,抹掉 70Hz 帧率阶跃纹波。
 * 大 → 纹波滤得更干净但引入相位滞后;小 → 前馈更紧跟但纹波进电机。 */

/* ---------------- pitch 重力前馈(新结构必做,见 .Doc/pitch轴优化计划.md) ---------------- */

/* 模型:I_ff(θ) = GAIN × (A·cos(θ − φ) + B),θ 取 **IMU 欧拉 Pitch**(重力是世界系量;
 * 编码器角在 IMU 侧装/翻装/零位偏移时与"重力方向"不等价,不能用)。
 * A/φ/B 由 P2 逐角度静置读稳态电流、最小二乘拟合并标定,禁止盲给(DEC-004 已否决过一次)。 */

#define PITCH_GRAVITY_FF_GAIN 0.0f
/* [待实测] 重力前馈总增益。0 = 关闭(默认,行为与整定前完全一致);
 * 标定后从 0.5 起逐档 0.6/0.8/1.0,首档必须先做符号检查(稳态电流必须下降)。 */

#define PITCH_GRAVITY_A 0.0f
/* [待实测] 最大重力矩等效电流,单位 C620 原始码,**带符号**。P2 拟合得到。
 * 量级参考:C620 ±16384 码 ↔ ±20A,Kt≈0.3N·m/A;若新结构 m≈1.2kg、质心距 0.10m,
 * 则 τ_gmax≈1.18N·m ≈ 3220 码——接近 MaxOut=4000,说明新结构很可能顶到限幅,须复核。 */

#define PITCH_GRAVITY_PHASE 0.0f
/* [待实测] 质心相位 φ,deg。理论 0(质心在俯仰轴正上方或正下方);
 * 明显偏离说明有配重块或线缆拖拽把等效质心拉偏。 */

#define PITCH_GRAVITY_OFFSET 0.0f
/* [待实测] 常值偏置 B,C620 码。不含重力,只吸收线缆扭力/零点静摩擦那类与角度无关的恒值项。 */

#define PITCH_GRAVITY_FF_LIMIT (1.2f * ((PITCH_GRAVITY_A < 0.0f ? -PITCH_GRAVITY_A : PITCH_GRAVITY_A) + \
                                        (PITCH_GRAVITY_OFFSET < 0.0f ? -PITCH_GRAVITY_OFFSET : PITCH_GRAVITY_OFFSET)))
/* [理论] 前馈自身限幅,码,由 A/B 自动派生(|A|+|B| 再留 20% 余量),A=B=0 时恰为 0。
 * **为什么必须限幅**:`CURRENT_FEEDFORWARD` 在 `DJIMotorControl` 里是在速度环输出**之后**叠加的,
 * 前面没有任何钳位 → `MaxOut=4000` 管不到前馈,总电流可达 4000+FF。
 * 拟合偏差或相位写错时,这层限幅是唯一的物理上限。 */

#define PITCH_FF_CALIB_LOG 0
/* [配置] P1/P2 标定日志开关,每 0.5s 一行 `[pitchcal] ang/out/spd/cur`。
 * **默认置 0**:RTT buffer 1024 B,四个 *cal 同开会互相淹没;做 pitch 标定时再单独置 1。
 * 打开原因:P2 要"角度 + 稳态电流"成对读;B7 敲击时 out/cur 全为 0,振动信号另经 telemetry 的
 * extra(Gyro[0])或 Ozone 直接看 IMU。标定结束后与 extra 语义一并恢复。 */

#define YAW_FRICTION_FF_B 15.0f
/* [实测-部分] yaw 摩擦/反电动势前馈系数。ff = B × 相对转速,补偿"世界系反馈看不见
 * 相对转速"造成的稳态残差。
 * 15 → 约 60% 欠补偿,保守稳定;
 * 18 → 实测失稳,已回退。故 15 是"已知不稳值之下的安全档",不是最优值。 */

/* ---------------- 陀螺仪安装方向 (1 同向, -1 反向) ---------------- */

#define GYRO2GIMBAL_DIR_YAW 1
#define GYRO2GIMBAL_DIR_PITCH 1
#define GYRO2GIMBAL_DIR_ROLL 1
/* [不适用] 由 IMU 实际安装姿态决定,不是可调参数。改轴向/翻装后按现象逐个取反:
 * 云台自稳时单向发散、或 pitch 抬头却低头,即对应轴取反。
 * 注意:attitude_t 的 Gyro 轴是芯片原始轴,做欧拉角速度反馈前必须先确认映射。 */

/* ---------------- 键鼠控制 ---------------- */

/* 链路:电脑 MicroUSB 接 NDJ6/DT7 遥控器,键鼠字段随 DBUS 帧下发;无键鼠数据时恒 0,永不激活。
 * 键盘任意键 / 鼠标移动或点击即切入键鼠模式,超过 KBM_TIMEOUT_MS 无输入自动回落遥控。
 *
 * 映射模型(FPS 式线性 1:1,DEV-20261106-01,已取代位置域 + 加速曲线方案 DEV-20261102-03/04):
 * 帧计数 = 本帧(14ms)内的鼠标位移 → 换算为目标角速度 → 按控制周期积分,
 * 使"每计数恰好施加一次",灵敏度与 70Hz 帧率 / 200Hz 控制率解耦。
 * 固定增益、无加速度曲线(CSGO 式),保肌肉记忆。
 *
 * 灵敏度语义:KBM_*_SENS = deg / 帧计数,再乘运行时倍率 kbm_sens_scale
 * (V 降 / B 升 ×0.85,G 重置,范围 0.2~2.5,RTT 回显)。 */

#define KBM_YAW_SENS 0.7f
/* [实测-部分] 鼠标 x 计数 → yaw 转角,deg/帧计数。
 * 大 → 同手速转身更快,但微调精度下降;
 * 小 → 瞄准稳,大角度转身需要更大幅度。
 * 基线换算:0.7 = 旧方案有效增益(0.25 × 驻留 2.86),**实车精调尚未进行**,从此刻起步用 V/B 微调。 */

#define KBM_PITCH_SENS 0.43f
/* [实测-部分] 鼠标 y 计数 → pitch 转角,deg/帧计数。基线换算同上(0.15 × 2.86)。
 * 符号若反了(上滑低头)改负值,不要改 KBM_PITCH_DIR 以外的逻辑。 */

#define KBM_DBUS_FRAME_S 0.014f
/* [规格] DBUS 帧周期,s(约 70Hz)。用于把"每帧计数"归一化成角速度,是灵敏度不随帧率漂移的关键。
 * **陷阱:若实际遥控器帧周期不同,此值必须同步,否则灵敏度整体偏差。** */

#define KBM_CMD_DT_S 0.005f
/* [规格] cmd 控制周期,s(200Hz)。角速度 → 目标角的积分步长,与任务调度频率绑定。 */

#define KBM_SLOW_RATIO 0.4f
/* [待实测] Shift 慢速比例。按住 Shift 时平移速度乘此系数,用于精准走位。
 * 大 → 慢速档不够慢,微调仍糙;小 → 慢到没意义。
 * 0.4 为经验值,无调参记录。 */

#define KBM_TIMEOUT_MS 3000.0f
/* [待实测] 键鼠无输入回落遥控的超时,ms。
 * 大 → 放下鼠标后长时间仍处键鼠模式,遥控杆被忽略;
 * 小 → 思考间隙就掉回遥控,再次操作要重新激活。
 * 3000 为经验值,无依据。 */

#define KBM_MOUSE_DEADZONE 2
/* [待实测] 鼠标静止计数死区,原始计数。用于滤掉静止时的抖动。
 * 大 → 小幅度微调被吃掉;
 * 小 → 手抖导致视角持续微漂。
 * 2 为估值,未测静止噪声幅值。 */

#define KBM_INPUT_LP 0.6f
/* [实测-部分] 输入一阶低通系数。0.6 约 1.5 周期收敛,群延迟约 3ms(200Hz)。
 * 大 → 跟随更紧,帧间抖动进视角;
 * 小 → 更柔但发钝,**回到 0.35(9ms 延迟)会明显发钝**(DEV-20261106-01)。
 * 原则:低通只滤帧间抖动,不承担手感塑形。 */

#define KBM_YAW_MAX_RATE 450.0f
/* [实测-部分] yaw 甩枪角速度上限,deg/s。取值贴合速度环 Kp 调度的高速区。
 * 大 → 允许更快的甩枪,但云台可能跟不上、出现拖影感;
 * 小 → 大甩被削顶,转身"被拉住"。
 * **陷阱:改动必须同步 YAW_REF_FF_MAX_RATE,否则前馈被判跳变并静默清零。** */

#define KBM_PITCH_MAX_RATE 150.0f
/* [理论] pitch 甩动角速度上限,deg/s。刻意低于 yaw,防甩过头翻枪。
 * 校验:150 × 0.005s = 0.75° < 前馈跳变阈值 2.52°,故不触发前馈清零。
 * 超过该阈值前必须重算上面的关系。 */

/* 灵敏度运行时调档:V 降档、B 升档(乘 STEP)、G 重置为 1.0。
 * 全局倍率作用于 yaw/pitch 两轴,对 KBM_*_SENS 基准缩放,RTT 打印当前倍率。
 * 目的:调手感不必反复重编译。 */

#define KBM_SENS_SCALE_MIN 0.2f
/* [待实测] 倍率下限。从 1.0 起按 0.85 步进约 10 次触底。 */
#define KBM_SENS_SCALE_STEP 0.85f
/* [待实测] 每档步进系数。大 → 调档手感跳跃;小 → 要按很多次才到位。 */
#define KBM_SENS_SCALE_MAX 2.5f
/* [待实测] 倍率上限。从 1.0 起约 6 次触顶。 */

/* 键鼠方向:本车 chassis 的 vx/vy 系数是整体取反标定的,故方向只能靠这两个符号裁决。 */

#define KBM_STRAFE_DIR (+1.0f)
/* [实测] 平移方向符号。A 左 / D 右;若左右反了改 -1,不改逻辑(DEV-20261102-05 实车裁决)。 */

#define KBM_PITCH_DIR (-1.0f)
/* [实测] 鼠标 pitch 符号。取反使"上滑 = 枪口上抬"(mouse.y 上滑为负);
 * 若上下反了改 +1(DEV-20261102-05)。 */

/* ---------------- 保护 ---------------- */

#define CMD_WATCHDOG_TIMEOUT_MS 200.0f
/* [理论] 命令断流看门狗超时,ms。消费端超时未收到新命令即切安全态。
 * 200ms = 200Hz 下 40 个周期,足以跨过一次调度抖动,又不会让失控持续太久。
 * 大 → 上游卡死后动作保持更久;小 → 调度抖动可能误触发安全态。 */
