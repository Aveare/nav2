/**
 * @file    config_chassis.h
 * @brief   底盘参数:几何/速度/跟随律
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/spec/本车调参结论.md。
 */
#pragma once

/* ---------------- 底盘几何 (mm) ---------------- */

#define CHASSIS_WHEEL_BASE 500
/* [实测 2026-11-11] 纵向轴距(前进后退方向),mm。取前后轮轴中心的距离。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_TRACK_WIDTH 460
/* [实测 2026-11-11] 横向轮距(左右方向),mm。左右轮外侧面距 500 − 轮宽 40。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_GIMBAL_OFFSET_X 0
/* [实测-部分] 云台轴相对底盘几何中心的前后偏心,mm。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_GIMBAL_OFFSET_Y 0
/* [不适用] 云台轴左右偏心,mm。本车云台居中,保持 0。 */
#define CHASSIS_WHEEL_RADIUS 75
/* [实测 2026-11-09] 轮子半径,mm。实测外径约 150,取半值。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_WHEEL_REDUCTION 19.0f
/* [规格] M3508 内置行星减速比 19:1,官方规格。编码器测的是转子,输出轴 = 转子/19。 */
#define CHASSIS_MAX_MOTOR_RPM 6000.0f
/* [实测] 轮电机转子转速限幅,RPM。**2026-11-17 按用户要求回退到 11-05 的值 6000**(原 7000)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 底盘速度 (平移 mm/s,自转 deg/s) ---------------- */
#define CHASSIS_MAX_SPEED 1500.0f
/* [待实测] 底盘平移最大速度,mm/s = 1.5 m/s。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_ROTATE_SPEED 300.0f
/* [实测] 小陀螺自转速度,deg/s(物理域)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_WZ_LIMIT 400.0f
/* [实测 2026-11-20] 跟随律 wz 的**总限幅**,deg/s;0 = 不限幅。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_TRANSLATE_BIAS_ANGLE (10.0f)/* [实测-部分] 小陀螺平移方向偏置角,deg。自转时存在固定方向漂移(实测正转向右微漂),
 * 把平移指令预先旋转该角度使轨迹回正。负 = 向右偏,正 = 向左偏;仅 ROTATE 模式生效。
 * 大 → 修正过头,按 W 会向左偏;
 * 小 → 仍向右偏。10 为暂定值,**用户未回报最终效果**,需按下面方法复调。
 * 调法:纯自转看漂移方向 → 按 W 看轨迹 → 逐步加大至轨迹与云台指向重合(DEV-20261103-03)。 */

/* ---------------- 运动平顺 ---------------- */

#define CHASSIS_RC_DEADBAND 30
/* [实测] 摇杆进入运动的阈值,原始刻度 [-660, 660]。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_RC_RELEASE 20
/* [实测] 摇杆退出运动的阈值,原始刻度。与 CHASSIS_RC_DEADBAND 构成迟滞:
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VX_RAMP 5000.0f
/* [理论] 前进/后退方向的指令加速度上限,mm/s²。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VY_RAMP 5000.0f
/* [理论] 左右平移的指令加速度上限,mm/s²。与 VX 同值,理由同上。 */
#define CHASSIS_VX_JERK 20000.0f
/* [理论] 前进/后退指令的加速度变化率上限,mm/s³(S 曲线)。
 * 线性斜坡在起步/刹车时加速度阶跃,牵引峰值高;jerk 限幅让加速度平滑爬升/卸掉。
 * 0=退回线性斜坡。 */
