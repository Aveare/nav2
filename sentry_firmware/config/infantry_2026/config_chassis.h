/**
 * @file    config_chassis.h
 * @brief   底盘参数:几何/速度/跟随律
 * @layer   CONFIG
 * @author  小猫大王
* @note    2026-11-20 从 config/infantry_2026/config.h 机械拆分而来,**宏名与数值零改动**;
*          伞头统一 include,业务代码只包含伞头,不直接包含本文件。
* @note    每个参数只保留"当前值 + 一句话依据";判据与踩坑见 .Doc/robots/infantry/params/调参结论.md。
 */
#pragma once

/* ---------------- 底盘几何 (mm) ---------------- */

#define CHASSIS_WHEEL_BASE 500
/* [实测 2026-11-11] 纵向轴距(前进后退方向),mm。取前后轮轴中心的距离。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_TRACK_WIDTH 460
/* [实测 2026-11-11] 横向轮距(左右方向),mm。左右轮外侧面距 500 − 轮宽 40。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_GIMBAL_OFFSET_X 0
/* [实测-部分] 云台轴相对底盘几何中心的前后偏心,mm。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_GIMBAL_OFFSET_Y 0
/* [不适用] 云台轴左右偏心,mm。本车云台居中,保持 0。 */
#define CHASSIS_WHEEL_RADIUS 75
/* [实测 2026-11-09] 轮子半径,mm。实测外径约 150,取半值。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_WHEEL_REDUCTION 19.0f
/* [规格] M3508 内置行星减速比 19:1,官方规格。编码器测的是转子,输出轴 = 转子/19。 */
#define CHASSIS_MAX_MOTOR_RPM 7500.0f
/* [实测] 轮电机转子转速限幅,RPM。**2026-11-27 按用户要求 7000→7500**(上限,不再上调)。
 * 7500 = 机械 8911 的 84%;接近空载转速时扭矩骤降、电流环易饱和,只剩约 16% 转速余量,
 * 长时间满载必须复测电机/电调温度。纯平移 2000 只占 3,420rpm,不受影响;
 * 抬到 7500 只为小陀螺 520deg/s 与组合工况。 */
/* ---------------- 底盘速度 (平移 mm/s,自转 deg/s) ---------------- */
#define CHASSIS_MAX_SPEED 2000.0f
/* [待实测] 底盘**纯平移**最大速度,mm/s = 2.0 m/s。2026-11-27 按用户要求 1500→1800→2000。
 * 2000 轮速 3,420rpm、功率约 43W,离轮速/88W 都很远;限制来自落地打滑与操控,
 * 1500 只做过架空实测(峰值 1501),2000 未落地验证。 */
#define CHASSIS_ROTATE_SPEED 520.0f
/* [实测] 小陀螺自转速度,deg/s(物理域);纯自转匀速档用。
 * 2026-11-27 按用户要求 400→520:对应 7,500rpm 软限(520×14.33≈7,453rpm),
 * 功率约 70W(实测 390→50~55W 外推),88W 内。 */
#define CHASSIS_ROTATE_SPEED_TRANS 520.0f
/* [实测] 匀速档+平移时的自转速度,deg/s。2026-11-27 按用户要求 400→520。
 * 带平移时实际值由轮速余量夹低(1,000 平移约 404、1,500 约 344、2,000 约 285),
 * 520 等价"给到预算上限"。运行期变量同名可写。 */
#define CHASSIS_FOLLOW_WZ_LIMIT 400.0f
/* [实测 2026-11-20] 跟随律 wz 的**总限幅**,deg/s;0 = 不限幅。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_TRANSLATE_BIAS_ANGLE (10.0f)/* [实测-部分] 小陀螺平移方向偏置角,deg。自转时存在固定方向漂移(实测正转向右微漂),
 * 把平移指令预先旋转该角度使轨迹回正。负 = 向右偏,正 = 向左偏;仅 ROTATE 模式生效。
 * 大 → 修正过头,按 W 会向左偏;
 * 小 → 仍向右偏。10 为暂定值,**用户未回报最终效果**,需按下面方法复调。
 * 调法:纯自转看漂移方向 → 按 W 看轨迹 → 逐步加大至轨迹与云台指向重合(DEV-20261103-03)。 */

