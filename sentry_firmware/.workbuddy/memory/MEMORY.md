# MEMORY.md — JIT-RM 1029 项目长期笔记

## 工程统一化基线(2026-11-23,DEV-20261123-01;改前必看)

- 目录:`apps/<robot>/app/`(profile/robot/robot_def/robot_task)+ `<robot>/{chassis,cmd,gimbal,shoot}`;
  `components/{motor,control,crc,math,imu,remote,referee,ax650,calib,super_cap,can_comm,daemon,alarm}`;
  `config/infantry_2026/config.h` 伞头 + `config_<域>.h`;旧 `application/modules/bsp` 仅历史文档。
- 命名:文件/函数/变量 snake_case;函数 `module_verb_object`;类型 `_t`、枚举 `_e`;宏全大写带域前缀。
  规范 `.Doc/spec/命名规范.md`;机器闸门 `python tools/check_naming.py` + `.clang-tidy`。
- 宏:禁止裸宏/单字母;`TRUE/FALSE`→`stdbool`;`PI`→`FW_PI`;`MatAdd/msin/mat` 已删,统一 `math_matrix_t`/`MATH_*`;
  config 跨域 `#if` 全部上移伞头,子头跨文件 `#if` = 0。
- 注释:135 个第一方 .c/.h 文件头 100%,`@file/@layer/@author` 全覆盖;历史 DEV 条目路径不改写。
- 删除:旧视觉栈 master_process/seasky(RM-011 结案)、3 个 0 字节头;DM/HT/servo/super_cap/can_comm 保留为
  OPTIONAL/DORMANT,登记 `components/README.md`。
- 基线:tag `naming-v1`;text 117,336 / bss 126,680;四套主机测试 + 全部静态闸门 PASS。
- 注意:不要再给 `config/` 子头起 `gimbal.h`/`chassis.h` 这种与 apps 同名的裸名(include 顺序会拿错头);
  不要"去重" `MATH_MALLOC`(会从 malloc 静默切 pvPortMalloc)。

## yaw 前馈三条铁律(2026-11-12 深审定案,改前必看)

1. **惯量前馈主语是世界系 α,摩擦前馈是关节量,不要换源**:yaw 与底盘自转同为竖直轴,惯性系欧拉方程
   `J·α_world = τ_m − τ_f(ω_joint)`。维持恒速只需摩擦力矩(B·ω_joint 覆盖),惯量项在小陀螺给 0 是正确行为。
   关节系模型必须补 +J·α_chassis,只换 α 源不补这项 = 底盘加减速段注入反向力矩 → **"换源关节 α"已否决**
   (初版铁律1"惯量前馈必须用关节 α"系物理错误,同日深审推翻)。可选改进:参考轨迹差分 α_ref 替代含噪
   实测 α(P4,宏 `YAW_INERTIA_FF_SRC`)。J 标定必须在底盘静止工况加激励,恒速扫掠辨识不出 J。
2. **`YAW_FRICTION_FF_B=15` 不是摩擦系数**:300°/s 时输出 0.40N·m,比真实摩擦估算(库仑 0.05~0.15 +
   粘性 0.005~0.03 N·m·s/rad,**合计 0.08~0.31N·m**)大 1.3~5 倍(初版"<0.05N·m/大一个数量级"自相矛盾,已修正),
   大头在补电压域非线性/反电动势残差。电流模式必须重标定,预期 τc≈370~1100 码、B≈0.6~3.9 码/(deg/s)
   (7380 码/N·m);落在区间外先怀疑量纲错。`B=18 失稳`是等效负阻尼。
3. **摩擦模型要有库仑项**:只有 `B·ω` 时零速给不出恒值 τc,这是"静止残差 ±1.4°"和微动粘滑的物理来源。
   补 `τc·tanh(ω/ε)`(tanh 而不是 sign,避免零速抖)。佐证:FF 全关时摩擦经"角度环P×速度环P"通路建立
   (IL=100 码扛不动),300°/s 档稳态 offset≈1.5°,与 ±1.4° 实测吻合。

计划全文见 `.Doc/yaw轴深化改造计划.md`(2026-11-12 已按深审结论修订:P4 改 α_ref 前馈、P2 重写、
§3.2 数字修正、判据与风险矩阵同步)。

