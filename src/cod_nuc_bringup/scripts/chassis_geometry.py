"""T_A_B maps coordinates in B into A. No ROS dependencies."""
import numpy as np


def transform(position, quaternion):
    x, y, z, w = quaternion
    norm = np.linalg.norm(quaternion)
    if not np.isfinite(norm) or norm < 1e-12:
        raise ValueError('invalid quaternion')
    x, y, z, w = np.array([x, y, z, w]) / norm
    result = np.eye(4)
    result[:3, :3] = [
        [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)],
    ]
    result[:3, 3] = position
    if not np.isfinite(result).all():
        raise ValueError('non-finite transform')
    return result


def quaternion(rotation):
    # Largest diagonal component avoids the trace singularity at roll=pi.
    candidates = np.array([1+rotation[0, 0]-rotation[1, 1]-rotation[2, 2],
                           1-rotation[0, 0]+rotation[1, 1]-rotation[2, 2],
                           1-rotation[0, 0]-rotation[1, 1]+rotation[2, 2],
                           1+np.trace(rotation)])
    i = int(np.argmax(candidates))
    q = np.zeros(4)
    q[i] = np.sqrt(max(candidates[i], 0)) / 2
    k = 4*q[i]
    if i == 3:
        q[:3] = [rotation[2, 1]-rotation[1, 2], rotation[0, 2]-rotation[2, 0], rotation[1, 0]-rotation[0, 1]]
        q[:3] /= k
    else:
        j, h = (i+1) % 3, (i+2) % 3
        q[j] = (rotation[j, i]+rotation[i, j])/k
        q[h] = (rotation[h, i]+rotation[i, h])/k
        q[3] = (rotation[h, j]-rotation[j, h])/k
    return q / np.linalg.norm(q)


class ChassisGeometry:
    def __init__(self, imu_from_lidar):
        self.imu_from_lidar = imu_from_lidar
        self.odom_from_world = None

    def update(self, world_from_imu, base_from_lidar):
        base_from_imu = base_from_lidar @ np.linalg.inv(self.imu_from_lidar)
        if self.odom_from_world is None:
            # Define the initial base pose as identity, and freeze C thereafter.
            self.odom_from_world = base_from_imu @ np.linalg.inv(world_from_imu)
        return self.odom_from_world @ world_from_imu @ np.linalg.inv(base_from_imu)


def body_twist(previous, current, dt):
    if dt <= 0:
        raise ValueError('timestamps must increase')
    linear = current[:3, :3].T @ (current[:3, 3]-previous[:3, 3])/dt
    q = quaternion(previous[:3, :3].T @ current[:3, :3])
    if q[3] < 0:
        q = -q
    length = np.linalg.norm(q[:3])
    angular = np.zeros(3) if length < 1e-12 else q[:3]/length * (2*np.arctan2(length, q[3])/dt)
    return linear, angular
