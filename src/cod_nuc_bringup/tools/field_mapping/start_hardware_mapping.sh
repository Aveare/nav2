#!/usr/bin/env bash
set -Eeuo pipefail

SERIAL_PORT="/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_305E356D3034-if00"
RADAR_IP="192.168.1.144"
DEPLOY_DIR="/home/rm/cod_deploy"
LOG_DIR="${DEPLOY_DIR}/logs"
RVIZ_CONFIG="${DEPLOY_DIR}/hardware_mapping.rviz"
PID_FILE="${LOG_DIR}/hardware_mapping_session.pid"
STARTED_AT="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${LOG_DIR}/hardware_mapping_${STARTED_AT}.log"
CHECK_ONLY=false

case "${1:-}" in
  "") ;;
  --check) CHECK_ONLY=true ;;
  *)
    echo "用法：$0 [--check]"
    exit 2
    ;;
esac

launch_pid=""
rviz_pid=""

stop_group() {
  local pid="$1"
  local signal
  [[ -n "${pid}" ]] || return 0
  kill -0 "${pid}" 2>/dev/null || return 0
  for signal in INT TERM KILL; do
    kill -"${signal}" -- "-${pid}" 2>/dev/null || kill -"${signal}" "${pid}" 2>/dev/null || true
    for _ in $(seq 1 20); do
      kill -0 "${pid}" 2>/dev/null || return 0
      sleep 0.1
    done
  done
}

cleanup() {
  trap - EXIT INT TERM HUP
  echo
  echo "[INFO] 正在停止本次扫描建图……"
  stop_group "${rviz_pid}"
  stop_group "${launch_pid}"
  [[ -n "${rviz_pid}" ]] && wait "${rviz_pid}" 2>/dev/null || true
  [[ -n "${launch_pid}" ]] && wait "${launch_pid}" 2>/dev/null || true
  rm -f "${PID_FILE}"
  echo "[INFO] 本次扫描建图已停止。"
}
trap cleanup EXIT INT TERM HUP

mkdir -p "${LOG_DIR}" /home/rm/maps

if [[ -f "${PID_FILE}" ]]; then
  old_pid="$(tr -dc '0-9' < "${PID_FILE}")"
  if [[ -n "${old_pid}" ]] && kill -0 "${old_pid}" 2>/dev/null; then
    echo "[ERROR] 扫描建图已经在运行，终端进程 PID=${old_pid}。"
    echo "        请切换到原终端；需要结束时在原终端按 Ctrl+C。"
    exit 1
  fi
  rm -f "${PID_FILE}"
fi
printf '%s\n' "$$" > "${PID_FILE}"

if pgrep -f '[r]os2 launch cod_nuc_bringup nuc_hardware.launch.py' >/dev/null 2>&1 || \
   pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1; then
  echo "[ERROR] 检测到另一套实车导航或建图进程，拒绝重复启动。"
  pgrep -af 'nuc_hardware.launch.py|async_slam_toolbox_node' || true
  exit 1
fi

if [[ ! -r /home/rm/cod_deploy/env.sh ]]; then
  echo "[ERROR] 找不到 /home/rm/cod_deploy/env.sh。"
  exit 1
fi
# shellcheck disable=SC1091
set +u
source /home/rm/cod_deploy/env.sh
set -u

if [[ -z "${DISPLAY:-}" ]]; then
  display_value="$(systemctl --user show-environment 2>/dev/null | sed -n 's/^DISPLAY=//p' | head -n 1)"
  [[ -n "${display_value}" ]] && export DISPLAY="${display_value}"
fi
if [[ -z "${XAUTHORITY:-}" ]]; then
  xauth_value="$(systemctl --user show-environment 2>/dev/null | sed -n 's/^XAUTHORITY=//p' | head -n 1)"
  [[ -n "${xauth_value}" ]] && export XAUTHORITY="${xauth_value}"
fi

command -v ros2 >/dev/null || { echo "[ERROR] ROS 2 环境未加载。"; exit 1; }
command -v rviz2 >/dev/null || { echo "[ERROR] 找不到 rviz2。"; exit 1; }

if [[ ! -e "${SERIAL_PORT}" ]]; then
  echo "[ERROR] 未找到 STM32 稳定串口：${SERIAL_PORT}"
  echo "        请检查虚拟串口 USB 连接。"
  exit 1
fi
if [[ ! -r "${SERIAL_PORT}" || ! -w "${SERIAL_PORT}" ]]; then
  echo "[ERROR] 当前账户无权读写 ${SERIAL_PORT}。"
  exit 1
fi
if command -v fuser >/dev/null && fuser "${SERIAL_PORT}" >/dev/null 2>&1; then
  echo "[ERROR] 串口已被其他程序占用：${SERIAL_PORT}"
  fuser -v "${SERIAL_PORT}" 2>/dev/null || true
  exit 1
fi
if ! ping -c 1 -W 1 "${RADAR_IP}" >/dev/null 2>&1; then
  echo "[ERROR] MID360S 雷达 ${RADAR_IP} 不可达，请检查雷达供电和有线网配置。"
  exit 1
fi
if [[ ! -r "${RVIZ_CONFIG}" ]]; then
  echo "[ERROR] 找不到 RViz 配置：${RVIZ_CONFIG}"
  exit 1
fi

if [[ "${CHECK_ONLY}" == true ]]; then
  echo "[CHECK] 雷达、STM32 串口、ROS 2 环境和 RViz 配置均已通过检查。"
  echo "[CHECK] 未启动任何建图、导航或执行机构进程。"
  exit 0
fi

echo "============================================================"
echo " COD 实车扫描建图"
echo " 雷达：${RADAR_IP}"
echo " 串口：${SERIAL_PORT}（只读取姿态，底盘输出关闭）"
echo " 日志：${LOG_FILE}"
echo " 停止：在本终端按 Ctrl+C"
echo "============================================================"

setsid ros2 launch cod_nuc_bringup nuc_hardware.launch.py \
  mode:=mapping \
  start_lidar:=true \
  start_navigation:=false \
  start_serial:=true \
  serial_port:="${SERIAL_PORT}" \
  enable_drive:=false \
  enable_autoaim:=false \
  start_async_workflow:=false \
  imu_extrinsics_confirmed:=true \
  > >(tee -a "${LOG_FILE}") 2>&1 &
launch_pid=$!

echo "[INFO] 正在等待 /map 话题，最多 90 秒……"
map_ready=false
for _ in $(seq 1 90); do
  if ! kill -0 "${launch_pid}" 2>/dev/null; then
    echo "[ERROR] 建图启动进程提前退出，请查看日志：${LOG_FILE}"
    exit 1
  fi
  if ros2 topic list 2>/dev/null | grep -qx '/map'; then
    map_ready=true
    break
  fi
  sleep 1
done
if [[ "${map_ready}" != true ]]; then
  echo "[ERROR] 90 秒内没有出现 /map，请查看日志：${LOG_FILE}"
  exit 1
fi

echo "[INFO] /map 已就绪，正在打开 RViz……"
setsid rviz2 -d "${RVIZ_CONFIG}" >> "${LOG_FILE}" 2>&1 &
rviz_pid=$!

echo "[READY] 可以开始人工移动底盘扫描环境。"
echo "[SAFE]  本脚本不发布底盘速度；不要在建图时点击 Nav2 Goal。"
echo "[SAVE]  建图完成后按说明保存 /home/rm/maps/test，再按 Ctrl+C 停止。"
wait "${launch_pid}"
