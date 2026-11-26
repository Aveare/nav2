#!/usr/bin/env python3
"""Real-time MID-360S lidar obstacle detection checker (/livox/lidar, /scan, /local_costmap/costmap)."""
import argparse
import json
import math
import struct
import time

import rclpy
from nav_msgs.msg import OccupancyGrid
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from sensor_msgs.msg import LaserScan, PointCloud2


class LidarObstacleChecker(Node):
    def __init__(self, cloud_topic: str, scan_topic: str, costmap_topic: str,
                 min_z: float, max_z: float, min_r: float, max_r: float, alert_r: float):
        super().__init__('lidar_obstacle_checker')
        self.min_z, self.max_z = min_z, max_z
        self.min_r, self.max_r, self.alert_r = min_r, max_r, alert_r

        self.cloud_msgs = 0
        self.cloud_points_in_band = 0
        self.cloud_nearest = None

        self.scan_msgs = 0
        self.scan_valid = 0
        self.scan_in_alert = 0
        self.scan_nearest = None

        self.costmap_msgs = 0
        self.costmap_lethal = 0
        self.costmap_inflated = 0
        self.costmap_size = (0, 0)

        sensor_qos = QoSProfile(
            depth=5,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
        )
        reliable_qos = QoSProfile(
            depth=5,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
        )
        self.create_subscription(PointCloud2, cloud_topic, self._on_cloud, sensor_qos)
        self.create_subscription(LaserScan, scan_topic, self._on_scan, reliable_qos)
        self.create_subscription(OccupancyGrid, costmap_topic, self._on_costmap, reliable_qos)

    def _on_cloud(self, msg: PointCloud2):
        self.cloud_msgs += 1
        offsets = {f.name: f.offset for f in msg.fields}
        if not ({'x', 'y', 'z'} <= offsets.keys()):
            return
        ox, oy, oz = offsets['x'], offsets['y'], offsets['z']
        step = msg.point_step
        data = msg.data
        in_band = 0
        best = None
        for base in range(0, len(data) - step + 1, step):
            x = struct.unpack_from('<f', data, base + ox)[0]
            y = struct.unpack_from('<f', data, base + oy)[0]
            z = struct.unpack_from('<f', data, base + oz)[0]
            if not (math.isfinite(x) and math.isfinite(y) and math.isfinite(z)):
                continue
            # Check raw sensor band or upright band symmetrically so both mounted/handheld frames report hits
            r = math.hypot(x, y)
            if self.min_r <= r <= self.max_r and self.min_z <= abs(z) <= self.max_z:
                in_band += 1
                if best is None or r < best[0]:
                    best = (r, math.degrees(math.atan2(y, x)), x, y, z)
        self.cloud_points_in_band = in_band
        self.cloud_nearest = best

    def _on_scan(self, msg: LaserScan):
        self.scan_msgs += 1
        valid = 0
        in_alert = 0
        best = None
        for idx, r in enumerate(msg.ranges):
            if not math.isfinite(r) or r < msg.range_min or r > msg.range_max:
                continue
            valid += 1
            ang = math.degrees(msg.angle_min + idx * msg.angle_increment)
            if r <= self.alert_r:
                in_alert += 1
            if best is None or r < best[0]:
                best = (r, ang)
        self.scan_valid = valid
        self.scan_in_alert = in_alert
        self.scan_nearest = best

    def _on_costmap(self, msg: OccupancyGrid):
        self.costmap_msgs += 1
        self.costmap_size = (msg.info.width, msg.info.height)
        lethal = 0
        inflated = 0
        for v in msg.data:
            if v >= 99:
                lethal += 1
            elif v >= 50:
                inflated += 1
        self.costmap_lethal = lethal
        self.costmap_inflated = inflated

    def summary(self):
        return {
            'cloud_msgs': self.cloud_msgs,
            'cloud_points_in_band': self.cloud_points_in_band,
            'cloud_nearest': (
                {
                    'range_m': round(self.cloud_nearest[0], 3),
                    'angle_deg': round(self.cloud_nearest[1], 1),
                    'xyz': [round(v, 3) for v in self.cloud_nearest[2:]],
                }
                if self.cloud_nearest
                else None
            ),
            'scan_msgs': self.scan_msgs,
            'scan_valid_rays': self.scan_valid,
            'scan_alert_rays': self.scan_in_alert,
            'scan_nearest': (
                {
                    'range_m': round(self.scan_nearest[0], 3),
                    'angle_deg': round(self.scan_nearest[1], 1),
                }
                if self.scan_nearest
                else None
            ),
            'costmap_msgs': self.costmap_msgs,
            'costmap_size': list(self.costmap_size),
            'costmap_lethal_cells': self.costmap_lethal,
            'costmap_inflated_cells': self.costmap_inflated,
            'obstacle_detected': bool(
                (self.scan_msgs > 0 and self.scan_valid > 0)
                or (self.cloud_msgs > 0 and self.cloud_points_in_band > 0)
                or (self.costmap_msgs > 0 and self.costmap_lethal > 0)
            ),
        }


def main():
    parser = argparse.ArgumentParser(description='Check lidar obstacle detection chain')
    parser.add_argument('--cloud-topic', default='/livox/lidar')
    parser.add_argument('--scan-topic', default='/scan')
    parser.add_argument('--costmap-topic', default='/local_costmap/costmap')
    parser.add_argument('--duration', type=float, default=3.0)
    parser.add_argument('--alert-range', type=float, default=2.0)
    args, _ = parser.parse_known_args()

    rclpy.init()
    node = LidarObstacleChecker(
        args.cloud_topic,
        args.scan_topic,
        args.costmap_topic,
        min_z=0.05,
        max_z=1.2,
        min_r=0.3,
        max_r=8.0,
        alert_r=args.alert_range,
    )
    end = time.monotonic() + args.duration
    try:
        while time.monotonic() < end:
            rclpy.spin_once(node, timeout_sec=0.05)
        result = node.summary()
        print(json.dumps(result, ensure_ascii=False, indent=2))
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
