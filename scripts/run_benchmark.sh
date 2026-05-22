#!/usr/bin/env bash
# =============================================================================
#  run_benchmark.sh
#  Build → Flash → Start Server → Wait → GDB telemetry dump
#
#  Usage:  bash run_benchmark.sh [--port /dev/ttyACM0] [--baud 921600]
#                                [--no-flash] [--log-dir ./logs]
#
#  RPi auto-discovery:
#    1) Tries mDNS (raspberrypi.local)
#    2) Falls back to ARP scan for Raspberry Pi MAC vendor prefixes
#  Override manually with:  RPI_HOST=192.168.x.y ./run_benchmark.sh
# =============================================================================
set -euo pipefail

# ── Self-locate ──────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Defaults ─────────────────────────────────────────────────────────────────
PORT="${PORT:-/dev/ttyACM0}"
BAUD="${BAUD:-921600}"
STLINK_SN="${STLINK_SN:-}"
RPI_USER="${RPI_USER:-pi}"
RPI_PORT="${RPI_PORT:-/dev/ttyAMA0}"
BUILD_DIR_BASE="build"
LOG_DIR="logs"
FLASH=true
CURVE="${CURVE:-BLS12_381}"   # BN254 or BLS12_381
FLASH_VIA="${FLASH_VIA:-auto}"
PPK2_PORT="${PPK2_PORT:-/dev/ttyACM0}"
PPK2_VOLTAGE_MV="${PPK2_VOLTAGE_MV:-3300}"

# ── GDB selection: prefer gdb-multiarch, fall back to arm-none-eabi-gdb ────
if command -v gdb-multiarch &>/dev/null; then
    GDB_CMD="gdb-multiarch"
elif command -v arm-none-eabi-gdb &>/dev/null; then
    GDB_CMD="arm-none-eabi-gdb"
else
    GDB_CMD=""
fi

for arg in "$@"; do
    case "$arg" in
        --port=*)    PORT="${arg#*=}" ;;
        --baud=*)    BAUD="${arg#*=}" ;;
        --log-dir=*) LOG_DIR="${arg#*=}" ;;
        --no-flash)  FLASH=false ;;
        --curve=*)      CURVE="${arg#*=}" ;;
        --flash-via=*)  FLASH_VIA="${arg#*=}" ;;
        --ppk2-port=*)  PPK2_PORT="${arg#*=}" ;;
    esac
done

case "${FLASH_VIA}" in
    auto|stlink|rpi) ;;
    *) echo "Error: --flash-via must be auto/stlink/rpi" >&2; exit 1 ;;
esac

# ── Derive curve-specific paths ──────────────────────────────────────────────
case "${CURVE}" in
    BN254)
        CURVE_SUFFIX="bn254" ;;
    BLS12_381)
        CURVE_SUFFIX="bls12_381" ;;
    *)
        echo "Error: --curve must be BN254 or BLS12_381 (got: ${CURVE})" >&2
        exit 1 ;;
esac

BUILD_DIR="${BUILD_DIR_BASE}/${CURVE_SUFFIX}"
TARGET_NAME="amore_${CURVE_SUFFIX}"
ELF="${BUILD_DIR}/${TARGET_NAME}.elf"

STAMP="$(date +%Y%m%d_%H%M%S)"
SERVER_LOG="${LOG_DIR}/server_${STAMP}.log"
STM_REPORT="${LOG_DIR}/stm_report_${STAMP}.txt"
COMBINED="${LOG_DIR}/combined_report_${STAMP}.txt"

