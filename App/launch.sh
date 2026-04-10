#!/usr/bin/env bash
# Start every sensor simulator (one process per config_*.ini), wait 1s, then run the main process.
# Simulators are stopped when main exits or on Ctrl+C.
#
# Copy to device (example /home/root):
#   AppBin/SensorSimulator, AppBin/mainProc
#   config/config_MP.ini, config_*.ini, sensor_data.db
#
# Layout when this script lives next to AppBin (e.g. repo App/ or /home/root):
#   <APP_ROOT>/launch.sh
#   <APP_ROOT>/AppBin/SensorSimulator
#   <APP_ROOT>/AppBin/mainProc
#   <CONFIG_DIR>/config_MP.ini, config_*.ini, ...
#
# Override: EMS_APP_ROOT, EMS_CONFIG_DIR

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="${EMS_APP_ROOT:-$SCRIPT_DIR}"
BIN_DIR="${APP_ROOT}/AppBin"
SIM_BIN="${BIN_DIR}/SensorSimulator"
MAIN_BIN="${BIN_DIR}/mainProc"

if [[ -d "${APP_ROOT}/Sensor_Simulator/config" ]]; then
	CONFIG_DIR="${EMS_CONFIG_DIR:-${APP_ROOT}/Sensor_Simulator/config}"
else
	CONFIG_DIR="${EMS_CONFIG_DIR:-/home/root/config}"
fi

SIM_PIDS=()

cleanup() {
	local pid
	for pid in "${SIM_PIDS[@]}"; do
		kill "$pid" 2>/dev/null || true
	done
}

trap cleanup EXIT INT TERM

if [[ ! -x "$SIM_BIN" ]]; then
	echo "Missing executable: $SIM_BIN (build from Sensor_Simulator: make)" >&2
	exit 1
fi
if [[ ! -x "$MAIN_BIN" ]]; then
	echo "Missing executable: $MAIN_BIN (build from Main_Process: make)" >&2
	exit 1
fi

shopt -s nullglob
# config_MP.ini matches config_*.ini but is for mainProc only — never pass it to SensorSimulator.
configs=("${CONFIG_DIR}"/config_*.ini)
if [[ ${#configs[@]} -eq 0 ]]; then
	echo "No config_*.ini files in ${CONFIG_DIR}" >&2
	exit 1
fi

SIM_STARTED=0
for cfg in "${configs[@]}"; do
	[[ "$(basename "$cfg")" == "config_MP.ini" ]] && continue
	"$SIM_BIN" -c "$cfg" &
	SIM_PIDS+=("$!")
	SIM_STARTED=$((SIM_STARTED + 1))
done
if [[ ${SIM_STARTED} -eq 0 ]]; then
	echo "No simulator configs in ${CONFIG_DIR} (need config_*.ini other than config_MP.ini)" >&2
	exit 1
fi

sleep 1

# Foreground: when main exits, EXIT trap stops simulators.
"$MAIN_BIN" -n 7
