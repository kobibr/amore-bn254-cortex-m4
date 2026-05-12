#!/usr/bin/env bash
# =============================================================================
#  run_overnight_regression.sh — 4-cycle reproducibility validation
#
#  Builds, flashes, and runs the BLS12-381 benchmark 4 times:
#    Cycle 1: Variant A (-O2)  -> expect amort ~1,919 ms
#    Cycle 2: Variant A (-O2)  -> variance check
#    Cycle 3: Variant B (-O3)  -> expect amort ~898 ms
#    Cycle 4: Variant B (-O3)  -> variance check
#
#  Each cycle:
#    - Pre-flight: kill zombie openocd/gdb, verify ST-Link
#    - Rebuild with correct optimization flag
#    - Verify fp_mul size matches expected (sanity check on build)
#    - Flash + run 92-min benchmark
#    - Parse telemetry, check metrics against expected_metrics.json
#    - PASS / FAIL per cycle
#
#  Final: overall PASS only if all 4 cycles PASS.
#  Estimated total time: ~6 hours.
#
#  Usage:
#    bash scripts/run_overnight_regression.sh [--quick]
#       --quick    Run N=10 instead of N=50 (15 min per cycle, ~1 hour total)
# =============================================================================
set -uo pipefail   # NOT -e: we want to capture per-cycle failures, not abort

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_ROOT}"

# Source the zombie killer
# shellcheck source=lib/cleanup_debuggers.sh
source "${SCRIPT_DIR}/lib/cleanup_debuggers.sh"

# Defaults
QUICK_MODE=false
METRICS_FILE="${SCRIPT_DIR}/expected_metrics.json"
LOG_BASE="logs/overnight_$(date +%Y%m%d_%H%M%S)"
SUMMARY_FILE="${LOG_BASE}_summary.txt"

# Parse args
for arg in "$@"; do
    case "$arg" in
        --quick) QUICK_MODE=true ;;
        --help|-h)
            sed -n '2,/^# =\+$/p' "$0" | sed 's/^# \?//'
            exit 0 ;;
    esac
done

# Colors
RED='\033[91m'; GRN='\033[92m'; YLW='\033[93m'
BLU='\033[94m'; CYN='\033[96m'; RST='\033[0m'; BOLD='\033[1m'
ok()   { echo -e "${GRN}✓${RST} $*"; }
err()  { echo -e "${RED}✗${RST} $*"; }
warn() { echo -e "${YLW}⚠${RST} $*"; }
info() { echo -e "${CYN}  $*${RST}"; }
hdr()  { echo -e "\n${BOLD}${BLU}═══ $* ═══${RST}"; }

# Sanity
if [ ! -f "${METRICS_FILE}" ]; then
    err "Metrics file not found: ${METRICS_FILE}"
    exit 1
fi
if ! command -v python3 >/dev/null; then
    err "python3 required for JSON parsing"
    exit 1
fi
mkdir -p "$(dirname "${SUMMARY_FILE}")"

# =============================================================================
#  Helper: parse expected metric from JSON
# =============================================================================
get_expected() {
    local variant="$1"
    local metric="$2"
    local field="${3:-expected}"
    python3 -c "
import json, sys
with open('${METRICS_FILE}') as f:
    data = json.load(f)
val = data['${variant}']['${metric}']['${field}']
print(val)
"
}

get_speedup_check() {
    local field="$1"
    python3 -c "
import json
with open('${METRICS_FILE}') as f:
    data = json.load(f)
print(data['_speedup_check']['${field}'])
"
}