mkdir -p "${LOG_DIR}"

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[91m'; GRN='\033[92m'; YLW='\033[93m'
BLU='\033[94m'; CYN='\033[96m'; RST='\033[0m'; BOLD='\033[1m'
ok()   { echo -e "${GRN}✓${RST} $*"; }
err()  { echo -e "${RED}✗${RST} $*"; }
info() { echo -e "${CYN}  $*${RST}"; }
head() { echo -e "\n${BOLD}${BLU}$*${RST}"; }

# ─────────────────────────────────────────────────────────────────
# RPi pre-flight checks (added 2026-05-22)
# Catches failure modes from 2026-05-22 debug session:
#   - py_ecc missing after RPi reflash
#   - stale processes holding /dev/ttyAMA0
#   - stale UART RX buffer
# ─────────────────────────────────────────────────────────────────
rpi_preflight() {
    local rpi_host=$1
    local rpi_user=${2:-pi}

    info "RPi pre-flight: ${rpi_user}@${rpi_host}"

    # 1. SSH reachable
    if ! timeout 5 ssh -o ConnectTimeout=3 -o BatchMode=yes "${rpi_user}@${rpi_host}" 'echo ok' >/dev/null 2>&1; then
        err "RPi SSH unreachable: ${rpi_user}@${rpi_host}"
        return 1
    fi

    # 2. server.py present
    if ! ssh "${rpi_user}@${rpi_host}" 'test -f /home/pi/amore-bn254-cortex-m4/rpi/server.py' 2>/dev/null; then
        err "server.py not found at /home/pi/amore-bn254-cortex-m4/rpi/server.py"
        return 1
    fi

    # 3. /dev/ttyAMA0 exists
    if ! ssh "${rpi_user}@${rpi_host}" 'test -e /dev/ttyAMA0' 2>/dev/null; then
        err "/dev/ttyAMA0 missing on RPi"
        return 1
    fi

    # 4. Kill any stale processes holding the port
    local stale
    stale=$(ssh "${rpi_user}@${rpi_host}" 'sudo fuser /dev/ttyAMA0 2>/dev/null | wc -w' 2>/dev/null || echo 0)
    if [ "${stale:-0}" -gt 0 ]; then
        info "  killing ${stale} stale processes on /dev/ttyAMA0..."
        ssh "${rpi_user}@${rpi_host}" 'sudo fuser -k /dev/ttyAMA0 2>/dev/null' || true
        sleep 1
    fi

    # 5. py_ecc importable (auto-install if missing)
    if ! ssh "${rpi_user}@${rpi_host}" 'python3 -c "from py_ecc.bls12_381 import pairing" 2>/dev/null'; then
        info "  py_ecc.bls12_381 missing — auto-installing..."
        if ! ssh "${rpi_user}@${rpi_host}" 'pip3 install py_ecc --break-system-packages 2>&1' >/dev/null; then
            err "py_ecc auto-install failed"
            return 1
        fi
    fi

    # 6. Drain UART RX buffer + reset baud
    ssh "${rpi_user}@${rpi_host}" '
        sudo stty -F /dev/ttyAMA0 921600 raw -echo cs8 -parenb -cstopb 2>/dev/null
        sudo timeout 1 cat /dev/ttyAMA0 > /dev/null 2>&1 || true
    ' 2>/dev/null

    ok "RPi pre-flight OK"
    return 0
}


# ── Helper: take first line of stdin/argument without using `head -1`
#     Avoids SIGPIPE issues with pipefail
first_line() {
    local input="$1"
    # Return only the first non-empty line
    while IFS= read -r line; do
        [[ -n "$line" ]] && { printf '%s' "$line"; return 0; }
    done <<< "$input"
    return 0
}

# ── Cleanup on exit ───────────────────────────────────────────────────────────
cleanup() {
    local exit_code=$?
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        info "Stopping server (PID $SERVER_PID)..."
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    # Stop PPK2 if running
    type ppk2_stop &>/dev/null && ppk2_stop 2>/dev/null || true
    if [[ $exit_code -ne 0 ]]; then
        err "Script exited with code $exit_code"
    fi
}
trap cleanup EXIT


# ═══════════════════════════════════════════════════════════════
# RPi GPIO SWD support — added 2026-05-20
# ═══════════════════════════════════════════════════════════════

detect_stlink() {
    if command -v st-info &>/dev/null; then
        # st-info --probe prints "Found N stlink programmers" where N is a count.
        # "Found 0" means NO ST-LINK present — only count >= 1 means a device.
        local probe_out
        probe_out=$(st-info --probe 2>/dev/null)
        local count
        count=$(echo "$probe_out" | grep -oE "Found [0-9]+ stlink" | grep -oE "[0-9]+" | head -1)
        if [ -n "$count" ] && [ "$count" -gt 0 ]; then
            return 0
        fi
    fi
    return 1
}

detect_rpi_swd() {
    [[ -z "${RPI_HOST:-}" ]] && return 1
    ssh -o ConnectTimeout=3 -o BatchMode=yes "${RPI_USER}@${RPI_HOST}" \
        'which openocd && test -f /home/pi/rpi_swd.cfg' &>/dev/null
}

resolve_flash_method() {
    case "${FLASH_VIA}" in
        stlink) detect_stlink && echo "stlink" || echo "missing" ;;
        rpi)    detect_rpi_swd && echo "rpi_swd" || echo "missing" ;;
        auto)
            if detect_stlink; then
                echo "stlink"
            elif detect_rpi_swd; then
                echo "rpi_swd"
            else
                echo "missing"
            fi
            ;;
    esac
}

