#!/usr/bin/env python3
"""Convert LIO IMU/world output using sample-time mounting TF and fixed C."""
from collections import deque
from copy import deepcopy
import time
import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.time import Time
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from sensor_msgs.msg import PointCloud2, PointField
from tf2_ros import Buffer, TransformListener, TransformBroadcaster, TransformException
from chassis_geometry import ChassisGeometry, transform, quaternion, body_twist


def pose_matrix(pose):
    p, q = pose.position, pose.orientation
    return transform([p.x, p.y, p.z], [q.x, q.y, q.z, q.w])


class ChassisOdometry(Node):
    def __init__(self):
        super().__init__('chassis_odometry')
        if not self.declare_parameter('imu_extrinsics_confirmed', False).value:
            raise RuntimeError('Confirm MID-360S T_I_L calibration before enabling chassis output')
        translation = self.declare_parameter('extrinsic_T', [0.0, 0.0, 0.0]).value
        rotation = self.declare_parameter('extrinsic_R', [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]).value
        imu_from_lidar = np.eye(4)
        imu_from_lidar[:3, :3] = np.array(rotation).reshape(3, 3)
        imu_from_lidar[:3, 3] = translation
        if not np.isfinite(imu_from_lidar).all() or not np.allclose(imu_from_lidar[:3, :3].T @ imu_from_lidar[:3, :3], np.eye(3), atol=1e-6) or not np.isclose(np.linalg.det(imu_from_lidar[:3, :3]), 1):
            raise ValueError('extrinsic_R must be a rotation, extrinsic_T must be finite')
        self.geometry = ChassisGeometry(imu_from_lidar)
        self.max_age = self.declare_parameter('max_sample_age', 0.3).value
        self.buffer = Buffer()
        self.listener = TransformListener(self.buffer, self)
        self.broadcaster = TransformBroadcaster(self)
        self.odom_pub = self.create_publisher(Odometry, '/Odometry', 10)
        self.cloud_pub = self.create_publisher(PointCloud2, '/cloud_registered', 2)
        self.pending = deque(maxlen=100)
        self.cloud = None
        self.previous = None
        self.last_input = None
        self.fault = False
        self.create_subscription(Odometry, '/lio/imu_odometry', self.receive, 100)
        self.create_subscription(PointCloud2, '/lio/world_cloud', self.receive_cloud, 2)
        self.create_timer(0.01, self.process)

    def receive(self, msg):
        stamp = Time.from_msg(msg.header.stamp).nanoseconds
        if self.last_input is not None and stamp <= self.last_input:
            self.fault = True
            self.get_logger().error('LIO time reset/non-increasing: restart the complete hardware stack')
        self.last_input = stamp
        if msg.header.frame_id != 'lio_world' or msg.child_frame_id != 'lio_imu':
            self.fault = True
            self.get_logger().error('Unexpected raw LIO frames')
        self.pending.append((msg, time.monotonic()))

    def receive_cloud(self, msg):
        self.cloud = msg

    def process(self):
        if self.fault:
            return
        while self.pending:
            msg, received = self.pending[0]
            stamp = Time.from_msg(msg.header.stamp)
            age = (self.get_clock().now()-stamp).nanoseconds*1e-9
            if age > self.max_age or age < -0.05 or time.monotonic()-received > self.max_age:
                self.pending.popleft()
                self.get_logger().warning('Dropped LIO sample: stale/unsynchronized time or missing sample-time TF', throttle_duration_sec=2.0)
                continue
            try:
                mount = self.buffer.lookup_transform('base_link', 'livox_frame', stamp)
            except TransformException:
                # Wait for the bracketing platform sample; never substitute latest TF.
                break
            self.pending.popleft()
            p, q = mount.transform.translation, mount.transform.rotation
            try:
                base = self.geometry.update(pose_matrix(msg.pose.pose), transform([p.x, p.y, p.z], [q.x, q.y, q.z, q.w]))
            except ValueError as error:
                self.fault = True
                self.get_logger().error(str(error))
                return
            output = Odometry()
            output.header.stamp = msg.header.stamp
            output.header.frame_id = 'odom'
            output.child_frame_id = 'base_link'
            rotation = quaternion(base[:3, :3])
            output.pose.pose.position.x, output.pose.pose.position.y, output.pose.pose.position.z = base[:3, 3].tolist()
            output.pose.pose.orientation.x, output.pose.pose.orientation.y, output.pose.pose.orientation.z, output.pose.pose.orientation.w = rotation.tolist()
            # No propagated LIO/encoder covariance is available. Do not imply zero uncertainty.
            for i in range(6):
                output.pose.covariance[i*7] = 1e6
                output.twist.covariance[i*7] = 1e6
            if self.previous is not None:
                previous_time, previous_base = self.previous
                dt = (stamp.nanoseconds-previous_time)*1e-9
                if 0 < dt <= self.max_age:
                    linear, angular = body_twist(previous_base, base, dt)
                    output.twist.twist.linear.x, output.twist.twist.linear.y, output.twist.twist.linear.z = linear.tolist()
                    output.twist.twist.angular.x, output.twist.twist.angular.y, output.twist.twist.angular.z = angular.tolist()
            self.previous = (stamp.nanoseconds, base)
            tf = TransformStamped()
            tf.header = output.header
            tf.child_frame_id = output.child_frame_id
            tf.transform.translation.x, tf.transform.translation.y, tf.transform.translation.z = base[:3, 3].tolist()
            tf.transform.rotation = output.pose.pose.orientation
            self.broadcaster.sendTransform(tf)
            self.odom_pub.publish(output)
        if self.cloud is not None and self.geometry.odom_from_world is not None:
            msg, self.cloud = self.cloud, None
            age = (self.get_clock().now()-Time.from_msg(msg.header.stamp)).nanoseconds*1e-9
            if msg.header.frame_id != 'lio_world' or not 0 <= age <= self.max_age:
                return
            output = deepcopy(msg)
            output.header.frame_id = 'odom'
            output.data = bytearray(msg.data)
            fields = {field.name: field for field in msg.fields}
            if not all(name in fields and fields[name].datatype == PointField.FLOAT32 for name in ('x', 'y', 'z')):
                self.get_logger().error('Expected FLOAT32 xyz cloud')
                return
            axes = [np.ndarray((msg.height, msg.width), dtype='>f4' if msg.is_bigendian else '<f4', buffer=output.data,
                               offset=fields[name].offset, strides=(msg.row_step, msg.point_step)) for name in ('x', 'y', 'z')]
            points = np.stack(axes, axis=-1)
            c = self.geometry.odom_from_world
            points = points @ c[:3, :3].T + c[:3, 3]
            for i, axis in enumerate(axes):
                axis[:] = points[..., i]
            self.cloud_pub.publish(output)


def main():
    rclpy.init()
    node = ChassisOdometry()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
