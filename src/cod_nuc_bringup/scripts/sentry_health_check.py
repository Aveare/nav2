#!/usr/bin/env python3
"""哨兵导航与自瞄一键体检工具（纯大白话输出 + 逐项排查建议）。

用法（在小电脑终端直接运行）:
  python3 /home/rm/cod_ws/src/cod_nuc_bringup/scripts/sentry_health_check.py
  # 或：ros2 run cod_nuc_bringup sentry_health_check.py --duration 3.0
  # 输出机器可读 JSON：--json
  # 离线自检诊断规则：--self-test
"""
import argparse
import glob
import json
import math
import subprocess
import time


def diagnose(metrics: dict) -> list:
    """根据采集到的指标生成大白话诊断清单，每条包含：项目、等级、现象、排查建议。"""
    items = []
    dur = max(0.5, float(metrics.get('duration_s', 3.0)))

    # 1. 硬件物理连接
    lidar_ok = bool(metrics.get('lidar_ping_ok', False))
    tty_list = metrics.get('tty_devices') or []
    by_id_list = metrics.get('serial_by_id') or []
    if lidar_ok:
        items.append(('1. 雷达网线连接', '正常', '能 ping 通雷达 (192.168.1.144)', '无需处理'))
    else:
        items.append(('1. 雷达网线连接', '故障', 'ping 不通雷达 (192.168.1.144)',
                      '检查雷达供电是否亮灯、网线有没有插紧、小电脑网口静态 IP 是否在 192.168.1.x 网段'))

    if by_id_list:
        items.append(('2. USB 串口识别', '正常',
                      f'已识别稳定路径: {", ".join(by_id_list)}',
                      '无需处理'))
    elif tty_list:
        items.append(('2. USB 串口识别', '正常', f'已识别设备: {", ".join(tty_list)}',
                      '建议优先使用 /dev/serial/by-id/ 稳定路径'))
    else:
        items.append(('2. USB 串口识别', '故障', '没看到 /dev/ttyACM* 串口设备',
                      '检查小电脑到 C 板的 USB 线有没有插牢、C 板是否上电；拔插后重跑 ls /dev/ttyACM*'))

    # 2. 雷达点云与障碍物检测
    cloud_n = int(metrics.get('cloud_msgs', 0))
    scan_n = int(metrics.get('scan_msgs', 0))
    scan_valid = int(metrics.get('scan_valid_rays', 0))
    scan_min = metrics.get('scan_nearest_m')
    if cloud_n > 0 and scan_n > 0 and scan_valid > 0:
        near_txt = f'最近障碍 {scan_min:.2f} 米' if scan_min is not None else '无近处障碍'
        items.append(('3. 雷达看障碍物', '正常',
                      f'点云 {cloud_n/dur:.1f}Hz，2D扫描 {scan_n/dur:.1f}Hz（有效射线 {scan_valid} 条，{near_txt}）',
                      '无需处理；可在雷达前放个箱子再测，看最近距离是否跟着变小'))
    elif cloud_n > 0 and scan_n == 0:
        items.append(('3. 雷达看障碍物', '故障',
                      f'3D点云有数据({cloud_n/dur:.1f}Hz)，但 2D扫描 /scan 完全没数据',
                      '九成是坐标链断了（缺少大转盘角度导致 base_link->livox_frame 没连上），先看第 6 项大转盘反馈'))
    else:
        items.append(('3. 雷达看障碍物', '故障', '没收到雷达点云 /livox/lidar',
                      '确认雷达驱动节点已启动（livox_ros_driver2），且没有别的建图窗口占用雷达'))

    # 3. 定位/里程计
    odom_n = int(metrics.get('odom_msgs', 0))
    if odom_n > 0:
        items.append(('4. 自身位置推算', '正常', f'里程计 /Odometry 正常更新（{odom_n/dur:.1f}Hz）', '无需处理'))
    else:
        items.append(('4. 自身位置推算', '故障', '里程计 /Odometry 没有输出',
                      '先确认雷达点云正常，再检查第 6 项大转盘角度有没有更新（没大转盘角度时里程计会故意拒发防算错）'))

    # 4. 避障地图与路线
    cm_n = int(metrics.get('local_costmap_msgs', 0))
    cm_lethal = int(metrics.get('local_costmap_lethal', 0))
    cm_inf = int(metrics.get('local_costmap_inflated', 0))
    plan_n = int(metrics.get('plan_msgs', 0))
    cmd_n = int(metrics.get('cmd_vel_msgs', 0))
    cmd_max_v = float(metrics.get('cmd_vel_max_v', 0.0))
    cmd_max_w = float(metrics.get('cmd_vel_max_w', 0.0))

    if cm_n > 0 and cm_lethal > 0:
        items.append(('5. 避障危险区地图', '正常',
                      f'局部避障图正常刷新（{cm_n/dur:.1f}Hz，障碍格 {cm_lethal}，安全缓冲区 {cm_inf}）',
                      '无需处理；若贴墙太近才躲，调大 inflation_radius 或核对整车长宽 footprint'))
    elif cm_n > 0 and cm_lethal == 0:
        items.append(('5. 避障危险区地图', '注意',
                      f'避障图在刷新({cm_n/dur:.1f}Hz)，但没标出任何障碍格',
                      '在雷达前 1 米放个箱子再测；若仍为 0，检查雷达高度过滤(0.1~1.0m)和 livox_source 话题名'))
    else:
        items.append(('5. 避障危险区地图', '故障', '没收到局部避障地图 /local_costmap/costmap',
                      '检查导航是否已开启（start_navigation:=true）以及定位地图/初始位姿是否已下发'))

    # 5. USB 回报（大转盘角度 + 云台姿态）
    yaw_n = int(metrics.get('big_yaw_msgs', 0))
    pose_n = int(metrics.get('autoaim_pose_msgs', 0))
    bridge = metrics.get('usb_bridge_status')
    bridge_detail = ''
    if isinstance(bridge, dict):
        bridge_detail = (
            f'；clock_sync={bridge.get("clock_sync_ok")}，'
            f'full_pose={bridge.get("full_pose")}，'
            f'robot_id={bridge.get("robot_id")}，'
            f'tx_busy={bridge.get("tx_busy_count", 0)}，'
            f'reconnect={bridge.get("reconnect_count", 0)}'
        )
    if yaw_n > 0 and pose_n > 0:
        items.append(('6. 下位机串口回报', '正常',
                      f'大转盘角度 {yaw_n/dur:.1f}Hz + 云台姿态 {pose_n/dur:.1f}Hz 均正常收到{bridge_detail}',
                      '无需处理'))
    elif yaw_n > 0 or pose_n > 0:
        items.append(('6. 下位机串口回报', '注意',
                      f'只收到一半（大转盘 {yaw_n/dur:.1f}Hz，云台姿态 {pose_n/dur:.1f}Hz）{bridge_detail}',
                      '检查下位机是否同时开启导航回报与自瞄姿态上报，以及两端协议版本是否一致(NV v1 + 0x1022)'))
    else:
        items.append(('6. 下位机串口回报', '故障', '大转盘角度和云台姿态都是 0 帧',
                      '① 确认串口桥 sentry_usb_bridge 已启动且没报错；② 确认没有别的程序抢占 /dev/ttyACM0；③ 确认 C 板已烧录带 USB 协议的固件且未停在调试断点'))

    # 6. 导航规划输出 & 自瞄目标 & 异步管家
    if cmd_n > 0:
        over = cmd_max_v > 0.36 or cmd_max_w > 0.52
        level = '注意' if over else '正常'
        tip = '速度超出首测安全上限(0.35m/s, 0.5rad/s)，请先检查限速配置！' if over else '导航正在正常发速度指令'
        items.append(('7. 导航走路指令', level,
                      f'收到 /cmd_vel {cmd_n} 帧（路线 {plan_n} 次，最高平动 {cmd_max_v:.2f}m/s、转向 {cmd_max_w:.2f}rad/s）',
                      tip))
    else:
        items.append(('7. 导航走路指令', '注意', '这几秒内没有 /cmd_vel 走路指令',
                      '如果当前没给它发目标点，这是正常的；如果发了目标点还没反应，先看第 4、5 项有没有报错'))

    target_n = int(metrics.get('autoaim_target_msgs', 0))
    target_id = int(metrics.get('autoaim_last_id', 0))
    wf = metrics.get('workflow_status')
    if target_n > 0:
        lock_txt = f'已锁定目标 #{target_id}' if target_id else '当前无目标(待机)'
        items.append(('8. 自瞄视觉输出', '正常',
                      f'/autoaim/target 正常输出（{target_n/dur:.1f}Hz，{lock_txt}）',
                      '若云台没跟着转，检查 enable_autoaim 是否为 true，以及遥控器是否拨到允许自瞄挡位'))
    else:
        items.append(('8. 自瞄视觉输出', '注意', '没收到 /autoaim/target 自瞄指令',
                      '如果还没开视觉识别程序这是正常的；如果已开视觉程序但这里是 0，说明视觉程序还没连到 /autoaim/target 话题'))

    if wf:
        nav_st = wf.get('nav_state', 'UNKNOWN')
        decoupled = wf.get('decoupled_async_ok', False)
        items.append(('9. 异步工作流管家', '正常' if decoupled else '故障',
                      f'管家在线（导航状态={nav_st}，定位在线={wf.get("odom_fresh")}，自瞄锁定={wf.get("autoaim_target_locked")}）',
                      '无需处理；导航和自瞄哪边掉线看上面对应项即可'))
    else:
        items.append(('9. 异步工作流管家', '注意', '未检测到 /sentry/async_workflow_status',
                      '启动时加上 start_async_workflow:=true 即可开启导航+自瞄并行监控'))

    return items