ensure_rpi_swd_config() {
    local rpi_cfg='/home/pi/rpi_swd.cfg'
    if ssh -o BatchMode=yes "${RPI_USER}@${RPI_HOST}" "test -f ${rpi_cfg}" 2>/dev/null; then
        return 0
    fi
    info "Uploading rpi_swd.cfg to RPi..."
    local tmp_cfg=$(mktemp)
    cat > "${tmp_cfg}" << CFGEOF
adapter driver bcm2835gpio
bcm2835gpio peripheral_base 0x3F000000
bcm2835gpio speed_coeffs 194938 48
adapter gpio swclk 25
adapter gpio swdio 24
adapter gpio srst 18
transport select swd
reset_config srst_only srst_push_pull srst_nogate
adapter speed 500
source [find target/stm32f4x.cfg]
CFGEOF
    scp -q "${tmp_cfg}" "${RPI_USER}@${RPI_HOST}:${rpi_cfg}"
    rm -f "${tmp_cfg}"
    ok "Config uploaded"
}

PPK2_KEEPALIVE_PID=""

ppk2_start() {
    info "Starting PPK2 source mode @ ${PPK2_VOLTAGE_MV}mV..."
    pkill -f "ppk2_keep_alive" 2>/dev/null || true
    sleep 0.5

    # Fixed path - NOT mktemp - so the file persists
    PPK2_TMP_PY=/tmp/ppk2_keep_alive_$$.py
    cat > "${PPK2_TMP_PY}" << PYEOF_INNER
import sys, time, signal
from ppk2_api.ppk2_api import PPK2_API

def shutdown(sig, frame):
    try:
        ppk2.toggle_DUT_power("OFF")
        print("[PPK2] OFF", flush=True)
    except: pass
    sys.exit(0)

signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT, shutdown)

ppk2 = PPK2_API("${PPK2_PORT}", timeout=2, write_timeout=2)
ppk2.get_modifiers()
ppk2.set_source_voltage(${PPK2_VOLTAGE_MV})
ppk2.use_source_meter()
ppk2.toggle_DUT_power("ON")
print("[PPK2] ON", flush=True)

while True:
    time.sleep(2)
PYEOF_INNER

    # Verify file exists before invoking python
    if [[ ! -f "${PPK2_TMP_PY}" ]]; then
        err "Failed to create PPK2 keep-alive script at ${PPK2_TMP_PY}"
        return 1
    fi

    /home/kobi/amore-energy-study/.venv/bin/python3 "${PPK2_TMP_PY}" &
    PPK2_KEEPALIVE_PID=$!
    sleep 3
    
    # Verify the process is alive
    if ! kill -0 "${PPK2_KEEPALIVE_PID}" 2>/dev/null; then
        err "PPK2 keep-alive process died"
        return 1
    fi
    
    ok "PPK2 powering STM32 (PID ${PPK2_KEEPALIVE_PID})"
}

ppk2_stop() {
    if [[ -n "${PPK2_KEEPALIVE_PID:-}" ]] && kill -0 "${PPK2_KEEPALIVE_PID}" 2>/dev/null; then
        info "Stopping PPK2..."
        kill -TERM "${PPK2_KEEPALIVE_PID}" 2>/dev/null
        wait "${PPK2_KEEPALIVE_PID}" 2>/dev/null || true
        PPK2_KEEPALIVE_PID=""
    fi
}

