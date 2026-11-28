#!/usr/bin/env python3
"""Automatically publish AMCL /initialpose from saved red/blue spawn CSVs."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import rclpy
from geometry_msgs.msg import PoseWithCovarianceStamped
from rclpy.node import Node
from rm_interfaces.msg import AutoAimPose

try:
    from center_point_file import CenterPoint, load_center_point
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools" / "center_point"))
    from center_point_file import CenterPoint, load_center_point

DEFAULT_COV_XY = 0.25
DEFAULT_COV_YAW = 0.06853891909122467


def resolve_spawn_side(configured_side: str, robot_id: int | None) -> str | None:
    side = (configured_side or "auto").strip().lower()
    if side in ("none", "red", "blue"):
        return side
    if side != "auto":
        raise ValueError(f"Unsupported spawn_side: {configured_side!r}")
    if robot_id is None or robot_id <= 0:
        return None
    if 1 <= robot_id < 100:
        return "red"
    if 101 <= robot_id < 200:
        return "blue"
    return None


def default_spawn_paths(map_path: str) -> tuple[str, str]:
    base, _ = os.path.splitext(map_path) if map_path else ("/home/rm/maps/test", ".yaml")
    return f"{base}_red_spawn.csv", f"{base}_blue_spawn.csv"


def build_initial_pose_message(
    point: CenterPoint,
    stamp,
    frame_id: str = "map",
    cov_xy: float = DEFAULT_COV_XY,
    cov_yaw: float = DEFAULT_COV_YAW,
) -> PoseWithCovarianceStamped:
    validated = point.validated()
    message = PoseWithCovarianceStamped()
    message.header.stamp = stamp
    message.header.frame_id = frame_id
    message.pose.pose.position.x = validated.x
    message.pose.pose.position.y = validated.y
    message.pose.pose.position.z = 0.0
    message.pose.pose.orientation.x = validated.qx
    message.pose.pose.orientation.y = validated.qy
    message.pose.pose.orientation.z = validated.qz
    message.pose.pose.orientation.w = validated.qw
    covariance = [0.0] * 36
    covariance[0] = float(cov_xy)
    covariance[7] = float(cov_xy)
    covariance[35] = float(cov_yaw)
    message.pose.covariance = covariance
    return message


class SpawnInitialPose(Node):
    def __init__(self, **node_kwargs) -> None:
        super().__init__("spawn_initial_pose", **node_kwargs)
        self.spawn_side = str(self.declare_parameter("spawn_side", "auto").value)
        self.red_spawn_file = str(
            self.declare_parameter("red_spawn_file", "/home/rm/maps/test_red_spawn.csv").value
        )
        self.blue_spawn_file = str(
            self.declare_parameter("blue_spawn_file", "/home/rm/maps/test_blue_spawn.csv").value
        )
        self.pose_topic = str(self.declare_parameter("pose_topic", "/autoaim/pose").value)
        self.initialpose_topic = str(
            self.declare_parameter("initialpose_topic", "/initialpose").value
        )
        self.amcl_pose_topic = str(self.declare_parameter("amcl_pose_topic", "/amcl_pose").value)
        self.frame_id = str(self.declare_parameter("frame_id", "map").value)
        self.publish_period_sec = max(
            0.1, float(self.declare_parameter("publish_period_sec", 0.5).value)
        )
        self.max_attempts = max(1, int(self.declare_parameter("max_attempts", 10).value))
        self.cov_xy = float(self.declare_parameter("cov_xy", DEFAULT_COV_XY).value)
        self.cov_yaw = float(self.declare_parameter("cov_yaw", DEFAULT_COV_YAW).value)

        self.latest_robot_id: int | None = None
        self.resolved_side: str | None = None
        self.target_point: CenterPoint | None = None
        self.attempts = 0
        self.amcl_pose_count = 0
        self.amcl_pose_count_before_pub = 0
        self.finished = False

        self.pub = self.create_publisher(PoseWithCovarianceStamped, self.initialpose_topic, 10)
        self.create_subscription(AutoAimPose, self.pose_topic, self._on_pose, 10)
        self.create_subscription(
            PoseWithCovarianceStamped, self.amcl_pose_topic, self._on_amcl_pose, 10
        )
        self.create_subscription(
            PoseWithCovarianceStamped, self.initialpose_topic, self._on_external_initialpose, 10
        )
        self.timer = self.create_timer(self.publish_period_sec, self.step)

    def _on_pose(self, msg: AutoAimPose) -> None:
        self.latest_robot_id = int(msg.robot_id)

    def _on_amcl_pose(self, _msg: PoseWithCovarianceStamped) -> None:
        self.amcl_pose_count += 1
        if self.attempts > 0 and self.amcl_pose_count > self.amcl_pose_count_before_pub:
            if not self.finished:
                self.get_logger().info("AMCL confirmed initial pose; auto spawn init complete.")
            self.finished = True

    def _on_external_initialpose(self, _msg: PoseWithCovarianceStamped) -> None:
        if self.attempts == 0 and not self.finished:
            self.get_logger().info("External /initialpose detected; skipping auto spawn init.")
            self.finished = True

    def step(self) -> None:
        if self.finished:
            return

        if self.resolved_side is None:
            try:
                side = resolve_spawn_side(self.spawn_side, self.latest_robot_id)
            except ValueError as error:
                self.get_logger().error(str(error))
                self.finished = True
                return
            if side is None:
                return
            self.resolved_side = side
            if side == "none":
                self.finished = True
                return

            spawn_path = Path(self.red_spawn_file if side == "red" else self.blue_spawn_file)
            if not spawn_path.is_file():
                self.get_logger().warning(
                    f"Spawn file not found for {side}: {spawn_path}; skipping auto initialpose."
                )
                self.finished = True
                return
            try:
                self.target_point = load_center_point(spawn_path)
            except Exception as error:
                self.get_logger().error(f"Failed to load spawn file {spawn_path}: {error}")
                self.finished = True
                return
            self.get_logger().info(
                f"Resolved side={side} (robot_id={self.latest_robot_id}) from {spawn_path}: "
                f"x={self.target_point.x:.3f}, y={self.target_point.y:.3f}"
            )

        if self.target_point is None:
            self.finished = True
            return

        if self.count_subscribers(self.initialpose_topic) == 0:
            return

        if self.attempts == 0:
            self.amcl_pose_count_before_pub = self.amcl_pose_count
        elif self.amcl_pose_count > self.amcl_pose_count_before_pub:
            self.finished = True
            return

        if self.attempts >= self.max_attempts:
            self.get_logger().info("Reached max auto /initialpose attempts; stopping.")
            self.finished = True
            return

        msg = build_initial_pose_message(
            self.target_point,
            stamp=self.get_clock().now().to_msg(),
            frame_id=self.frame_id,
            cov_xy=self.cov_xy,
            cov_yaw=self.cov_yaw,
        )
        self.pub.publish(msg)
        self.attempts += 1
        self.get_logger().info(
            f"Published auto /initialpose ({self.attempts}/{self.max_attempts}, side={self.resolved_side})"
        )


def main() -> int:
    rclpy.init()
    node = SpawnInitialPose()
    try:
        while rclpy.ok() and not node.finished:
            rclpy.spin_once(node, timeout_sec=0.1)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
