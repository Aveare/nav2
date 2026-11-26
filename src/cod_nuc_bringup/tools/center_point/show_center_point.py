#!/usr/bin/env python3
"""Display a saved field center as RViz markers without sending a Nav2 goal."""

from __future__ import annotations

import argparse
import time

import rclpy
from geometry_msgs.msg import PoseStamped
from rclpy.duration import Duration
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from visualization_msgs.msg import Marker, MarkerArray

from center_point_file import CenterPoint, load_center_point


class CenterPointDisplay(Node):
    def __init__(self, arguments: argparse.Namespace, point: CenterPoint) -> None:
        super().__init__("field_center_display")
        self.arguments = arguments
        self.point = point
        qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.marker_publisher = self.create_publisher(MarkerArray, arguments.topic, qos)
        self.pose_publisher = self.create_publisher(PoseStamped, "/field_center_pose", qos)
        self.timer = self.create_timer(1.0, self.publish)
        self.publish()
        self.get_logger().info(
            "中心点显示中: x=%.3f, y=%.3f, topic=%s（不会发送导航目标）"
            % (point.x, point.y, arguments.topic)
        )

    def _base_marker(self, marker_id: int, marker_type: int) -> Marker:
        marker = Marker()
        marker.header.frame_id = self.arguments.frame_id
        marker.header.stamp = self.get_clock().now().to_msg()
        marker.ns = "field_center"
        marker.id = marker_id
        marker.type = marker_type
        marker.action = Marker.ADD
        marker.lifetime = Duration(seconds=0.0).to_msg()
        return marker

    def publish(self) -> None:
        arrow = self._base_marker(100000, Marker.ARROW)
        arrow.pose.position.x = self.point.x
        arrow.pose.position.y = self.point.y
        arrow.pose.position.z = self.point.z + 0.05
        arrow.pose.orientation.x = self.point.qx
        arrow.pose.orientation.y = self.point.qy
        arrow.pose.orientation.z = self.point.qz
        arrow.pose.orientation.w = self.point.qw
        arrow.scale.x = 0.7
        arrow.scale.y = 0.16
        arrow.scale.z = 0.16
        arrow.color.r = 1.0
        arrow.color.g = 0.45
        arrow.color.b = 0.0
        arrow.color.a = 1.0

        label = self._base_marker(100001, Marker.TEXT_VIEW_FACING)
        label.pose.position.x = self.point.x
        label.pose.position.y = self.point.y
        label.pose.position.z = self.point.z + 0.45
        label.pose.orientation.w = 1.0
        label.scale.z = 0.30
        label.color.r = 1.0
        label.color.g = 0.85
        label.color.b = 0.1
        label.color.a = 1.0
        label.text = self.arguments.label

        self.marker_publisher.publish(MarkerArray(markers=[arrow, label]))

        pose = PoseStamped()
        pose.header.frame_id = self.arguments.frame_id
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = self.point.x
        pose.pose.position.y = self.point.y
        pose.pose.position.z = self.point.z
        pose.pose.orientation.x = self.point.qx
        pose.pose.orientation.y = self.point.qy
        pose.pose.orientation.z = self.point.qz
        pose.pose.orientation.w = self.point.qw
        self.pose_publisher.publish(pose)


def parse_arguments() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description="Show a saved field center on RViz without starting navigation."
    )
    parser.add_argument("--input", default="/home/rm/maps/test_center.csv")
    parser.add_argument("--frame-id", default="map")
    parser.add_argument("--topic", default="/waypoints")
    parser.add_argument("--label", default="CENTER")
    parser.add_argument(
        "--duration-sec", type=float, default=0.0, help="0 keeps publishing until Ctrl+C"
    )
    return parser.parse_known_args()


def main() -> int:
    arguments, ros_arguments = parse_arguments()
    if arguments.duration_sec < 0:
        raise SystemExit("--duration-sec 不能为负数")
    point = load_center_point(arguments.input)
    rclpy.init(args=ros_arguments)
    node = CenterPointDisplay(arguments, point)
    started_at = time.monotonic()
    try:
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.2)
            if arguments.duration_sec and time.monotonic() - started_at >= arguments.duration_sec:
                break
        return 0
    except KeyboardInterrupt:
        return 0
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
