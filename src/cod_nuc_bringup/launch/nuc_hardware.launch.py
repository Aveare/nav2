"""Physical chassis navigation. Do not combine with legacy COD or loopback launches."""
import os
import math
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml


def setup(context):
    share = get_package_share_directory('cod_nuc_bringup')
    cod = get_package_share_directory('cod_bringup')
    value = lambda name: LaunchConfiguration(name).perform(context)
    enabled = lambda name: value(name).lower() == 'true'
    mode = value('mode')
    if mode not in ('localization', 'mapping'):
        raise ValueError('mode must be localization or mapping')
    with open(value('lio_params_file'), encoding='utf-8') as stream:
        lio = yaml.safe_load(stream)['small_point_lio']['ros__parameters']
    if lio['extrinsic_est_en']:
        raise ValueError('Hardware output requires fixed, calibrated T_I_L (extrinsic_est_en=false)')
    if not enabled('imu_extrinsics_confirmed'):
        raise ValueError('MID-360S internal T_I_L is unverified: calibrate lio_params_file, then set imu_extrinsics_confirmed:=true')
    def include(path, args=None):
        return IncludeLaunchDescription(PythonLaunchDescriptionSource(path),
                                        launch_arguments=(args or {}).items())
    nodes = [
        include(os.path.join(share, 'launch', 'mid360s_mount.launch.py')),
        Node(package='cod_nuc_bringup', executable='big_yaw_tf.py', output='screen'),
        Node(package='small_point_lio', executable='small_point_lio_node', name='small_point_lio',
             output='screen', parameters=[lio, {'raw_imu_output': True, 'use_sim_time': False}], remappings=[
                 ('/Odometry', '/lio/imu_odometry'), ('/cloud_registered', '/lio/world_cloud')]),
        Node(package='cod_nuc_bringup', executable='chassis_odometry.py', output='screen', parameters=[{
            'extrinsic_T': lio['extrinsic_T'], 'extrinsic_R': lio['extrinsic_R'],
            'imu_extrinsics_confirmed': True}]),
        Node(package='pointcloud_to_laserscan', executable='pointcloud_to_laserscan_node',
             name='pointcloud_to_laserscan', output='screen',
             remappings=[('cloud_in', '/livox/lidar'), ('scan', '/scan')], parameters=[{
                 'target_frame': 'base_link', 'transform_tolerance': 0.2,
                 'min_height': 0.1, 'max_height': 1.0, 'scan_time': 0.1,
                 'angle_min': -3.141592653589793, 'angle_max': 3.141592653589793,
                 'angle_increment': 0.008726646259972, 'range_min': 0.5,
                 'range_max': 20.0, 'use_inf': True, 'use_sim_time': False}]),
    ]
    if enabled('start_lidar'):
        nodes.append(include(os.path.join(cod, 'launch', 'mid360s_lidar.launch.py'), {'lidar_rviz': 'false'}))
    if mode == 'mapping':
        nodes.append(Node(package='slam_toolbox', executable='async_slam_toolbox_node',
                          name='slam_toolbox', output='screen', parameters=[
                              os.path.join(share, 'config', 'hardware_slam.yaml')]))
    else:
        nodes.extend([
            Node(package='nav2_map_server', executable='map_server', name='map_server', output='screen',
                 parameters=[{'yaml_filename': value('map'), 'use_sim_time': False}]),
            Node(package='nav2_amcl', executable='amcl', name='amcl', output='screen', parameters=[
                os.path.join(share, 'config', 'hardware_amcl.yaml')]),
            Node(package='nav2_lifecycle_manager', executable='lifecycle_manager',
                 name='lifecycle_manager_hardware_localization', output='screen', parameters=[{
                     'autostart': True, 'use_sim_time': False, 'node_names': ['map_server', 'amcl']}]),
        ])
    if enabled('start_navigation'):
        # Override the unmeasured legacy footprint; require the caller's measured polygon.
        import ast
        footprint = ast.literal_eval(value('footprint'))
        if len(footprint) < 3 or any(len(point) != 2 or not all(math.isfinite(v) for v in point) for point in footprint):
            raise ValueError('footprint must contain the measured base_link polygon (at least 3 xy points)')
        rewrites = {'footprint': value('footprint'),
                    'behavior_server.ros__parameters.robot_radius': str(max(math.hypot(*point) for point in footprint))}
        for costmap in ('local_costmap', 'global_costmap'):
            prefix = costmap + '.' + costmap + '.ros__parameters.stvl_voxel_layer.'
            rewrites[prefix + 'observation_sources'] = 'livox_source'
            rewrites[prefix + 'livox_source.topic'] = '/livox/lidar'
        nodes.append(include(os.path.join(cod, 'launch', 'navigation_launch.py'), {
            'params_file': RewrittenYaml(source_file=value('nav2_params_file'), param_rewrites=rewrites, convert_types=True),
            'use_sim_time': 'false', 'use_composition': 'False'}))
    if enabled('start_serial'):
        nodes.append(Node(package='cod_nuc_bringup', executable='sentry_usb_bridge.py', output='screen',
                          parameters=[{'port': value('serial_port'), 'enable_drive': enabled('enable_drive')}]))
    return nodes


def generate_launch_description():
    cod = get_package_share_directory('cod_bringup')
    return LaunchDescription([
        DeclareLaunchArgument('mode', default_value='localization'),
        DeclareLaunchArgument('start_lidar', default_value='true'),
        DeclareLaunchArgument('start_navigation', default_value='false'),
        DeclareLaunchArgument('start_serial', default_value='true'),
        DeclareLaunchArgument('serial_port', default_value='/dev/ttyACM0'),
        DeclareLaunchArgument('enable_drive', default_value='false'),
        DeclareLaunchArgument('imu_extrinsics_confirmed', default_value='false'),
        DeclareLaunchArgument('footprint', default_value='[]'),
        DeclareLaunchArgument('map', default_value=os.path.join(cod, 'maps', 'rmul2026.yaml')),
        DeclareLaunchArgument('lio_params_file', default_value=os.path.join(cod, 'params', 'mid360s_lio.yaml')),
        DeclareLaunchArgument('nav2_params_file', default_value=os.path.join(cod, 'params', 'multiplenav2_params.yaml')),
        OpaqueFunction(function=setup),
    ])
