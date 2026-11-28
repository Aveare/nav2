#!/usr/bin/env python3
"""Single USB CDC owner for navigation NV frames and auto-aim RMCV frames."""
import glob
import json
import math
import os
import time
import serial
import rclpy
from rclpy.node import Node
from rclpy.time import Time
from rclpy.executors import ExternalShutdownException
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from sensor_msgs.msg import JointState
from std_msgs.msg import String
from rm_interfaces.msg import AutoAimPose, AutoAimTarget
from nav_wire import Frame, Parser, ClockSync, COMMAND, FEEDBACK, SYNC_REQUEST, SYNC_RESPONSE, VALID, valid_command
from rmcv_wire import Parser as RMCVParser, FullPoseFrame, PoseFrame, TargetFrame, valid_target

PREFERRED_BY_ID_GLOBS = (
    '/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_*',
    '/dev/serial/by-id/usb-YueLuEmbedded_*',
)
DEFAULT_FALLBACK_PORT = '/dev/ttyACM0'


def resolve_serial_port(configured_port: str, by_id_globs=PREFERRED_BY_ID_GLOBS,
                        fallback_port: str = DEFAULT_FALLBACK_PORT) -> str:
    port = (configured_port or '').strip()
    if port and port.lower() != 'auto':
        return port
    for pattern in by_id_globs:
        matches = sorted(path for path in glob.glob(pattern) if os.path.exists(path))
        if matches:
            return matches[0]
    return fallback_port


def advance_due(due: float, now: float, period: float) -> float:
    if due <= 0.0:
        return now + period
    next_due = due + period
    if now - next_due > period:
        return now + period
    return next_due


