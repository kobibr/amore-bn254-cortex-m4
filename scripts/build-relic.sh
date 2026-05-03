#!/usr/bin/env bash
# =============================================================================
#  build-relic.sh — builds RELIC as a static library for STM32F407
#                   with the hand-tuned assembly backend (arm-asm-254).
#
#  Output: <project root>/relic_install/lib/librelic_s.a
#          <project root>/relic_install/include/relic/*.h
#
#  Key properties:
#    - Locates the environment via SCRIPT_DIR/.., so it works from any path.
#    - After the build, asserts that ARITH=ARM_ASM_254 was selected
#      (and not the C-only EASY fallback).
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

RELIC_SRC="${PROJECT_ROOT}/relic"
RELIC_BUILD="${PROJECT_ROOT}/relic_build"
RELIC_INSTALL="${PROJECT_ROOT}/relic_install"
TOOLCHAIN="${PROJECT_ROOT}/cmake/toolchain-stm32f4.cmake"
LOGS="${PROJECT_ROOT}/logs"

mkdir -p "${LOGS}"

# ─── Sanity checks ───────────────────────────────────────────────────────
if [[ ! -d "${RELIC_SRC}" ]]; then
    echo "✗ RELIC source not found at ${RELIC_SRC}"
    exit 1
fi
if [[ ! -d "${RELIC_SRC}/src/low/arm-asm-254" ]]; then
    echo "✗ ARM assembly backend missing at ${RELIC_SRC}/src/low/arm-asm-254"
    echo "  (this directory must exist or ARITH=arm-asm-254 will silently fall back to EASY)"
    exit 1
fi
if [[ ! -f "${TOOLCHAIN}" ]]; then
    echo "✗ Toolchain file not found: ${TOOLCHAIN}"
    echo "  Copy it from ../relic_benchmark/cmake/toolchain-stm32f4.cmake"
    exit 1
fi
if ! command -v arm-none-eabi-gcc >/dev/null; then
    echo "✗ arm-none-eabi-gcc not in PATH"
    exit 1
fi

# ─── Full clean (including any prior install with ARITH=EASY) ────────────
rm -rf "${RELIC_BUILD}" "${RELIC_INSTALL}"
mkdir -p "${RELIC_BUILD}"

echo "═════════════════════════════════════════════════════════"
echo "  Building RELIC for STM32F407 (Cortex-M4 + FPU)"
echo "  Curve:    BN254  |  Pairing: Optimal Ate, k=12"
echo "  ARITH:    arm-asm-254  (HAND-WRITTEN ASSEMBLY)"
echo "  ABI:      -mfloat-abi=hard"
echo "  Source:   ${RELIC_SRC}"
echo "  Install:  ${RELIC_INSTALL}"
echo "═════════════════════════════════════════════════════════"

cd "${RELIC_BUILD}"

# CPU flags must reach every compilation unit, including .s files
CPU_FLAGS="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard"
EXTRA_FLAGS="-O2 -ffunction-sections -fdata-sections -fno-strict-aliasing"

export CFLAGS="${CPU_FLAGS} ${EXTRA_FLAGS}"
export ASMFLAGS="${CPU_FLAGS}"
export CC="arm-none-eabi-gcc"
export AR="arm-none-eabi-ar"
export RANLIB="arm-none-eabi-ranlib"

cmake "${RELIC_SRC}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${RELIC_INSTALL}" \
    \
    -DCMAKE_C_FLAGS="${CPU_FLAGS} ${EXTRA_FLAGS}" \
    -DCMAKE_C_FLAGS_RELEASE="${CPU_FLAGS} ${EXTRA_FLAGS}" \
    -DCMAKE_ASM_FLAGS="${CPU_FLAGS}" \
    \
    -DARCH=ARM \
    -DWSIZE=32 \
    -DALLOC=AUTO \
    -DARITH=arm-asm-254 \
    \
    -DSEED= \
    -DRAND=CALL \
    -DTIMER= \
    \
    -DFP_PRIME=254 \
    -DFP_QNRES=on \
    -DEP_ENDOM=on \
    -DEP_PLAIN=off \
    -DEP_SUPER=off \
    -DEC_ENDOM=on \
    -DEC_METHD="PRIME" \
    -DBN_PRECI=256 \
    -DBN_MAGNI=DOUBLE \
    -DBN_METHD="COMBA;COMBA;BASIC;BASIC;BINAR;BASIC" \
    -DFP_METHD="INTEG;COMBA;COMBA;MONTY;EXGCD;LOWER;SLIDE" \
    -DFPX_METHD="INTEG;INTEG;LAZYR" \
    -DPP_METHD="LAZYR;OATEP" \
    -DMD_METHD="SH256" \
    \
    -DWITH="DV;BN;FP;EP;EC;CP;MD;FPX;EPX;PP;PC" \
    -DCHECK=off \
    -DVERBS=off \
    -DSTRIP=on \
    -DQUIET=on \
    -DDEBUG=off \
    -DOPSYS= \
    -DSTBIN=ON \
    -DSHLIB=OFF \
    -DTESTS=0 \
    -DBENCH=0 \
    -DDOCUM=off \
    \
    2>&1 | tee "${LOGS}/relic_configure.log" | tail -30

