#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${HERE}/_common.sh"
field_ensure_terminal "05 test 地图定位与中心点 RViz"
field_load_ros

launch_pid=""
rviz_pid=""

cleanup() {
  trap - EXIT INT TERM HUP
  echo
  echo "[INFO] 正在停止 test 地图定位和 RViz……"
  field_stop_group "${rviz_pid}"
  field_stop_group "${launch_pid}"
  rm -f "${FIELD_LOCALIZATION_PID_FILE}"
  echo "[INFO] test 地图定位已停止。"
}
trap cleanup EXIT INT TERM HUP

[[ -s "${FIELD_MAP_BASE}.yaml" ]] || {
  echo "[FAIL] 找不到 ${FIELD_MAP_BASE}.yaml，请先执行步骤 03。"
  exit 1
}
[[ -e "${FIELD_SERIAL_PORT}" ]] || {
  echo "[FAIL] STM32 稳定串口不存在。"
  exit 1
}
if pgrep -af '[n]uc_hardware.launch.py|[a]sync_slam_toolbox_node|[s]entry_usb_bridge' >/dev/null 2>&1; then
  echo "[FAIL] 发现冲突的硬件、建图或串口进程："
  pgrep -af '[n]uc_hardware.launch.py|[a]sync_slam_toolbox_node|[s]entry_usb_bridge' || true
  exit 1
fi
if command -v fuser >/dev/null 2>&1 && fuser "${FIELD_SERIAL_PORT}" >/dev/null 2>&1; then
  echo "[FAIL] STM32 串口已被占用。"
  exit 1
fi
if [[ -r "${FIELD_LOCALIZATION_PID_FILE}" ]]; then
  old_pid="$(field_read_pid "${FIELD_LOCALIZATION_PID_FILE}" || true)"
  if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
    echo "[FAIL] test 地图定位已经运行，PID=${old_pid}。"
    exit 1
  fi
  rm -f "${FIELD_LOCALIZATION_PID_FILE}"
fi

mkdir -p "${FIELD_LOG_DIR}"
printf '%s\n' "$$" > "${FIELD_LOCALIZATION_PID_FILE}"
timestamp="$(date +%Y%m%d_%H%M%S)"
launch_log="${FIELD_LOG_DIR}/field_localization_${timestamp}.log"
rviz_log="${FIELD_LOG_DIR}/field_localization_rviz_${timestamp}.log"

setsid ros2 launch cod_nuc_bringup nuc_hardware.launch.py \
  mode:=localization \
  map:="${FIELD_MAP_BASE}.yaml" \
  start_lidar:=true \
  start_navigation:=false \
  start_serial:=true \
  serial_port:="${FIELD_SERIAL_PORT}" \
  enable_drive:=false \
  enable_autoaim:=false \
  start_async_workflow:=false \
  imu_extrinsics_confirmed:=true \
  > "${launch_log}" 2>&1 &
launch_pid=$!

echo "[INFO] 正在等待 test 地图，最多 90 秒……"
map_ready=false
for _ in $(seq 1 90); do
  if ! kill -0 "${launch_pid}" 2>/dev/null; then
    echo "[FAIL] 定位启动进程提前退出：${launch_log}"
    tail -100 "${launch_log}" || true
    exit 1
  fi
  if ros2 topic list 2>/dev/null | grep -Fxq '/map'; then
    map_ready=true
    break
  fi
  sleep 1
done
[[ "${map_ready}" == true ]] || {
  echo "[FAIL] 90 秒内没有出现 /map：${launch_log}"
  exit 1
}

setsid rviz2 -d "${FIELD_RVIZ_CONFIG}" > "${rviz_log}" 2>&1 &
rviz_pid=$!
echo "[READY] test 地图与中心点 RViz 已启动。"
echo "[SAFE] 未启动 Nav2 导航，底盘速度输出关闭。"
echo "[INFO] 保持此终端运行，然后双击步骤 06。"
echo "[INFO] RViz 中记录中心点必须使用 Publish Point，不要使用 Nav2 Goal。"
wait "${launch_pid}"
