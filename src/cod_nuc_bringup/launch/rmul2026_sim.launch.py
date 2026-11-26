import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    share = get_package_share_directory('cod_nuc_bringup')
    nav = get_package_share_directory('cod_bringup')
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='true'),
        DeclareLaunchArgument('map', default_value=os.path.join(nav, 'maps', 'rmul2026.yaml')),
        DeclareLaunchArgument('waypoint_file', default_value='/home/rm/maps/rmul2026_waypoints.csv'),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(share, 'launch', 'nuc_sim.launch.py')),
            launch_arguments={'gui': LaunchConfiguration('gui'), 'map': LaunchConfiguration('map'),
                'rviz_config': os.path.join(share, 'config', 'rmul2026_sim.rviz')}.items()),
        Node(package='waypoint_editor', executable='waypoint_through_nav2', name='waypoint_through_nav2',
             parameters=[{'waypoint_file': LaunchConfiguration('waypoint_file'), 'frame_id': 'map'}], output='screen'),
    ])
