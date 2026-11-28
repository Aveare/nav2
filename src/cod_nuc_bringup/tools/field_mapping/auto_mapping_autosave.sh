#!/usr/bin/env bash
set -Eeuo pipefail
export TZ=Asia/Shanghai

DEPLOY_DIR="/home/rm/cod_deploy"
LOG_DIR="${DEPLOY_DIR}/logs"
MAP_DIR="/home/rm/maps"
MAP_BASE="${MAP_DIR}/test"
PID_FILE="${LOG_DIR}/auto_mapping_autosave.pid"
MAPPING_PID_FILE="${LOG_DIR}/hardware_mapping_session.pid"
SAVE_INTERVAL_SEC="${AUTO_MAP_SAVE_INTERVAL_SEC:-30}"
STARTED_AT="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${LOG_DIR}/auto_mapping_autosave_${STARTED_AT}.log"

case "${SAVE_INTERVAL_SEC}" in
  ''|*[!0-9]*)
    echo "[ERROR] AUTO_MAP_SAVE_INTERVAL_SEC 必须是整数秒。"
    exit 2
    ;;
esac
if ((SAVE_INTERVAL_SEC < 10)); then
  echo "[ERROR] 自动保存间隔不能小于 10 秒。"
  exit 2
fi

mkdir -p "${LOG_DIR}" "${MAP_DIR}"
exec > >(tee -a "${LOG_FILE}") 2>&1

mapping_wrapper_pid=""
started_mapping=false

read_pid() {
  local file="$1"
  [[ -r "${file}" ]] || return 1
  local pid
  pid="$(tr -dc '0-9' < "${file}")"
  [[ -n "${pid}" ]] || return 1
  printf '%s\n' "${pid}"
}

pid_has_token() {
  local pid="$1"
  local token="$2"
  kill -0 "${pid}" 2>/dev/null || return 1
  [[ -r "/proc/${pid}/cmdline" ]] || return 1
  tr '\0' ' ' < "/proc/${pid}/cmdline" | grep -Fq "${token}"
}

stop_mapping_wrapper() {
  local pid="$1"
  local signal
  kill -0 "${pid}" 2>/dev/null || return 0
  for signal in INT TERM KILL; do
    kill -"${signal}" "${pid}" 2>/dev/null || true
    for _ in $(seq 1 100); do
      kill -0 "${pid}" 2>/dev/null || return 0
      sleep 0.1
    done
  done
}

cleanup() {
  trap - EXIT INT TERM HUP
  echo
  echo "[INFO] 正在停止 30 秒自动保存建图……"
  if [[ "${started_mapping}" == true && -n "${mapping_wrapper_pid}" ]]; then
    stop_mapping_wrapper "${mapping_wrapper_pid}"
    wait "${mapping_wrapper_pid}" 2>/dev/null || true
  fi
  rm -f "${PID_FILE}"
  echo "[INFO] 自动保存建图已停止。"
}

if old_pid="$(read_pid "${PID_FILE}")" && pid_has_token "${old_pid}" 'auto_mapping_autosave.sh'; then
  echo "[ERROR] 自动保存建图已经运行，PID=${old_pid}。"
  exit 1
fi
printf '%s\n' "$$" > "${PID_FILE}"
trap cleanup EXIT INT TERM HUP

if [[ ! -r "${DEPLOY_DIR}/env.sh" ]]; then
  echo "[ERROR] 找不到 ${DEPLOY_DIR}/env.sh。"
  exit 1
fi
set +u
# shellcheck disable=SC1091
source "${DEPLOY_DIR}/env.sh"
set -u

if pgrep -af '[n]uc_hardware.launch.py.*mode:=localization|[b]t_navigator|[c]ontroller_server' >/dev/null 2>&1; then
  echo "[ERROR] 检测到定位或导航进程，拒绝自动启动建图，避免抢占雷达、串口和 TF。"
  pgrep -af 'nuc_hardware.launch.py|bt_navigator|controller_server' || true
  exit 1
fi

existing_mapping_pid=""
if existing_mapping_pid="$(read_pid "${MAPPING_PID_FILE}")" && \
   pid_has_token "${existing_mapping_pid}" 'start_hardware_mapping.sh' && \
   pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1; then
  mapping_wrapper_pid="${existing_mapping_pid}"
  started_mapping=true
  echo "[INFO] 检测到本套脚本已启动的扫描建图，会话 PID=${mapping_wrapper_pid}，只补充自动保存。"
