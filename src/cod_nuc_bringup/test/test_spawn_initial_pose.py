"""Tests for automatic AMCL /initialpose initialization from spawn CSVs."""

from __future__ import annotations

import pathlib
import sys
import unittest
from unittest.mock import Mock

import pytest
import rclpy
from geometry_msgs.msg import PoseWithCovarianceStamped
from rclpy.parameter import Parameter
from rm_interfaces.msg import AutoAimPose

SCRIPTS = pathlib.Path(__file__).resolve().parents[1] / "scripts"
TOOLS = pathlib.Path(__file__).resolve().parents[1] / "tools" / "center_point"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(TOOLS))

from center_point_file import CenterPoint, save_center_point  # noqa: E402
from spawn_initial_pose import (  # noqa: E402
    DEFAULT_COV_XY,
    DEFAULT_COV_YAW,
    SpawnInitialPose,
    build_initial_pose_message,
    default_spawn_paths,
    resolve_spawn_side,
)


def test_resolve_spawn_side():
    assert resolve_spawn_side("auto", 7) == "red"
    assert resolve_spawn_side("auto", 107) == "blue"
    assert resolve_spawn_side("auto", 0) is None
    assert resolve_spawn_side("auto", None) is None
    assert resolve_spawn_side("red", 107) == "red"
    assert resolve_spawn_side("blue", 7) == "blue"
    assert resolve_spawn_side("none", 107) == "none"
    with pytest.raises(ValueError):
        resolve_spawn_side("invalid", 7)


def test_default_spawn_paths():
    red, blue = default_spawn_paths("/home/rm/maps/test.yaml")
    assert red == "/home/rm/maps/test_red_spawn.csv"
    assert blue == "/home/rm/maps/test_blue_spawn.csv"


class SpawnInitialPoseRosTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()

    @classmethod
    def tearDownClass(cls):
        if rclpy.ok():
            rclpy.shutdown()

    def test_build_initial_pose_message(self):
        node = rclpy.create_node("test_build_initial_pose")
        try:
            stamp = node.get_clock().now().to_msg()
            msg = build_initial_pose_message(
                CenterPoint(1.5, -2.0, 0.3, 0.0, 0.0, 1.0, 0.0),
                stamp=stamp,
            )
            self.assertEqual(msg.header.frame_id, "map")
            self.assertAlmostEqual(msg.pose.pose.position.x, 1.5)
            self.assertAlmostEqual(msg.pose.pose.position.y, -2.0)
            self.assertAlmostEqual(msg.pose.pose.position.z, 0.0)
            self.assertAlmostEqual(msg.pose.pose.orientation.z, 1.0)
            self.assertAlmostEqual(msg.pose.covariance[0], DEFAULT_COV_XY)
            self.assertAlmostEqual(msg.pose.covariance[7], DEFAULT_COV_XY)
            self.assertAlmostEqual(msg.pose.covariance[35], DEFAULT_COV_YAW)
        finally:
            node.destroy_node()

    def test_spawn_node_publishes_and_stops_after_amcl_pose(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = pathlib.Path(tmp)
            red_csv = save_center_point(tmp_path / "test_red_spawn.csv", CenterPoint(9.6, -5.5))
            blue_csv = save_center_point(
                tmp_path / "test_blue_spawn.csv", CenterPoint(-0.4, 0.0, qz=1.0, qw=0.0)
            )
            node = SpawnInitialPose(
                parameter_overrides=[
                    Parameter("red_spawn_file", value=str(red_csv)),
                    Parameter("blue_spawn_file", value=str(blue_csv)),
                ]
            )
            try:
                node.pub = Mock()
                node.count_subscribers = Mock(return_value=1)

                node.step()
                self.assertEqual(node.pub.publish.call_count, 0)
                self.assertFalse(node.finished)

                pose_msg = AutoAimPose()
                pose_msg.robot_id = 107
                node._on_pose(pose_msg)
                node.step()
                self.assertEqual(node.pub.publish.call_count, 1)
                published = node.pub.publish.call_args.args[0]
                self.assertAlmostEqual(published.pose.pose.position.x, -0.4)
                self.assertAlmostEqual(published.pose.pose.orientation.z, 1.0)
                self.assertFalse(node.finished)

                node._on_amcl_pose(PoseWithCovarianceStamped())
                self.assertTrue(node.finished)
                node.step()
                self.assertEqual(node.pub.publish.call_count, 1)
            finally:
                node.destroy_node()

    def test_spawn_node_respects_external_initialpose_and_missing_file(self):
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = pathlib.Path(tmp)
            node = SpawnInitialPose(
                parameter_overrides=[
                    Parameter("red_spawn_file", value=str(tmp_path / "missing_red.csv")),
                    Parameter("blue_spawn_file", value=str(tmp_path / "missing_blue.csv")),
                    Parameter("spawn_side", value="red"),
                ]
            )
            try:
                node.pub = Mock()
                node.count_subscribers = Mock(return_value=1)
                node.step()
                self.assertTrue(node.finished)
                self.assertEqual(node.pub.publish.call_count, 0)
            finally:
                node.destroy_node()

        node = SpawnInitialPose()
        try:
            node.pub = Mock()
            node._on_external_initialpose(PoseWithCovarianceStamped())
            self.assertTrue(node.finished)
            node.step()
            self.assertEqual(node.pub.publish.call_count, 0)
        finally:
            node.destroy_node()