/* ---------------- 运动平顺 ---------------- */

#define CHASSIS_RC_DEADBAND 30
/* [实测] 摇杆进入运动的阈值,原始刻度 [-660, 660]。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_RC_RELEASE 20
/* [实测] 摇杆退出运动的阈值,原始刻度。与 CHASSIS_RC_DEADBAND 构成迟滞:
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VX_RAMP 5000.0f
/* [理论] 前进/后退方向的指令加速度上限,mm/s²。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VY_RAMP 5000.0f
/* [理论] 左右平移的指令加速度上限,mm/s²。与 VX 同值,理由同上。 */
#define CHASSIS_VX_JERK 20000.0f
/* [理论] 前进/后退指令的加速度变化率上限,mm/s³(S 曲线)。
 * 线性斜坡在起步/刹车时加速度阶跃,牵引峰值高;jerk 限幅让加速度平滑爬升/卸掉。
 * 0=退回线性斜坡。 */
#define CHASSIS_VY_JERK 20000.0f
/* [理论] 左右平移的 jerk 上限,mm/s³。与 VX 同值。 */
#define CHASSIS_VX_BRAKE_RAMP 8000.0f
/* [待实测] 减速段(松杆/反向扳杆)的前进/后退减速度,mm/s²。
 * 加速段仍用 CHASSIS_VX_RAMP,避免起步变猛;本值只作用于 |target|<|cur| 或反向。
 * 2026-11-27 提速 2000 后,5000/20000 的理论停车为 0.65s/650mm;
 * 8000/40000 约 0.45s/450mm。
 * **2026-11-27 起制动轮已退出功率预算(DEV-20261127-200745),本值会真正生效**:
 * 此前满刹车被功率 scale 压到 0.20,8000 与 5000 实际几乎无差别。
 * 实车若打滑(轮子尖叫/抱死反而滑更远),运行期先降到 5500~6000 再试;
 * 任一写 0 即回退到加速段参数(对称斜坡)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「CHASSIS_VX_BRAKE_RAMP」节 */
#define CHASSIS_VX_BRAKE_JERK 40000.0f
/* [待实测] 减速段的 jerk 上限,mm/s³。与 CHASSIS_VX_BRAKE_RAMP 配套。 */
#define CHASSIS_VY_BRAKE_RAMP 8000.0f
/* [待实测] 减速段的左右平移减速度,mm/s²。与 VX 同值,理由同上。 */
#define CHASSIS_VY_BRAKE_JERK 40000.0f
/* [待实测] 减速段的左右平移 jerk 上限,mm/s³。与 VX 同值。 */
#define CHASSIS_CALIB_LOG 1
/* [配置] 底盘标定日志开关:每 0.5s 一行 `[chcal] vx/vy/wz/gz/p/be`。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* ---------------- 云台跟随 (底盘追云台) ---------------- */
#define CHASSIS_FOLLOW_KP 8.0f
/* [换算+待实测] 跟随位置环增益,(deg/s) 每 deg。**2026-11-20 重写跟随律:平方律 → 线性**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_DEADBAND 2.0f
/* [实测] 跟随软死区,deg。offset 取自 yaw 电机单圈 ecd,残差永不归零,
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_TARGET_OFFSET 0.0f
/* [理论] 跟随目标相对角,deg(云台相对底盘,逆时针为正)。**2026-11-20 新增**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_FF_SLEW 0.0f
/* [实测 2026-11-21] 前馈爬升限速,deg/s²。**只管起步那一段**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_FF 0.75f
/* [理论+实测] 云台绕世界 yaw 角速度的前馈系数,无量纲。**0.9 → 1.0(2026-11-20 重写)**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_BRAKE_ON_RATE 5.0f
/* [实测 2026-11-21] **提前刹车的触发门槛**,deg/s:云台目标角速度低于本值即视为"松手"。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_BRAKE_ANT 0.4f
/* [待实测 2026-11-21] **提前刹车**增益,(deg/s) 每 (deg/s),无量纲。默认 0 = 关。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_KD 0.0f
/* [理论] 相对角速度阻尼增益,(deg/s) 每 (deg/s),无量纲。**2026-11-21 归零,默认关闭**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_P_RESERVE 0.25f
/* [理论] 位置项强制保留的额度比例,无量纲(0~1)。**2026-11-20 新增**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF 0.085f
/* [理论+待实测] 加速度前馈系数,单位 s((deg/s) 每 (deg/s²))。**2026-11-21 新增,默认 0 = 关**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_LP 0.30f
/* [理论] AFF 用的角加速度低通系数(200Hz 一阶,截止约 10Hz)。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_LIMIT 350.0f

#define CHASSIS_FOLLOW_NO_OVERSHOOT 0
/* [实测 2026-11-21] 起步硬约束:一旦底盘已追平云台(|est| ≥ |gz|),任何项都不许把
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_GATE 20.0f
/* [实测 2026-11-21] AFF 的速度门控,deg/s。**只有在转的时候才让 AFF 出力**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_ACCEL_ON 0
/* [实测 2026-11-21] 加速段(α>0)是否也让 AFF 出力。**0 = 只做刹车**。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_FOLLOW_AFF_RATE_MIN 300.0f
/* [理论] AFF 的角加速度死区,deg/s²。低于此值不输出。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
/* [理论] AFF 单项限幅,deg/s。防止换向/打杆瞬间的 α 尖峰把指令推到满额反向。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
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
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */
#define CHASSIS_VOFA_SEND_DIVIDER 1u
/* [理论] 发送分频，1=200Hz（chassis_task 200Hz），2=100Hz。
 * 细节与失效/标法/风险 → .Doc/robots/infantry/params/参数证据表.md「B1b 迁移增补」同名节 */

