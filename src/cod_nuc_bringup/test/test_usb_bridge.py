import os
import sys
import time
from pathlib import Path
import unittest
import rclpy
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from rclpy.time import Time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from sentry_usb_bridge import SentryUSBBridge
from nav_wire import Parser, Frame, COMMAND, FEEDBACK, SYNC_RESPONSE


class USBBridgeTest(unittest.TestCase):
    def setUp(self):
        self.master, slave = os.openpty()
        self.slave = slave
        os.set_blocking(self.master, False)
        rclpy.init(args=['--ros-args', '-p', 'port:='+os.ttyname(slave)])
        self.node = SentryUSBBridge()
        self.parser = Parser()

    def tearDown(self):
        self.node.close()
        self.node.destroy_node()
        rclpy.shutdown()
        os.close(self.master)
        os.close(self.slave)

    def read_frames(self):
        time.sleep(.002)
        return self.parser.feed(os.read(self.master, 4096))

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


if __name__ == '__main__':
    unittest.main()