def print_report(items: list):
    icons = {'正常': '✅', '注意': '⚠️', '故障': '❌'}
    print('\n================ 哨兵导航 & 自瞄一键体检报告 ================')
    bad = 0
    warn = 0
    for name, level, desc, advice in items:
        if level == '故障':
            bad += 1
        elif level == '注意':
            warn += 1
        print(f'{icons.get(level, "•")} [{level}] {name}')
        print(f'    现象：{desc}')
        if level != '正常':
            print(f'    怎么查：{advice}')
    print('-------------------------------------------------------------')
    if bad == 0 and warn == 0:
        print('总结论：全部链路健康！')
    elif bad == 0:
        print(f'总结论：核心链路正常，有 {warn} 项提示（按需确认即可）。')
    else:
        print(f'总结论：发现 {bad} 项故障、{warn} 项注意。请按编号【从小到大】优先修第 1 个❌（前面通了后面通常会自动恢复）！')
    print('=============================================================\n')


def probe_live(duration: float, lidar_ip: str) -> dict:
    ping_ok = subprocess.run(
        ['ping', '-c', '1', '-W', '1', lidar_ip],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode == 0
    ttys = sorted(glob.glob('/dev/ttyACM*'))
    serial_by_id = sorted(glob.glob('/dev/serial/by-id/usb-YueLuEmbedded*'))

    import rclpy
    from geometry_msgs.msg import Twist
    from nav_msgs.msg import OccupancyGrid, Odometry, Path
    from rclpy.node import Node
    from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
    from rm_interfaces.msg import AutoAimPose, AutoAimTarget
    from sensor_msgs.msg import JointState, LaserScan, PointCloud2
    from std_msgs.msg import String

    rclpy.init()
    node = Node('sentry_health_check')
    m = {
        'duration_s': duration,
        'lidar_ping_ok': ping_ok,
        'tty_devices': ttys,
        'serial_by_id': serial_by_id,
        'cloud_msgs': 0,
        'scan_msgs': 0,
        'scan_valid_rays': 0,
        'scan_nearest_m': None,
        'odom_msgs': 0,
        'local_costmap_msgs': 0,
        'local_costmap_lethal': 0,
        'local_costmap_inflated': 0,
        'plan_msgs': 0,
        'cmd_vel_msgs': 0,
        'cmd_vel_max_v': 0.0,
        'cmd_vel_max_w': 0.0,
        'big_yaw_msgs': 0,
        'autoaim_pose_msgs': 0,
        'autoaim_target_msgs': 0,
        'autoaim_last_id': 0,
        'workflow_status': None,
        'usb_bridge_status': None,
    }

    best_effort = QoSProfile(
        depth=5, reliability=ReliabilityPolicy.BEST_EFFORT, durability=DurabilityPolicy.VOLATILE
    )

    def on_scan(msg: LaserScan):
        m['scan_msgs'] += 1
        valid = 0
        best = None
        for r in msg.ranges:
            if math.isfinite(r) and msg.range_min <= r <= msg.range_max:
                valid += 1
                if best is None or r < best:
                    best = r
        m['scan_valid_rays'] = valid
        if best is not None:
            m['scan_nearest_m'] = round(best, 3)

    def on_cm(msg: OccupancyGrid):
        m['local_costmap_msgs'] += 1
        m['local_costmap_lethal'] = sum(1 for v in msg.data if v >= 99)
        m['local_costmap_inflated'] = sum(1 for v in msg.data if 50 <= v < 99)

    def on_cmd(msg: Twist):
        m['cmd_vel_msgs'] += 1
        m['cmd_vel_max_v'] = max(m['cmd_vel_max_v'], abs(msg.linear.x), abs(msg.linear.y))
        m['cmd_vel_max_w'] = max(m['cmd_vel_max_w'], abs(msg.angular.z))

    def on_target(msg: AutoAimTarget):
        m['autoaim_target_msgs'] += 1
        m['autoaim_last_id'] = int(msg.target_id)

    def on_wf(msg: String):
        try:
            m['workflow_status'] = json.loads(msg.data)
        except Exception:
            pass

    def on_bridge(msg: String):
        try:
            m['usb_bridge_status'] = json.loads(msg.data)
        except Exception:
            pass

    node.create_subscription(PointCloud2, '/livox/lidar', lambda _: m.__setitem__('cloud_msgs', m['cloud_msgs'] + 1), best_effort)
    node.create_subscription(LaserScan, '/scan', on_scan, 10)
    node.create_subscription(Odometry, '/Odometry', lambda _: m.__setitem__('odom_msgs', m['odom_msgs'] + 1), 10)
    node.create_subscription(OccupancyGrid, '/local_costmap/costmap', on_cm, 5)
    node.create_subscription(Path, '/plan', lambda _: m.__setitem__('plan_msgs', m['plan_msgs'] + 1), 5)
    node.create_subscription(Twist, '/cmd_vel', on_cmd, 10)
    node.create_subscription(JointState, '/big_yaw/joint_states', lambda _: m.__setitem__('big_yaw_msgs', m['big_yaw_msgs'] + 1), 10)
    node.create_subscription(AutoAimPose, '/autoaim/pose', lambda _: m.__setitem__('autoaim_pose_msgs', m['autoaim_pose_msgs'] + 1), 10)
    node.create_subscription(AutoAimTarget, '/autoaim/target', on_target, 10)
    node.create_subscription(String, '/sentry/async_workflow_status', on_wf, 10)
    node.create_subscription(String, '/sentry/usb_bridge_status', on_bridge, 10)

    end = time.monotonic() + duration
    try:
        while time.monotonic() < end:
            rclpy.spin_once(node, timeout_sec=0.05)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return m


def self_test():
    # 验证全链路正常与典型故障（串口无回报导致坐标链断开）两种场景的诊断输出
    ok_metrics = {
        'duration_s': 2.0, 'lidar_ping_ok': True, 'tty_devices': ['/dev/ttyACM0'],
        'cloud_msgs': 20, 'scan_msgs': 20, 'scan_valid_rays': 650, 'scan_nearest_m': 0.85,
        'odom_msgs': 20, 'local_costmap_msgs': 20, 'local_costmap_lethal': 1200, 'local_costmap_inflated': 800,
        'plan_msgs': 2, 'cmd_vel_msgs': 20, 'cmd_vel_max_v': 0.25, 'cmd_vel_max_w': 0.3,
        'big_yaw_msgs': 100, 'autoaim_pose_msgs': 200, 'autoaim_target_msgs': 50, 'autoaim_last_id': 3,
        'workflow_status': {'nav_state': 'NAVIGATING', 'odom_fresh': True, 'autoaim_target_locked': True, 'decoupled_async_ok': True},
    }
    res_ok = diagnose(ok_metrics)
    assert all(level == '正常' for _, level, _, _ in res_ok), res_ok

    broken_usb = dict(ok_metrics, scan_msgs=0, odom_msgs=0, big_yaw_msgs=0, autoaim_pose_msgs=0)
    res_bad = dict((name, level) for name, level, _, _ in diagnose(broken_usb))
    assert res_bad['3. 雷达看障碍物'] == '故障'
    assert res_bad['4. 自身位置推算'] == '故障'
    assert res_bad['6. 下位机串口回报'] == '故障'
    print('SELF_TEST_PASS: 9/9 diagnostic checks verified')


def main():
    parser = argparse.ArgumentParser(description='哨兵导航与自瞄一键体检工具')
    parser.add_argument('--duration', type=float, default=3.0, help='采样时长(秒)，默认 3.0')
    parser.add_argument('--lidar-ip', default='192.168.1.144', help='雷达 IP')
    parser.add_argument('--json', action='store_true', help='输出 JSON 格式')
    parser.add_argument('--self-test', action='store_true', help='离线自检诊断规则')
    args, _ = parser.parse_known_args()

    if args.self_test:
        self_test()
        return

    metrics = probe_live(args.duration, args.lidar_ip)
    items = diagnose(metrics)
    if args.json:
        print(json.dumps({'metrics': metrics, 'diagnosis': [
            {'item': n, 'level': l, 'summary': d, 'advice': a} for n, l, d, a in items
        ]}, ensure_ascii=False, indent=2))
    else:
        print_report(items)


if __name__ == '__main__':
    main()