/* ---------------- 小陀螺抗自瞄轨迹(DEV-20261124-06,2026-11-25 改正弦) ---------------- */
/* 设计依据、开源对照与验收判据 → .Doc/robots/infantry/plans/小陀螺抗自瞄改造计划.md。
 * 2026-11-25(DEV-20261125-09):反转段实车超功率,BOOST/GLIDE/BRAKE 的加速节拍
 * 也抬高瞬时功率;状态机整体移除,轨迹 = 巡航转速 × (1 + 双尺度正弦),输出恒正。 */
#define CHASSIS_SPIN_VARY_EN 1
/* [配置] 1=变速轨迹;0=回退恒速 CHASSIS_ROTATE_SPEED,用于 A/B 隔离。 */
#define CHASSIS_SPIN_CRUISE_DPS 310.0f
/* [实测-部分] 正弦中心转速,deg/s。2026-11-27 按用户要求跟纯自转上限到 520:
 * 310 配合 A1/A2=0.451/0.226(总 0.677)得到 100~520,峰值与匀速档 520 对齐。
 * 峰值 520 轮速 7,453rpm,在 7,500 软限内;功率约 70W。 */
#define CHASSIS_SPIN_LOW_DPS 100.0f
/* [理论] 输出下限,deg/s。310×(1-0.677)≈100,正弦完整落在下限上,无平底。 */
#define CHASSIS_SPIN_HIGH_DPS 520.0f
/* [实测] 输出上限,deg/s。2026-11-27 按用户要求 400→520:与匀速档峰值一致。
 * 520 纯自转 7,453rpm(7,500 软限的 99%),功率约 70W,88W 内。
 * 平移段仍由 headroom governor 按轮速夹低。 */
#define CHASSIS_SPIN_ALPHA_MAX 1500.0f
/* [待实测] 角加速度限幅,deg/s^2。2026-11-27 800→1500:100~520 带宽下 0.83Hz 分量
 * 理论需约 2,400deg/s^2,1500 会把快速分量的峰谷圆滑到约 130~490;完全跑满需再抬,
 * 但电流尖峰风险大,先台架看峰值电流。88W 功率 scale 兜底。 */
#define CHASSIS_SPIN_JERK_MAX 6000.0f
/* [换算] 角加速度变化率上限,deg/s^3。α 从 0 爬到 800 约 0.13s,
 * 抑制起步与正弦换向的扭矩阶跃;仿真 0→300 约 490ms(DEV-20261124-11)。 */
