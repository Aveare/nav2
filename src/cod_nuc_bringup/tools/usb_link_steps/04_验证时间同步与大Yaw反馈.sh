#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"

usb_begin_step "04" "验证 NV 时间同步与大 yaw 单帧反馈"
usb_enable_report_tee
usb_load_ros

if ! usb_bridge_is_running || ! usb_node_exists; then
  echo "[FAIL] 安全串口桥未运行，请先执行步骤 02。"
  exit 1
fi

sample_file="$(mktemp)"
trap 'rm -f "${sample_file}"' EXIT
set +e
timeout 10 ros2 topic echo --once /big_yaw/joint_states > "${sample_file}" 2>&1
status=$?
set -e
cat "${sample_file}"

if [[ ${status} -ne 0 ]]; then
  echo "[FAIL] 10 秒内没有收到 /big_yaw/joint_states。"
  echo "       可能缺少 FEEDBACK=2、SYNC_RESPONSE=4，或大 yaw CAN 反馈无效。"
  exit 1
fi

grep -Fq 'big_yaw_joint' "${sample_file}" || {
  echo "[FAIL] 收到的 JointState 不含 big_yaw_joint。"
  exit 1
}
echo "[PASS] 已收到带 ROS 时间戳的大 yaw 反馈。"
echo "[INFO] 该话题只有在 SYNC_REQUEST/RESPONSE 成功且 FEEDBACK 有效时才发布。"
echo "[LIMIT] 现行协议没有 COMMAND 接收确认帧，本步骤不能证明底盘已接收命令。"
usb_print_next "05_测量大Yaw反馈频率.sh"