## 构建环境坑(反复踩,先看这里)

- `build/CMakeCache.txt` 的 `CMAKE_HOME_DIRECTORY` 曾残留旧路径 `C:/RoboMaster/RM-JIT-CR`(工程改名过)。
  `ninja -C build` 报 "source directory ... does not contain CMakeLists.txt" → 删 CMakeCache.txt 重建。
- `CMakeLists.txt` 用裸名 `arm-none-eabi-gcc`,靠 PATH 解析。重建缓存时会抓到 `C:\Toolchain\bin`(15.3.1),
  与历史产物(14.2 Rel1)不是同一套 → 要复现历史构建,先把 14.2 rel1 的 bin 放到 PATH 最前。
- 沙箱内 gcc/as 子进程写不进 `build/`(`Permission denied` / `error writing to -: Invalid argument`),
  编译必须提权执行;与代码无关。
- **本机 cmake/ninja 不在 PATH**,用 STM32Cube 自带:
  `C:/Users/ZhuanZ/AppData/Local/stm32cube/bundles/cmake/4.3.1+st.1/bin/cmake.exe`、
  `.../bundles/ninja/1.13.2+st.1/bin/ninja.exe`。CMake 源文件列表是配置期 GLOB_RECURSE,
  **新增 .c 后必须重跑 `cmake -B build`**,否则只报 undefined reference(编译产物里根本没有该文件)。
  build 缓存的编译器是 `C:/Toolchain/bin`(15.3.1),与 PATH 里的 14.2 Rel1 不是同一套;别为此删缓存。

## yaw GM6020 命令帧(电压 0x2FF / 电流 0x2FE)

