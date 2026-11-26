import sys
from pathlib import Path
import unittest
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chassis_geometry import ChassisGeometry, transform, quaternion, body_twist


def yaw(angle):
    return transform([0, 0, 0], [0, 0, np.sin(angle/2), np.cos(angle/2)])


class GeometryTest(unittest.TestCase):
    def setUp(self):
        self.mount = transform([-.053, 0, .474], [1, 0, 0, 0])
        self.extrinsic = transform([-.011, -.02329, .04412], [0, 0, 0, 1])
        self.world_from_odom = transform([3, -2, 1], quaternion(yaw(.7)[:3, :3]))

    def test_stationary_chassis_with_rotating_offset_platform(self):
        geometry = ChassisGeometry(self.extrinsic)
        previous = None
        for angle in np.linspace(-.8, 7, 100):
            base_from_lidar = yaw(angle) @ self.mount
            world_from_imu = self.world_from_odom @ base_from_lidar @ np.linalg.inv(self.extrinsic)
            base = geometry.update(world_from_imu, base_from_lidar)
            np.testing.assert_allclose(base, np.eye(4), atol=1e-12)
            if previous is not None:
                linear, angular = body_twist(previous, base, .02)
                np.testing.assert_allclose(linear, 0, atol=1e-10)
                np.testing.assert_allclose(angular, 0, atol=1e-10)
            previous = base
            # A fixed world wall remains fixed in exported odom through all yaw angles.
            np.testing.assert_allclose(geometry.odom_from_world @ self.world_from_odom @ [2, 3, 0, 1], [2, 3, 0, 1], atol=1e-12)

    def test_moving_chassis_and_platform(self):
        geometry = ChassisGeometry(self.extrinsic)
        geometry.update(self.world_from_odom @ self.mount @ np.linalg.inv(self.extrinsic), self.mount)
        expected = yaw(.4)
        expected[:3, 3] = [1, 2, .1]
        mount = yaw(1.7) @ self.mount
        world_from_imu = self.world_from_odom @ expected @ mount @ np.linalg.inv(self.extrinsic)
        np.testing.assert_allclose(geometry.update(world_from_imu, mount), expected, atol=1e-12)

    def test_mount_quarter_turn_and_inversion(self):
        mount = yaw(np.pi/2) @ self.mount
        np.testing.assert_allclose(mount[:3, 3], [0, -.053, .474], atol=1e-12)
        np.testing.assert_allclose(self.mount[:3, :3] @ [0, 0, 1], [0, 0, -1], atol=1e-12)
        np.testing.assert_allclose(transform(self.mount[:3, 3], quaternion(self.mount[:3, :3])), self.mount, atol=1e-12)

    def test_body_velocity_and_angle_wrap(self):
        previous, current = yaw(np.pi/2), yaw(np.pi/2)
        current[:3, 3] = [0, .1, 0]
        linear, _ = body_twist(previous, current, .1)
        np.testing.assert_allclose(linear, [1, 0, 0], atol=1e-12)
        _, angular = body_twist(yaw(np.pi-.01), yaw(-np.pi+.01), .1)
        np.testing.assert_allclose(angular, [0, 0, .2], atol=1e-12)


if __name__ == '__main__':
    unittest.main()
