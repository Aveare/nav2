import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    share = get_package_share_directory('cod_nuc_bringup')
    behavior = get_package_share_directory('cod_behavior')
    return LaunchDescription([
        Node(package='cod_behavior', executable='tree_1', name='cod_behavior', output='screen',
             parameters=[os.path.join(share, 'config', 'sim_decision.yaml'),
                         {'cod_bt_path': os.path.join(behavior, 'cod_bt', 'singlenav_basic.xml')}]),
    ])