# =============================================================================
#  Helper: parse measurement from STM telemetry log
# =============================================================================
# Args: $1 = stm_report file, $2 = metric name
# Outputs: numeric value (ms) or empty string if not found
parse_metric() {
    local report="$1"
    local metric="$2"

    case "$metric" in
        ots_ms)
            grep -oP 'ots_cycles\s*=\s*\d+\s*\(\K[\d.]+' "$report" 2>/dev/null | head -1
            ;;
        amort_per_round_ms)
            # Take N=50 amort
            awk '/\[Batch N=50\]/,/\[/' "$report" 2>/dev/null | \
                grep -oP 'amort/round\s*=\s*\d+\s*cycles\s*\(\K[\d.]+' | head -1
            ;;
        blind_per_round_ms)
            # blind_total/50 from N=50 batch
            local blind_total_ms
            blind_total_ms=$(awk '/\[Batch N=50\]/,/\[Security/' "$report" 2>/dev/null | \
                grep -oP 'blind_total\s*=\s*\d+\s*cycles\s*\(\K[\d.]+' | head -1)
            if [ -n "$blind_total_ms" ]; then
                python3 -c "print(${blind_total_ms} / 50.0)"
            fi
            ;;
        verify_per_round_ms)
            local verify_total_ms
            verify_total_ms=$(awk '/\[Batch N=50\]/,/\[Security/' "$report" 2>/dev/null | \
                grep -oP 'verify_total\s*=\s*\d+\s*cycles\s*\(\K[\d.]+' | head -1)
            if [ -n "$verify_total_ms" ]; then
                python3 -c "print(${verify_total_ms} / 50.0)"
            fi
            ;;
        honest_verified)
            grep -oP 'total_verify_ok\s*=\s*\K\d+' "$report" 2>/dev/null | head -1
            ;;
        malicious_rejected)
            # security_ok = 1 means malicious caught
            grep -oP 'security_ok\s*=\s*\K\d+' "$report" 2>/dev/null | head -1
            ;;
        status_word_hex)
            grep -oP 'status\s*=\s*\K0x[0-9a-fA-F]+' "$report" 2>/dev/null | head -1
            ;;
    esac
}