#define CHASSIS_VY_JERK 20000.0f
/* [理论] 左右平移的 jerk 上限,mm/s³。与 VX 同值。 */
#define CHASSIS_CALIB_LOG 1
/* [配置] 底盘标定日志开关:每 0.5s 一行 `[chcal] vx/vy/wz/gz/p/be`。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 云台跟随 (底盘追云台) ---------------- */
#define CHASSIS_FOLLOW_KP 8.0f
/* [换算+待实测] 跟随位置环增益,(deg/s) 每 deg。**2026-11-20 重写跟随律:平方律 → 线性**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_DEADBAND 2.0f
/* [实测] 跟随软死区,deg。offset 取自 yaw 电机单圈 ecd,残差永不归零,
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_TARGET_OFFSET 0.0f
/* [理论] 跟随目标相对角,deg(云台相对底盘,逆时针为正)。**2026-11-20 新增**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_FF_SLEW 0.0f
/* [实测 2026-11-21] 前馈爬升限速,deg/s²。**只管起步那一段**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_FF 0.75f
/* [理论+实测] 云台绕世界 yaw 角速度的前馈系数,无量纲。**0.9 → 1.0(2026-11-20 重写)**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_BRAKE_ON_RATE 5.0f
/* [实测 2026-11-21] **提前刹车的触发门槛**,deg/s:云台目标角速度低于本值即视为"松手"。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_BRAKE_ANT 0.4f
/* [待实测 2026-11-21] **提前刹车**增益,(deg/s) 每 (deg/s),无量纲。默认 0 = 关。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_KD 0.0f
/* [理论] 相对角速度阻尼增益,(deg/s) 每 (deg/s),无量纲。**2026-11-21 归零,默认关闭**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_P_RESERVE 0.25f
/* [理论] 位置项强制保留的额度比例,无量纲(0~1)。**2026-11-20 新增**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF 0.085f
/* [理论+待实测] 加速度前馈系数,单位 s((deg/s) 每 (deg/s²))。**2026-11-21 新增,默认 0 = 关**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_LP 0.30f
/* [理论] AFF 用的角加速度低通系数(200Hz 一阶,截止约 10Hz)。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_LIMIT 350.0f

#define CHASSIS_FOLLOW_NO_OVERSHOOT 0
/* [实测 2026-11-21] 起步硬约束:一旦底盘已追平云台(|est| ≥ |gz|),任何项都不许把
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_GATE 20.0f
/* [实测 2026-11-21] AFF 的速度门控,deg/s。**只有在转的时候才让 AFF 出力**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_ACCEL_ON 0
/* [实测 2026-11-21] 加速段(α>0)是否也让 AFF 出力。**0 = 只做刹车**。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_RATE_MIN 300.0f
/* [理论] AFF 的角加速度死区,deg/s²。低于此值不输出。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* [理论] AFF 单项限幅,deg/s。防止换向/打杆瞬间的 α 尖峰把指令推到满额反向。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 四轮速度环 PID(M3508,反馈 deg/s) ---------------- */
#define CHASSIS_SPEED_PID_KP 0.75f
/* [实车] 四轮速度环 Kp。速度反馈为 deg/s(M3508 实测)。调大有稳态刚性但易抖,调小发软。 */
#define CHASSIS_SPEED_PID_KI 1.0f
/* [实车] 积分项:补偿纯 P 速度环的稳态滞后。 */
#define CHASSIS_SPEED_PID_IL 3000
/* [理论] 积分限幅(C620 原始码)。 */
#define CHASSIS_SPEED_PID_MAXOUT 10000
/* [理论] 输出限幅(C620 满量程 16384 的 61%)。 */
#define CHASSIS_SPEED_PID_RC 0.3f
/* [理论] 速度环输出低通 RC。 */
#define CHASSIS_MOTOR_ID_LF 4
/* [配置] 四轮 CAN1 电调 ID:左前/右前/左后/右后(见 README 拨码表)。 */
#define CHASSIS_MOTOR_ID_RF 1
#define CHASSIS_MOTOR_ID_LB 2
#define CHASSIS_MOTOR_ID_RB 3

#define CHASSIS_BOARD_CAN_TX 0x312
/* [协议] 双板(CHASSIS_BOARD)板间 CANComm 的 tx/rx id;单板不编译。 */
#define CHASSIS_BOARD_CAN_RX 0x311

/* ---------------- 底盘 VOFA 调试输出(占用 USART1 = 视觉口) ---------------- */

#define CHASSIS_VOFA_UART1_ENABLE 0
/* [配置] USART1 编译期所有者宏:1=改作底盘 VOFA+ JustFloat 调试；0=让给 AX650。
 * USART1(视觉口 PA9=TX / PB7=RX)调试输出 200Hz。
 * **物理独占视觉口**：开这个必须同时把 ax650.h 的 AX650_UART1_ENABLE 置 0（两个都开会编译期 #error），
 * 也就是拔掉 AX650 视觉板、在视觉口上接 USB-TTL。串口沿用 CubeMX 的 921600 8N1，**不是** AX650 的 8E1。
 * **USART6 不属于调试通道**：裁判系统热量/冷却/弹速/功率固定走 USART6。
 * 当前为 0：不注册、不发送，USART1 留给 AX650 921600 8E1 自瞄链路（2026-11-22 交接恢复）。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VOFA_SEND_DIVIDER 1u
/* [理论] 发送分频，1=200Hz（chassis_task 200Hz），2=100Hz。
 * 细节与失效/标法/风险 → .Doc/spec/参数证据表.md「B1b 迁移增补」同名节 */