class SentryUSBBridge(Node):
    def __init__(self):
        super().__init__('sentry_usb_bridge')
        self.configured_port = str(self.declare_parameter('port', 'auto').value)
        self.port = resolve_serial_port(self.configured_port)
        self.enable_drive = self.declare_parameter('enable_drive', False).value
        self.enable_autoaim = self.declare_parameter('enable_autoaim', False).value
        self.autoaim_timeout = max(.02, float(self.declare_parameter('autoaim_timeout', .1).value))
        self.connection = None
        self.has_connected = False
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
        self.last_big_yaw_rad = 0.0
        self.sync_at = float('-inf')
        self.aim_due = 0.0
        self.aim_sequence = 0
        self.target = TargetFrame(0)
        self.target_at = float('-inf')
        self.last_pose_sample_ms = None
        self.last_pose_stamp = None
        self.last_pose_full = False
        self.last_pose_valid_flags = 0
        self.last_pose_mode = 0
        self.last_robot_id = 0
        self.last_projectile_speed = 0.0
        self.tx = bytearray()
        self.tx_started = 0.0
        self.command_due = 0.0
        self.status_due = 0.0
        self.status_window_at = time.monotonic()
        self.reconnect_count = 0
        self.tx_busy_count = 0
        self.tx_blocked_count = 0
        self.tx_bytes = 0
        self.rx_bytes = 0
        self.tx_command_count = 0
        self.tx_command_valid_count = 0
        self.tx_sync_count = 0
        self.rx_sync_count = 0
        self.rx_feedback_count = 0
        self.rx_pose_count = 0
        self.rx_full_pose_count = 0
        self.rx_legacy_pose_count = 0
        self.tx_target_count = 0
        self.tx_target_live_count = 0
        self.last_target_id = 0
        self.last_target_shoot = 0
        self._rate_prev = {
            'tx_command': 0,
            'rx_feedback': 0,
            'rx_pose': 0,
            'tx_target': 0,
        }
        self.joint_pub = self.create_publisher(JointState, '/big_yaw/joint_states', 20)
        self.pose_pub = self.create_publisher(AutoAimPose, '/autoaim/pose', 20)
        self.status_pub = self.create_publisher(String, '/sentry/usb_bridge_status', 10)
        self.create_subscription(Twist, '/cmd_vel', self.receive_command, 1)
        self.create_subscription(Odometry, '/Odometry', self.receive_odometry, 1)
        self.create_subscription(AutoAimTarget, '/autoaim/target', self.receive_target, 1)
        # Read at 200Hz so the 20ms RTT bound does not include a whole 50Hz command period.
        self.create_timer(.005, self.poll)

    def receive_command(self, msg):
        values = (msg.linear.x, msg.linear.y, msg.angular.z)
        if not valid_command(values):
            self.command = (0.0, 0.0, 0.0)
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
            self.target = TargetFrame(0)
            self.target_at = float('-inf')
            self.get_logger().error('Rejected invalid auto-aim target', throttle_duration_sec=2.0)
            return
        self.target = target
        self.target_at = time.monotonic()

    def _reset_mcu_session(self):
        self.clock = ClockSync()
        self.last_sample_us = self.last_joint_stamp = None
        self.yaw_at = float('-inf')
        self.last_pose_sample_ms = self.last_pose_stamp = None

    def drive_gate_reasons(self, now: float, now_ns: int):
        reasons = []
        if not self.enable_drive:
            reasons.append('enable_drive_false')
        if now - self.command_at > .2:
            reasons.append('cmd_vel_stale')
        if now - self.odom_at > .2:
            reasons.append('odom_stale')
        if now - self.yaw_at > .2:
            reasons.append('big_yaw_stale')
        odom_age = float('inf') if self.odom_stamp is None else (now_ns - self.odom_stamp) * 1e-9
        if not (-.02 <= odom_age <= .2):
            reasons.append('odom_stamp_out_of_window')
        if now - self.clock.updated > 2.0:
            reasons.append('clock_sync_stale')
        return (len(reasons) == 0), reasons

    def disconnect(self, error):
        if self.connection is not None:
            self.connection.close()
        self.connection = None
        self.retry_at = time.monotonic()+1.0
        if self.has_connected:
            self.reconnect_count += 1
        self.tx.clear()
        self.command_at = self.odom_at = self.yaw_at = float('-inf')
        self.target_at = float('-inf')
        self._reset_mcu_session()
        self.parser = Parser()
        self.rmcv_parser = RMCVParser()
        self.get_logger().warning('USB disconnected: '+str(error), throttle_duration_sec=2.0)

    def feedback(self, frame, now_ns, now):
        if frame.kind == SYNC_RESPONSE:
            if self.clock.response(frame, now_ns, now):
                self.rx_sync_count += 1
            return
        if frame.kind != FEEDBACK:
            return
        if self.last_sample_us is not None and frame.sample_us < self.last_sample_us:
            # MCU reset/tick wrap: never preserve an offset from the previous boot.
            self._reset_mcu_session()
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
        self.last_big_yaw_rad = float(frame.values[0])
        self.yaw_at = now
        self.rx_feedback_count += 1

    def pose_feedback(self, frame, now_ns, now):
        if self.last_pose_sample_ms is not None and frame.sample_time_ms < self.last_pose_sample_ms:
            self._reset_mcu_session()
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
        if isinstance(frame, FullPoseFrame):
            q = (frame.q_w, frame.q_x, frame.q_y, frame.q_z)
            norm = math.sqrt(sum(value * value for value in q))
            w, x, y, z = (value / norm for value in q)
            r01 = 2.0 * (x * y - w * z)
            r11 = 1.0 - 2.0 * (x * x + z * z)
            r20 = 2.0 * (x * z - w * y)
            r21 = max(-1.0, min(1.0, 2.0 * (y * z + w * x)))
            r22 = 1.0 - 2.0 * (x * x + y * y)
            msg.yaw = math.degrees(math.atan2(-r01, r11))
            msg.pitch = math.degrees(math.asin(r21))
            msg.roll = math.degrees(math.atan2(-r20, r22))
            msg.full_pose = True
            msg.big_yaw_quaternion = [w, x, y, z]
            msg.big_yaw_rel = frame.big_yaw_rel
            msg.small_yaw = frame.small_yaw
            msg.barrel_pitch = frame.pitch
            msg.big_yaw_sample_time_ms = frame.big_yaw_sample_time_ms
            msg.small_yaw_sample_time_ms = frame.small_yaw_sample_time_ms
            msg.pitch_sample_time_ms = frame.pitch_sample_time_ms
            msg.valid_flags = frame.valid_flags
            self.rx_full_pose_count += 1
            self.last_pose_full = True
            self.last_pose_valid_flags = int(frame.valid_flags)
        else:
            msg.yaw, msg.pitch, msg.roll = frame.yaw, frame.pitch, frame.roll
            msg.full_pose = False
            self.rx_legacy_pose_count += 1
            self.last_pose_full = False
            self.last_pose_valid_flags = 0
        msg.projectile_speed = frame.projectile_speed
        msg.mode, msg.robot_id = frame.mode, frame.robot_id
        self.pose_pub.publish(msg)
        self.last_pose_stamp = stamp
        self.rx_pose_count += 1
        self.last_pose_mode = int(frame.mode)
        self.last_robot_id = int(frame.robot_id)
        self.last_projectile_speed = float(frame.projectile_speed)

    def build_status(self, now: float, now_ns: int) -> dict:
        dt = max(1e-3, now - self.status_window_at)
        tx_command_hz = round((self.tx_command_count - self._rate_prev['tx_command']) / dt, 1)
        rx_feedback_hz = round((self.rx_feedback_count - self._rate_prev['rx_feedback']) / dt, 1)
        rx_pose_hz = round((self.rx_pose_count - self._rate_prev['rx_pose']) / dt, 1)
        tx_target_hz = round((self.tx_target_count - self._rate_prev['tx_target']) / dt, 1)
        drive_valid, drive_reasons = self.drive_gate_reasons(now, now_ns)
        clock_ok = (self.clock.offset_ns is not None) and ((now - self.clock.updated) <= 2.0)
        return {
            'connected': self.connection is not None,
            'configured_port': self.configured_port,
            'resolved_port': self.port,
            'enable_drive': bool(self.enable_drive),
            'enable_autoaim': bool(self.enable_autoaim),
            'drive_valid': drive_valid,
            'drive_gate_reasons': drive_reasons,
            'reconnect_count': self.reconnect_count,
            'tx_busy_count': self.tx_busy_count,
            'tx_blocked_count': self.tx_blocked_count,
            'tx_bytes': self.tx_bytes,
            'rx_bytes': self.rx_bytes,
            'tx_command_count': self.tx_command_count,
            'tx_command_valid_count': self.tx_command_valid_count,
            'tx_command_hz': tx_command_hz,
            'tx_sync_count': self.tx_sync_count,
            'rx_sync_count': self.rx_sync_count,
            'clock_sync_ok': clock_ok,
            'clock_offset_ms': round(self.clock.offset_ns * 1e-6, 3) if self.clock.offset_ns is not None else None,
            'clock_rtt_ms': round(self.clock.last_rtt_s * 1e3, 3) if self.clock.last_rtt_s is not None else None,
            'rx_feedback_count': self.rx_feedback_count,
            'rx_feedback_hz': rx_feedback_hz,
            'big_yaw_rad': round(self.last_big_yaw_rad, 4),
            'rx_pose_count': self.rx_pose_count,
            'rx_full_pose_count': self.rx_full_pose_count,
            'rx_legacy_pose_count': self.rx_legacy_pose_count,
            'rx_pose_hz': rx_pose_hz,
            'full_pose': self.last_pose_full,
            'pose_valid_flags': self.last_pose_valid_flags,
            'pose_mode': self.last_pose_mode,
            'robot_id': self.last_robot_id,
            'projectile_speed': round(self.last_projectile_speed, 3),
            'tx_target_count': self.tx_target_count,
            'tx_target_live_count': self.tx_target_live_count,
            'tx_target_hz': tx_target_hz,
            'last_target_id': self.last_target_id,
            'last_target_shoot': self.last_target_shoot,
            'nv_crc_errors': self.parser.crc_errors,
            'nv_resync_bytes': self.parser.resync_bytes,
            'rmcv_crc8_errors': self.rmcv_parser.crc8_errors,
            'rmcv_crc16_errors': self.rmcv_parser.crc16_errors,
            'rmcv_length_errors': self.rmcv_parser.length_errors,
            'rmcv_validation_errors': self.rmcv_parser.validation_errors,
            'rmcv_resync_bytes': self.rmcv_parser.resync_bytes,
        }

    def publish_status(self, now: float, now_ns: int):
        payload = self.build_status(now, now_ns)
        msg = String()
        msg.data = json.dumps(payload, ensure_ascii=False)
        self.status_pub.publish(msg)
        self.status_window_at = now
        self._rate_prev = {
            'tx_command': self.tx_command_count,
            'rx_feedback': self.rx_feedback_count,
            'rx_pose': self.rx_pose_count,
            'tx_target': self.tx_target_count,
        }

    def _write_pending(self, now: float) -> bool:
        sent = self.connection.write(self.tx)
        written = int(sent or 0)
        if written > 0:
            self.tx_bytes += written
            del self.tx[:written]
        if self.tx:
            self.tx_busy_count += 1
            return False
        return True

    def poll(self):
        now = time.monotonic()
        now_ns = self.get_clock().now().nanoseconds
        try:
            if self.connection is None:
                if now < self.retry_at:
                    if now >= self.status_due:
                        self.status_due = advance_due(self.status_due, now, .5)
                        self.publish_status(now, now_ns)
                    return
                self.port = resolve_serial_port(self.configured_port)
                self.connection = serial.Serial(self.port, 115200, timeout=0, write_timeout=0, exclusive=True)
                self.connection.reset_input_buffer()
                self.has_connected = True
                self.sync_at = float('-inf')
                self.command_due = 0.0
                self.aim_due = 0.0
            received = self.connection.read(4096)
            if received:
                self.rx_bytes += len(received)
            for frame in self.parser.feed(received):
                self.feedback(frame, now_ns, now)
            for frame in self.rmcv_parser.feed(received):
                if isinstance(frame, (PoseFrame, FullPoseFrame)):
                    self.pose_feedback(frame, now_ns, now)
            if self.tx:
                if now-self.tx_started > .1:
                    self.tx_blocked_count += 1
                    raise serial.SerialException('TX blocked; discard queued command')
                if not self._write_pending(now):
                    if now >= self.status_due:
                        self.status_due = advance_due(self.status_due, now, .5)
                        self.publish_status(now, now_ns)
                    return
            if now >= self.command_due:
                self.command_due = advance_due(self.command_due, now, .02)
                valid, _ = self.drive_gate_reasons(now, now_ns)
                command = self.command if valid else (0.0, 0.0, 0.0)
                self.tx.extend(Frame(COMMAND, self.sequence, now_ns//1000, command,
                                     VALID if valid else 0).encode())
                self.sequence = (self.sequence+1) & 0xffffffff
                self.tx_command_count += 1
                if valid:
                    self.tx_command_valid_count += 1
                if now-self.sync_at >= .5:
                    self.sync_sequence = (self.sync_sequence+1) & 0xffffffff
                    self.clock.request(self.sync_sequence, now_ns, now)
                    self.tx.extend(Frame(SYNC_REQUEST, self.sync_sequence, now_ns//1000).encode())
                    self.tx_sync_count += 1
                    self.sync_at = now
            if now >= self.aim_due:
                self.aim_due = advance_due(self.aim_due, now, .01)
                live = self.enable_autoaim and now-self.target_at <= self.autoaim_timeout
                source = self.target if live else TargetFrame(0)
                target_id = source.target_id if live else 0
                shoot = source.shoot if (live and target_id != 0) else 0
                target = TargetFrame(self.aim_sequence, source.yaw, source.pitch,
                                     source.yaw_speed, source.pitch_speed,
                                     source.yaw_acc, source.pitch_acc, source.distance,
                                     shoot, target_id)
                self.tx.extend(target.encode())
                self.aim_sequence = (self.aim_sequence+1) & 0xff
                self.tx_target_count += 1
                if live and target_id != 0:
                    self.tx_target_live_count += 1
                self.last_target_id = int(target_id)
                self.last_target_shoot = int(shoot)
            if self.tx:
                self.tx_started = now
                self._write_pending(now)
        except (serial.SerialException, OSError) as error:
            self.disconnect(error)
        if now >= self.status_due:
            self.status_due = advance_due(self.status_due, now, .5)
            self.publish_status(now, now_ns)
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
                if isinstance(frame, (PoseFrame, FullPoseFrame)):
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