#define CHASSIS_SPIN_NOMINAL_DT_MS 5.0f
/* [规格] 任务周期兜底,ms(200Hz)。dt<=0 或 >50ms 时使用。 */
#define CHASSIS_SPIN_MOD_F1_HZ 0.31f
#define CHASSIS_SPIN_MOD_F2_HZ 0.83f
/* [理论] 两路调制频率,Hz。不可通约,避免形成固定节拍。 */
#define CHASSIS_SPIN_MOD_A1 0.451f
#define CHASSIS_SPIN_MOD_A2 0.226f
/* [理论] 调制幅度。合计 0.677(2026-11-27 跟纯自转上限到 520):310×1.677=520、
 * 310×0.323=100,与 LOW/HIGH 对齐,全程不削顶/削底。 */
#define CHASSIS_SPIN_MOD_LIMIT 0.677f
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

/* ---------------- 小陀螺平移/刹车耦合(DEV-20261126-25) ---------------- */
/* 轮速换算(45°麦轮几何):平移 1 mm/s ≈10.26 deg/s 转子,自转 1 deg/s ≈86.0 deg/s 转子。
 * 2000 mm/s + 520 deg/s 需约 65240 deg/s=10873 rpm,远超 7500 rpm 软限;统一缩比例会把
 * 正弦削平,速度环饱和又把模型功率抬高。以下参数把"平移占掉的轮速"从自转预算里扣,
 * 并在刹车瞬间让自转让出摩擦圆。 */
#define CHASSIS_SPIN_MAX_SPEED 2000.0f
/* [理论] 小陀螺模式下平移最大速度,mm/s。2026-11-27 跟纯平移上限到 2000;
 * 与 CHASSIS_MAX_SPEED 相等,即组合工况允许平移拉满,自转由轮速 governor 自动夹低
 * (2000 平移约 285、1500 约 344)。运行期变量同名可写。 */
#define CHASSIS_SPIN_WHEEL_RPM_MAX 7500.0f
/* [理论] 小陀螺模式允许的轮转子转速上限,rpm。2026-11-27 按用户要求 6800→7500,
 * 与 CHASSIS_MAX_MOTOR_RPM 同值(= 机械 8911 的 84%,无额外速度环余量);
 * 纯自转 520 占 7,453rpm,组合平移占掉的轮速从自转里扣,变速轨迹整体下移而不是削顶。
 * 运行期可写。DEV-20261126-26: 5600→5800;DEV-27 放宽到 6800。 */
#define CHASSIS_SPIN_CENTER_MIN_DPS 100.0f
/* [理论] 变速轨迹中心下限,deg/s。平移占满时仍保留最低自转,避免完全停转。 */
#define CHASSIS_SPIN_CENTER_LP 0.05f
/* [理论] 轨迹中心跟随目标的低通系数(每拍,200Hz):0.05≈100ms。 */
#define CHASSIS_SPIN_TRANS_LOW_DPS 150.0f
/* [待实测] 平移段变速下界,deg/s;和 HIGH 一起定义平移时的正弦带(150~450)。
 * 低端沿用 150,高端 340→450(2026-11-27 跟纯自转上限抬高);高段实际仍由
 * 轮速余量夹低(1000 平移约 404、1500 约 344、2000 约 285)。运行期可写。 */
#define CHASSIS_SPIN_TRANS_HIGH_DPS 450.0f
/* [待实测] 平移段变速上界,deg/s;2026-11-27 340→450。实际取 min(本值, 轮速余量):
 * 500 平移+450 轮速约 7,000rpm(7500 软限内);1000 平移+450 需 8,880rpm 会被夹到 404,
 * 2000 平移夹到 285。运行期可写。 */
#define CHASSIS_SPIN_HANDOFF_HOLD_MS 200.0f
/* [待实测] 平移→纯自转交接:按当前正弦输出选最近端点(低于 310→100,否则→520)保持 200ms,
 * 再从该端点相位重启纯自转正弦;0=直接切。运行期可写。 */
#define CHASSIS_SPIN_HANDOFF_INTEGRAL_SCALE 0.25f
/* [待实测] 交接 RAMP/HOLD 段把四轮速度环积分限幅压到 25%(P 项保留):
 * 松杆瞬态的 5~6k 电流里约 3000 码来自平移段积累的积分,压掉它可直接削峰。
 * 1=不压;0=完全去积分(可能稳态滞后)。运行期可写。 */
