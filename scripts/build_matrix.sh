#!/usr/bin/env bash
# build_matrix.sh — compile every Pre-PPK2 firmware variant.
# Used to validate that all (curve, mode, N) tuples build cleanly.
# Does NOT flash; only produces .elf/.bin/.hex in build/<variant>/.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CURVES=("BN254" "BLS12_381")
MODE_A_NS=()                  # Mode A has no N parameter (full benchmark)
MODE_B_NS=("1" "3" "9" "30")  # Mode B sweep per PRD

banner() { printf '\n══════════════════════════════════════════════════════════\n  %s\n══════════════════════════════════════════════════════════\n' "$*"; }

total_built=0
total_failed=0
declare -a failures=()

build_one() {
    local label="$1"; shift
    local out_dir="$1"; shift
    echo
    echo "----- ${label} -----"
    rm -rf "${out_dir}"
    if cmake -B "${out_dir}" "$@" -DCMAKE_BUILD_TYPE=Release > "${out_dir%/*}/${label}_cmake.log" 2>&1; then
        if cmake --build "${out_dir}" --parallel "$(nproc)" > "${out_dir%/*}/${label}_build.log" 2>&1; then
            echo "  ✓ ${label} OK"
            total_built=$((total_built+1))
            return 0
        fi
    fi
    echo "  ✗ ${label} FAILED"
    total_failed=$((total_failed+1))
    failures+=("${label}")
    tail -20 "${out_dir%/*}/${label}_build.log" 2>/dev/null || true
    return 1
}

mkdir -p build

banner "Mode A — AmorE protocol (full benchmark)"
for curve in "${CURVES[@]}"; do
    build_one "${curve,,}_A" "build/${curve,,}_A" \
        -DCURVE="${curve}" -DMEASUREMENT_MODE=A -DAMORE_TRIGGERS_ENABLED=1 || true
done

banner "Mode B — direct pairing (requires RELIC)"
if [ -f "relic_install/lib/librelic_s.a" ]; then
    for curve in "${CURVES[@]}"; do
        for n in "${MODE_B_NS[@]}"; do
            build_one "${curve,,}_B_N${n}" "build/${curve,,}_B_N${n}" \
                -DCURVE="${curve}" -DMEASUREMENT_MODE=B -DBENCH_N="${n}" \
                -DAMORE_TRIGGERS_ENABLED=1 || true
        done
    done
else
    echo "  ⚠ relic_install/lib/librelic_s.a not found — skipping Mode B."
    echo "    Run scripts/build-relic.sh first to enable Mode B."
fi

banner "SUMMARY"
echo "  Built : ${total_built}"
echo "  Failed: ${total_failed}"
if [ "${total_failed}" -gt 0 ]; then
    echo "  Failures:"
    for f in "${failures[@]}"; do echo "    - ${f}"; done
    exit 1
fi
echo "  ✓ all variants built"
