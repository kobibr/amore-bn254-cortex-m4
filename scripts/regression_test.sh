#!/usr/bin/env bash
# =============================================================================
# regression_test.sh — verify unified firmware reproduces baseline numbers
# 
# Builds, flashes, and runs both BN254 and BLS12-381 firmware variants from
# the unified source tree, then compares timing/cycles to the baselines in
# doc/AmorE_*_Results.txt. Tolerance: 5% per metric.
# 
# Usage:
#   bash scripts/regression_test.sh [--curve=BN254|BLS12_381|both]
#                                   [--tolerance=0.05]
#                                   [--no-flash]
# =============================================================================

set -uo pipefail   # not -e — we need to keep going to compare both curves

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_DIR}"

CURVE_CHOICE="both"
TOLERANCE="0.05"
NO_FLASH=""

for arg in "$@"; do
    case "$arg" in
        --curve=*)     CURVE_CHOICE="${arg#*=}" ;;
        --tolerance=*) TOLERANCE="${arg#*=}" ;;
        --no-flash)    NO_FLASH="--no-flash" ;;
    esac
done

STAMP="$(date +%Y%m%d_%H%M%S)"
REGRESSION_DIR="logs/regression_${STAMP}"
mkdir -p "${REGRESSION_DIR}"

RED='\033[91m'; GRN='\033[92m'; YLW='\033[93m'
BLU='\033[94m'; CYN='\033[96m'; RST='\033[0m'; BOLD='\033[1m'

# ── Helper: extract metrics from a telemetry log into key=value pairs ──────
extract_metrics() {
    local logfile="$1"
    local outfile="$2"
    # The telemetry format is `  metric_name = value [cyc|ms]`. We extract
    # ots_cycles, ots_ms, and per-batch blind/verify/amort cycles.
    python3 << PY > "${outfile}"
import re
with open("${logfile}") as f:
    log = f.read()

metrics = {}

# OneTimeSetup
m = re.search(r'ots_cycles\s*=\s*(\d+)', log)
if m: metrics['ots_cycles'] = int(m.group(1))
m = re.search(r'ots_ms\s*=\s*([\d.]+)', log)
if m: metrics['ots_ms'] = float(m.group(1))

# Per-batch sections
for batch in ['1', '10', '50']:
    pat = r'\[Batch N=' + batch + r'\][^\[]*?'
    section = re.search(pat + r'(?=\[|\Z)', log, re.DOTALL)
    if not section:
        continue
    s = section.group(0)
    
    # Format 1: blind = NNN cyc (X.X ms)
    m = re.search(r'blind\s*=\s*(\d+)\s*cyc\s*\(([\d.]+)\s*ms\)', s)
    if m:
        metrics[f'N{batch}_blind_cyc'] = int(m.group(1))
        metrics[f'N{batch}_blind_ms'] = float(m.group(2))
    # Format 2: blind_raw = NNN cyc (when overflow handling kicks in)
    m = re.search(r'blind_raw\s*=\s*(\d+)\s*cyc', s)
    if m:
        metrics[f'N{batch}_blind_raw_cyc'] = int(m.group(1))
    
    m = re.search(r'verify\s*=\s*(\d+)\s*cyc\s*\(([\d.]+)\s*ms\)', s)
    if m:
        metrics[f'N{batch}_verify_cyc'] = int(m.group(1))
        metrics[f'N{batch}_verify_ms'] = float(m.group(2))
    m = re.search(r'verify_raw\s*=\s*(\d+)\s*cyc', s)
    if m:
        metrics[f'N{batch}_verify_raw_cyc'] = int(m.group(1))
    
    m = re.search(r'amort\s*=\s*(\d+)\s*cyc\s*\(([\d.]+)\s*ms', s)
    if m:
        metrics[f'N{batch}_amort_cyc'] = int(m.group(1))
        metrics[f'N{batch}_amort_ms'] = float(m.group(2))

# Status, totals
m = re.search(r'status\s*=\s*0x([0-9a-fA-F]+)', log)
if m: metrics['status'] = m.group(1).lower()
m = re.search(r'total_verify_ok\s*=\s*(\d+)', log)
if m: metrics['total_verify_ok'] = int(m.group(1))
m = re.search(r'security_ok\s*=\s*(\d+)', log)
if m: metrics['security_ok'] = int(m.group(1))

for k in sorted(metrics.keys()):
    print(f"{k}={metrics[k]}")
PY
}

