# 实机导航首次低速测试（2026-11-21）

2026-11-22 更新：本车改用 cod_nuc_bringup/nuc_hardware.launch.py，详见同级源码包
cod_nuc_bringup/HARDWARE_FRAMES.md。不要再用旧单点/多点组合入口启动本车。
以下速度与制动测试仍适用；实际启动、TF、里程计与串口接法以新说明为准。

单点、多点 launch 分别默认加载 singlenav2_params.yaml、multiplenav2_params.yaml。
本次只调整这两份源配置；未部署到机器人，未运行实机验收。仿真 sim_nav2.yaml 不变。

## 当前基线

- 每轴平移速度 ±0.35 m/s，对角线合速度上界约 0.50 m/s；角速度 ±0.5 rad/s。
- 每轴平移加速度 0.35 m/s²、减速度幅值 0.5 m/s²；平滑器与 MPPI 一致。
- 距目标 0.8 m 起限合速度 0.15 m/s；禁用直接覆盖 MPPI 的近目标驱动，保留避障输出。
- 50 Hz 控制、0.02 s 模型步长、80 步 = 1.6 s 预测时域。
- 指令断流 0.2 s 后平滑器开始减速，不代表 0.2 s 内停车；无进展 15 s 后恢复。
- 保留 OPEN_LOOP：当前 small_point_lio_node.cpp 的 Odometry twist 赋值被注释，且反馈坐标与 base_link_fake 需要核实，不能直接启用 CLOSED_LOOP。

## 未知项与适用边界

底盘含突出部件的实际轮廓、地面抓地力、里程计/TF/下发链路延迟尚未测量。
现有 footprint 单点为 20×20 cm、多点为 30×30 cm，本次未猜测尺寸替换。
尺寸核实前只能在空旷区域低速测试，不能据此认定狭窄通道可通过。
未提高 transform_tolerance，也未把增加 TF 等待时间当作延迟补偿。
本次参数没有加入定位停更独立停车保护；需要实测定位断流和底盘通信看门狗。
fake_vel_transform 将角速度替换为 spin_speed（默认 0），因此 Nav2 的角速度限制不约束独立设定的小陀螺转速；首次测试保持 spin_speed=0。

## 部署与验收

1. 在机器人 ROS 工作区更新源配置，执行 colcon build --packages-select cod_bringup，重新 source install/setup.bash，重启导航。
   用 ros2 param get controller_server FollowPath.vx_max 和 ros2 param get velocity_smoother max_velocity 核实加载值为 0.35 和 [0.35, 0.35, 0.5]。
2. 记录 /Odometry、/tf、/tf_static、/cmd_vel_nav、/cmd_vel、/aft_cmd_vel；记录障碍物输入和控制器超时日志。
   用 Odometry 的 header.stamp 与接收时刻差测延迟，确认主机/传感器时钟一致；检查 TF 断流。
3. 空旷场地依次测试直行、横移、斜行、停车和接近目标，检查方向正确、无持续摆动/打滑，并测实际停车距离与终点误差。每种方向至少重复 5 次。
4. 在可控条件下验证指令断流、定位断流时的停车行为，并确认底盘通信看门狗能独立停机；未通过前不放速。
5. 当前估算仅假设延迟 0.20 s、有效减速度 0.50 m/s²：0.50 m/s 时 v*延迟+v²/(2a) 约 0.35 m，不包含碰撞轮廓、定位误差和全部链路滞后，也不是实测保证。
   后续用延迟高分位值、最差实测减速度、实际终点误差重新计算减速距离与障碍物余量。
6. 实测合格后每次只小幅提高速度或加速度，并同步修改 MPPI 与 velocity_smoother；出现滑移、振荡、位置滞后或计算超时即退回上一个通过值。
