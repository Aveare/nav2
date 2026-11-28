"""Local sentry USB v1 contract, shared with components/navigation/nav_protocol.h."""
from dataclasses import dataclass
import math
import struct

COMMAND, FEEDBACK, SYNC_REQUEST, SYNC_RESPONSE = 1, 2, 3, 4
VALID = 1
HEADER = struct.Struct('<2sBBIQfffH')
FRAME_SIZE = 32


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


@dataclass(frozen=True)
class Frame:
    kind: int
    sequence: int
    sample_us: int
    values: tuple = (0.0, 0.0, 0.0)
    flags: int = 0

    def encode(self):
        data = HEADER.pack(b'NV', 1, self.kind, self.sequence & 0xffffffff,
                           self.sample_us, *self.values, self.flags)
        return data + struct.pack('<H', crc16(data))


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.frames_ok = 0
        self.crc_errors = 0
        self.resync_bytes = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while len(self.buffer) >= FRAME_SIZE:
            raw = self.buffer[:FRAME_SIZE]
            if raw[:3] != b'NV\x01':
                self.resync_bytes += 1
                del self.buffer[0]
                continue
            if crc16(raw[:30]) != struct.unpack_from('<H', raw, 30)[0]:
                self.crc_errors += 1
                self.resync_bytes += 1
                del self.buffer[0]
                continue
            _, _, kind, sequence, sample_us, x, y, z, flags = HEADER.unpack(raw[:30])
            frames.append(Frame(kind, sequence, sample_us, (x, y, z), flags))
            self.frames_ok += 1
            del self.buffer[:FRAME_SIZE]
        return frames


def valid_command(values):
    return all(math.isfinite(v) and abs(v) <= limit for v, limit in zip(values, (.35001, .35001, .50001)))


class ClockSync:
    """Midpoint estimate from lowest RTT of recent exchanges; no hardware sync claim."""
    def __init__(self):
        self.pending = {}
        self.samples = []
        self.offset_ns = None
        self.updated = float('-inf')
        self.last_rtt_s = None

    def request(self, sequence, ros_ns, monotonic):
        self.pending = {sequence: (ros_ns, monotonic)}

    def response(self, frame, ros_ns, monotonic):
        sent = self.pending.pop(frame.sequence, None)
        if sent is None:
            return False
        start_ns, start_monotonic = sent
        elapsed = monotonic - start_monotonic
        if not 0 < elapsed <= .02 or abs((ros_ns-start_ns)*1e-9 - elapsed) > .005:
            return False
        offset = (start_ns+ros_ns)//2-frame.sample_us*1000
        self.samples = [s for s in self.samples if monotonic-s[0] < 10.0]
        self.samples.append((monotonic, elapsed, offset))
        self.samples = self.samples[-20:]
        self.offset_ns = min(self.samples, key=lambda sample: sample[1])[2]
        self.updated = monotonic
        self.last_rtt_s = elapsed
        return True

    def stamp(self, sample_us, ros_ns, monotonic):
        if self.offset_ns is None or monotonic-self.updated > 2.0:
            return None
        stamp = sample_us*1000+self.offset_ns
        return stamp if -.02 <= (ros_ns-stamp)*1e-9 <= .2 else None
