#!/usr/bin/env bash
# =============================================================================
#  run_pairing_bench.sh
#  Build → Flash → Wait → GDB telemetry dump
#
#  v3 fix: PASS/FAIL detection uses tolerant regex (was failing on whitespace)
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

BUILD_DIR="build"
LOG_DIR="logs"
ELF="${BUILD_DIR}/relic_bench.elf"
STAMP="$(date +%Y%m%d_%H%M%S)"
STM_REPORT="${LOG_DIR}/relic_report_${STAMP}.txt"

WAIT_AFTER_FLASH_SEC="${WAIT_AFTER_FLASH_SEC:-90}"

mkdir -p "${LOG_DIR}"

RED='\033[91m'; GRN='\033[92m'; YLW='\033[93m'
BLU='\033[94m'; CYN='\033[96m'; RST='\033[0m'; BOLD='\033[1m'
ok()   { echo -e "${GRN}✓${RST} $*"; }
err()  { echo -e "${RED}✗${RST} $*"; }
info() { echo -e "${CYN}  $*${RST}"; }
sec()  { echo -e "\n${BOLD}${BLU}$*${RST}"; }

if command -v gdb-multiarch &>/dev/null; then
    GDB_CMD="gdb-multiarch"
elif command -v arm-none-eabi-gdb &>/dev/null; then
    GDB_CMD="arm-none-eabi-gdb"
else
    err "GDB not found"
    exit 1
fi

# =============================================================================
#  Step 0 — Verify prerequisites
# =============================================================================
sec "══ STEP 0: Verify RELIC library ══"
if [[ ! -f "${PROJECT_ROOT}/relic_install/lib/librelic_s.a" ]]; then
    err "RELIC not built yet"
    err "Run:  ${SCRIPT_DIR}/build-relic.sh"
    exit 1
fi
ok "Found relic_install/lib/librelic_s.a"

HEAP_VAL=$(grep -oE "_Min_Heap_Size\s*=\s*0x[0-9a-fA-F]+" \
           "${PROJECT_ROOT}/STM32F407VGTX_FLASH.ld" \
           | grep -oE "0x[0-9a-fA-F]+$" || echo "")
if [[ "${HEAP_VAL}" != "0x4000" ]]; then
    err "Linker script not patched (heap = ${HEAP_VAL:-unknown})"
    err "Run:  ${SCRIPT_DIR}/patch_linker.sh"
    exit 1
fi
ok "Linker heap = 16KB (patched)"

# =============================================================================
#  Step 1 — Build
# =============================================================================
sec "══ STEP 1: Build relic_bench.elf ══"

if [[ ! -d "${BUILD_DIR}" ]] || [[ ! -e "${BUILD_DIR}/CMakeCache.txt" ]]; then
    info "Configuring fresh build..."
    cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release \
        2>&1 | tee "${LOG_DIR}/cmake_${STAMP}.log" | tail -10
else
    cmake -S . -B "${BUILD_DIR}" 2>&1 | tail -5
fi

cmake --build "${BUILD_DIR}" --target relic_bench.elf --parallel "$(nproc)" \
    2>&1 | tee "${LOG_DIR}/build_relic_${STAMP}.log" | tail -15

if [[ "${PIPESTATUS[0]}" -ne 0 ]]; then
    err "Build failed."
    grep -i "error:" "${LOG_DIR}/build_relic_${STAMP}.log" | tail -10 || true
    exit 1
fi
ok "Built ${ELF}"

# =============================================================================
#  Step 2 — Flash
# =============================================================================
sec "══ STEP 2: Flash relic_bench.bin ══"
if ! command -v st-flash &>/dev/null; then
    err "st-flash not found"
    exit 1
fi

info "Detaching cdc_acm..."
sudo modprobe -r cdc_acm 2>/dev/null || true
sleep 0.5

if ! cmake --build "${BUILD_DIR}" --target flash_relic 2>&1; then
    info "Retrying with sudo..."
    if ! sudo cmake --build "${BUILD_DIR}" --target flash_relic 2>&1; then
        sudo modprobe cdc_acm 2>/dev/null || true
        err "Flash failed"
        exit 1
    fi
fi

sudo modprobe cdc_acm 2>/dev/null || true
sleep 1
ok "Flashed."

# =============================================================================
#  Step 3 — Wait
# =============================================================================
sec "══ STEP 3: Waiting ${WAIT_AFTER_FLASH_SEC}s for benchmark to complete... ══"
info "(STM32 is running 10 pairings — pure C is slower than asm; could take ~30-60s)"
for i in $(seq 1 "${WAIT_AFTER_FLASH_SEC}"); do
    sleep 1
    if (( i % 10 == 0 )); then
        info "  ... ${i}s elapsed"
    fi
done

# =============================================================================
#  Step 4 — GDB telemetry
# =============================================================================
sec "══ STEP 4: GDB telemetry dump ══"

