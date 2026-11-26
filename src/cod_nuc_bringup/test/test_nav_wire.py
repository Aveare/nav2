import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from nav_wire import Frame, Parser, ClockSync, COMMAND, SYNC_RESPONSE, valid_command, crc16


class NavWireTest(unittest.TestCase):
    def test_crc_and_fragmented_noisy_stream(self):
        self.assertEqual(crc16(b'123456789'), 0x4b37)
        frame = Frame(COMMAND, 7, 123456789, (.25, -.125, .5), 1)
        parser = Parser()
        corrupted = bytearray(frame.encode())
        corrupted[19] ^= 1
        stream = b'garbage'+corrupted+frame.encode()+frame.encode()
        result = []
        for byte in stream:
            result.extend(parser.feed(bytes([byte])))
        self.assertEqual(result, [frame, frame])

    def test_nonfinite_and_speed_limits(self):
        for value in (float('nan'), float('inf'), float('-inf'), .36):
            self.assertFalse(valid_command((value, 0, 0)))
        self.assertFalse(valid_command((0, 0, .51)))
        self.assertTrue(valid_command((.35, -.35, -.5)))

    def test_sample_time_mapping_and_expiry(self):
        sync = ClockSync()
        sync.request(7, 10000000000, 1.0)
        self.assertTrue(sync.response(Frame(SYNC_RESPONSE, 7, 2000000), 10010000000, 1.01))
        self.assertEqual(sync.stamp(2000000, 10020000000, 1.02), 10005000000)
        self.assertIsNone(sync.stamp(2000000, 10020000000, 4.0))
        self.assertIsNone(sync.stamp(1000000, 10020000000, 1.02))
        sync.request(8, 10100000000, 1.1)
        self.assertFalse(sync.response(Frame(SYNC_RESPONSE, 8, 2100000), 10300000000, 1.3))


if __name__ == '__main__':
    unittest.main()