flash_via_rpi() {
    local elf_file="$1"
    local elf_name=$(basename "${elf_file}")
    local rpi_elf="/home/pi/${elf_name}"
    
    ensure_rpi_swd_config
    
    info "Transferring ELF to RPi: ${elf_name}"
    scp -q "${elf_file}" "${RPI_USER}@${RPI_HOST}:${rpi_elf}"
    
    info "Flashing via RPi GPIO SWD..."
    ssh "${RPI_USER}@${RPI_HOST}" \
        "sudo openocd -f /home/pi/rpi_swd.cfg -c 'init; reset halt; program ${rpi_elf} verify reset exit'" 2>&1
}


# =============================================================================
#  Step 1 — Build
# =============================================================================
head "══ STEP 1: Build ══"
info "Cleaning build directory (paranoid mode)..."
rm -rf "${BUILD_DIR}"
mkdir -p "${LOG_DIR}"
BUILD_LOG="${LOG_DIR}/build_${STAMP}.log"
info "Build log: ${BUILD_LOG}"

# Force clean reconfigure — stale CMakeCache silently drops BUILD_TYPE


rm -rf "${BUILD_DIR}"


cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCURVE="${CURVE}" \
    2>&1 | tee "${BUILD_LOG}" | tail -5

if [[ "${PIPESTATUS[0]}" -ne 0 ]]; then
    err "CMake configure FAILED"
    err "Full log: ${BUILD_LOG}"
    grep -i "error\|not found\|missing\|No such" "${BUILD_LOG}" | tail -20 || true
    exit 1
fi

cmake --build "${BUILD_DIR}" --parallel "$(nproc)" \
    2>&1 | tee -a "${BUILD_LOG}" | tail -5

if [[ "${PIPESTATUS[0]}" -ne 0 ]]; then
    err "Build FAILED"
    err "Full log: ${BUILD_LOG}"
    grep -i "error:" "${BUILD_LOG}" | grep -v "^--" | tail -20 || true
    exit 1
fi
ok "Build complete: ${ELF}"

# =============================================================================
#  Step 2 — Flash STM32 (auto-detect ST-LINK USB vs RPi GPIO SWD)
# =============================================================================
if $FLASH; then
    head "══ STEP 2: Flash ══"
    
    # Discover RPi early (used for both flash and server)
    if [[ -z "${RPI_HOST:-}" ]]; then
        info "Discovering RPi..."
        mdns_output="$(getent hosts raspberrypi.local 2>/dev/null || true)"
        if [[ -n "$mdns_output" ]]; then
            RPI_HOST="$(echo "$mdns_output" | awk '{print $1}' | sed -n 1p)"
        fi
    fi
    
    FLASH_METHOD="$(resolve_flash_method)"
    info "Flash method requested: ${FLASH_VIA}"
    info "Flash method resolved : ${FLASH_METHOD}"
    
    case "${FLASH_METHOD}" in
        stlink)
            if ! command -v st-flash &>/dev/null; then
                err "st-flash not found — install stlink-tools"
                exit 1
            fi
            FLASH_BIN="${SCRIPT_DIR}/${BUILD_DIR}/${TARGET_NAME}.bin"
            info "Detaching cdc_acm so st-flash can access ST-Link..."
            sudo modprobe -r cdc_acm 2>/dev/null || true
            sleep 0.5
            info "Running: make flash via ST-LINK USB"
            if ! cmake --build "${BUILD_DIR}" --target flash 2>&1; then
                info "Retrying with sudo..."
                if ! sudo cmake --build "${BUILD_DIR}" --target flash 2>&1; then
                    sudo modprobe cdc_acm 2>/dev/null || true
                    err "Flash failed — check USB: st-info --probe"
                    exit 1
                fi
            fi
            sudo modprobe cdc_acm 2>/dev/null || true
            sleep 1
            ok "Flash done via ST-LINK USB"
            ;;
        rpi_swd)
            if [[ -z "${RPI_HOST:-}" ]]; then
                err "RPi not reachable for SWD flash"
                exit 1
            fi
            ppk2_start
            flash_output=$(flash_via_rpi "${ELF}" 2>&1)
            echo "${flash_output}" | tail -30
            if echo "${flash_output}" | grep -q 'Programming Finished' && \
               echo "${flash_output}" | grep -q 'Verified OK'; then
                ok "Flash done via RPi GPIO SWD"
            else
                err "Flash via RPi SWD failed"
                ppk2_stop
                exit 1
            fi
            ;;
        missing)
            err "No flash method available"
            err "  - ST-LINK USB: not detected"
            err "  - RPi GPIO SWD: not configured"
            exit 1
            ;;
    esac
    
    sleep 1
    info "STM32 booting"