GDB_SCRIPT=$(mktemp /tmp/relic_bench_XXXX.gdb)
cat > "${GDB_SCRIPT}" << GDBEOF
set pagination off
set print pretty on

target extended-remote | openocd \\
    -f interface/stlink.cfg \\
    -f target/stm32f4x.cfg \\
    -c "gdb_port pipe; log_output /dev/null"

file ${ELF}

printf "\\n=== RELIC PAIRING BENCHMARK — TELEMETRY ===\\n\\n"

printf "[Identity]\\n"
printf "  magic        = 0x%08x  (expected 0xb0ccaa00)\\n", g_pb_results.magic
printf "  fw_version   = 0x%08x\\n",  g_pb_results.fw_version
printf "  core_mhz     = %u MHz\\n",  g_pb_results.core_mhz
printf "\\n"

printf "[Phase / Errors]\\n"
printf "  current_phase   = 0x%02x\\n", g_pb_results.current_phase
printf "  last_error      = 0x%08x\\n", g_pb_results.last_error
printf "  error_iteration = %u\\n",     g_pb_results.error_iteration
printf "\\n"

printf "[Init]\\n"
printf "  cycles = %u  (%.1f ms)\\n", g_pb_results.init_cycles, (float)g_pb_results.init_cycles / 168000.0
printf "  ok     = %u\\n", g_pb_results.init_ok
printf "\\n"

printf "[Sanity (g1 mul)]\\n"
printf "  cycles = %u  (%.2f ms)\\n", g_pb_results.sanity_cycles, (float)g_pb_results.sanity_cycles / 168000.0
printf "  ok     = %u\\n", g_pb_results.sanity_ok
printf "\\n"

printf "[Random points]\\n"
printf "  cycles = %u  (%.2f ms)\\n", g_pb_results.gen_rand_cycles, (float)g_pb_results.gen_rand_cycles / 168000.0
printf "  ok     = %u\\n", g_pb_results.gen_rand_ok
printf "\\n"

printf "[Pairing iterations]\\n"
set \$i = 0
while \$i < g_pb_results.n_iterations
  printf "  [%2u] cycles = %12u  (%8.1f ms)  ok=%u\\n", \$i, g_pb_results.pairing_cycles[\$i], (float)g_pb_results.pairing_cycles[\$i] / 168000.0, g_pb_results.pairing_ok[\$i]
  set \$i = \$i + 1
end
printf "\\n"

printf "[Pairing aggregate stats]\\n"
printf "  min cycles  = %12u  (%8.1f ms)\\n", g_pb_results.pairing_min_cycles, (float)g_pb_results.pairing_min_cycles / 168000.0
printf "  max cycles  = %12u  (%8.1f ms)\\n", g_pb_results.pairing_max_cycles, (float)g_pb_results.pairing_max_cycles / 168000.0
printf "  avg cycles  = %12u  (%8.1f ms)\\n", g_pb_results.pairing_avg_cycles, (float)g_pb_results.pairing_avg_cycles / 168000.0
printf "  total       = %12u  (%8.1f ms)\\n", g_pb_results.pairing_total_cycles, (float)g_pb_results.pairing_total_cycles / 168000.0
printf "\\n"

printf "[Bilinearity check]\\n"
printf "  cycles = %u  (%.1f ms)\\n", g_pb_results.bilinear_cycles, (float)g_pb_results.bilinear_cycles / 168000.0
printf "  ok     = %u\\n", g_pb_results.bilinear_ok
printf "\\n"

printf "[Phase ring-log]\\n"
set \$head = g_pb_results.log_head
set \$i = 0
while \$i < 32
  set \$idx = (\$head - 32 + \$i) % 32
  set \$e = g_pb_results.log[\$idx]
  if \$e.cycles != 0
    printf "  [%2d] phase=0x%02x iter=%u cycles=%u extra=0x%08x\\n", \$i, \$e.phase, \$e.iteration, \$e.cycles, \$e.extra
  end
  set \$i = \$i + 1
end
printf "\\n"

printf "[Final]\\n"
printf "  wall_ms = %u ms (%.1f s)\\n", g_pb_results.wall_ms, (float)g_pb_results.wall_ms / 1000.0
printf "  status  = 0x%08x  (0x600D0000 = all passed)\\n", g_pb_results.status
printf "\\n=== END RELIC DUMP ===\\n"

quit
GDBEOF

"${GDB_CMD}" -nx -batch -x "${GDB_SCRIPT}" 2>&1 \
    | grep -v -E "^(Open On-Chip|Licensed|For bug|http|Info :|Reading|warning|A debugging|Inferior|\[Inferior|Quit anyway|determining|0x[0-9a-f]+ in|^$)" \
    | tee "${STM_REPORT}"

rm -f "${GDB_SCRIPT}"
ok "Report saved → ${STM_REPORT}"

