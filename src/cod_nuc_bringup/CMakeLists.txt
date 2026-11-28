cmake_minimum_required(VERSION 3.8)
project(cod_nuc_bringup)
find_package(ament_cmake REQUIRED)
install(DIRECTORY launch config maps DESTINATION share/${PROJECT_NAME})
install(FILES HARDWARE_FRAMES.md DESTINATION share/${PROJECT_NAME})
install(PROGRAMS scripts/big_yaw_tf.py scripts/chassis_odometry.py scripts/sentry_usb_bridge.py
  scripts/check_lidar_obstacles.py scripts/sentry_async_workflow.py scripts/sentry_health_check.py
  scripts/spawn_initial_pose.py
  tools/center_point/record_center_point.py tools/center_point/show_center_point.py
  DESTINATION lib/${PROJECT_NAME})
install(FILES scripts/chassis_geometry.py scripts/nav_wire.py scripts/rmcv_wire.py
  tools/center_point/center_point_file.py DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY tools/center_point/ DESTINATION share/${PROJECT_NAME}/tools/center_point
  FILES_MATCHING PATTERN "*.md")
if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)
  ament_add_pytest_test(chassis_geometry test/test_chassis_geometry.py)
  ament_add_pytest_test(chassis_ros test/test_chassis_ros.py
    ENV ROS_DOMAIN_ID=217 ROS_LOCALHOST_ONLY=1)
  ament_add_pytest_test(hardware_launch test/test_hardware_launch.py)
  ament_add_pytest_test(nav_wire test/test_nav_wire.py)
  ament_add_pytest_test(rmcv_wire test/test_rmcv_wire.py)
  ament_add_pytest_test(center_point_file test/test_center_point_file.py)
  ament_add_pytest_test(usb_bridge test/test_usb_bridge.py ENV ROS_DOMAIN_ID=218 ROS_LOCALHOST_ONLY=1)
  ament_add_pytest_test(spawn_initial_pose test/test_spawn_initial_pose.py
    ENV ROS_DOMAIN_ID=219 ROS_LOCALHOST_ONLY=1)
endif()
ament_package()