/* ---------------- 小陀螺抗自瞄轨迹(DEV-20261124-06,2026-11-25 改正弦) ---------------- */
/* 设计依据、开源对照与验收判据 → .Doc/plan/小陀螺抗自瞄改造计划.md。
 * 2026-11-25(DEV-20261125-09):反转段实车超功率,BOOST/GLIDE/BRAKE 的加速节拍
 * 也抬高瞬时功率;状态机整体移除,轨迹 = 巡航转速 × (1 + 双尺度正弦),输出恒正。 */
#define CHASSIS_SPIN_VARY_EN 1
/* [配置] 1=变速轨迹;0=回退恒速 CHASSIS_ROTATE_SPEED,用于 A/B 隔离。 */
#define CHASSIS_SPIN_CRUISE_DPS 265.0f
/* [实测-部分] 正弦中心转速,取 (HIGH+LOW)/2=265,使正弦完整覆盖 180~350 不削顶;
 * 平均转速比 300 低 12%,功率更低(DEV-20261126-18)。 */
#define CHASSIS_SPIN_LOW_DPS 180.0f
/* [理论] 输出下限,deg/s。265×0.68≈180,正弦完整落在下限上,无平底。 */
#define CHASSIS_SPIN_HIGH_DPS 350.0f
/* [实测-部分] 输出上限,deg/s。265×1.32≈350,正弦峰值正好到顶、不再削平;
 * 390 地面实测约 100W,峰值 350 做开环降额(DEV-20261126-16/18)。 */
#define CHASSIS_SPIN_ALPHA_MAX 800.0f
/* [待实测] 角加速度限幅,deg/s^2。台架电流峰值<恒速2倍后可上调,上限1500。 */
#define CHASSIS_SPIN_JERK_MAX 6000.0f
/* [换算] 角加速度变化率上限,deg/s^3。α 从 0 爬到 800 约 0.13s,
 * 抑制起步与正弦换向的扭矩阶跃;仿真 0→300 约 490ms(DEV-20261124-11)。 */
#define CHASSIS_SPIN_NOMINAL_DT_MS 5.0f
/* [规格] 任务周期兜底,ms(200Hz)。dt<=0 或 >50ms 时使用。 */
#define CHASSIS_SPIN_MOD_F1_HZ 0.31f
#define CHASSIS_SPIN_MOD_F2_HZ 0.83f
/* [理论] 两路调制频率,Hz。不可通约,避免形成固定节拍。 */
#define CHASSIS_SPIN_MOD_A1 0.213f
#define CHASSIS_SPIN_MOD_A2 0.107f
/* [理论] 调制幅度。合计 0.32,265×1.32≈350、265×0.68≈180,全程不削顶/削底。 */
#define CHASSIS_SPIN_MOD_LIMIT 0.32f
/* [理论] 调制总限幅,防止极端相位叠加把转速压进低速跟踪区。 */
#define CHASSIS_SPIN_MOD_A_SCALE_MIN 0.7f
#define CHASSIS_SPIN_MOD_A_SCALE_MAX 1.0f
/* [理论] 每次进入对两路幅度整体随机缩放(0.7~1.0),峰值仍受 MOD_LIMIT/HIGH 约束;
 * 不同对局轨迹不同,避免固定幅度谱被预先拟合(DEV-20261126-17)。 */
#define CHASSIS_SPIN_MOD_F_SCALE_MIN 0.85f
#define CHASSIS_SPIN_MOD_F_SCALE_MAX 1.15f
/* [理论] 每次进入对两路频率整体随机缩放(±15%),不让 0.31/0.83Hz 成为固定谱线。 */
#define CHASSIS_SPIN_PHASE_WALK_STEP 0.30f
/* [理论] 旋内相位随机游走步长,rad/次。相位持续抖动 → 瞬时频率随机,
 * 破坏对方在 1~2s 窗口内对正弦频率的拟合;只改相位,不改峰值/平均转速。 */
#define CHASSIS_SPIN_PHASE_WALK_MS 500.0f
/* [理论] 相位游走周期,ms。0=关闭。 */