else
  if pgrep -af '[a]sync_slam_toolbox_node|[s]entry_usb_bridge|[l]ivox_ros_driver2' >/dev/null 2>&1; then
    echo "[ERROR] 检测到不属于本套脚本的雷达、串口或 SLAM 进程，拒绝重复启动。"
    pgrep -af 'async_slam_toolbox_node|sentry_usb_bridge|livox_ros_driver2' || true
    exit 1
  fi
  echo "[INFO] 正在启动 MID360S、STM32 串口、SLAM Toolbox 和 RViz。"
  echo "[SAFE] enable_drive=false，脚本不会控制底盘运动。"
  "${DEPLOY_DIR}/start_hardware_mapping.sh" &
  mapping_wrapper_pid=$!
  started_mapping=true
fi

echo "[INFO] 等待 /map 话题，最多 120 秒……"
map_ready=false
for _ in $(seq 1 120); do
  if [[ "${started_mapping}" == true ]] && ! kill -0 "${mapping_wrapper_pid}" 2>/dev/null; then
    echo "[ERROR] 建图启动器提前退出。"
    exit 1
  fi
  if ros2 topic list 2>/dev/null | grep -Fxq '/map'; then
    map_ready=true
    break
  fi
  sleep 1
done
if [[ "${map_ready}" != true ]]; then
  echo "[ERROR] 120 秒内没有出现 /map。"
  exit 1
fi

backup_dir="${MAP_DIR}/history/test_before_autosave_${STARTED_AT}"
history_session_dir="${MAP_DIR}/history/autosave_${STARTED_AT}"
if [[ -e "${MAP_BASE}.yaml" || -e "${MAP_BASE}.pgm" || -e "${MAP_BASE}.png" ]]; then
  mkdir -p "${backup_dir}"
  for old_map in "${MAP_BASE}.yaml" "${MAP_BASE}.pgm" "${MAP_BASE}.png"; do
    [[ -e "${old_map}" ]] && cp -a "${old_map}" "${backup_dir}/"
  done
  echo "[BACKUP] 自动保存前的旧 test 地图已备份到：${backup_dir}"
fi
mkdir -p "${history_session_dir}"

save_map_once() {
  local snapshot_stamp save_dir image_name image_source image_extension temp_image temp_yaml
  snapshot_stamp="$(date +%Y%m%d_%H%M%S)"
  save_dir="${history_session_dir}/${snapshot_stamp}"
  mkdir -p "${save_dir}"
  if ! timeout --signal=INT --kill-after=5s 25s \
       ros2 run nav2_map_server map_saver_cli -f "${save_dir}/test"; then
    echo "[WARN] 本轮地图保存失败，30 秒后重试。"
    case "${save_dir}" in
      "${history_session_dir}"/*) rm -rf -- "${save_dir}" ;;
    esac
    return 1
  fi

  image_name="$(awk '$1 == "image:" {print $2; exit}' "${save_dir}/test.yaml")"
  image_source="${save_dir}/${image_name}"
  if [[ -z "${image_name}" || ! -s "${image_source}" ]]; then
    echo "[WARN] 本轮地图图像无效，30 秒后重试。"
    case "${save_dir}" in
      "${history_session_dir}"/*) rm -rf -- "${save_dir}" ;;
    esac
    return 1
  fi

  image_extension="${image_name##*.}"
  temp_image="${MAP_BASE}.${image_extension}.autosave.$$"
  temp_yaml="${MAP_BASE}.yaml.autosave.$$"
  install -m 0644 "${image_source}" "${temp_image}"
  mv -f "${temp_image}" "${MAP_BASE}.${image_extension}"
  install -m 0644 "${save_dir}/test.yaml" "${temp_yaml}"
  mv -f "${temp_yaml}" "${MAP_BASE}.yaml"
  LAST_SNAPSHOT_DIR="${save_dir}"
  return 0
}

echo "[READY] 扫描建图已运行；请用下位机遥控机器人来回覆盖场地。"
echo "[SAVE] 30 秒后第一次保存，之后每 ${SAVE_INTERVAL_SEC} 秒生成一份永久历史并更新 /home/rm/maps/test。"
save_count=0
LAST_SNAPSHOT_DIR=""
while true; do
  sleep "${SAVE_INTERVAL_SEC}"
  if ! pgrep -f '[a]sync_slam_toolbox_node' >/dev/null 2>&1 || \
     ! ros2 topic list 2>/dev/null | grep -Fxq '/map'; then
    echo "[WARN] 建图节点或 /map 不可用，跳过本轮保存。"
    continue
  fi
  if save_map_once; then
    save_count=$((save_count + 1))
    echo "[PASS] 第 ${save_count} 次自动保存完成：$(date '+%F %T')"
    echo "[HISTORY] ${LAST_SNAPSHOT_DIR}"
    ls -lh "${MAP_BASE}.yaml" "${MAP_BASE}.pgm" 2>/dev/null || true
  fi
done
