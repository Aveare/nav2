#pragma once
#include <behaviortree_cpp/action_node.h>
#include <behaviortree_cpp/bt_factory.h>
#include <behaviortree_cpp/blackboard.h>
#include <behaviortree_cpp/tree_node.h>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "nav2_msgs/action/navigate_through_poses.hpp"
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include "rclcpp_action/rclcpp_action.hpp"
#include "std_msgs/msg/int32.hpp"
#include <rm_interfaces/msg/serial_receive_data.hpp>
#include <coroutine>
#include <unordered_map>
#include <chrono>

#include "nav2_msgs/action/follow_waypoints.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>
#include <vector>

#include <ament_index_cpp/get_package_share_directory.hpp>

#include <../../BehaviorTree.ROS2/behaviortree_ros2/include/behaviortree_ros2/bt_action_node.hpp>

// ======================== 资源路径解析（可移植） ========================
// 原始代码在行为树 XML / 参数里写死了作者机器上的绝对路径
// （如 /home/cod-sentry/dyx_ws/cod-sentry/src/cod_bringup/wps/patrol3.csv），
// 换机器、换工作空间、只装 install/ 而不带 src/ 时必然找不到文件。
// 这里统一做一次"尽力而为"的解析，依次尝试：
//   1) 原样（绝对路径，或相对当前工作目录）
//   2) <包 share 目录>/<原路径>
//   3) <包 share 目录>/<文件名>
//   4) <包 share 目录>/<原路径的父目录名>/<文件名>   ← 如 wps/xxx.csv、cod_bt/xxx.xml
// 全部失败时打印所有候选路径并返回空串，调用方据此返回 FAILURE，
// 而不是抛异常把整棵行为树带崩。
inline std::string resolveAssetPath(const std::string &package,
                                    const std::string &requested,
                                    const std::string &what = "资源") {
        namespace fs = std::filesystem;
        std::error_code ec;

        std::vector<fs::path> candidates;
        const fs::path given(requested);
        candidates.push_back(given);

        std::string share;
        try {
                share = ament_index_cpp::get_package_share_directory(package);
        } catch (const std::exception &) {
                // 包没装（比如只编了一半），下面会打印候选并报错
                share.clear();
        }

        if (!share.empty()) {
                candidates.push_back(fs::path(share) / given);
                candidates.push_back(fs::path(share) / given.filename());
                if (!given.parent_path().empty()) {
                        candidates.push_back(fs::path(share) /
                                             given.parent_path().filename() /
                                             given.filename());
                }
        }

        for (const auto &cand : candidates) {
                if (fs::exists(cand, ec)) {
                        return fs::absolute(cand, ec).string();
                }
        }

        std::string tried;
        for (const auto &cand : candidates) {
                tried += "\n    - " + cand.string();
        }
        RCLCPP_ERROR(rclcpp::get_logger("resolveAssetPath"),
                     "%s 未找到: %s (包: %s)，已尝试:%s",
                     what.c_str(), requested.c_str(), package.c_str(), tried.c_str());
        return {};
}

//读取自定义的航点函数
geometry_msgs::msg::PoseStamped
loadPoseStamped(
    const rclcpp::Node::SharedPtr &node,
    const std::string &prefix) {
    geometry_msgs::msg::PoseStamped pose;

    pose.header.frame_id =
            node->get_parameter(prefix + ".frame_id").as_string();

    pose.pose.position.x =
            node->get_parameter(prefix + ".position.x").as_double();
    pose.pose.position.y =
            node->get_parameter(prefix + ".position.y").as_double();
    pose.pose.position.z =
            node->get_parameter(prefix + ".position.z").as_double();
    pose.pose.orientation.w =
            node->get_parameter(prefix + ".orientation.w").as_double();
    return pose;
}

// 从 waypoint_editor 生成的 CSV 文件中加载航点列表
// CSV 格式: id,pose_x,pose_y,pose_z,rot_x,rot_y,rot_z,rot_w,command,wait_sec
// wait_sec 列可选，缺省为 0（不等待）
inline bool loadWaypointsFromCSV(
    const std::string &filename,
    std::vector<geometry_msgs::msg::PoseStamped> &waypoints,
    const std::string &frame_id = "map",
    std::vector<double> *wait_times = nullptr) {
        std::ifstream file(filename);
        if (!file.is_open()) {
                return false;
        }

        std::string line;
        // 跳过 CSV 表头
        std::getline(file, line);

        while (std::getline(file, line)) {
                if (line.empty()) continue;

                std::istringstream ss(line);
                std::string token;
                std::vector<std::string> tokens;

                while (std::getline(ss, token, ',')) {
                        tokens.push_back(token);
                }

                // 至少需要 8 列: id, pose_x, pose_y, pose_z, rot_x, rot_y, rot_z, rot_w
                if (tokens.size() < 8) {
                        continue;
                }

                try {
                        geometry_msgs::msg::PoseStamped pose;
                        pose.header.frame_id = frame_id;
                        pose.header.stamp = rclcpp::Time(0);

                        pose.pose.position.x = std::stod(tokens[1]);
                        pose.pose.position.y = std::stod(tokens[2]);
                        pose.pose.position.z = std::stod(tokens[3]);

                        pose.pose.orientation.x = std::stod(tokens[4]);
                        pose.pose.orientation.y = std::stod(tokens[5]);
                        pose.pose.orientation.z = std::stod(tokens[6]);
                        pose.pose.orientation.w = std::stod(tokens[7]);

                        waypoints.push_back(pose);

                        // 第 9 列 (index 9) 为 wait_sec，可选
                        if (wait_times) {
                                double wt = 0.0;
                                if (tokens.size() > 9 && !tokens[9].empty()) {
                                        wt = std::stod(tokens[9]);
                                }
                                wait_times->push_back(wt);
                        }
                } catch (const std::exception &) {
                        continue;
                }
        }

        file.close();
        return !waypoints.empty();
}