#define CHASSIS_SPIN_ENTRY_HOLD_MS 300.0f
/* [待实测] 平移切入后自转先保持低位的时长,ms;之后放宽到 150~320。
 * 200ms 实测只到 ct=184(中心低通 100ms 拖慢),延长到 300ms 让中心真正落到 165。
 * 0=关闭。运行期可写。 */
#define CHASSIS_SPIN_ENTRY_BAND_DPS 30.0f
/* [待实测] 入口保持段的自转带宽:150~180(中心 165、±9%)。
 * 太窄起步自转弱,太宽入口电流大。运行期可写。 */
#define CHASSIS_SPIN_MOD_LIMIT_LP 0.10f
/* [理论] 平移/纯自转切换时调制限幅的低通(每拍,200Hz):0.1≈50ms,避免限幅跳变。 */
#define CHASSIS_SPIN_MOD_TRANS_MIN_DPS 3000.0f
/* [理论] 判定"平移在占轮速"的阈值,deg/s 转子;约 292mm/s 平移。
 * 超过即进入平移带,低于则触发回纯自转交接。 */
#define CHASSIS_SPIN_BRAKE_GAIN 0.60f
/* [待实测] 刹车时自转削减比例(0~1):按平移减速度线性削到 1-GAIN;
 * 松杆/反向时自转先让出摩擦圆,刹车结束后自动恢复。0=关闭。运行期可写。 */
#define CHASSIS_SPIN_BRAKE_MIN 0.35f
/* [理论] 刹车削减下限,防止自转完全停转导致云台/底盘相对角跳变。 */
#define CHASSIS_SPIN_BRAKE_LP 0.20f
/* [理论] 刹车削减低通(每拍,200Hz):0.2≈25ms,比轨迹 α 限幅快,保证刹车及时。 */
#define CHASSIS_SPIN_BRAKE_SPEED_MIN 200.0f
/* [理论] 触发刹车削减的最低平移速度,mm/s。低于此值剩余动能小,不再牺牲自转。 */

/* ---------------- 底盘功率控制 ---------------- */
/* 2026 通信协议 V1.2.0 删除了 0x0202 的 chassis_power(前 8 字节改保留位),
 * 裁判不再回传功率,改为电机模型预测:用四轮 C620 电流码与转子转速直接算功率。
 * 模型形式与天工大 304 开源(MIT)一致,系数取自其实测 M3508 值。
 * scale 只乘四轮电流指令(chassis_limit_output → output_scale),轮速目标不动:
 * 速度环仍按操作手目标算误差,由电流限幅自然减速(DEV-20261126-12)。 */
#define CHASSIS_POWER_CTRL_EN 1
/* [配置] 0=关闭,scale 恒 1,回退旧行为。 */
#define CHASSIS_POWER_LIMIT_DEFAULT 90.0f
/* [规则] 裁判上限无效时的兜底功率,W。2026-11-26 真实功率标定用 200 采完后
 * 已恢复 90;控制目标取 90-MARGIN=88W(2026-11-27 按用户要求)。 */
#define CHASSIS_POWER_LIMIT_MAX 90.0f
/* [规则] 硬上限。有裁判时有效上限取 min(裁判上限,90)-2=88W;
 * 2026-11-26 真实功率标定临时 90→200 采完后已恢复。 */
#define CHASSIS_POWER_MARGIN_W 2.0f
/* [实测] 控制余量,W:控制目标取 90-2=88W(2026-11-27 按用户要求 5→2)。
 * **风险**:裁判实测 85W 目标时纯自转实际 85~87W,且 4~6k 码瞬态模型外推、
 * DEV-20261126-16 曾见约 10W 超调;88W 目标下实际可能短时到 90~95W,靠缓冲吸收,
 * 持续超限会掉缓冲。赛前若未重拟合,宁可回 5W(85W)。 */
/* [实测] 控制余量,W:控制目标取 90-5=85W(DEV-20261126-26 按用户要求 10→5)。
 * 2026-11-26 裁判实测 10~55W 的 16 个稳态点:304 形状误差 ≤5W 且整体偏低
 * 5~10%,已用 CHASSIS_POWER_MODEL_SCALE=1.10 校正(见该宏注释),5W 余量保留给
 * 云台/发射等模型外负载与残差。 */
