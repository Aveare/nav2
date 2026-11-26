#!/usr/bin/env python3
"""Input contract: sampled platform angle, radians, CCW, in JointState."""
import math
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import TransformStamped
from sensor_msgs.msg import JointState
from tf2_ros import TransformBroadcaster


class BigYawTF(Node):
    def __init__(self):
        super().__init__('big_yaw_tf')
        self.joint = self.declare_parameter('joint_name', 'big_yaw_joint').value
        self.last_stamp = 0
        self.broadcaster = TransformBroadcaster(self)
        self.create_subscription(JointState, '/big_yaw/joint_states', self.receive, 100)

    def receive(self, msg):
        stamp = msg.header.stamp.sec*10**9 + msg.header.stamp.nanosec
        if self.joint not in msg.name:
            return
        i = msg.name.index(self.joint)
        if i >= len(msg.position) or not math.isfinite(msg.position[i]) or stamp <= self.last_stamp:
            self.get_logger().error('Rejected invalid angle or non-increasing sample time')
            return
        tf = TransformStamped()
        tf.header.stamp = msg.header.stamp
        tf.header.frame_id = 'base_link'
        tf.child_frame_id = 'big_yaw_link'
        tf.transform.rotation.z = math.sin(msg.position[i]/2)
        tf.transform.rotation.w = math.cos(msg.position[i]/2)
        self.broadcaster.sendTransform(tf)
        self.last_stamp = stamp


def main():
    rclpy.init()
    node = BigYawTF()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