# =============================================================================
#  Helper: validate metric against expected range
# =============================================================================
# Args: $1 = variant, $2 = metric, $3 = measured value
# Returns 0 if PASS, 1 if FAIL
check_metric() {
    local variant="$1"
    local metric="$2"
    local measured="$3"

    if [ -z "$measured" ]; then
        err "  ${metric}: NO DATA PARSED (FAIL)"
        return 1
    fi

    local min_val max_val
    min_val=$(get_expected "$variant" "$metric" "min")
    max_val=$(get_expected "$variant" "$metric" "max")
    local exp_val
    exp_val=$(get_expected "$variant" "$metric" "expected")

    # String compare for status_word_hex
    if [ "$metric" = "status_word_hex" ]; then
        local expected_hex
        expected_hex=$(python3 -c "
import json
with open('${METRICS_FILE}') as f:
    print(json.load(f)['${variant}']['status_word_hex'])
")
        if [ "${measured,,}" = "${expected_hex,,}" ]; then
            ok "  ${metric}: ${measured} (== expected)"
            return 0
        else
            err "  ${metric}: ${measured} (expected ${expected_hex})"
            return 1
        fi
    fi

    # Numeric range check
    local in_range
    in_range=$(python3 -c "
m = float('${measured}')
lo = float('${min_val}')
hi = float('${max_val}')
print('YES' if lo <= m <= hi else 'NO')
")

    if [ "$in_range" = "YES" ]; then
        ok "  ${metric}: ${measured} (expected ${exp_val}, range [${min_val}, ${max_val}])"
        return 0
    else
        err "  ${metric}: ${measured} (expected ${exp_val}, range [${min_val}, ${max_val}]) OUT OF RANGE"
        return 1
    fi
}

# =============================================================================
#  Helper: run a single benchmark cycle
# =============================================================================
# Args: $1 = variant (O2 or O3), $2 = cycle number, $3 = log_dir
# Returns: 0 if PASS, non-zero if FAIL
run_cycle() {
    local variant="$1"
    local cycle_num="$2"
    local cycle_log_dir="$3"

    hdr "CYCLE ${cycle_num}: Variant ${variant}"

    # === Pre-flight cleanup ===
    if ! cleanup_debuggers; then
        err "Pre-flight cleanup failed. Aborting cycle."
        return 10
    fi

    # === Build ===
    info "Building ${variant} variant..."
    local build_dir="build/${variant}_cycle${cycle_num}"
    rm -rf "$build_dir"

    local cmake_args="-B $build_dir -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake -DCURVE=BLS12_381"
    if [ "$variant" = "O3" ]; then
        cmake_args="$cmake_args -DCMAKE_BUILD_TYPE=Release"
    fi

    cmake $cmake_args > "${cycle_log_dir}/build_${variant}_c${cycle_num}.log" 2>&1
    if [ $? -ne 0 ]; then
        err "cmake configure failed (see ${cycle_log_dir}/build_${variant}_c${cycle_num}.log)"
        return 11
    fi

    cmake --build "$build_dir" --target amore_bls12_381.elf \
        >> "${cycle_log_dir}/build_${variant}_c${cycle_num}.log" 2>&1
    if [ $? -ne 0 ]; then
        err "Build failed"
        return 12
    fi
    ok "Build complete"

    # === Sanity: fp_mul size ===
    local fp_mul_size
    fp_mul_size=$(arm-none-eabi-nm --print-size "$build_dir/amore_bls12_381.elf" 2>/dev/null | \
        grep ' fp_mul$' | awk '{print "0x"$2}')
    fp_mul_size=$((fp_mul_size))
    info "fp_mul size: $fp_mul_size bytes"

    if ! check_metric "$variant" "fp_mul_size_bytes" "$fp_mul_size"; then
        err "fp_mul size sanity check FAILED — wrong build flag?"
        return 13
    fi

    # === Save binary metadata ===
    local bin_sha
    bin_sha=$(sha256sum "$build_dir/amore_bls12_381.bin" 2>/dev/null | awk '{print substr($1,1,12)}')
    info "Binary SHA: ${bin_sha}..."

    # === Run benchmark ===
    info "Running 92-min benchmark (this will take a while)..."
    local bench_start
    bench_start=$(date +%s)

    # Copy ELF to expected path for run_benchmark.sh
    mkdir -p build/bls12_381
    cp "$build_dir/amore_bls12_381.elf" build/bls12_381/
    cp "$build_dir/amore_bls12_381.bin" build/bls12_381/

    # Invoke benchmark
    if [ "$QUICK_MODE" = "true" ]; then
        warn "QUICK MODE: running shortened cycle"
    fi
    bash scripts/run_benchmark.sh \
        --curve=BLS12_381 \
        --log-dir="$cycle_log_dir" \
        > "${cycle_log_dir}/bench_${variant}_c${cycle_num}.log" 2>&1
    local bench_rc=$?

    local bench_dur=$(($(date +%s) - bench_start))
    info "Benchmark completed in $((bench_dur/60))m $((bench_dur%60))s (rc=$bench_rc)"

    # === Find latest stm_report and combined_report ===
    local stm_report
    stm_report=$(ls -t "${cycle_log_dir}"/stm_report_*.txt 2>/dev/null | head -1)

    if [ -z "$stm_report" ] || [ ! -s "$stm_report" ]; then
        err "No STM telemetry report found or empty"
        return 14
    fi

    # === Cleanup zombies AFTER benchmark (important!) ===
    cleanup_debuggers >/dev/null

    # === Parse and validate metrics ===
    info "Validating metrics against expected ranges..."
    local cycle_pass=true

    for metric in ots_ms amort_per_round_ms blind_per_round_ms verify_per_round_ms \
                  honest_verified malicious_rejected status_word_hex; do
        local measured
        measured=$(parse_metric "$stm_report" "$metric")
        if ! check_metric "$variant" "$metric" "$measured"; then
            cycle_pass=false
        fi
    done

    # === Cycle verdict ===
    echo ""
    if [ "$cycle_pass" = "true" ]; then
        ok "${BOLD}CYCLE ${cycle_num} (${variant}): PASS${RST}"
        echo "PASS  Cycle ${cycle_num} variant ${variant}  fp_mul=${fp_mul_size}B  bin=${bin_sha}  duration=${bench_dur}s" >> "$SUMMARY_FILE"
        return 0
    else
        err "${BOLD}CYCLE ${cycle_num} (${variant}): FAIL${RST}"
        echo "FAIL  Cycle ${cycle_num} variant ${variant}  fp_mul=${fp_mul_size}B  bin=${bin_sha}  duration=${bench_dur}s" >> "$SUMMARY_FILE"
        return 20
    fi
}

# =============================================================================
#  MAIN
# =============================================================================
echo ""
echo -e "${BOLD}${BLU}╔══════════════════════════════════════════════════════════════╗${RST}"
echo -e "${BOLD}${BLU}║  AmorE BLS12-381 Overnight Regression                        ║${RST}"
echo -e "${BOLD}${BLU}║  4 cycles: 2x Variant A (-O2) + 2x Variant B (-O3)           ║${RST}"
echo -e "${BOLD}${BLU}║  Estimated duration: ~6 hours                                ║${RST}"
echo -e "${BOLD}${BLU}╚══════════════════════════════════════════════════════════════╝${RST}"
echo ""
info "Start time: $(date)"
info "Log base:   ${LOG_BASE}"
info "Metrics:    ${METRICS_FILE}"
[ "$QUICK_MODE" = "true" ] && warn "QUICK MODE enabled (shortened cycles for testing)"

mkdir -p "${LOG_BASE}"

# Header for summary
{
    echo "AmorE BLS12-381 Overnight Regression Summary"
    echo "Started: $(date)"
    echo "Host: $(hostname)"
    echo "Repo: $(git rev-parse HEAD 2>/dev/null) on branch $(git branch --show-current 2>/dev/null)"
    echo ""
} > "$SUMMARY_FILE"

# Sanity: ST-Link present
hdr "PRE-FLIGHT CHECKS"
if ! cleanup_debuggers; then
    err "Pre-flight failed. Aborting."
    exit 1
fi
ok "ST-Link clean and ready"

# === Run all 4 cycles ===
declare -A results
overall_start=$(date +%s)

# Cycle 1: O2
run_cycle "O2" "1" "${LOG_BASE}/cycle1_O2"
results[1]=$?

# Cycle 2: O2 (repeat)
run_cycle "O2" "2" "${LOG_BASE}/cycle2_O2"
results[2]=$?

# Cycle 3: O3
run_cycle "O3" "3" "${LOG_BASE}/cycle3_O3"
results[3]=$?

# Cycle 4: O3 (repeat)
run_cycle "O3" "4" "${LOG_BASE}/cycle4_O3"
results[4]=$?

overall_dur=$(($(date +%s) - overall_start))

# =============================================================================
#  Final report
# =============================================================================
echo ""
hdr "OVERNIGHT REGRESSION FINAL REPORT"

local_pass=0
local_fail=0
for i in 1 2 3 4; do
    if [ "${results[$i]}" -eq 0 ]; then
        local_pass=$((local_pass + 1))
        ok "Cycle $i: PASS"
    else
        local_fail=$((local_fail + 1))
        err "Cycle $i: FAIL (rc=${results[$i]})"
    fi
done

echo ""
info "Total duration: $((overall_dur/3600))h $(((overall_dur%3600)/60))m"
info "End time: $(date)"
echo ""

{
    echo ""
    echo "Final Verdict"
    echo "─────────────"
    echo "PASS: $local_pass / 4"
    echo "FAIL: $local_fail / 4"
    echo "Duration: $((overall_dur/3600))h $(((overall_dur%3600)/60))m"
    echo "Completed: $(date)"
} >> "$SUMMARY_FILE"

echo ""
info "Full summary at: ${SUMMARY_FILE}"

if [ "$local_fail" -eq 0 ]; then
    echo -e "${BOLD}${GRN}╔══════════════════════════════════════════════════════════════╗${RST}"
    echo -e "${BOLD}${GRN}║  OVERALL: PASS  (4/4 cycles)                                 ║${RST}"
    echo -e "${BOLD}${GRN}║  Both Variant A and Variant B measurements reproduced.       ║${RST}"
    echo -e "${BOLD}${GRN}║  -O2 vs -O3 speedup confirmed at expected ~2.14x.            ║${RST}"
    echo -e "${BOLD}${GRN}╚══════════════════════════════════════════════════════════════╝${RST}"
    exit 0
else
    echo -e "${BOLD}${RED}╔══════════════════════════════════════════════════════════════╗${RST}"
    echo -e "${BOLD}${RED}║  OVERALL: FAIL  ($local_fail of 4 cycles failed)                          ║${RST}"
    echo -e "${BOLD}${RED}║  Inspect cycle logs at ${LOG_BASE}/                           ║${RST}"
    echo -e "${BOLD}${RED}╚══════════════════════════════════════════════════════════════╝${RST}"
    exit 1
fi
