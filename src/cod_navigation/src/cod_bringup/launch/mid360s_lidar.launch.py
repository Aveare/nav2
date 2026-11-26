"""MID360S sensor input for COD; does not start chassis control or navigation."""
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    share = get_package_share_directory('cod_bringup')
    return LaunchDescription([
        DeclareLaunchArgument('lidar_config', default_value=os.path.join(share, 'params', 'MID360s_config.json')),
        DeclareLaunchArgument('lidar_rviz', default_value='true'),
        Node(package='livox_ros_driver2', executable='livox_ros_driver2_node',
             name='livox_lidar_publisher', output='screen',
             parameters=[os.path.join(share, 'params', 'mid360s_driver.yaml'),
                         {'user_config_path': LaunchConfiguration('lidar_config')}]),
        Node(package='rviz2', executable='rviz2', name='cod_lidar_rviz', output='screen',
             arguments=['-d', os.path.join(share, 'rviz', 'mid360s_points.rviz')],
             condition=IfCondition(LaunchConfiguration('lidar_rviz'))),
    ])
