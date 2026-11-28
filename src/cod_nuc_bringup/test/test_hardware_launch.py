import importlib.util
from pathlib import Path
import unittest
import yaml
from launch import LaunchContext
from launch.actions import IncludeLaunchDescription
from launch.utilities import perform_substitutions, normalize_to_list_of_substitutions
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

spec = importlib.util.spec_from_file_location('hardware_launch', Path(__file__).resolve().parents[1] / 'launch/nuc_hardware.launch.py')
hardware = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hardware)


class HardwareLaunchTest(unittest.TestCase):
    def context(self, mode):
        cod = Path(get_package_share_directory('cod_bringup'))
        context = LaunchContext()
        context.launch_configurations.update({
            'mode': mode, 'start_lidar': 'false', 'start_navigation': 'true',
            'start_serial': 'false', 'imu_extrinsics_confirmed': 'true',
            'footprint': '[[0.4, 0.3], [0.4, -0.3], [-0.4, -0.3], [-0.4, 0.3]]',
            'map': str(cod/'maps/rmul2026.yaml'),
            'lio_params_file': str(cod/'params/mid360s_lio.yaml'),
            'nav2_params_file': str(cod/'params/multiplenav2_params.yaml'),
        })
        return context

    def test_modes_and_rewritten_navigation(self):
        for mode in ('mapping', 'localization'):
            context = self.context(mode)
            actions = hardware.setup(context)
            packages = [perform_substitutions(context, normalize_to_list_of_substitutions(node.node_package)) for node in actions if isinstance(node, Node)]
            self.assertNotIn('fake_vel_transform', packages)
            self.assertNotIn('cod_serial_ul26', packages)
            self.assertEqual(packages.count('slam_toolbox'), int(mode == 'mapping'))
            self.assertEqual(packages.count('nav2_amcl'), int(mode == 'localization'))
            executables = [perform_substitutions(context, normalize_to_list_of_substitutions(node.node_executable)) for node in actions if isinstance(node, Node)]
            self.assertEqual(executables.count('spawn_initial_pose.py'), int(mode == 'localization'))
            for action in actions:
                if isinstance(action, IncludeLaunchDescription):
                    args = dict(action.launch_arguments)
                    if 'params_file' in args:
                        filename = perform_substitutions(context, normalize_to_list_of_substitutions(args['params_file']))
                        try:
                            with open(filename) as stream:
                                params = yaml.safe_load(stream)
                            for name in ('local_costmap', 'global_costmap'):
                                p = params[name][name]['ros__parameters']
                                self.assertEqual(p['robot_base_frame'], 'base_link')
                                self.assertEqual(p['footprint'], context.launch_configurations['footprint'])
                                self.assertAlmostEqual(p['robot_radius'], .5)
                                self.assertEqual(p['stvl_voxel_layer']['observation_sources'], 'livox_source')
                                self.assertEqual(p['stvl_voxel_layer']['livox_source']['topic'], '/livox/lidar')
                            self.assertEqual(params['velocity_smoother']['ros__parameters']['max_velocity'], [.35, .35, .5])
                            self.assertAlmostEqual(params['behavior_server']['ros__parameters']['robot_radius'], .5)
                        finally:
                            Path(filename).unlink()

    def test_unverified_calibration_and_missing_footprint(self):
        context = self.context('localization')
        context.launch_configurations['imu_extrinsics_confirmed'] = 'false'
        with self.assertRaisesRegex(ValueError, 'T_I_L'):
            hardware.setup(context)
        context.launch_configurations['imu_extrinsics_confirmed'] = 'true'
        context.launch_configurations['footprint'] = '[]'
        with self.assertRaisesRegex(ValueError, 'footprint'):
            hardware.setup(context)

    def test_serial_bridge_defaults_to_auto_by_id_resolution(self):
        context = self.context('localization')
        context.launch_configurations['start_serial'] = 'true'
        context.launch_configurations['serial_port'] = 'auto'
        actions = hardware.setup(context)
        serial_nodes = [
            node for node in actions
            if isinstance(node, Node) and perform_substitutions(
                context, normalize_to_list_of_substitutions(node.node_executable)
            ) == 'sentry_usb_bridge.py'
        ]
        self.assertEqual(len(serial_nodes), 1)
        ld = hardware.generate_launch_description()
        serial_arg = [e for e in ld.entities if getattr(e, 'name', None) == 'serial_port'][0]
        self.assertEqual(perform_substitutions(context, serial_arg.default_value), 'auto')


if __name__ == '__main__':
    unittest.main()