else
    info "Skipping flash (--no-flash)"
fi

# =============================================================================
#  Step 3 — Start Python server on Raspberry Pi via SSH
# =============================================================================
head "══ STEP 3: Start server on Raspberry Pi ══"

# ── Auto-discover the RPi IP via mDNS, with ARP scan as fallback ───────────
# Can be overridden with RPI_HOST=<ip> ./run_benchmark.sh
#
# Note: avoid `head -1` to prevent SIGPIPE under pipefail.
# Instead, split the output into a bash array and take the first element.
if [[ -z "${RPI_HOST:-}" ]]; then
    info "Locating Raspberry Pi on local network..."

    # Attempt 1: mDNS (raspberrypi.local) — fastest and cleanest
    mdns_output="$(getent hosts raspberrypi.local 2>/dev/null || true)"
    if [[ -n "$mdns_output" ]]; then
        # awk returns only the first field (IP); if there are multiple lines, take the first via bash
        mdns_ips="$(echo "$mdns_output" | awk '{print $1}')"
        IFS=$'\n' read -rd '' -a ips_array < <(printf '%s\0' "$mdns_ips") || true
        RPI_HOST="${ips_array[0]:-}"
    else
        RPI_HOST=""
    fi

    # Attempt 2: ARP table — look for a Raspberry Pi Foundation MAC
    if [[ -z "$RPI_HOST" ]]; then
        info "mDNS did not respond; trying ARP scan..."
        # Compute the subnet
        route_output="$(ip -4 route 2>/dev/null || true)"
        SUBNET=""
        while IFS= read -r line; do
            if [[ "$line" == *"default"* ]]; then
                # Extract gateway IP and strip the last octet
                gw="$(echo "$line" | awk '{print $3}')"
                SUBNET="$(echo "$gw" | awk -F. '{print $1"."$2"."$3}')"
                break
            fi
        done <<< "$route_output"

        if [[ -n "$SUBNET" ]]; then
            info "Pinging subnet ${SUBNET}.0/24..."
            for i in $(seq 1 254); do
                (ping -c1 -W1 "${SUBNET}.${i}" &>/dev/null) &
            done
            wait
        fi

        # Search the ARP table by MAC vendor
        neigh_output="$(ip neigh 2>/dev/null || true)"
        while IFS= read -r line; do
            # Raspberry Pi Foundation MAC OUI prefixes
            if [[ "$line" =~ [Bb]8:27:[Ee][Bb] ]] \
               || [[ "$line" =~ [Dd][Cc]:[Aa]6:32 ]] \
               || [[ "$line" =~ [Dd]8:3[Aa]:[Dd][Dd] ]] \
               || [[ "$line" =~ [Ee]4:5[Ff]:01 ]] \
               || [[ "$line" =~ 2[Cc]:[Cc][Ff]:67 ]]; then
                RPI_HOST="$(echo "$line" | awk '{print $1}')"
                break
            fi
        done <<< "$neigh_output"
    fi

    if [[ -z "$RPI_HOST" ]]; then
        err "Could not find a Raspberry Pi on the network"
        err "You can set it manually: RPI_HOST=192.168.x.y $0"
        exit 1
    fi
    ok "RPi found at: ${RPI_HOST}"
else
    info "Using manual override: RPI_HOST=${RPI_HOST}"
fi

# Pre-flight: verify RPi is in a clean state BEFORE launching server.py
if ! rpi_preflight "${RPI_HOST}" "${RPI_USER}"; then
    err "RPi pre-flight failed — aborting"
    exit 1
fi


RPI_PORT_DEV="${RPI_PORT:-/dev/ttyAMA0}"

info "RPi host       : ${RPI_USER}@${RPI_HOST}"
info "RPi port       : ${RPI_PORT_DEV} @ ${BAUD} baud"
info "Honest rounds  : 61  (N=1 + N=10 + N=50)"
info "Server log     : ${SERVER_LOG}"
echo ""