# =============================================================================
#  Step 5 — Final verdict + AmorE comparison
#
#  Note: each check's regex uses `[[:space:]]+` inside grep -E so that
#     it tolerates whitespace variation. The earlier version required
#     exactly one space, which matched "status = 0x600D0000" but not
#     "status  = 0x600D0000" (two spaces).
# =============================================================================
echo ""
sec "══ FINAL VERDICT ══"

# Print parse results — for transparency
parse_dbg=""

# Check 1: status word — tolerant to any whitespace/case
if grep -qiE 'status[[:space:]]*=[[:space:]]*0x600d0000' "${STM_REPORT}" 2>/dev/null; then
    status_ok=1
    parse_dbg+="✓ status field matched\n"
else
    status_ok=0
    actual_status=$(grep -iE 'status[[:space:]]*=' "${STM_REPORT}" | head -1 || true)
    parse_dbg+="✗ status field check failed. Found: '${actual_status}'\n"
fi

# Check 2: bilinearity — find the line *under* the [Bilinearity check] header
if awk '
    /\[Bilinearity check\]/ { in_section=1; next }
    in_section && /^\[/ { in_section=0 }
    in_section && /ok[[:space:]]*=[[:space:]]*1/ { found=1; exit }
    END { exit !found }
' "${STM_REPORT}"; then
    bilinear_ok=1
    parse_dbg+="✓ bilinearity ok=1\n"
else
    bilinear_ok=0
    parse_dbg+="✗ bilinearity check did not show ok=1\n"
fi

# Check 3: all 10 iterations show ok=1 (none with ok=0)
if grep -qE 'ok=0' "${STM_REPORT}" 2>/dev/null; then
    all_iter_ok=0
    fail_count=$(grep -cE 'ok=0' "${STM_REPORT}" || echo 0)
    parse_dbg+="✗ found ${fail_count} iterations with ok=0\n"
else
    iter_count=$(grep -cE 'ok=1' "${STM_REPORT}" || echo 0)
    if [[ "${iter_count}" -ge 10 ]]; then
        all_iter_ok=1
        parse_dbg+="✓ all ${iter_count} iterations have ok=1\n"
    else
        all_iter_ok=0
        parse_dbg+="✗ only ${iter_count} iterations with ok=1 (expected ≥10)\n"
    fi
fi

# Print parse diagnostics — full transparency
echo -e "${CYN}Parse diagnostics:${RST}"
echo -e "${parse_dbg}" | sed 's/^/  /'

# Verdict
if [[ "${status_ok}" == "1" && "${bilinear_ok}" == "1" && "${all_iter_ok}" == "1" ]]; then
    echo -e "${BOLD}${GRN}  ✓ PASS — ALL CHECKS GOOD${RST}"
elif [[ "${status_ok}" == "1" && "${bilinear_ok}" == "1" ]]; then
    echo -e "${BOLD}${YLW}  ⚠ PASS WITH WARNING — pairing math is correct, but iteration sanity flag failed${RST}"
else
    echo -e "${BOLD}${RED}  ✗ FAIL${RST}"
    echo -e "${YLW}     Inspect: ${STM_REPORT}${RST}"
fi
echo ""

# Always show the headline number — using awk for safe extraction
avg_line="$(grep -E 'avg cycles' "${STM_REPORT}" 2>/dev/null | head -1 || true)"
if [[ -n "$avg_line" ]]; then
    echo -e "${BOLD}  RELIC pairing average:${RST}"
    echo -e "${BOLD}  ${avg_line}${RST}"
    echo ""
    # Pull the ms number with awk - look for "(<num> ms)" pattern
    relic_ms=$(echo "$avg_line" | awk -F'[()]' '{
        for (i=1; i<=NF; i++) {
            if ($i ~ /[0-9]+\.[0-9]+[[:space:]]+ms/) {
                gsub(/[[:space:]]*ms.*/, "", $i)
                gsub(/^[[:space:]]+/, "", $i)
                print $i
                exit
            }
        }
    }')

    if [[ -n "$relic_ms" ]]; then
        echo -e "${BOLD}  ─── COMPARISON ───${RST}"
        echo -e "${BOLD}  AmorE per-round:        381.8 ms${RST}"
        echo -e "${BOLD}  RELIC pairing local:    ${relic_ms} ms${RST}"
        ratio=$(awk -v r="$relic_ms" 'BEGIN{printf "%.2f", 381.8/r}')
        echo -e "${BOLD}  Ratio (AmorE / RELIC):  ${ratio}×${RST}"
        echo ""
        echo -e "${CYN}  Interpretation:${RST}"
        if (( $(echo "$ratio < 1" | bc -l) )); then
            echo -e "${CYN}    AmorE saves time vs local pairing (ratio < 1)${RST}"
        else
            echo -e "${CYN}    AmorE is slower than local pairing (ratio > 1)${RST}"
            echo -e "${CYN}    AmorE's value is in code size & memory, not speed${RST}"
        fi
    else
        echo -e "${YLW}  Could not extract numeric ms value from avg_line${RST}"
        echo -e "${YLW}    raw: '${avg_line}'${RST}"
    fi
fi
echo ""
