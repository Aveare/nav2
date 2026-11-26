import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    share = get_package_share_directory('cod_nuc_bringup')
    nav = get_package_share_directory('cod_bringup')
    params = os.path.join(share, 'config', 'sim_nav2.yaml')
    return LaunchDescription([
        DeclareLaunchArgument('gui', default_value='true'),
        DeclareLaunchArgument('map', default_value=os.path.join(share, 'maps', 'practice.yaml')),
        DeclareLaunchArgument('rviz_config', default_value=os.path.join(get_package_share_directory('nav2_bringup'), 'rviz', 'nav2_default_view.rviz')),
        Node(package='nav2_map_server', executable='map_server', name='map_server',
             parameters=[{'yaml_filename': LaunchConfiguration('map'), 'use_sim_time': False}], output='screen'),
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager', name='lifecycle_manager_map',
             parameters=[{'autostart': True, 'use_sim_time': False, 'node_names': ['map_server']}], output='screen'),
        Node(package='nav2_loopback_sim', executable='loopback_simulator', name='loopback_simulator',
             parameters=[{'base_frame_id': 'base_link', 'scan_frame_id': 'base_link',
                          'enable_stamped_cmd_vel': False, 'publish_clock': False}], output='screen'),
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             parameters=[{'robot_description': '<robot name="cod_practice"><link name="base_link"><visual><origin xyz="0 0 0.08"/><geometry><cylinder radius="0.22" length="0.16"/></geometry><material name="blue"><color rgba="0.2 0.5 0.9 1"/></material></visual></link></robot>'}], output='screen'),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(nav, 'launch', 'navigation_launch.py')),
             launch_arguments={'params_file': params, 'use_sim_time': 'false', 'autostart': 'true', 'use_composition': 'False'}.items()),
        Node(package='rviz2', executable='rviz2', name='rviz2',
             arguments=['-d', LaunchConfiguration('rviz_config')],
             condition=IfCondition(LaunchConfiguration('gui')), output='screen'),
    ])