# RPI_SERVER_PATH points at the directory on the RPi that contains server.py.
# Defaults to ~/amore-bn254-cortex-m4/rpi/ on the RPi user's home directory.
# Override with: RPI_SERVER_PATH=/some/other/dir ./run_benchmark.sh
# Note: the default uses '~' so that it expands on the RPi side,
#       not on the laptop running this script.
RPI_SERVER_PATH="${RPI_SERVER_PATH:-~/amore-bn254-cortex-m4/rpi}"

info "Using server.py from ${RPI_SERVER_PATH} on the RPi"

ssh "${RPI_USER}@${RPI_HOST}" \
    "python3 ${RPI_SERVER_PATH}/server.py --port ${RPI_PORT_DEV} --baud ${BAUD} --honest-rounds ${HONEST_ROUNDS:-61}" \
    2>&1 | tee "${SERVER_LOG}" &
SERVER_PID=$!
info "Server running on RPi (local PID=${SERVER_PID}) — waiting for all rounds..."
echo ""

wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""
echo ""
ok "Server exited — benchmark complete."

# =============================================================================
#  Step 4 — Dump STM32 telemetry via GDB
# =============================================================================
head "══ STEP 4: STM32 telemetry dump via GDB ══"

info "GDB_CMD  : ${GDB_CMD:-<not found>}"
info "openocd  : $(command -v openocd 2>/dev/null || echo '<not found>')"
info "ELF      : ${ELF}"
echo ""

if [[ -z "${GDB_CMD}" ]]; then
    err "GDB not found. Install it with:"
    err "  sudo apt-get install -y gdb-multiarch"
    exit 1
fi
ok "GDB found: ${GDB_CMD}"

if ! command -v openocd &>/dev/null; then
    err "openocd not found. Install it with:"
    err "  sudo apt-get install -y openocd"
    exit 1
fi
ok "OpenOCD found: $(command -v openocd)"

if [[ ! -f "${ELF}" ]]; then
    err "ELF does not exist: ${ELF}"
    exit 1
fi
ok "ELF exists: ${ELF}"
echo ""

# Build OpenOCD invocation based on flash method used
if [[ "${FLASH_METHOD:-}" == "rpi_swd" ]] && [[ -n "${RPI_HOST:-}" ]]; then
    info "Using RPi as remote OpenOCD for GDB"
    OPENOCD_INVOCATION="ssh ${RPI_USER}@${RPI_HOST} 'sudo openocd -f /home/pi/rpi_swd.cfg -c \"gdb_port pipe; log_output /dev/null\"'"
else
    OPENOCD_INVOCATION="openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c \"gdb_port pipe; log_output /dev/null\""
fi

GDB_SCRIPT_FILE=$(mktemp /tmp/amore_gdb_XXXX.gdb)

cat > "${GDB_SCRIPT_FILE}" << GDBEOF
set pagination off
set print pretty on

target extended-remote | ${OPENOCD_INVOCATION}

file ${ELF}

printf "\\n"
printf "=== STM32 TELEMETRY DUMP ===\\n"
printf "\\n"
printf "[Identity]\\n"
printf "  magic        = 0x%08x  (expected 0xa0aaaa00 = done)\\n", g_results.magic
printf "  fw_version   = 0x%08x\\n",    g_results.fw_version
printf "  core_mhz     = %u MHz\\n",    g_results.core_mhz
printf "\\n"

printf "[Phase]\\n"
printf "  current_phase = 0x%02x  ", g_results.current_phase
printf "  last_error    = 0x%08x\\n", g_results.last_error
printf "  error_batch   = %u  (0=N1 1=N10 2=N50 3=sec)\\n", g_results.error_batch
printf "  error_round   = %u\\n",      g_results.error_round
printf "\\n"

printf "[OneTimeSetup]\\n"
printf "  ots_cycles = %u  (%.1f ms @ 168MHz)\\n", g_results.ots_cycles, (float)g_results.ots_cycles / 168000.0
printf "  ots_ok     = %u\\n", g_results.ots_ok
printf "\\n"

printf "[Batch N=1]\\n"
printf "  sent/recv_ok/verify_ok/verify_fail/uart_err = %u %u %u %u %u\\n", g_results.rounds_sent[0], g_results.rounds_recv_ok[0], g_results.rounds_verify_ok[0], g_results.rounds_verify_fail[0], g_results.rounds_uart_err[0]
printf "  blind_total  = %llu cycles  (%.1f ms)\\n", g_results.blind_total_cycles[0], (double)g_results.blind_total_cycles[0] / 168000.0
printf "  verify_total = %llu cycles  (%.1f ms)\\n", g_results.verify_total_cycles[0], (double)g_results.verify_total_cycles[0] / 168000.0
printf "  amort/round  = %u cycles  (%.1f ms)\\n", g_results.amort_cycles[0], (float)g_results.amort_cycles[0] / 168000.0
printf "\\n"

