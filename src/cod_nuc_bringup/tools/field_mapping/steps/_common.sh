#!/usr/bin/env bash

FIELD_STEPS_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
FIELD_MAP_DIR="/home/rm/maps"
FIELD_MAP_BASE="${FIELD_MAP_DIR}/test"
FIELD_CENTER_FILE="${FIELD_MAP_DIR}/test_center.csv"
FIELD_RED_SPAWN_FILE="${FIELD_MAP_DIR}/test_red_spawn.csv"
FIELD_BLUE_SPAWN_FILE="${FIELD_MAP_DIR}/test_blue_spawn.csv"
FIELD_SERIAL_PORT="/dev/serial/by-id/usb-YueLuEmbedded_Vision_Comm_port_305E356D3034-if00"
FIELD_LOG_DIR="/home/rm/cod_deploy/logs"
FIELD_MAPPING_PID_FILE="${FIELD_LOG_DIR}/hardware_mapping_session.pid"
FIELD_LOCALIZATION_PID_FILE="${FIELD_LOG_DIR}/field_localization_session.pid"
FIELD_CENTER_PID_FILE="${FIELD_LOG_DIR}/field_center_display_session.pid"
FIELD_RVIZ_CONFIG="/home/rm/cod_ws/install/cod_nuc_bringup/share/cod_nuc_bringup/config/rmul2026_sim.rviz"
export TZ=Asia/Shanghai

field_ensure_terminal() {
  local title="$1"
  if [[ "${FIELD_IN_TERMINAL:-0}" != "1" && ! -t 1 ]]; then
    exec gnome-terminal --title="${title}" -- bash -lc \
      'export FIELD_IN_TERMINAL=1; bash "$1"; code=$?; echo; read -r -p "按回车关闭窗口……"; exit "$code"' \
      _ "$0"
  fi
}

field_load_ros() {
  if [[ ! -r /home/rm/cod_deploy/env.sh ]]; then
    echo "[FAIL] 找不到 /home/rm/cod_deploy/env.sh"
    return 1
  fi
  set +u
  # shellcheck disable=SC1091
  source /home/rm/cod_deploy/env.sh
  set -u
}

field_read_pid() {
  local file="$1"
  [[ -r "${file}" ]] || return 1
  local pid
  pid="$(tr -dc '0-9' < "${file}")"
  [[ -n "${pid}" ]] || return 1
  printf '%s\n' "${pid}"
}

field_pid_has_token() {
  local pid="$1"
  local token="$2"
  kill -0 "${pid}" 2>/dev/null || return 1
  [[ -r "/proc/${pid}/cmdline" ]] || return 1
  tr '\0' ' ' < "/proc/${pid}/cmdline" | grep -Fq "${token}"
}

field_stop_group() {
  local pid="$1"
  local signal
  [[ -n "${pid}" ]] || return 0
  kill -0 "${pid}" 2>/dev/null || return 0
  for signal in INT TERM KILL; do
    kill -"${signal}" -- "-${pid}" 2>/dev/null || kill -"${signal}" "${pid}" 2>/dev/null || true
    for _ in $(seq 1 30); do
      kill -0 "${pid}" 2>/dev/null || return 0
      sleep 0.1
    done
  done
}

field_stop_wrapper() {
  local pid="$1"
  kill -0 "${pid}" 2>/dev/null || return 0
  kill -INT "${pid}" 2>/dev/null || true
  for _ in $(seq 1 150); do
    kill -0 "${pid}" 2>/dev/null || return 0
    sleep 0.1
  done
  field_stop_group "${pid}"
}

field_stop_owned_session() {
  local pid_file="$1"
  local token="$2"
  local label="$3"
  local pid
  pid="$(field_read_pid "${pid_file}")" || {
    echo "[INFO] ${label}没有本套脚本记录的运行会话。"
    return 0
  }
  if ! field_pid_has_token "${pid}" "${token}"; then
    echo "[FAIL] ${pid_file} 指向的不是 ${label}，出于安全原因不终止 PID=${pid}。"
    return 1
  fi
  field_stop_wrapper "${pid}"
  rm -f "${pid_file}"
  echo "[PASS] ${label}已停止。"
}

field_print_next() {
  echo
  echo "[NEXT] 下一步：$1"
}