/* ---------------- 底盘功率控制 ---------------- */
/* 2026 通信协议 V1.2.0 删除了 0x0202 的 chassis_power(前 8 字节改保留位),
 * 裁判不再回传功率,改为电机模型预测:用四轮 C620 电流码与转子转速直接算功率。
 * 模型形式与天工大 304 开源(MIT)一致,系数取自其实测 M3508 值。
 * scale 同时乘到轮速目标与速度环 MaxOut:稳态不饱和时只压 MaxOut 限不住功率
 * (DEV-20261125-10),必须压轮速目标才能让小陀螺真正降功率。 */
#define CHASSIS_POWER_CTRL_EN 1
/* [配置] 0=关闭,scale 恒 1,回退旧行为。 */
#define CHASSIS_POWER_LIMIT_DEFAULT 90.0f
/* [规则] 裁判上限无效时的兜底功率,W(本队规则上限)。 */
#define CHASSIS_POWER_LIMIT_MAX 90.0f
/* [规则] 硬上限:裁判按机器人等级下发,实测本车给了 120W;本队规则是 90W,
 * 取二者较小值,防止等级配置偏高把限制放宽 30W。 */
#define CHASSIS_POWER_MARGIN_W 10.0f
/* [实测] 控制余量,W:实测超调约 10W,控制目标取 90-10=80W。
 * DEV-20261126-16:从 5W 加到 10W,配合小陀螺峰值 350 做开环降额。 */
#define CHASSIS_POWER_SCALE_LP 0.6f
/* [理论] 衰减系数一阶低通(每拍,200Hz)。0.6 约 3 拍到 95%,滤电流指令噪声;
 * 1=不滤波(直通),0=冻结(勿用)。DEV-20261126-12 从"恢复步长"改为对称低通。 */
#define CHASSIS_POWER_SCALE_MIN 0.05f
/* [安全] 衰减系数下限,防止功率受限时完全没力。0.05→电流指令压到 5%,
 * 四轮合计约 58W@24V 量级;再低转向/平移明显发顿。 */
#define CHASSIS_SPIN_POWER_LP 0.1f
/* [理论] 小陀螺功率衰减低通(每拍,200Hz):小陀螺模式下总功率超限时**只压自转**,
 * 平移指令保持(与跟随同速);0.1≈50ms,避免自转随功率抖动。0=冻结(勿用),1=直通。 */
#define CHASSIS_POWER_BUFFER_MIN_SCALE 0.5f
/* [参考 304] 缓冲能量为 0 时的功率上限系数:limit *= 0.5 + 0.5*buffer/MAX_J。
 * 缓冲快耗尽时主动降额,优先让缓冲回充;裁判离线时该衰减不生效。 */
#define CHASSIS_POWER_BUFFER_MAX_J 60.0f
/* [规格] 缓冲能量满值,J。2026 规则缓冲 60J;若规则变更需同步。 */
#define CHASSIS_POWER_MODEL_SCALE 1.0f
/* [标定] 模型整体比例修正:P_corr = SCALE*P_model + BIAS。
 * 默认 1.0=不改;本车实测拟合后填这里(见 tools/fit_power_model.py)。 */
#define CHASSIS_POWER_MODEL_BIAS 0.0f
/* [标定] 模型整车偏置,W。默认 0;拟合后填。 */

/* M3508 功率模型:P = K0 + K1*|i| + K2*|w| + K3*|i|*|w| + K4*i^2 + K5*w^2
 * i = 电流指令码 / CHASSIS_POWER_CURRENT_DIV,w = 转子转速 deg/s(模型内转 rpm)。
 * 系数来源:天工大 304 战队 2026 开源实测(real_current_conversion=1000,MIT);
 * 304 运行时取绝对值,本工程一致(DEV-20261126-12)。
 * 量级校验:静态四轮 K0 之和 = 2.6W,与裁判 OLED 显示 2.3W 吻合。
 * [待实测] 系数是 304 车的,本车需按 tools/fit_power_model.py 重拟合。 */
#define CHASSIS_POWER_CURRENT_DIV 1000.0f
/* [换算] 电流码→模型输入。304 用 1000;若改用物理安培须同步重拟全部系数。 */
#define CHASSIS_POWER_MODEL_K0 0.65213f
#define CHASSIS_POWER_MODEL_K1 (-0.15659f)
#define CHASSIS_POWER_MODEL_K2 0.00041660f
#define CHASSIS_POWER_MODEL_K3 0.00235415f
#define CHASSIS_POWER_MODEL_K4 0.20022f
#define CHASSIS_POWER_MODEL_K5 0.000000108f
