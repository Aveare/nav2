#!/usr/bin/env python3
"""Asynchronous Auto-Aim + Navigation workflow coordinator.

Decouples Nav2 chassis navigation/obstacle avoidance from auto-aim gimbal tracking
using separate ReentrantCallbackGroups on a MultiThreadedExecutor so neither pipeline
can block the other, while sharing a single USB CDC owner (sentry_usb_bridge.py).
"""
import csv
import json
import math
import pathlib
import threading
import time

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped, Twist
from nav2_msgs.action import NavigateToPose
from nav_msgs.msg import Odometry
from rclpy.action import ActionClient
from rclpy.callback_groups import ReentrantCallbackGroup
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node
from rm_interfaces.msg import AutoAimPose, AutoAimTarget
from sensor_msgs.msg import JointState
from std_msgs.msg import String


class SentryAsyncWorkflow(Node):
    def __init__(self):
        super().__init__('sentry_async_workflow')
        self.waypoints_file = str(self.declare_parameter('waypoints_file', '').value)
        self.auto_patrol = bool(self.declare_parameter('auto_patrol', False).value)
        self.patrol_loop = bool(self.declare_parameter('patrol_loop', True).value)
        self.autoaim_timeout_s = max(0.05, float(self.declare_parameter('autoaim_timeout_s', 0.15).value))
        self.nav_server_name = str(self.declare_parameter('nav_action_name', 'navigate_to_pose').value)

        # Independent callback groups so auto-aim callbacks never wait on Nav2 action callbacks.
        self.nav_cb_group = ReentrantCallbackGroup()
        self.aim_cb_group = ReentrantCallbackGroup()
        self.status_cb_group = ReentrantCallbackGroup()

        self._lock = threading.Lock()
        self._now = time.monotonic

        # Navigation async state
        self.nav_client = ActionClient(
            self, NavigateToPose, self.nav_server_name, callback_group=self.nav_cb_group
        )
        self.nav_state = 'IDLE'
        self.nav_goal_handle = None
        self.nav_distance_remaining = None
        self.nav_goals_sent = 0
        self.nav_goals_succeeded = 0
        self.nav_goals_failed = 0
        self.waypoints = self._load_waypoints(self.waypoints_file) if self.waypoints_file else []
        self.waypoint_idx = 0

        # Telemetry freshness counters
        self.odom_count = 0
        self.odom_at = float('-inf')
        self.cmd_vel_count = 0
        self.cmd_vel_at = float('-inf')
        self.last_cmd_vel = (0.0, 0.0, 0.0)
        self.big_yaw_count = 0
        self.big_yaw_at = float('-inf')
        self.big_yaw_rad = 0.0

        # Auto-aim async state
        self.pose_count = 0
        self.pose_at = float('-inf')
        self.last_pose_euler = (0.0, 0.0, 0.0)
        self.last_pose_full = False
        self.last_pose_valid_flags = 0
        self.last_pose_mode = 0
        self.last_robot_id = 0
        self.last_projectile_speed = 0.0
        self.bridge_status = None
        self.bridge_status_at = float('-inf')
        self.target_count = 0
        self.target_at = float('-inf')
        self.target_id = 0
        self.target_shoot = 0
        self.target_distance = 0.0

        self.start_t = self._now()

        # Navigation subscriptions
        self.create_subscription(
            Odometry, '/Odometry', self._on_odom, 10, callback_group=self.nav_cb_group
        )
        self.create_subscription(
            Twist, '/cmd_vel', self._on_cmd_vel, 10, callback_group=self.nav_cb_group
        )
        self.create_subscription(
            JointState, '/big_yaw/joint_states', self._on_big_yaw, 10, callback_group=self.nav_cb_group
        )
        self.create_subscription(
            PoseStamped, '/sentry/nav_goal', self._on_external_goal, 5, callback_group=self.nav_cb_group
        )

        # Auto-aim subscriptions (isolated callback group)
        self.create_subscription(
            AutoAimPose, '/autoaim/pose', self._on_autoaim_pose, 20, callback_group=self.aim_cb_group
        )
        self.create_subscription(
            AutoAimTarget, '/autoaim/target', self._on_autoaim_target, 20, callback_group=self.aim_cb_group
        )
        self.create_subscription(
            String, '/sentry/usb_bridge_status', self._on_bridge_status, 10, callback_group=self.status_cb_group
        )

        # Diagnostics & patrol timers
        self.status_pub = self.create_publisher(String, '/sentry/async_workflow_status', 10)
        self.create_timer(0.2, self._publish_status, callback_group=self.status_cb_group)
        self.create_timer(0.5, self._patrol_tick, callback_group=self.nav_cb_group)

    @staticmethod
    def _load_waypoints(path_str: str):
        p = pathlib.Path(path_str)
        if not p.is_file():
            return []
        points = []
        with p.open('r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    points.append((
                        float(row['pose_x']),
                        float(row['pose_y']),
                        float(row.get('rot_z', 0.0)),
                        float(row.get('rot_w', 1.0)),
                    ))
                except (KeyError, ValueError):
                    continue
        return points

    def _on_odom(self, msg: Odometry):
        with self._lock:
            self.odom_count += 1
            self.odom_at = self._now()

    def _on_cmd_vel(self, msg: Twist):
        with self._lock:
            self.cmd_vel_count += 1
            self.cmd_vel_at = self._now()
            self.last_cmd_vel = (
                round(float(msg.linear.x), 3),
                round(float(msg.linear.y), 3),
                round(float(msg.angular.z), 3),
            )

    def _on_big_yaw(self, msg: JointState):
        with self._lock:
            self.big_yaw_count += 1
            self.big_yaw_at = self._now()
            if msg.position:
                self.big_yaw_rad = float(msg.position[0])

    def _on_autoaim_pose(self, msg: AutoAimPose):
        with self._lock:
            self.pose_count += 1
            self.pose_at = self._now()
            self.last_pose_euler = (
                round(float(msg.yaw), 2),
                round(float(msg.pitch), 2),
                round(float(msg.roll), 2),
            )
            self.last_pose_full = bool(msg.full_pose)
            self.last_pose_valid_flags = int(msg.valid_flags)
            self.last_pose_mode = int(msg.mode)
            self.last_robot_id = int(msg.robot_id)
            self.last_projectile_speed = round(float(msg.projectile_speed), 2)

    def _on_bridge_status(self, msg: String):
        try:
            parsed = json.loads(msg.data)
        except Exception:
            return
        with self._lock:
            self.bridge_status = parsed
            self.bridge_status_at = self._now()

    def _on_autoaim_target(self, msg: AutoAimTarget):
        with self._lock:
            self.target_count += 1
            self.target_at = self._now()
            self.target_id = int(msg.target_id)
            self.target_shoot = int(msg.shoot)
            self.target_distance = round(float(msg.distance), 3)

    def _on_external_goal(self, pose_msg: PoseStamped):
        self._send_nav_goal_async(pose_msg)

    def _patrol_tick(self):
        if not self.auto_patrol or not self.waypoints:
            return
        with self._lock:
            busy = self.nav_state in ('SENDING', 'NAVIGATING')
        if busy:
            return
        if not self.nav_client.server_is_ready():
            return
        if self.waypoint_idx >= len(self.waypoints):
            if not self.patrol_loop:
                return
            self.waypoint_idx = 0
        x, y, rz, rw = self.waypoints[self.waypoint_idx]
        self.waypoint_idx += 1
        pose = PoseStamped()
        pose.header.frame_id = 'map'
        pose.header.stamp = self.get_clock().now().to_msg()
        pose.pose.position.x = x
        pose.pose.position.y = y
        pose.pose.orientation.z = rz
        pose.pose.orientation.w = rw
        self._send_nav_goal_async(pose)

    def _send_nav_goal_async(self, pose_msg: PoseStamped):
        if not self.nav_client.server_is_ready():
            self.get_logger().warning('Nav2 action server not ready; skipping goal', throttle_duration_sec=2.0)
            return
        goal = NavigateToPose.Goal()
        goal.pose = pose_msg
        if not goal.pose.header.frame_id:
            goal.pose.header.frame_id = 'map'
        with self._lock:
            self.nav_state = 'SENDING'
            self.nav_goals_sent += 1
            self.nav_distance_remaining = None
        future = self.nav_client.send_goal_async(goal, feedback_callback=self._on_nav_feedback)
        future.add_done_callback(self._on_goal_response)

    def _on_goal_response(self, future):
        try:
            handle = future.result()
        except Exception as exc:
            with self._lock:
                self.nav_state = 'ERROR'
                self.nav_goals_failed += 1
            self.get_logger().error(f'Nav2 goal send exception: {exc}')
            return
        if not handle.accepted:
            with self._lock:
                self.nav_state = 'REJECTED'
                self.nav_goals_failed += 1
            return
        with self._lock:
            self.nav_goal_handle = handle
            self.nav_state = 'NAVIGATING'
        result_future = handle.get_result_async()
        result_future.add_done_callback(self._on_nav_result)

    def _on_nav_feedback(self, feedback_msg):
        dist = getattr(feedback_msg.feedback, 'distance_remaining', None)
        if dist is not None and math.isfinite(dist):
            with self._lock:
                self.nav_distance_remaining = round(float(dist), 3)

    def _on_nav_result(self, future):
        try:
            status = future.result().status
        except Exception:
            status = GoalStatus.STATUS_ABORTED
        with self._lock:
            self.nav_goal_handle = None
            if status == GoalStatus.STATUS_SUCCEEDED:
                self.nav_state = 'SUCCEEDED'
                self.nav_goals_succeeded += 1
            elif status == GoalStatus.STATUS_CANCELED:
                self.nav_state = 'CANCELED'
            else:
                self.nav_state = 'ABORTED'
                self.nav_goals_failed += 1

    def _publish_status(self):
        now = self._now()
        elapsed = max(1e-3, now - self.start_t)
        with self._lock:
            aim_pose_fresh = (now - self.pose_at) <= 0.5
            aim_target_fresh = (now - self.target_at) <= self.autoaim_timeout_s
            odom_fresh = (now - self.odom_at) <= 0.5
            cmd_fresh = (now - self.cmd_vel_at) <= 0.5
            big_yaw_fresh = (now - self.big_yaw_at) <= 0.5
            bridge_fresh = (now - self.bridge_status_at) <= 1.5
            bridge = self.bridge_status if (bridge_fresh and isinstance(self.bridge_status, dict)) else None
            payload = {
                'nav_state': self.nav_state,
                'nav_server_ready': self.nav_client.server_is_ready(),
                'nav_goals_sent': self.nav_goals_sent,
                'nav_goals_succeeded': self.nav_goals_succeeded,
                'nav_goals_failed': self.nav_goals_failed,
                'nav_distance_remaining': self.nav_distance_remaining,
                'odom_fresh': odom_fresh,
                'odom_count': self.odom_count,
                'odom_hz': round(self.odom_count / elapsed, 1),
                'cmd_vel_fresh': cmd_fresh,
                'cmd_vel_count': self.cmd_vel_count,
                'last_cmd_vel': list(self.last_cmd_vel),
                'big_yaw_fresh': big_yaw_fresh,
                'big_yaw_rad': round(self.big_yaw_rad, 4),
                'autoaim_pose_fresh': aim_pose_fresh,
                'autoaim_pose_count': self.pose_count,
                'autoaim_pose_hz': round(self.pose_count / elapsed, 1),
                'autoaim_pose_deg': list(self.last_pose_euler),
                'autoaim_full_pose': self.last_pose_full,
                'autoaim_valid_flags': self.last_pose_valid_flags,
                'autoaim_mode': self.last_pose_mode,
                'robot_id': self.last_robot_id,
                'projectile_speed': self.last_projectile_speed,
                'usb_bridge_fresh': bridge_fresh,
                'usb_bridge_connected': bool(bridge.get('connected')) if bridge else None,
                'usb_drive_valid': bool(bridge.get('drive_valid')) if bridge else None,
                'usb_clock_sync_ok': bool(bridge.get('clock_sync_ok')) if bridge else None,
                'usb_reconnect_count': bridge.get('reconnect_count') if bridge else None,
                'usb_tx_busy_count': bridge.get('tx_busy_count') if bridge else None,
                'autoaim_target_fresh': aim_target_fresh,
                'autoaim_target_count': self.target_count,
                'autoaim_target_locked': bool(aim_target_fresh and self.target_id != 0),
                'autoaim_target_id': self.target_id if aim_target_fresh else 0,
                'autoaim_target_shoot': self.target_shoot if aim_target_fresh else 0,
                'autoaim_target_distance': self.target_distance if aim_target_fresh else 0.0,
                'decoupled_async_ok': True,
            }
        msg = String()
        msg.data = json.dumps(payload, ensure_ascii=False)
        self.status_pub.publish(msg)


def main():
    rclpy.init()
    node = SentryAsyncWorkflow()
    executor = MultiThreadedExecutor(num_threads=4)
    executor.add_node(node)
    try:
        executor.spin()
    finally:
        executor.shutdown()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
