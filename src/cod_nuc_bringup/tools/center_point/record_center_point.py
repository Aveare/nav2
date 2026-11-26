#!/usr/bin/env python3
"""Record one RViz Publish Point click as a field center waypoint CSV."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import rclpy
from geometry_msgs.msg import PointStamped
from rclpy.node import Node

from center_point_file import CenterPoint, quaternion_from_yaw_degrees, save_center_point


class CenterPointRecorder(Node):
    def __init__(self, arguments: argparse.Namespace) -> None:
        super().__init__("field_center_recorder")
        self.arguments = arguments
        self.saved = False
        self.subscription = self.create_subscription(
            PointStamped, arguments.topic, self._on_point, 10
        )
        self.get_logger().info(
            f"等待 RViz Publish Point: topic={arguments.topic}, frame={arguments.frame_id}"
        )

    def _on_point(self, message: PointStamped) -> None:
        if self.saved:
            return
        if message.header.frame_id != self.arguments.frame_id:
            self.get_logger().warning(
                "忽略 frame_id=%s 的点；需要 frame_id=%s"
                % (message.header.frame_id or "<empty>", self.arguments.frame_id)
            )
            return
        qx, qy, qz, qw = quaternion_from_yaw_degrees(self.arguments.yaw_deg)
        point = CenterPoint(
            x=message.point.x,
            y=message.point.y,
            z=message.point.z,
            qx=qx,
            qy=qy,
            qz=qz,
            qw=qw,
        )
        try:
            destination = save_center_point(
                self.arguments.output, point, overwrite=self.arguments.force
            )
        except Exception as error:  # The message must remain visible in the field terminal.
            self.get_logger().error(f"中心点保存失败: {error}")
            self.saved = True
            self.exit_code = 1
            return
        self.get_logger().info(
            "中心点已保存: %s; x=%.3f, y=%.3f, yaw=%.1f deg"
            % (destination, point.x, point.y, self.arguments.yaw_deg)
        )
        self.saved = True
        self.exit_code = 0


def parse_arguments() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description="Save one /clicked_point message as a field center CSV."
    )
    parser.add_argument("--output", default="/home/rm/maps/test_center.csv")
    parser.add_argument("--topic", default="/clicked_point")
    parser.add_argument("--frame-id", default="map")
    parser.add_argument("--yaw-deg", type=float, default=0.0)
    parser.add_argument("--timeout-sec", type=float, default=0.0)
    parser.add_argument(
        "--force", action="store_true", help="replace an existing center CSV"
    )
    return parser.parse_known_args()


def main() -> int:
    arguments, ros_arguments = parse_arguments()
    output = Path(arguments.output).expanduser()
    if output.exists() and not arguments.force:
        print(
            f"拒绝覆盖已有中心点: {output}\n"
            "确认重新标定时再加 --force。",
            file=sys.stderr,
        )
        return 2
    if arguments.timeout_sec < 0:
        print("--timeout-sec 不能为负数", file=sys.stderr)
        return 2

    rclpy.init(args=ros_arguments)
    node = CenterPointRecorder(arguments)
    started_at = time.monotonic()
    try:
        while rclpy.ok() and not node.saved:
            rclpy.spin_once(node, timeout_sec=0.2)
            if arguments.timeout_sec and time.monotonic() - started_at >= arguments.timeout_sec:
                node.get_logger().error("等待 RViz 点击超时，未修改中心点文件")
                return 3
        return getattr(node, "exit_code", 1)
    except KeyboardInterrupt:
        node.get_logger().info("已取消，未修改中心点文件")
        return 130
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