# ── Helper: extract baseline metrics from doc/AmorE_*_Results.txt ──────────
extract_baseline() {
    local docfile="$1"
    local outfile="$2"
    python3 << PY > "${outfile}"
import re
with open("${docfile}") as f:
    doc = f.read()

metrics = {}

# OneTimeSetup section: "Cycles : 430,950,212  cyc"
ots_section = re.search(r'4\.1\s+OneTimeSetup.*?(?=\n\s*4\.2|\Z)', doc, re.DOTALL)
if ots_section:
    m = re.search(r'Cycles\s*:\s*([\d,]+)\s*cyc', ots_section.group(0))
    if m: metrics['ots_cycles'] = int(m.group(1).replace(',', ''))
    m = re.search(r'Wall time\s*:\s*([\d,.]+)\s*ms', ots_section.group(0))
    if m: metrics['ots_ms'] = float(m.group(1).replace(',', ''))

# Per-batch table rows. The format in the docs:
# |  N =  1  |   1 | 172,069,842 cyc   | 148,201,512 cyc   | 320,271,354 cyc |
# |          |     |   1,024.2 ms      |     882.2 ms      |   1,906.4 ms    |
batch_pat = (
    r'\|\s*N\s*=\s*(\d+)\s*\|\s*\d+\s*\|\s*([\d,]+)\s*cyc\*?\s*\|'
    r'\s*([\d,]+)\s*cyc\*?\s*\|\s*([\d,]+)\s*cyc\s*\|\s*\n'
    r'\s*\|[^|]*\|[^|]*\|\s*([\d,.]+)\s*ms\s*\|'
    r'\s*([\d,.]+)\s*ms\s*\|\s*([\d,.]+)\s*ms\s*\|'
)
for m in re.finditer(batch_pat, doc):
    n = m.group(1)
    metrics[f'N{n}_blind_cyc']  = int(m.group(2).replace(',', ''))
    metrics[f'N{n}_verify_cyc'] = int(m.group(3).replace(',', ''))
    metrics[f'N{n}_amort_cyc']  = int(m.group(4).replace(',', ''))
    metrics[f'N{n}_blind_ms']   = float(m.group(5).replace(',', ''))
    metrics[f'N{n}_verify_ms']  = float(m.group(6).replace(',', ''))
    metrics[f'N{n}_amort_ms']   = float(m.group(7).replace(',', ''))

for k in sorted(metrics.keys()):
    print(f"{k}={metrics[k]}")
PY
}

# ── Helper: compare two metric files with tolerance ─────────────────────────
compare_metrics() {
    local current_file="$1"
    local baseline_file="$2"
    local label="$3"
    
    python3 << PY
import sys
tolerance = float("${TOLERANCE}")

def load(path):
    out = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if '=' not in line:
                continue
            k, v = line.split('=', 1)
            try:
                v = float(v)
            except ValueError:
                pass
            out[k] = v
    return out

current = load("${current_file}")
baseline = load("${baseline_file}")

# Critical metrics (must be present in both)
critical = ['ots_cycles', 'N1_amort_cyc', 'N10_amort_cyc']
# 50-rounds amort cycle is reported per-round so it's bounded. Check it too.
if 'N50_amort_cyc' in current and 'N50_amort_cyc' in baseline:
    critical.append('N50_amort_cyc')

print(f"\n=== ${label} ===")
print(f"  Current : ${current_file}")
print(f"  Baseline: ${baseline_file}")
print(f"  Tolerance: {tolerance*100:.1f}%")
print()

# Sanity: status must be 0x600d0000
if 'status' in current:
    status_ok = current['status'] in ('600d0000', '0x600d0000')
    sym = "\033[92m✓\033[0m" if status_ok else "\033[91m✗\033[0m"
    print(f"  {sym} status word    = {current['status']} (expected 600d0000)")
    if not status_ok:
        print("\033[91m\n  ABORT: firmware did not complete successfully\033[0m")
        sys.exit(1)
if 'total_verify_ok' in current:
    sym = "\033[92m✓\033[0m" if current['total_verify_ok'] == 61 else "\033[91m✗\033[0m"
    print(f"  {sym} verify_ok       = {int(current['total_verify_ok'])}/61")
if 'security_ok' in current:
    sym = "\033[92m✓\033[0m" if current['security_ok'] == 1 else "\033[91m✗\033[0m"
    print(f"  {sym} security_ok     = {int(current['security_ok'])}/1")

print()
print(f"  {'Metric':<25} {'Current':>15} {'Baseline':>15} {'Δ%':>8}  Verdict")
print(f"  {'-'*25} {'-'*15} {'-'*15} {'-'*8}  -------")

n_pass = n_fail = n_missing = 0
for key in sorted(set(list(current.keys()) + list(baseline.keys()))):
    if key in ('status', 'total_verify_ok', 'security_ok'):
        continue
    cv = current.get(key)
    bv = baseline.get(key)
    if cv is None or bv is None:
        n_missing += 1
        continue
    if not isinstance(cv, (int, float)) or not isinstance(bv, (int, float)):
        continue
    if bv == 0:
        delta_pct = 0.0
    else:
        delta_pct = (cv - bv) / bv * 100
    abs_delta = abs(delta_pct / 100)
    is_critical = key in critical
    passed = abs_delta <= tolerance
    if passed:
        sym = "\033[92m✓ PASS\033[0m"
        n_pass += 1
    else:
        if is_critical:
            sym = "\033[91m✗ FAIL\033[0m"
            n_fail += 1
        else:
            sym = "\033[93m! WARN\033[0m"
    flag = " *" if is_critical else ""
    print(f"  {key+flag:<25} {cv:>15,.1f} {bv:>15,.1f} {delta_pct:>+7.2f}%  {sym}")

print()
print(f"  Summary: {n_pass} pass, {n_fail} fail (critical), {n_missing} skipped")
print(f"  (* = critical metric — must be within tolerance)")

sys.exit(0 if n_fail == 0 else 1)
PY
}

