import math
import pathlib
import sys

import pytest


TOOLS = pathlib.Path(__file__).parents[1] / "tools" / "center_point"
sys.path.insert(0, str(TOOLS))

from center_point_file import (  # noqa: E402
    CenterPoint,
    load_center_point,
    quaternion_from_yaw_degrees,
    save_center_point,
)


def test_round_trip_and_quaternion_normalization(tmp_path):
    qx, qy, qz, qw = quaternion_from_yaw_degrees(90.0)
    path = tmp_path / "center.csv"
    save_center_point(path, CenterPoint(1.25, -2.5, qx=qx, qy=qy, qz=qz, qw=qw))

    loaded = load_center_point(path)
    assert loaded.x == pytest.approx(1.25)
    assert loaded.y == pytest.approx(-2.5)
    assert loaded.qz == pytest.approx(math.sqrt(0.5))
    assert loaded.qw == pytest.approx(math.sqrt(0.5))


def test_existing_file_is_protected(tmp_path):
    path = tmp_path / "center.csv"
    save_center_point(path, CenterPoint(0.0, 0.0))
    with pytest.raises(FileExistsError):
        save_center_point(path, CenterPoint(1.0, 1.0))
    assert load_center_point(path).x == pytest.approx(0.0)


def test_invalid_csv_and_quaternion_are_rejected(tmp_path):
    path = tmp_path / "broken.csv"
    path.write_text("pose_x,pose_y\n1,2\n", encoding="utf-8")
    with pytest.raises(ValueError, match="missing columns"):
        load_center_point(path)
    with pytest.raises(ValueError, match="zero length"):
        CenterPoint(0.0, 0.0, qw=0.0).validated()
