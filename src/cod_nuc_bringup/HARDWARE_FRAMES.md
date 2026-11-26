# 实车导航坐标系接入（2026-11-22）

依据《COD导航坐标系接入与原因.md》实现，物理 TF 链为：

```text
map -> odom -> base_link -> big_yaw_link -> livox_frame
```

## 启动入口与 TF 所有权

使用 `cod_nuc_bringup nuc_hardware.launch.py`。不要同时运行旧版
`cod_bringup singlenav_launch.py`、`multiplenav_launch.py`、loopback 仿真或手持建图。
旧版入口中的静态 map/odom 与 fake_vel_transform 不属于本方案。
单点和多点都通过本实车入口提供的 Nav2 action 使用；可通过 `nav2_params_file`
选择 singlenav2_params.yaml 或 multiplenav2_params.yaml，均保留 0.35 m/s 每轴低速基线。

| 关系 | 唯一发布者 |
|---|---|
| map -> odom | localization 模式为 AMCL；mapping 模式为 SLAM Toolbox |
| odom -> base_link | chassis_odometry.py |
| base_link -> big_yaw_link | big_yaw_tf.py，使用平台反馈采样时间 |
| big_yaw_link -> livox_frame | mid360s_mount.launch.py |

固定安装 XYZ=(-0.053,0,0.474) m，roll=pi。驱动 JSON 安装外参保持零，
避免重复倒装。LIO 内部 T_I_L 不加入这个倒装角。

## 电控角度输入接口

2026-11-25 已按本地电控实现 USB 接收，桥接输出以下 ROS 接口：

- 话题 `/big_yaw/joint_states`，类型 `sensor_msgs/msg/JointState`。
- `name` 包含 `big_yaw_joint`，对应 `position` 为平台相对底盘的角度，rad。
- 平台 X 与底盘 X 同向时为零；从上向下看逆时针为正。
- `header.stamp` 是真实采样时刻，和雷达、IMU 使用统一 ROS 时间基准。
- 若原始反馈是电机编码器，USB 驱动必须先按齿数/减速比、方向、零位和跨圈换算。
- 建议提供连续展开的角度；相邻样本真实转角必须小于 pi，保证 TF 四元数插值无歧义。

节点只在收到有效的新样本时发布，不用定时器给旧角度盖新时间。
重复或倒退的时间戳、无效数值被拒绝。TF 查询缺失时不取最新角度替代。

## LIO 与底盘输出

Small Point-LIO 新增 `raw_imu_output`，默认 false，原手持及其他入口行为保留。
实车入口开启该模式，并重映射输出为：

- `/lio/imu_odometry`：IMU 在内部世界 W 中的位姿，帧 lio_world / lio_imu。
- `/lio/world_cloud`：内部世界 W 中的点云；此模式不再广播旧 odom/base_link TF。

chassis_odometry.py 与 LIO 共用同一份 yaml 的 T_I_L，禁止在线估计外参。
通过采样时刻的完整安装 TF 计算 T_B_I=T_B_L*inverse(T_I_L)。首次有效样本定义
T_O_B(0)=Identity，固定 C=T_B_I(0)*inverse(T_W_I(0))；后续
T_O_B=C*T_W_I*inverse(T_B_I)。全局地图初始位姿由 AMCL 的 /initialpose 给出。

底盘输出 `/Odometry` 的 pose 在 odom、twist 在 base_link。
twist 使用连续补偿后的底盘位姿差分，首帧或超过有效间隔时不推断速度。
当前没有传播滤波器、角度反馈、安装标定的联合协方差，因此 pose/twist 六个
协方差对角项填 1e6，明确表示未标定的不确定性；不能当成已验证的融合输入。
保留速度平滑器 OPEN_LOOP，需用实测验证差分噪声后再考虑闭环。

点云始终只乘固定 C，保留时间戳与其他字段。PCD 保存仍属于 LIO 内部 W，
实车默认 save_pcd=false；需要导出 odom 点云时录制 /cloud_registered。

底盘输出最多等待 0.3 s 的采样时刻 TF，过旧或明显超前样本丢弃并告警。
LIO 时间倒退时停止转换，要求重启完整实车链路重新对齐；不要单独重启 LIO。
这不是底盘独立失联停车保护，通信看门狗仍须由实际桥接与电控保证。

## 定位、观测与速度

localization 启动 map_server、AMCL 和生命周期管理器，采用 OmniMotionModel；
必须在 map 中设置 /initialpose。mapping 只启动 SLAM Toolbox，开启 map/odom
动态发布，不启动 AMCL、map_server 或静态 map/odom。