printf "[Batch N=10]\\n"
printf "  sent/recv_ok/verify_ok/verify_fail/uart_err = %u %u %u %u %u\\n", g_results.rounds_sent[1], g_results.rounds_recv_ok[1], g_results.rounds_verify_ok[1], g_results.rounds_verify_fail[1], g_results.rounds_uart_err[1]
printf "  blind_total  = %llu cycles  (%.1f ms)\\n", g_results.blind_total_cycles[1], (double)g_results.blind_total_cycles[1] / 168000.0
printf "  verify_total = %llu cycles  (%.1f ms)\\n", g_results.verify_total_cycles[1], (double)g_results.verify_total_cycles[1] / 168000.0
printf "  amort/round  = %u cycles  (%.1f ms)\\n", g_results.amort_cycles[1], (float)g_results.amort_cycles[1] / 168000.0
printf "\\n"

printf "[Batch N=50]\\n"
printf "  sent/recv_ok/verify_ok/verify_fail/uart_err = %u %u %u %u %u\\n", g_results.rounds_sent[2], g_results.rounds_recv_ok[2], g_results.rounds_verify_ok[2], g_results.rounds_verify_fail[2], g_results.rounds_uart_err[2]
printf "  blind_total  = %llu cycles  (%.1f ms)\\n", g_results.blind_total_cycles[2], (double)g_results.blind_total_cycles[2] / 168000.0
printf "  verify_total = %llu cycles  (%.1f ms)\\n", g_results.verify_total_cycles[2], (double)g_results.verify_total_cycles[2] / 168000.0
printf "  amort/round  = %u cycles  (%.1f ms)\\n", g_results.amort_cycles[2], (float)g_results.amort_cycles[2] / 168000.0
printf "\\n"

printf "[Security check]\\n"
printf "  sec_sent           = %u\\n", g_results.sec_sent
printf "  sec_recv_ok        = %u\\n", g_results.sec_recv_ok
printf "  sec_verify_result  = %u  (0=rejected(good), 1=accepted(bug), 2=no-resp)\\n", g_results.sec_verify_result
printf "  security_ok        = %u  (1 = malicious server caught)\\n", g_results.security_ok
printf "\\n"

printf "\\n"
printf "[Per-round N=50] (extended telemetry — overflow debug)\\n"
set \$i = 0
while \$i < 50
  printf "  round %2u: blind = %10llu cyc (%6.1f ms)   verify = %10llu cyc (%6.1f ms)\\n", \
    \$i, g_results.per_round_blind_n50[\$i], (double)g_results.per_round_blind_n50[\$i] / 168000.0, \
    g_results.per_round_verify_n50[\$i], (double)g_results.per_round_verify_n50[\$i] / 168000.0
  set \$i = \$i + 1
end
printf "\\n"

printf "[Overflow detection]\\n"
printf "  overflow_detected = %u  (0 = no overflow during aggregation, 1 = overflow occurred)\\n", g_results.overflow_detected
printf "  blind_total[2]  raw uint64 = %llu  (low32: %u, high32: %u)\\n", g_results.blind_total_cycles[2], (unsigned int)(g_results.blind_total_cycles[2] & 0xFFFFFFFFu), (unsigned int)(g_results.blind_total_cycles[2] >> 32)
printf "  verify_total[2] raw uint64 = %llu  (low32: %u, high32: %u)\\n", g_results.verify_total_cycles[2], (unsigned int)(g_results.verify_total_cycles[2] & 0xFFFFFFFFu), (unsigned int)(g_results.verify_total_cycles[2] >> 32)
printf "\\n"

printf "[Totals]\\n"
printf "  total_rounds_sent = %u\\n", g_results.total_rounds_sent
printf "  total_verify_ok   = %u\\n", g_results.total_verify_ok
printf "  total_uart_errors = %u\\n", g_results.total_uart_errors
printf "  wall_ms           = %u ms  (%.1f s)\\n", g_results.wall_ms, (float)g_results.wall_ms / 1000.0
printf "\\n"

