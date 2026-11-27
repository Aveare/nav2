#!/usr/bin/env python3
"""Single USB CDC owner for navigation NV frames and auto-aim RMCV frames."""
import math
import time
import serial
import rclpy
from rclpy.node import Node
from rclpy.time import Time
from rclpy.executors import ExternalShutdownException
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import JointState
from rm_interfaces.msg import AutoAimPose, AutoAimTarget
from nav_wire import Frame, Parser, ClockSync, COMMAND, FEEDBACK, SYNC_REQUEST, SYNC_RESPONSE, VALID, valid_command
from rmcv_wire import Parser as RMCVParser, PoseFrame, TargetFrame, valid_target


class SentryUSBBridge(Node):
    def __init__(self):
        super().__init__('sentry_usb_bridge')
        self.port = self.declare_parameter('port', '/dev/ttyACM0').value
        self.enable_drive = self.declare_parameter('enable_drive', False).value
        self.enable_autoaim = self.declare_parameter('enable_autoaim', False).value
        self.autoaim_timeout = max(.02, float(self.declare_parameter('autoaim_timeout', .1).value))
        self.connection = None
        self.retry_at = 0.0
        self.sequence = 0
        self.sync_sequence = 0
        self.command = (0.0, 0.0, 0.0)
        self.command_at = self.odom_at = self.yaw_at = float('-inf')
        self.odom_stamp = None
        self.parser = Parser()
        self.rmcv_parser = RMCVParser()
        self.clock = ClockSync()
        self.last_sample_us = None
        self.last_joint_stamp = None
        self.sync_at = float('-inf')
        self.aim_due = 0.0
        self.aim_sequence = 0
        self.target = TargetFrame(0)
        self.target_at = float('-inf')
        self.last_pose_sample_ms = None
        self.last_pose_stamp = None
        self.tx = bytearray()
        self.tx_started = 0.0
        self.command_due = 0.0
        self.joint_pub = self.create_publisher(JointState, '/big_yaw/joint_states', 20)
        self.pose_pub = self.create_publisher(AutoAimPose, '/autoaim/pose', 20)
        self.create_subscription(Twist, '/cmd_vel', self.receive_command, 1)
        self.create_subscription(Odometry, '/Odometry', self.receive_odometry, 1)
        self.create_subscription(AutoAimTarget, '/autoaim/target', self.receive_target, 1)
        # Read at 200Hz so the 20ms RTT bound does not include a whole 50Hz command period.
        self.create_timer(.005, self.poll)

    def receive_command(self, msg):
        values = (msg.linear.x, msg.linear.y, msg.angular.z)
        if not valid_command(values):
            self.command_at = float('-inf')
            self.get_logger().error('Rejected non-finite or over-limit chassis command', throttle_duration_sec=2.0)
            return
        self.command = values
        self.command_at = time.monotonic()

    def receive_odometry(self, msg):
        if msg.header.frame_id == 'odom' and msg.child_frame_id == 'base_link':
            self.odom_at = time.monotonic()
            self.odom_stamp = Time.from_msg(msg.header.stamp).nanoseconds

    def receive_target(self, msg):
        target = TargetFrame(0, msg.yaw, msg.pitch, msg.yaw_speed, msg.pitch_speed,
                             msg.yaw_acc, msg.pitch_acc, msg.distance,
                             int(msg.shoot), int(msg.target_id))
        if not valid_target(target):
            self.target_at = float('-inf')
            self.get_logger().error('Rejected invalid auto-aim target', throttle_duration_sec=2.0)
            return
        self.target = target
        self.target_at = time.monotonic()

    def disconnect(self, error):
        if self.connection is not None:
            self.connection.close()
        self.connection = None
        self.retry_at = time.monotonic()+1.0
        self.tx.clear()
        self.command_at = self.odom_at = self.yaw_at = float('-inf')
        self.target_at = float('-inf')
        self.clock = ClockSync()
        self.parser = Parser()
        self.rmcv_parser = RMCVParser()
        self.last_sample_us = self.last_joint_stamp = None
        self.last_pose_sample_ms = self.last_pose_stamp = None
        self.get_logger().warning('USB disconnected: '+str(error), throttle_duration_sec=2.0)

    def feedback(self, frame, now_ns, now):
        if frame.kind == SYNC_RESPONSE:
            self.clock.response(frame, now_ns, now)
            return
        if frame.kind != FEEDBACK:
            return
        if self.last_sample_us is not None and frame.sample_us < self.last_sample_us:
            # MCU reset/tick wrap: never preserve an offset from the previous boot.
            self.clock = ClockSync()
            self.last_joint_stamp = None
            self.yaw_at = float('-inf')
        if frame.sample_us == self.last_sample_us:
            return
        self.last_sample_us = frame.sample_us
        if frame.flags != VALID or not math.isfinite(frame.values[0]):
            self.yaw_at = float('-inf')
            return
        stamp = self.clock.stamp(frame.sample_us, now_ns, now)
        if stamp is None or (self.last_joint_stamp is not None and stamp <= self.last_joint_stamp):
            return
        msg = JointState()
        msg.header.stamp = Time(nanoseconds=stamp).to_msg()
        msg.name, msg.position = ['big_yaw_joint'], [frame.values[0]]
        self.joint_pub.publish(msg)
        self.last_joint_stamp = stamp
        self.yaw_at = now

    def pose_feedback(self, frame, now_ns, now):
        if self.last_pose_sample_ms is not None and frame.sample_time_ms < self.last_pose_sample_ms:
            self.clock = ClockSync()
            self.last_pose_stamp = None
        if frame.sample_time_ms == self.last_pose_sample_ms:
            return
        self.last_pose_sample_ms = frame.sample_time_ms
        stamp = self.clock.stamp(frame.sample_time_ms * 1000, now_ns, now)
        if stamp is None or (self.last_pose_stamp is not None and stamp <= self.last_pose_stamp):
            return
        msg = AutoAimPose()
        msg.header.stamp = Time(nanoseconds=stamp).to_msg()
        msg.header.frame_id = 'gimbal_imu'
        msg.sequence = frame.sequence
        msg.mcu_sample_time_ms = frame.sample_time_ms
        msg.yaw, msg.pitch, msg.roll = frame.yaw, frame.pitch, frame.roll
        msg.projectile_speed = frame.projectile_speed
        msg.mode, msg.robot_id = frame.mode, frame.robot_id
        self.pose_pub.publish(msg)
        self.last_pose_stamp = stamp

    def poll(self):
        now = time.monotonic()
        try:
            if self.connection is None:
                if now < self.retry_at:
                    return
                self.connection = serial.Serial(self.port, 115200, timeout=0, write_timeout=0, exclusive=True)
                self.connection.reset_input_buffer()
                self.sync_at = float('-inf')
                self.command_due = 0.0
                self.aim_due = 0.0
            now_ns = self.get_clock().now().nanoseconds
            received = self.connection.read(4096)
            for frame in self.parser.feed(received):
                self.feedback(frame, now_ns, now)
            for frame in self.rmcv_parser.feed(received):
                if isinstance(frame, PoseFrame):
                    self.pose_feedback(frame, now_ns, now)
            if self.tx:
                if now-self.tx_started > .1:
                    raise serial.SerialException('TX blocked; discard queued command')
                sent = self.connection.write(self.tx)
                del self.tx[:sent]
                if self.tx:
                    return
            if now >= self.command_due:
                self.command_due = now+.02
                odom_age = float('inf') if self.odom_stamp is None else (now_ns-self.odom_stamp)*1e-9
                valid = (self.enable_drive and now-self.command_at <= .2 and now-self.odom_at <= .2 and
                         now-self.yaw_at <= .2 and -.02 <= odom_age <= .2 and now-self.clock.updated <= 2.0)
                command = self.command if valid else (0.0, 0.0, 0.0)
                self.tx.extend(Frame(COMMAND, self.sequence, now_ns//1000, command,
                                     VALID if valid else 0).encode())
                self.sequence = (self.sequence+1) & 0xffffffff
                if now-self.sync_at >= .5:
                    self.sync_sequence = (self.sync_sequence+1) & 0xffffffff
                    self.clock.request(self.sync_sequence, now_ns, now)
                    self.tx.extend(Frame(SYNC_REQUEST, self.sync_sequence, now_ns//1000).encode())
                    self.sync_at = now
            if now >= self.aim_due:
                self.aim_due = now+.01
                live = self.enable_autoaim and now-self.target_at <= self.autoaim_timeout
                source = self.target if live else TargetFrame(0)
                target = TargetFrame(self.aim_sequence, source.yaw, source.pitch,
                                     source.yaw_speed, source.pitch_speed,
                                     source.yaw_acc, source.pitch_acc, source.distance,
                                     source.shoot if live else 0,
                                     source.target_id if live else 0)
                self.tx.extend(target.encode())
                self.aim_sequence = (self.aim_sequence+1) & 0xff
            if not self.tx:
                return
            self.tx_started = now
            sent = self.connection.write(self.tx)
            del self.tx[:sent]
        except (serial.SerialException, OSError) as error:
            self.disconnect(error)

    def close(self):
        if self.connection is not None:
            try:
                # A partial preceding frame may require parser resynchronization; MCU timeout remains authoritative.
                stop = Frame(COMMAND, self.sequence, 0).encode()
                no_target = TargetFrame(self.aim_sequence).encode()
                self.connection.write(stop + no_target)
            except (serial.SerialException, OSError):
                pass
            self.connection.close()


def main():
    rclpy.init()
    node = SentryUSBBridge()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    except Exception:
        # SIGINT/SIGTERM can invalidate the ROS context while a timer callback is
        # publishing. Only suppress that shutdown race; preserve live-context faults.
        if rclpy.ok():
            raise
    finally:
        node.close()
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