- 驱动层 `dji_motor.c` 早已支持两种帧(分组自动选 id,同组混用 fail-fast),切换只需业务层参数。
- 统一开关:`YAW_ESC_CURRENT_MODE`(config),派生 `YAW_ESC_OUT_MAX` 与 `YAW_CURRENT_DOMAIN_SCALE`(0.6554)。
  所有"输出单位"的增益(速度环 Kp/**Ki**、惯量/摩擦前馈)都要乘域系数;角度环在输入侧不用。
  —— 历史实现漏乘 Ki,切域后积分相对强 1.5 倍。
- **电流帧不被接受时的现象极易误判**:反馈报文(角度/转速/温度)完全正常,只是不出力。
  **2026-11-11 A/B 定案:0x2FE 被忽略的根因是 RoboMaster Assistant 里"电流环"开关未开**
  (同一代码电压帧出力、电流帧零扭矩;Assistant 开启后电流帧自检 PASS,快转-反向刹停)。
  电调侧设置才是这个功能的开关,代码勿再排查。复验工具 = `YAW_ESC_SELFTEST_ENABLE` 空载自检(±3000 码脉冲)。
- 电流模式闭环未整定:域换算 0.6554 是等效换算,高速区会变"硬"(无反电动势钳制);
  YAW_SPEED_KP_HIGH=40 的调度本为补反电动势,电流模式下大概率要降回 20;
  力矩上限 ±3A≈2.2N·m,急停/小陀螺最高转速会比电压模式软。

## RM2027 自定义终端(Custom Terminal)技术事实

来源:《RoboMaster 2027 高校系列赛通信协议 V1.0.0(20261029)》+ 开源实现核对
(github.com/BUAA-Transistor-alg/RM2027-CustomTerminal-CommunicationBackend)

- 2027 把原 Custom Controller + Custom Client 整合为「自定义终端」;终端 **RJ45 有线直连赛事引擎**,规则严禁无线收发;尺寸/电压受限(2026 版 Client 30V/400³mm,Controller 30V/600³mm,2027 以新手册为准)。
- 数据通道:MQTT broker `192.168.12.1:3333`,qos1,topic=指令名,Protobuf v3(≥3.15);终端自身 IP `192.168.12.2`,clientID = 机器人 ID。共 32 条指令(上行 9 / 下行 23)。
- **图传通道:UDP 3334,HEVC/H.265,1920×1080@60fps**,无重传;包头 8 字节大端 = 帧号(u16)+分片序号(u16)+帧总字节(u32);单包 ≤1400B(负载 1392B)。
- 结论:**画面在终端侧解码,数字变焦/开镜放大完全可行**(FFmpeg 解 H.265 → crop+resize → 显示,附加延迟 <2ms)。代价是有效分辨率下降(2x 变焦后 960×540)。
- 控制不能走终端:官方 `CustomControl` 指令仅 ≤60B 且低频(官方默认约 5Hz),做不了 200Hz 实时瞄准。→ 键鼠控制仍走原 DBUS/图传链路,灵敏度在机器人端算。
- 架构建议:右键开镜由「终端」和「机器人」各自独立判断,画面放大在终端、灵敏度切换在固件,两边不通信、天然同步。

## 本车关键参数(易错点)

- **2026-11-09 轮径修正**:`RADIUS_WHEEL` 实测 75mm(原配 60,直径 120 → 实际约 150)。因 `wheel_ratio = 360×19/(2πR)` 是倒数关系,原配置让**一切速度比指令快 25%**:自转指令 300 实际跑 375,平移 1500 实际 1875。
- 由此解开"小陀螺功率仅 30W 是否异常"的疑问:**30W 对应的是 375 deg/s(转子 3359 RPM)**,与转速其实匹配。修正后同一指令为 300 deg/s / 转子 2687 RPM / 约 22W,均属正常(4×3508 空载)。
- 力臂 = √((轴距/2)²+(轮距/2)²)。**力臂只影响自转与跟随追赶速度,不影响平移**。代码用 (WB/2+TW/2)×0.7071 近似对角线,两值差 20% 内误差 <1%。
- **2026-11-11 几何量全部实测**:`WHEEL_BASE`=500(纵向外缘≈轮心平面,滚子离散±15)、`TRACK_WIDTH`=460(横向外侧面距 500 − 轮宽 40)、轮宽 40、轮径 150。力臂 212 → **340mm**(+60%),同一自转指令实际转速比此前快约 60%,`CHASSIS_ROTATE_SPEED` 体感要重对;自转+平移叠加更易触 6000 RPM 限幅。
  **测量方向不对称的坑**:沿车长方向的极值是轮心平面(不减轮宽),沿车宽方向的极值是滚子端面(减一个轮宽,不是两个)。
- 跃鹿上游 `wz = 4000` 是量纲残缺的经验魔数,**不可跨工程照抄**(本工程已补 √2 与 wheel_ratio,照抄会被 6000RPM 限幅削满)。
- `ext_power_heat_data_t` 把 0x0202 的 float power 写成 `reserved3` 且未解析 → 下位机目前读不到底盘功率。

## 注释规范(2026-11-09 制定)

权威文档:`.Doc/注释规范.md`。改写注释前必读,按它执行,不要另立风格。

- 三级体系:文件头(必须)/ 接口注释(公共 API 必须)/ 实现注释(只写 why)。
- 文件头只留 `@file @brief @layer @note`,**去掉 @author/@date/@version/@copyright**(靠 git)。
- 函数标签只用 `@brief @param @return @note`,不用 @retval/@attention。
- 硬规则:**每个数值必须带单位**;**每个魔法数必须写来源**(实测/规则/标定/上游遗留);**.h 与 .c 不重复注释**;注释不重复函数名。
- 历史引用格式:`结论句(DEV-YYYYMMDD-NN)`,编号与结论缺一不可。
- **参数注释四级分类**:A 手感/命中/安全(转速/PID/限幅/阈值/超时)、B 几何常量(测量方式)、C 换算系数(量纲与域)、D 功能开关(关掉后果)。
- **首要规矩:现象必须带证据等级**——`[实测 日期]` / `[理论]` / `[上游]` / `[换算]` / `[待实测]`。没实测就不许写具体数值现象;`[待实测]` 是合法写法,用来标出知识空白。改注释前必须 `grep` DEVLOG 与《本车调参结论》反查实测记录,不得凭印象写。
- 单位必须标数据域:物理 deg/s、转子 deg/s、输出轴 deg、遥控器原始刻度,四者不可混写。
- 禁止:复述代码、过期注释、注释掉的死代码(改 `#if 0`)、模板占位符、装饰线滥用。
- 开工前的现状:128 个第一方文件,61%(78 个)无文件头,1 个文件头张冠李戴(power_control.h 写成 dji_motor.h),19 个含占位符,30 个 @todo。先改 application(17 个)。