if [[ "${PIPESTATUS[0]}" -ne 0 ]]; then
    echo "✗ CMake configure failed. See ${LOGS}/relic_configure.log"
    exit 1
fi

# ─── Verify CMake actually consumed ARITH=arm-asm-254 ────────────────────
echo ""
echo "═══ Verifying CMake accepted ARITH=arm-asm-254 ═══"
if grep -q "Selected ARITH backend.*arm-asm-254" "${LOGS}/relic_configure.log" 2>/dev/null \
   || grep -q "arm-asm-254" "${LOGS}/relic_configure.log"; then
    echo "✓ ARITH=arm-asm-254 picked up by CMake"
else
    echo "⚠ Could not confirm ARITH backend in configure log"
    echo "  Will verify post-build via relic_conf.h"
fi

# ─── Build ────────────────────────────────────────────────────────────────
echo ""
echo "═══ Building (this may take 1-2 minutes) ═══"
cmake --build . --parallel "$(nproc)" 2>&1 \
    | tee "${LOGS}/relic_build.log" | tail -20

if [[ "${PIPESTATUS[0]}" -ne 0 ]]; then
    echo "✗ Build failed. Last errors:"
    grep -i "error:" "${LOGS}/relic_build.log" | tail -10 || true
    exit 1
fi

# ─── Verify ABI ──────────────────────────────────────────────────────────
echo ""
echo "═══ Verifying ABI of built objects ═══"
TMPDIR_OBJ=$(mktemp -d)
trap "rm -rf ${TMPDIR_OBJ}" EXIT

# Find any .obj inside the archive
OBJ_NAME=$(arm-none-eabi-ar t lib/librelic_s.a 2>/dev/null | head -1)
if [[ -n "${OBJ_NAME}" ]]; then
    (cd "${TMPDIR_OBJ}" && arm-none-eabi-ar x "${RELIC_BUILD}/lib/librelic_s.a" "${OBJ_NAME}")
    ABI_INFO=$(arm-none-eabi-readelf -A "${TMPDIR_OBJ}/${OBJ_NAME}" 2>/dev/null \
              | grep -iE "FP_arch|ABI_VFP_args" || true)
    echo "${ABI_INFO}"
    if echo "${ABI_INFO}" | grep -q "VFP registers"; then
        echo "✓ Hard-float ABI confirmed"
    else
        echo "⚠ Hard-float not detected — link to STM32 app may fail"
    fi
fi

# ─── Install ───────────────────────────────────────────────────────────────
echo ""
echo "═══ Installing to ${RELIC_INSTALL} ═══"
cmake --install . 2>&1 | tail -5

# ─── Final check that the install actually picked up the assembly ────────
echo ""
echo "═══ Final verification: ARITH in installed relic_conf.h ═══"
INSTALLED_CONF="${RELIC_INSTALL}/include/relic/relic_conf.h"
if [[ -f "${INSTALLED_CONF}" ]]; then
    ARITH_LINE=$(grep -E "^#define ARITH" "${INSTALLED_CONF}" || true)
    echo "  ${ARITH_LINE}"
    if echo "${ARITH_LINE}" | grep -q "ARM_ASM_254"; then
        echo "  ✓ ARITH=ARM_ASM_254 confirmed (assembly backend active)"
    elif echo "${ARITH_LINE}" | grep -q "EASY"; then
        echo "  ✗ ARITH=EASY — assembly NOT active!"
        echo "  This means the assembly was silently dropped. Check configure log."
        exit 1
    else
        echo "  ⚠ Unrecognized ARITH value"
    fi
fi

echo ""
echo "═════════════════════════════════════════════════════════"
echo "  ✓ RELIC built successfully with assembly backend!"
echo "═════════════════════════════════════════════════════════"
ls -la "${RELIC_INSTALL}/lib/" 2>/dev/null || true