激光切片在 base_link 中处理原始点云，Z 范围暂沿用 0.1~1.0 m（离地约
0.165~1.065 m），后续按障碍物实测调整。Nav2 两张代价地图在新入口中仅使用
原始 /livox/lidar 作为雷达观测；没有误接旧的雷达系车体裁剪结果，也没有启动
尚未标定安装 TF 的深度相机。车体过滤与快速平台旋转下的逐点去畸变仍需实测，
不能把一次整帧 TF 投影视为逐点补偿。

### RViz 原始点云显示

`cod_bringup/rviz/rviz2.rviz` 的 Fixed Frame 使用 `base_link`，PointCloud2
话题使用 `/livox/lidar`。MID-360S 驱动实际发布的消息坐标系是
`livox_frame`，RViz 通过 `base_link -> big_yaw_link -> livox_frame` 的 TF
把原始点云转换到底盘坐标系。不要使用旧配置中的 `imu_link` 和
`/livox/lidar_PointCloud2`，前者不属于当前接入链，后者在当前驱动中没有发布者。

无串口测试模式下，`base_link -> big_yaw_link` 是零角静态 TF，因此大 yaw
必须机械锁定在零位；大 yaw 转动后仍使用静态 TF，会让点云方向与实物不一致。

最终 cmd_vel 的 linear.x/linear.y/angular.z 分别为底盘前进/左移/逆时针角速度。
新入口不启动 fake_vel_transform。2026-11-25 起使用 sentry_usb_bridge.py 的 NV v1
32 字节协议，替换旧 15 字节串口发送链。start_serial 默认 true，但 enable_drive
默认 false；仅测时和回传大 yaw。固件工作副本位于 ../../sentry_firmware，详见
../../sentry_firmware/NAVIGATION_USB.md。旧 cod_serial_ul26 不属于当前实车链路。

## 编译与启动

在 NUC 的 ROS 工作区执行（先安装本工程依赖）：

```bash
source /opt/ros/humble/setup.bash
colcon build --packages-select cod_bringup small_point_lio cod_nuc_bringup
source install/setup.bash
```

先独立核对固定安装关系，可运行：

```bash
ros2 launch cod_nuc_bringup mid360s_mount.launch.py
ros2 run cod_nuc_bringup big_yaw_tf.py
```

完整入口默认检查 MID-360S 内部外参状态。必须先核实/更新
`cod_bringup/params/mid360s_lio.yaml` 的 T_I_L，才设置 imu_extrinsics_confirmed。
当前继承的 MID-360 数值没有因为本次代码修改自动变成已验证的 MID-360S 标定。

完成标定并已接入角度反馈后，先运行定位/建图、观测和 USB 反馈，不启动导航动作：

```bash
ros2 launch cod_nuc_bringup nuc_hardware.launch.py \
  mode:=localization imu_extrinsics_confirmed:=true
# 建图时改为 mode:=mapping
```

先验收底盘静止、只转平台时 /Odometry 基本不变，固定墙体不随平台转动；
再验收底盘前进、左移和转向。零位雷达原点为 (-0.053,0,0.474)，
平台 +90° 时为 (0,-0.053,0.474)。

启动导航需传入实测 footprint（base_link 中至少三个 XY 顶点，单位 m），
当前不填写猜测尺寸。格式为：

```text
ros2 launch cod_nuc_bringup nuc_hardware.launch.py mode:=localization
  imu_extrinsics_confirmed:=true start_navigation:=true footprint:='<实测多边形>'
```

上面的多边形占位符不是可执行值。单点配置可另传
`nav2_params_file:=<cod_bringup共享目录>/params/singlenav2_params.yaml`。
未测尺寸时保持 start_navigation=false；未完成方向、测时和超时验收前保持 enable_drive=false。

## 本地验证与实机边界

测试覆盖固定安装几何、偏心平台整周转动时底盘不动、平台与底盘同时运动、
初始世界对齐、固定世界点云、底盘坐标速度、跨 pi 角度、采样时间 TF 等待、
过期样本丢弃及两种定位模式互斥。ROS 2 Humble 本地构建和自动测试可执行：

```bash
colcon test --packages-select cod_nuc_bringup
colcon test-result --verbose
```

本次未连接 NUC、雷达或底盘；USB 双端代码已本地实现，实机时间同步、内部外参、方向和实际轮廓仍待验收。
