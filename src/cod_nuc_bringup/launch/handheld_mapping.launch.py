"""Handheld MID360S mapping. The isolated base_link is a virtual sensor frame."""
import os
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace


def generate_launch_description():
    share = get_package_share_directory('cod_nuc_bringup')
    cod = get_package_share_directory('cod_bringup')
    def params(filename, node):
        with open(os.path.join(share, 'config', filename)) as stream:
            return yaml.safe_load(stream)[node]['ros__parameters']
    # Keep this virtual mapping body and all TF isolated from the real chassis.
    tf = [('/tf', 'tf'), ('/tf_static', 'tf_static')]
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='true'),
        DeclareLaunchArgument('start_lidar', default_value='true'),
        GroupAction([
            PushRosNamespace('handheld_mapping'),
            Node(package='livox_ros_driver2', executable='livox_ros_driver2_node',
                 name='livox_lidar_publisher', output='screen',
                 condition=IfCondition(LaunchConfiguration('start_lidar')),
                 parameters=[{'xfer_format': 0, 'multi_topic': 0, 'data_src': 0,
                              'publish_freq': 10.0, 'output_data_type': 0,
                              'frame_id': 'livox_frame', 'use_sim_time': False,
                              'cmdline_input_bd_code': 'livox0000000001',
                              'user_config_path': os.path.join(cod, 'params', 'MID360s_config.json')}]),
            # This identity defines a handheld reference; it is not a chassis calibration.
            Node(package='tf2_ros', executable='static_transform_publisher',
                 name='handheld_reference', output='screen', remappings=tf,
                 arguments=['--x', '0', '--y', '0', '--z', '0', '--roll', '0', '--pitch', '0', '--yaw', '0',
                            '--frame-id', 'base_link', '--child-frame-id', 'livox_frame']),
            Node(package='small_point_lio', executable='small_point_lio_node',
                 name='small_point_lio', output='screen',
                 parameters=[params('handheld_lio.yaml', 'small_point_lio')],
                 remappings=tf + [('/Odometry', 'odometry'), ('/cloud_registered', 'cloud_registered')]),
            Node(package='pointcloud_to_laserscan', executable='pointcloud_to_laserscan_node',
                 name='pointcloud_to_laserscan', output='screen', remappings=tf + [
                     ('cloud_in', '/handheld_mapping/livox/lidar'), ('scan', '/handheld_mapping/scan')],
                 parameters=[{'target_frame': 'base_link', 'transform_tolerance': 0.2,
                              'min_height': -0.2, 'max_height': 0.3,
                              'angle_min': -3.141592653589793, 'angle_max': 3.141592653589793,
                              'angle_increment': 0.008726646259972, 'scan_time': 0.1,
                              'range_min': 0.5, 'range_max': 20.0, 'use_inf': True,
                              'inf_epsilon': 1.0, 'use_sim_time': False}]),
            Node(package='slam_toolbox', executable='async_slam_toolbox_node',
                 name='slam_toolbox', output='screen',
                 remappings=tf + [('/map', 'map'), ('/map_metadata', 'map_metadata')],
                 parameters=[params('handheld_slam.yaml', 'slam_toolbox')]),
            Node(package='rviz2', executable='rviz2', name='mapping_rviz', output='screen',
                 remappings=tf, arguments=['-d', os.path.join(share, 'config', 'handheld_mapping.rviz')],
                 condition=IfCondition(LaunchConfiguration('gui'))),
        ]),
    ])
