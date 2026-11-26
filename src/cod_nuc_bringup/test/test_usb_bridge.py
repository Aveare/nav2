import os
import sys
import time
from pathlib import Path
import unittest
import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.time import Time
from rm_interfaces.msg import AutoAimTarget

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from sentry_usb_bridge import SentryUSBBridge
from nav_wire import Parser, Frame, COMMAND, FEEDBACK, SYNC_RESPONSE
from rmcv_wire import Parser as RMCVParser, PoseFrame, TargetFrame


class USBBridgeTest(unittest.TestCase):
    def setUp(self):
        self.master, slave = os.openpty()
        self.slave = slave
        os.set_blocking(self.master, False)
        rclpy.init(args=['--ros-args', '-p', 'port:='+os.ttyname(slave)])
        self.node = SentryUSBBridge()
        self.parser = Parser()
        self.rmcv_parser = RMCVParser()

    def tearDown(self):
        self.node.close()
        self.node.destroy_node()
        rclpy.shutdown()
        os.close(self.master)
        os.close(self.slave)

    def read_bytes(self):
        time.sleep(.002)
        return os.read(self.master, 4096)

    def read_frames(self):
        return self.parser.feed(self.read_bytes())

    def ready(self):
        self.node.enable_drive = True
        now = time.monotonic()
        self.node.clock.updated = now
        self.node.yaw_at = now
        command = Twist()
        command.linear.x, command.linear.y, command.angular.z = .25, -.125, .5
        self.node.receive_command(command)
        odometry = Odometry()
        odometry.header.frame_id, odometry.child_frame_id = 'odom', 'base_link'
        odometry.header.stamp = self.node.get_clock().now().to_msg()
        self.node.receive_odometry(odometry)

    def test_real_pty_start_stop_drive_and_stale_odometry(self):
        self.node.poll()
        frames = self.read_frames()
        self.assertEqual(frames[0].kind, COMMAND)
        self.assertEqual(frames[0].flags, 0)
        self.ready()
        time.sleep(.021)
        self.node.poll()
        command = self.read_frames()[0]
        self.assertEqual(command.values, (.25, -.125, .5))
        self.assertEqual(command.flags, 1)
        self.node.odom_at -= 1.0
        time.sleep(.021)
        self.node.poll()
        command = self.read_frames()[0]
        self.assertEqual(command.flags, 0)
        self.assertEqual(command.values, (0.0, 0.0, 0.0))

    def test_fragmented_feedback_maps_original_mcu_sample(self):
        self.node.poll()
        request = [f for f in self.read_frames() if f.kind == 3][0]
        now = self.node.get_clock().now().nanoseconds
        # Sample at the sync response, then at +1ms. Both preserve MCU sample time.
        os.write(self.master, Frame(SYNC_RESPONSE, request.sequence, 1000000).encode())
        self.node.poll()
        self.assertIsNotNone(self.node.clock.offset_ns)
        feedback = Frame(FEEDBACK, 1, 1001000, (1.0, 0.0, 0.0), 1).encode()
        os.write(self.master, feedback[:11])
        self.node.poll()
        self.assertIsNone(self.node.last_joint_stamp)
        os.write(self.master, feedback[11:])
        self.node.poll()
        self.assertEqual(self.node.last_joint_stamp, 1001000*1000+self.node.clock.offset_ns)
        self.assertLess(abs(self.node.last_joint_stamp-now), 30000000)
        self.node.feedback(Frame(FEEDBACK, 2, 100, (0.0, 0.0, 0.0), 1), now, time.monotonic())
        self.assertIsNone(self.node.clock.offset_ns)  # MCU 重启后不能复用旧偏移。

    def test_autoaim_target_gate_timeout_and_fragmented_pose(self):
        self.node.poll()
        initial = self.read_bytes()
        initial_targets = [frame for frame in self.rmcv_parser.feed(initial)
                           if isinstance(frame, TargetFrame)]
        self.assertEqual(initial_targets[-1].target_id, 0)

        self.node.enable_autoaim = True
        target = AutoAimTarget()
        target.yaw, target.pitch = 30.0, -5.0
        target.yaw_speed, target.pitch_speed = .25, -.125
        target.yaw_acc, target.pitch_acc = 1.0, -2.0
        target.distance, target.shoot, target.target_id = 4.5, 1, 3
        self.node.receive_target(target)
        time.sleep(.011)
        self.node.poll()
        targets = [frame for frame in self.rmcv_parser.feed(self.read_bytes())
                   if isinstance(frame, TargetFrame)]
        self.assertEqual(targets[-1].target_id, 3)
        self.assertEqual(targets[-1].shoot, 1)

        self.node.target_at -= 1.0
        time.sleep(.011)
        self.node.poll()
        targets = [frame for frame in self.rmcv_parser.feed(self.read_bytes())
                   if isinstance(frame, TargetFrame)]
        self.assertEqual(targets[-1].target_id, 0)
        self.assertEqual(targets[-1].shoot, 0)

        sample_ms = 1000
        now_ns = self.node.get_clock().now().nanoseconds
        self.node.clock.offset_ns = now_ns - sample_ms * 1000000
        self.node.clock.updated = time.monotonic()
        pose = PoseFrame(7, sample_ms, 10.0, -2.0, .5, 15.5, 1, 107).encode()
        os.write(self.master, pose[:11])
        self.node.poll()
        self.assertIsNone(self.node.last_pose_stamp)
        os.write(self.master, pose[11:])
        self.node.poll()
        self.assertEqual(self.node.last_pose_sample_ms, sample_ms)
        self.assertEqual(self.node.last_pose_stamp, now_ns)


if __name__ == '__main__':
    unittest.main()
