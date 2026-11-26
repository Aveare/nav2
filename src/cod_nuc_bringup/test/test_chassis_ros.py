"""Run under ROS 2 Humble, with no hardware or serial connection."""
import sys
from pathlib import Path
import unittest
from unittest.mock import Mock
import numpy as np
import rclpy
from rclpy.time import Time
from geometry_msgs.msg import TransformStamped
from nav_msgs.msg import Odometry
from sensor_msgs.msg import JointState, PointCloud2, PointField
from rclpy.serialization import serialize_message

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chassis_odometry import ChassisOdometry
from big_yaw_tf import BigYawTF
from chassis_geometry import transform, quaternion


class RosTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init(args=['--ros-args', '-p', 'imu_extrinsics_confirmed:=true'])

    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()

    def setUp(self):
        self.node = ChassisOdometry()
        self.node.odom_pub = Mock()
        self.node.cloud_pub = Mock()
        self.node.broadcaster = Mock()

    def tearDown(self):
        self.node.destroy_node()

    def sample(self, ns, angle=0):
        mount = transform([0, 0, 0], [0, 0, np.sin(angle/2), np.cos(angle/2)]) @ transform([-.053, 0, .474], [1, 0, 0, 0])
        tf = TransformStamped()
        tf.header.stamp = Time(nanoseconds=ns).to_msg()
        tf.header.frame_id = 'base_link'
        tf.child_frame_id = 'livox_frame'
        tf.transform.translation.x, tf.transform.translation.y, tf.transform.translation.z = mount[:3, 3].tolist()
        q = quaternion(mount[:3, :3])
        tf.transform.rotation.x, tf.transform.rotation.y, tf.transform.rotation.z, tf.transform.rotation.w = q.tolist()
        raw = Odometry()
        raw.header.stamp = tf.header.stamp
        raw.header.frame_id, raw.child_frame_id = 'lio_world', 'lio_imu'
        raw.pose.pose.position.x, raw.pose.pose.position.y, raw.pose.pose.position.z = mount[:3, 3].tolist()
        raw.pose.pose.orientation = tf.transform.rotation
        return tf, raw

    def test_waits_for_sample_time_tf_then_publishes_stationary_base(self):
        now = self.node.get_clock().now().nanoseconds
        for ns, angle in [(now-40000000, 0), (now-20000000, .4)]:
            tf, raw = self.sample(ns, angle)
            self.node.receive(raw)
            before = self.node.odom_pub.publish.call_count
            self.node.process()
            self.assertEqual(self.node.odom_pub.publish.call_count, before)
            self.node.buffer.set_transform(tf, 'test')
            self.node.process()
            output = self.node.odom_pub.publish.call_args.args[0]
            np.testing.assert_allclose([output.pose.pose.position.x, output.pose.pose.position.y, output.pose.pose.position.z], 0, atol=1e-12)
            np.testing.assert_allclose([output.twist.twist.linear.x, output.twist.twist.linear.y, output.twist.twist.angular.z], 0, atol=1e-10)
            self.assertEqual(output.header.stamp, raw.header.stamp)
            self.assertEqual(output.child_frame_id, 'base_link')
            serialize_message(output)

    def test_stale_sample_and_time_reset(self):
        ns = self.node.get_clock().now().nanoseconds-1000000000
        _, raw = self.sample(ns)
        self.node.receive(raw)
        self.node.process()
        self.node.odom_pub.publish.assert_not_called()
        self.assertEqual(len(self.node.pending), 0)
        self.node.receive(raw)
        self.assertTrue(self.node.fault)

    def test_cloud_uses_fixed_world_alignment_and_preserves_fields(self):
        ns = self.node.get_clock().now().nanoseconds-20000000
        tf, raw = self.sample(ns)
        self.node.buffer.set_transform(tf, 'test')
        self.node.receive(raw)
        self.node.process()
        cloud = PointCloud2()
        cloud.header = raw.header
        cloud.height, cloud.width, cloud.point_step, cloud.row_step = 1, 1, 16, 16
        cloud.fields = [PointField(name=name, offset=i*4, datatype=PointField.FLOAT32, count=1) for i, name in enumerate(('x', 'y', 'z', 'intensity'))]
        cloud.data = np.array([1, 2, 3, 7], dtype='<f4').tobytes()
        self.node.receive_cloud(cloud)
        self.node.process()
        output = self.node.cloud_pub.publish.call_args.args[0]
        self.assertEqual(output.header.frame_id, 'odom')
        np.testing.assert_allclose(np.frombuffer(output.data, dtype='<f4'), [1, 2, 3, 7])
        serialize_message(output)

    def test_platform_feedback_preserves_stamp_and_rejects_repeat(self):
        node = BigYawTF()
        try:
            node.broadcaster = Mock()
            msg = JointState()
            msg.header.stamp = self.node.get_clock().now().to_msg()
            msg.name, msg.position = ['big_yaw_joint'], [np.pi/2]
            node.receive(msg)
            output = node.broadcaster.sendTransform.call_args.args[0]
            self.assertEqual(output.header.stamp, msg.header.stamp)
            self.assertEqual(output.child_frame_id, 'big_yaw_link')
            self.assertAlmostEqual(output.transform.rotation.z, np.sqrt(.5))
            node.receive(msg)
            self.assertEqual(node.broadcaster.sendTransform.call_count, 1)
        finally:
            node.destroy_node()


if __name__ == '__main__':
    unittest.main()
