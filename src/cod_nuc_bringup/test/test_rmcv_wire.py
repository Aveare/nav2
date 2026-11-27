import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from rmcv_wire import FullPoseFrame, Parser, PoseFrame, TargetFrame, crc8, crc16, valid_target


class RMCVWireTest(unittest.TestCase):
    def test_crc_vectors_and_fragmented_pose(self):
        self.assertEqual(crc8(bytes.fromhex('a5160000')), 0x58)
        pose = PoseFrame(7, 123456, 10.0, -2.0, .5, 15.5, 1, 107)
        raw = pose.encode()
        self.assertEqual(crc16(raw[:-2]), int.from_bytes(raw[-2:], 'little'))
        corrupted = bytearray(raw)
        corrupted[12] ^= 1
        parser = Parser()
        result = []
        stream = b'NV\x01noise' + corrupted + raw + raw
        for index in range(0, len(stream), 7):
            result.extend(parser.feed(stream[index:index+7]))
        self.assertEqual(result, [pose, pose])

    def test_target_validation_and_size(self):
        target = TargetFrame(3, 30.0, -5.0, .25, -.125, 1.0, -2.0, 4.5, 1, 3)
        self.assertTrue(valid_target(target))
        raw = target.encode()
        self.assertEqual(len(raw), 39)
        parser = Parser()
        self.assertEqual(parser.feed(raw[:5]), [])
        self.assertEqual(parser.feed(raw[5:]), [target])
        self.assertFalse(valid_target(TargetFrame(0, distance=-1.0)))
        self.assertFalse(valid_target(TargetFrame(0, yaw=float('nan'))))
        self.assertFalse(valid_target(TargetFrame(0, shoot=4)))
        self.assertFalse(valid_target(TargetFrame(0, target_id=10)))

    def test_full_pose_layout_fragmentation_and_crc(self):
        pose = FullPoseFrame(
            255, 1.0, 0.0, 0.0, 0.0, .25, -.5, .125, 24.5,
            100, 101, 102, 103, 0x03ff, 1, 107)
        raw = pose.encode()
        self.assertEqual(len(raw), 61)
        self.assertEqual(raw[5:7], bytes((0x22, 0x10)))
        self.assertEqual(crc16(raw[:-2]), int.from_bytes(raw[-2:], 'little'))
        parser = Parser()
        frames = []
        for index in range(0, len(raw), 3):
            frames.extend(parser.feed(raw[index:index + 3]))
        self.assertEqual(frames, [pose])

        invalid = bytearray(raw)
        invalid[55:57] = (0xfc00).to_bytes(2, 'little')
        invalid[-2:] = crc16(invalid[:-2]).to_bytes(2, 'little')
        self.assertEqual(Parser().feed(invalid), [])


if __name__ == '__main__':
    unittest.main()