#define CHASSIS_POWER_SCALE_LP 0.15f
/* [理论] scale **恢复方向**一阶低通(每拍,200Hz):0.15≈30ms;降额方向同拍生效
 * (快降慢升)。原对称 0.6 连降额也要 3 拍(~15ms)才到位,正好盖住切入尖峰
 * (DEV-20261126-31)。1=不滤波(直通),0=冻结(勿用)。 */
#define CHASSIS_POWER_TRANS_ENTRY_MS 350.0f
/* [待实测] 平移切入瞬态窗口,ms;覆盖 S 曲线加速 + 入口自转保持(300ms)。
 * 窗口内速度环积分限幅压 25%;功率总 scale 已改为**永远生效**,不再由本窗口开关。
 * 运行期可写。 */
#define CHASSIS_POWER_TRANS_TAIL_MS 100.0f
/* [待实测] 松杆交接结束(PURE)后的积分压限尾巴,ms;避免交接末尾电流恢复的二次尖峰。
 * 运行期可写。 */
#define CHASSIS_POWER_MIN_SCALE 0.20f
/* [安全] 总功率 scale 下限:20% 电流权限;跟转实测 0.19 仍能稳住,20% 是保底,
 * 再低平移/刹车几乎没有力。模型内部仍有 CHASSIS_POWER_SCALE_MIN(0.05)兜底。 */
#define CHASSIS_POWER_SCALE_MIN 0.05f
/* [安全] 模型衰减系数内部下限;最终输出再受 CHASSIS_POWER_MIN_SCALE 夹住。 */
#define CHASSIS_POWER_BUFFER_MIN_SCALE 0.5f
/* [参考 304] 缓冲能量为 0 时的功率上限系数:limit *= 0.5 + 0.5*buffer/MAX_J。
 * 缓冲快耗尽时主动降额,优先让缓冲回充;裁判离线时该衰减不生效。 */
#define CHASSIS_POWER_BUFFER_MAX_J 60.0f
/* [规格] 缓冲能量满值,J。2026 规则缓冲 60J;若规则变更需同步。 */
#define CHASSIS_POWER_MODEL_SCALE 1.10f
/* [实测] 模型整体比例修正:P_corr = SCALE*P_model + BIAS。
 * 2026-11-26 裁判实测 16 点(纯自转 200~390、平移 500~1500、300+1000、390+500):
 * 304 形状各点误差 ≤5W,但整体偏低(纯自转比值 1.06~1.13、组合 1.02~1.09、
 * 1500 平移 0.96)。取 1.10 把纯自转/组合拉回实测,平移略保守(安全方向);
 * 无约束重拟合会得到 K4<0、高电流段不保守,已否决(DEV-20261126-34)。 */
#define CHASSIS_POWER_MODEL_BIAS 0.0f
/* [标定] 模型整车偏置,W。默认 0;拟合后填。 */

/* M3508 功率模型:P = K0 + K1*|i| + K2*|w| + K3*|i|*|w| + K4*i^2 + K5*w^2
 * i = 电流指令码 / CHASSIS_POWER_CURRENT_DIV,w = 转子转速 deg/s(模型内转 rpm)。
 * 系数来源:天工大 304 战队 2026 开源实测(real_current_conversion=1000,MIT);
 * 304 运行时取绝对值,本工程一致(DEV-20261126-12)。
 * 2026-11-26 用裁判真实功率复核(DEV-20261126-34):静态 2.45W 对 4*K0=2.6W,
 * 16 个稳态点误差 ≤5W;保留 304 形状,只加 MODEL_SCALE 比例校正。
 * 换电机/电调/轮系或供电链路后必须按 tools/fit_power_model.py 重录重拟。 */
#define CHASSIS_POWER_CURRENT_DIV 1000.0f
/* [换算] 电流码→模型输入。304 用 1000;若改用物理安培须同步重拟全部系数。 */
#define CHASSIS_POWER_MODEL_K0 0.65213f
#define CHASSIS_POWER_MODEL_K1 (-0.15659f)
#define CHASSIS_POWER_MODEL_K2 0.00041660f
#define CHASSIS_POWER_MODEL_K3 0.00235415f
#define CHASSIS_POWER_MODEL_K4 0.20022f
#define CHASSIS_POWER_MODEL_K5 0.000000108f
