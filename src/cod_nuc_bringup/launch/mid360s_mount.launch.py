"""Confirmed rigid mounting; platform angle is published separately."""
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(package='tf2_ros', executable='static_transform_publisher',
             name='big_yaw_to_livox', output='screen', arguments=[
                 '--x', '-0.053', '--y', '0', '--z', '0.474',
                 '--roll', '3.141592653589793', '--pitch', '0', '--yaw', '0',
                 '--frame-id', 'big_yaw_link', '--child-frame-id', 'livox_frame']),
    ])