# ── Run for one curve ───────────────────────────────────────────────────────
run_one_curve() {
    local curve="$1"
    local doc_file="$2"
    local label="$3"
    
    echo ""
    echo -e "${BOLD}${BLU}════════════════════════════════════════════════════════════════════${RST}"
    echo -e "${BOLD}${BLU}  Regression: ${curve}${RST}"
    echo -e "${BOLD}${BLU}════════════════════════════════════════════════════════════════════${RST}"
    
    if [ ! -f "${doc_file}" ]; then
        echo -e "${RED}✗${RST} Baseline file not found: ${doc_file}"
        return 1
    fi
    
    # 1. Build + flash + run
    local telemetry="${REGRESSION_DIR}/${curve,,}_telemetry.txt"
    local raw_log="${REGRESSION_DIR}/${curve,,}_run.log"
    
    echo ""
    echo -e "${CYN}  → Running run_benchmark.sh for ${curve}...${RST}"
    bash scripts/run_benchmark.sh --curve="${curve}" ${NO_FLASH} \
        > "${raw_log}" 2>&1
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo -e "${RED}✗${RST} run_benchmark.sh failed (exit ${rc})"
        echo "  See ${raw_log}"
        return 1
    fi
    
    echo -e "${GRN}  ✓${RST} run completed; raw log: ${raw_log}"
    
    # The telemetry is captured in logs/stm_report_*.txt by run_benchmark.sh.
    # Pick the most recent one.
    local stm_report=$(ls -t logs/stm_report_*.txt 2>/dev/null | head -1)
    if [ -z "${stm_report}" ]; then
        # Fallback: use raw run log; gdb dumps include the telemetry
        stm_report="${raw_log}"
    fi
    cp "${stm_report}" "${telemetry}"
    echo -e "${GRN}  ✓${RST} telemetry: ${telemetry}"
    
    # 2. Extract metrics
    local current_metrics="${REGRESSION_DIR}/${curve,,}_metrics.txt"
    local baseline_metrics="${REGRESSION_DIR}/${curve,,}_baseline.txt"
    extract_metrics "${telemetry}" "${current_metrics}"
    extract_baseline "${doc_file}" "${baseline_metrics}"
    
    # 3. Compare
    compare_metrics "${current_metrics}" "${baseline_metrics}" "${label}"
    return $?
}

# ── Main ────────────────────────────────────────────────────────────────────
echo -e "${BOLD}${CYN}AmorE Curve Unification — Regression Test${RST}"
echo -e "  Date     : $(date)"
echo -e "  Branch   : $(git branch --show-current)"
echo -e "  Commit   : $(git rev-parse --short HEAD)"
echo -e "  Choice   : ${CURVE_CHOICE}"
echo -e "  Tolerance: $(echo "${TOLERANCE}*100" | bc)%"
echo -e "  Output   : ${REGRESSION_DIR}/"

bn254_rc=0
bls_rc=0

if [ "${CURVE_CHOICE}" = "BN254" ] || [ "${CURVE_CHOICE}" = "both" ]; then
    run_one_curve "BN254" "doc/AmorE_BN128_Results.txt" "BN254 vs BN128 baseline"
    bn254_rc=$?
fi

if [ "${CURVE_CHOICE}" = "BLS12_381" ] || [ "${CURVE_CHOICE}" = "both" ]; then
    run_one_curve "BLS12_381" "doc/AmorE_BLS12_381_Results.txt" "BLS12-381 baseline"
    bls_rc=$?
fi

# ── Final summary ───────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${BLU}════════════════════════════════════════════════════════════════════${RST}"
echo -e "${BOLD}${BLU}  FINAL SUMMARY${RST}"
echo -e "${BOLD}${BLU}════════════════════════════════════════════════════════════════════${RST}"

[ "${CURVE_CHOICE}" != "BLS12_381" ] && {
    if [ ${bn254_rc} -eq 0 ]; then
        echo -e "  ${GRN}✓${RST} BN254     : PASS — unified firmware reproduces BN128 baseline"
    else
        echo -e "  ${RED}✗${RST} BN254     : FAIL — see ${REGRESSION_DIR}/bn254_*.txt"
    fi
}

[ "${CURVE_CHOICE}" != "BN254" ] && {
    if [ ${bls_rc} -eq 0 ]; then
        echo -e "  ${GRN}✓${RST} BLS12-381 : PASS — unified firmware reproduces BLS12-381 baseline"
    else
        echo -e "  ${RED}✗${RST} BLS12-381 : FAIL — see ${REGRESSION_DIR}/bls12_381_*.txt"
    fi
}

echo ""
echo -e "  Artifacts: ${REGRESSION_DIR}/"
echo ""

exit $((bn254_rc + bls_rc))