printf "[Phase ring-log (last events)]\\n"
set \$head = g_results.log_head
set \$i = 0
while \$i < 32
  set \$idx = (\$head - 32 + \$i) % 32
  set \$e = g_results.log[\$idx]
  if \$e.cycles != 0
    printf "  [%2d] phase=0x%02x batch=%u round=%4u cycles=%u extra=0x%08x\\n", \$i, \$e.phase, \$e.batch, \$e.round, \$e.cycles, \$e.extra
  end
  set \$i = \$i + 1
end
printf "\\n"

printf "[Final status]\\n"
printf "  status = 0x%08x  (0x600D0000 = all passed)\\n", g_results.status
printf "\\n"
printf "=== END STM32 DUMP ===\\n"

quit
GDBEOF

info "Running GDB..."
echo ""

# ── Run GDB and capture only relevant lines into STM_REPORT ────────────────
"${GDB_CMD}" -nx -batch \
    -x "${GDB_SCRIPT_FILE}" \
    2>&1 \
    | grep -v "^$" \
    | grep -v "^Open On-Chip" \
    | grep -v "^Licensed" \
    | grep -v "^For bug" \
    | grep -v "^http" \
    | grep -v "^Info :" \
    | grep -v "^Reading" \
    | grep -v "^warning" \
    | grep -v "^A debugging" \
    | grep -v "^.*Inferior" \
    | grep -v "^Quit anyway" \
    | grep -v "^\[Inferior" \
    | grep -v "^determining" \
    | grep -v "^0x[0-9a-f]* in" \
    | tee "${STM_REPORT}"

rm -f "${GDB_SCRIPT_FILE}"
ok "STM32 report → ${STM_REPORT}"

# =============================================================================
#  Step 5 — Combined report
# =============================================================================
head "══ STEP 5: Combined report ══"
{
    echo "AmorE BN128 Benchmark — Combined Report"
    echo "Generated: $(date)"
    echo ""
    echo "════════════════════════════════════════"
    echo "  SERVER LOG SUMMARY"
    echo "════════════════════════════════════════"
    # strip ANSI codes from server log before grepping
    sed 's/\x1b\[[0-9;]*m//g' "${SERVER_LOG}" \
        | grep -A 200 "SERVER REPORT" \
        | head -80 || true
    echo ""
    echo "════════════════════════════════════════"
    echo "  STM32 TELEMETRY"
    echo "════════════════════════════════════════"
    cat "${STM_REPORT}"
} > "${COMBINED}"

echo ""
echo -e "${BOLD}${GRN}════════════════════════════════════════${RST}"
echo -e "${BOLD}${GRN}  All done.  Reports saved to ${LOG_DIR}/  ${RST}"
echo -e "${BOLD}${GRN}════════════════════════════════════════${RST}"
echo ""
info "Server log : ${SERVER_LOG}"
info "STM report : ${STM_REPORT}"
info "Combined   : ${COMBINED}"
echo ""

# ── PASS/FAIL check: direct grep -q, no hex parsing ────────────────────────
# This is the right approach: do not try to extract and compare strings;
# simply search for the expected string directly.
if grep -qi 'status = 0x600d0000' "${STM_REPORT}" 2>/dev/null; then
    echo -e "${BOLD}${GRN}  ✓  BENCHMARK PASS  (status=0x600D0000)${RST}"
else
    # Extract the status only for the failure message
    raw_status_line="$(grep 'status = 0x' "${STM_REPORT}" 2>/dev/null || true)"
    RAW_STATUS=""
    if [[ -n "$raw_status_line" ]]; then
        # Take the first line of output (in case there are several)
        first_status_line="${raw_status_line%%$'\n'*}"
        # Extract the hex value
        if [[ "$first_status_line" =~ (0x[0-9a-fA-F]+) ]]; then
            RAW_STATUS="${BASH_REMATCH[1]}"
        fi
    fi
    echo -e "${BOLD}${RED}  ✗  BENCHMARK FAIL  (status=${RAW_STATUS:-unknown})${RST}"
    echo -e "${YLW}     Check last_error and phase ring-log in ${STM_REPORT}${RST}"
fi
echo ""
