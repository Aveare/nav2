"""RMCV 0xA5 framing shared with sentry_firmware/components/ax650."""
from dataclasses import dataclass
import math
import struct

SOF = 0xA5
POSE_COMMAND = 0x1021
FULL_POSE_COMMAND = 0x1022
TARGET_COMMAND = 0x0503
MAX_PAYLOAD = 64
POSE_PAYLOAD = struct.Struct('<ffffBIB')
FULL_POSE_PAYLOAD = struct.Struct('<ffffffffIIIIHBB')
TARGET_PAYLOAD = struct.Struct('<fffffffBB')


def crc8(data):
    crc = 0xff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8c if crc & 1 else 0)
    return crc


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
    return crc


def encode_frame(command, sequence, payload):
    header = bytearray(struct.pack('<BHB', SOF, len(payload), sequence & 0xff))
    header.append(crc8(header))
    body = header + struct.pack('<H', command) + payload
    return bytes(body + struct.pack('<H', crc16(body)))


@dataclass(frozen=True)
class PoseFrame:
    sequence: int
    sample_time_ms: int
    yaw: float
    pitch: float
    roll: float
    projectile_speed: float
    mode: int
    robot_id: int

    def encode(self):
        payload = POSE_PAYLOAD.pack(self.yaw, self.pitch, self.roll, self.projectile_speed,
                                    self.mode, self.sample_time_ms & 0xffffffff, self.robot_id)
        return encode_frame(POSE_COMMAND, self.sequence, payload)


@dataclass(frozen=True)
class FullPoseFrame:
    sequence: int
    q_w: float
    q_x: float
    q_y: float
    q_z: float
    big_yaw_rel: float
    small_yaw: float
    pitch: float
    projectile_speed: float
    imu_sample_time_ms: int
    big_yaw_sample_time_ms: int
    small_yaw_sample_time_ms: int
    pitch_sample_time_ms: int
    valid_flags: int
    mode: int
    robot_id: int

    @property
    def sample_time_ms(self):
        return self.imu_sample_time_ms

    def encode(self):
        payload = FULL_POSE_PAYLOAD.pack(
            self.q_w, self.q_x, self.q_y, self.q_z,
            self.big_yaw_rel, self.small_yaw, self.pitch, self.projectile_speed,
            self.imu_sample_time_ms & 0xffffffff,
            self.big_yaw_sample_time_ms & 0xffffffff,
            self.small_yaw_sample_time_ms & 0xffffffff,
            self.pitch_sample_time_ms & 0xffffffff,
            self.valid_flags & 0xffff, self.mode, self.robot_id)
        return encode_frame(FULL_POSE_COMMAND, self.sequence, payload)


@dataclass(frozen=True)
class TargetFrame:
    sequence: int
    yaw: float = 0.0
    pitch: float = 0.0
    yaw_speed: float = 0.0
    pitch_speed: float = 0.0
    yaw_acc: float = 0.0
    pitch_acc: float = 0.0
    distance: float = 0.0
    shoot: int = 0
    target_id: int = 0

    def encode(self):
        payload = TARGET_PAYLOAD.pack(self.yaw, self.pitch, self.yaw_speed, self.pitch_speed,
                                      self.yaw_acc, self.pitch_acc, self.distance,
                                      self.shoot, self.target_id)
        return encode_frame(TARGET_COMMAND, self.sequence, payload)


def valid_target(frame):
    values = (frame.yaw, frame.pitch, frame.yaw_speed, frame.pitch_speed,
              frame.yaw_acc, frame.pitch_acc, frame.distance)
    return (all(math.isfinite(value) for value in values) and frame.distance >= 0.0 and
            0 <= frame.shoot <= 3 and 0 <= frame.target_id <= 9)


class Parser:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while self.buffer:
            try:
                start = self.buffer.index(SOF)
            except ValueError:
                self.buffer.clear()
                break
            if start:
                del self.buffer[:start]
            if len(self.buffer) < 5:
                break
            if crc8(self.buffer[:4]) != self.buffer[4]:
                del self.buffer[0]
                continue
            payload_size = struct.unpack_from('<H', self.buffer, 1)[0]
            if payload_size > MAX_PAYLOAD:
                del self.buffer[0]
                continue
            frame_size = payload_size + 9
            if len(self.buffer) < frame_size:
                break
            raw = self.buffer[:frame_size]
            if crc16(raw[:-2]) != struct.unpack_from('<H', raw, frame_size-2)[0]:
                del self.buffer[0]
                continue
            command = struct.unpack_from('<H', raw, 5)[0]
            if command == POSE_COMMAND and payload_size == POSE_PAYLOAD.size:
                yaw, pitch, roll, speed, mode, sample_ms, robot_id = POSE_PAYLOAD.unpack(raw[7:-2])
                if all(math.isfinite(value) for value in (yaw, pitch, roll, speed)):
                    frames.append(PoseFrame(raw[3], sample_ms, yaw, pitch, roll, speed,
                                            mode, robot_id))
            elif command == FULL_POSE_COMMAND and payload_size == FULL_POSE_PAYLOAD.size:
                values = FULL_POSE_PAYLOAD.unpack(raw[7:-2])
                floats = values[:8]
                norm2 = sum(value * value for value in floats[:4])
                if (all(math.isfinite(value) for value in floats) and norm2 > 1e-12 and
                        values[12] & 0xfc00 == 0):
                    frames.append(FullPoseFrame(raw[3], *values))
            elif command == TARGET_COMMAND and payload_size == TARGET_PAYLOAD.size:
                values = TARGET_PAYLOAD.unpack(raw[7:-2])
                target = TargetFrame(raw[3], *values)
                if valid_target(target):
                    frames.append(target)
            del self.buffer[:frame_size]
        return frames
