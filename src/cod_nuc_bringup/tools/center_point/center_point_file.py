"""Read and write one field center pose in Waypoint Editor CSV format."""

from __future__ import annotations

import csv
import math
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path


CSV_FIELDS = (
    "id",
    "pose_x",
    "pose_y",
    "pose_z",
    "rot_x",
    "rot_y",
    "rot_z",
    "rot_w",
    "command",
)


@dataclass(frozen=True)
class CenterPoint:
    x: float
    y: float
    z: float = 0.0
    qx: float = 0.0
    qy: float = 0.0
    qz: float = 0.0
    qw: float = 1.0

    def validated(self) -> "CenterPoint":
        values = (self.x, self.y, self.z, self.qx, self.qy, self.qz, self.qw)
        if not all(math.isfinite(value) for value in values):
            raise ValueError("center point contains a non-finite number")
        norm = math.sqrt(self.qx**2 + self.qy**2 + self.qz**2 + self.qw**2)
        if norm < 1e-9:
            raise ValueError("center point quaternion has zero length")
        return CenterPoint(
            self.x,
            self.y,
            self.z,
            self.qx / norm,
            self.qy / norm,
            self.qz / norm,
            self.qw / norm,
        )


def quaternion_from_yaw_degrees(yaw_degrees: float) -> tuple[float, float, float, float]:
    if not math.isfinite(yaw_degrees):
        raise ValueError("yaw must be finite")
    half_yaw = math.radians(yaw_degrees) / 2.0
    return 0.0, 0.0, math.sin(half_yaw), math.cos(half_yaw)


def save_center_point(path: str | Path, point: CenterPoint, overwrite: bool = False) -> Path:
    destination = Path(path).expanduser()
    if destination.exists() and not overwrite:
        raise FileExistsError(f"center point already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    point = point.validated()

    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent, text=True
    )
    try:
        with os.fdopen(file_descriptor, "w", newline="", encoding="utf-8") as output:
            writer = csv.DictWriter(output, fieldnames=CSV_FIELDS)
            writer.writeheader()
            writer.writerow(
                {
                    "id": 0,
                    "pose_x": f"{point.x:.9g}",
                    "pose_y": f"{point.y:.9g}",
                    "pose_z": f"{point.z:.9g}",
                    "rot_x": f"{point.qx:.9g}",
                    "rot_y": f"{point.qy:.9g}",
                    "rot_z": f"{point.qz:.9g}",
                    "rot_w": f"{point.qw:.9g}",
                    "command": "",
                }
            )
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary_name, destination)
    except Exception:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    return destination


def load_center_point(path: str | Path) -> CenterPoint:
    source = Path(path).expanduser()
    with source.open(newline="", encoding="utf-8-sig") as input_file:
        reader = csv.DictReader(input_file)
        missing = set(CSV_FIELDS[:-1]) - set(reader.fieldnames or ())
        if missing:
            raise ValueError(f"center point CSV is missing columns: {sorted(missing)}")
        row = next(reader, None)
        if row is None:
            raise ValueError("center point CSV contains no waypoint")
        try:
            point = CenterPoint(
                x=float(row["pose_x"]),
                y=float(row["pose_y"]),
                z=float(row["pose_z"]),
                qx=float(row["rot_x"]),
                qy=float(row["rot_y"]),
                qz=float(row["rot_z"]),
                qw=float(row["rot_w"]),
            )
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"invalid center point row: {error}") from error
    return point.validated()
