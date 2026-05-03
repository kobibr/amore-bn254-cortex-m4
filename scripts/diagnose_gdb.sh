#!/usr/bin/env bash
# =============================================================================
#  diagnose_gdb.sh
#  Full diagnostic for the GDB telemetry stage — pinpoints exactly what is
#  broken when reading g_results from the STM32 fails.
#
#  Usage:  bash diagnose_gdb.sh [path/to/amorebn128.elf]
# =============================================================================
set -uo pipefail

ELF="${1:-build/amorebn128.elf}"

RED='\033[91m'; GRN='\033[92m'; YLW='\033[93m'
BLU='\033[94m'; CYN='\033[96m'; RST='\033[0m'; BOLD='\033[1m'
ok()    { echo -e "  ${GRN}✓${RST}  $*"; }
fail()  { echo -e "  ${RED}✗${RST}  $*"; }
warn()  { echo -e "  ${YLW}⚠${RST}  $*"; }
info()  { echo -e "  ${CYN}→${RST}  $*"; }
head()  { echo -e "\n${BOLD}${BLU}══ $* ══${RST}"; }
sep()   { echo -e "${BLU}────────────────────────────────────────────────────${RST}"; }

GDB_FOUND=""
OCD_FOUND=""
DIAG_ERRORS=0

echo ""
echo -e "${BOLD}${BLU}╔══════════════════════════════════════════════════╗${RST}"
echo -e "${BOLD}${BLU}║   AmorE GDB Telemetry — Diagnostic Tool          ║${RST}"
echo -e "${BOLD}${BLU}╚══════════════════════════════════════════════════╝${RST}"
echo ""

# =============================================================================
# STEP 1 — System info
# =============================================================================
head "STEP 1: System info"
info "OS       : $(uname -srm)"
info "User     : $(whoami)"
info "Shell    : $SHELL"
info "Date     : $(date)"
info "ELF arg  : ${ELF}"
echo ""

# =============================================================================
# STEP 2 — Locate an ARM-capable GDB
# =============================================================================
head "STEP 2: Locate ARM GDB"

GDB_CANDIDATES=(
    "arm-none-eabi-gdb"
    "gdb-multiarch"
    "arm-linux-gnueabihf-gdb"
    "/usr/bin/arm-none-eabi-gdb"
    "/usr/local/bin/arm-none-eabi-gdb"
    "/opt/arm-none-eabi/bin/arm-none-eabi-gdb"
    "/opt/gcc-arm-none-eabi/bin/arm-none-eabi-gdb"
    "$HOME/.local/bin/arm-none-eabi-gdb"
)

echo ""
info "Checking known candidates:"
for gdb in "${GDB_CANDIDATES[@]}"; do
    if command -v "$gdb" &>/dev/null 2>&1 || [[ -x "$gdb" ]]; then
        VER=$("$gdb" --version 2>/dev/null | head -1 || echo "unknown version")
        ok "Found: $(command -v "$gdb" 2>/dev/null || echo "$gdb")  →  $VER"
        [[ -z "$GDB_FOUND" ]] && GDB_FOUND="$(command -v "$gdb" 2>/dev/null || echo "$gdb")"
    else
        fail "Not found: $gdb"
    fi
done

echo ""
info "Broader filesystem search (find may take a few seconds)..."
mapfile -t FOUND_GDB < <(find /usr /opt /home "$HOME" -name "*arm*gdb*" -o -name "gdb-multiarch" 2>/dev/null | grep -v "\.py$" | grep -v "\.pyc$" | sort -u 2>/dev/null || true)
if [[ ${#FOUND_GDB[@]} -gt 0 ]]; then
    info "GDB-like files found on the filesystem:"
    for f in "${FOUND_GDB[@]}"; do
        if [[ -x "$f" ]]; then
            ok "  (executable) $f"
            [[ -z "$GDB_FOUND" ]] && GDB_FOUND="$f"
        else
            warn "  (not exec)   $f"
        fi
    done
else
    fail "No GDB binaries found under /usr /opt /home"
fi

echo ""
info "Current PATH:"
echo "  $PATH" | tr ':' '\n' | sed 's/^/    /'

echo ""
if [[ -z "$GDB_FOUND" ]]; then
    fail "No GDB available — this is the blocking issue."
    echo ""
    echo -e "  ${YLW}Suggested fix:${RST}"
    echo "    sudo apt-get install -y gdb-multiarch"
    echo "    # or:"
    echo "    sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi"
    echo "    # then expose it under the canonical name:"
    echo "    sudo ln -sf \$(which gdb-multiarch) /usr/local/bin/arm-none-eabi-gdb"
    DIAG_ERRORS=$((DIAG_ERRORS + 1))
else
    ok "GDB to be used: ${GDB_FOUND}"
fi

# =============================================================================
# STEP 3 — Locate OpenOCD
# =============================================================================
head "STEP 3: Locate OpenOCD"

OCD_CANDIDATES=(
    "openocd"
    "/usr/bin/openocd"
    "/usr/local/bin/openocd"
    "/opt/openocd/bin/openocd"
)

echo ""
for ocd in "${OCD_CANDIDATES[@]}"; do
    if command -v "$ocd" &>/dev/null 2>&1 || [[ -x "$ocd" ]]; then
        VER=$("$ocd" --version 2>&1 | head -1 || echo "unknown")
        ok "Found: $(command -v "$ocd" 2>/dev/null || echo "$ocd")  →  $VER"
        [[ -z "$OCD_FOUND" ]] && OCD_FOUND="$(command -v "$ocd" 2>/dev/null || echo "$ocd")"
    else
        fail "Not found: $ocd"
    fi
done

if [[ -z "$OCD_FOUND" ]]; then
    fail "OpenOCD is missing."
    echo ""
    echo -e "  ${YLW}Suggested fix:${RST}"
    echo "    sudo apt-get install -y openocd"
    DIAG_ERRORS=$((DIAG_ERRORS + 1))
else
    ok "OpenOCD to be used: ${OCD_FOUND}"

    echo ""
    info "Locating OpenOCD config files:"
    OCD_PREFIX=$("$OCD_FOUND" --version 2>&1 | grep -oP 'prefix=\K[^\s]+' || true)
    OCD_SHARE=""
    for candidate in \
        "/usr/share/openocd/scripts" \
        "/usr/local/share/openocd/scripts" \
        "/opt/openocd/share/openocd/scripts" \
        "${OCD_PREFIX}/share/openocd/scripts"
    do
        if [[ -d "$candidate" ]]; then
            ok "scripts dir: $candidate"
            OCD_SHARE="$candidate"
            break
        fi
    done
    [[ -z "$OCD_SHARE" ]] && fail "OpenOCD scripts directory not found"

    if [[ -n "$OCD_SHARE" ]]; then
        [[ -f "$OCD_SHARE/interface/stlink.cfg" ]] && \
            ok "  interface/stlink.cfg ✓" || \
            fail "  interface/stlink.cfg missing"
        [[ -f "$OCD_SHARE/target/stm32f4x.cfg" ]] && \
            ok "  target/stm32f4x.cfg ✓" || \
            fail "  target/stm32f4x.cfg missing"
    fi
fi

# =============================================================================
# STEP 4 — Inspect the ELF
# =============================================================================
head "STEP 4: Inspect the ELF"
echo ""

if [[ ! -f "$ELF" ]]; then
    fail "ELF does not exist: $ELF"
    info "Pass a path explicitly: bash diagnose_gdb.sh path/to/amorebn128.elf"
    DIAG_ERRORS=$((DIAG_ERRORS + 1))
else
    ok "ELF exists: $ELF"
    info "Size: $(du -h "$ELF" | cut -f1)"
    info "Type: $(file "$ELF")"

    # Check for the g_results symbol
    if command -v arm-none-eabi-nm &>/dev/null || command -v nm &>/dev/null; then
        NM_CMD=$(command -v arm-none-eabi-nm 2>/dev/null || command -v nm)
        GSYM=$("$NM_CMD" "$ELF" 2>/dev/null | grep "g_results" || true)
        if [[ -n "$GSYM" ]]; then
            ok "Symbol g_results found in ELF:"
            echo "    $GSYM"
        else
            fail "Symbol g_results not found in ELF — GDB will not be able to read results."
            DIAG_ERRORS=$((DIAG_ERRORS + 1))
        fi
    else
        warn "nm not available — cannot verify symbols"
    fi
fi

# =============================================================================
# STEP 5 — Check ST-Link / USB
# =============================================================================
head "STEP 5: ST-Link / USB"
echo ""

if command -v st-info &>/dev/null; then
    ok "st-info found: $(command -v st-info)"
    info "Running st-info --probe:"
    echo ""
    ST_OUT=$(st-info --probe 2>&1 || true)
    echo "$ST_OUT" | sed 's/^/    /'
    echo ""
    if echo "$ST_OUT" | grep -qi "serial\|stlink\|chip\|flash"; then
        ok "ST-Link detected"
    else
        fail "ST-Link not detected — check the USB connection."
        DIAG_ERRORS=$((DIAG_ERRORS + 1))
    fi
else
    fail "st-info not found — install stlink-tools"
    DIAG_ERRORS=$((DIAG_ERRORS + 1))
fi

# Direct USB check
echo ""
info "USB devices (lsusb):"
lsusb 2>/dev/null | grep -i "st\|stlink\|0483" | sed 's/^/    /' || warn "No ST-Link entry found in lsusb"

# =============================================================================
# STEP 6 — Try connecting via OpenOCD (if all tools are available)
# =============================================================================
head "STEP 6: Attempt OpenOCD connection (5 seconds)"
echo ""

if [[ -n "$OCD_FOUND" ]]; then
    info "Running OpenOCD for 5 seconds and checking for connection..."
    OCD_LOG=$(mktemp /tmp/ocd_diag_XXXX.log)

    timeout 5 "$OCD_FOUND" \
        -f interface/stlink.cfg \
        -f target/stm32f4x.cfg \
        -c "init; halt; sleep 100; exit" \
        > "$OCD_LOG" 2>&1 || true

    echo ""
    info "OpenOCD output:"
    cat "$OCD_LOG" | sed 's/^/    /'
    echo ""

    if grep -qi "halted\|halt\|cortex\|stm32\|target state" "$OCD_LOG"; then
        ok "OpenOCD connected to STM32 successfully"
    elif grep -qi "error\|failed\|no device\|not found\|libusb" "$OCD_LOG"; then
        fail "OpenOCD failed to connect:"
        grep -i "error\|failed\|not found" "$OCD_LOG" | head -5 | sed 's/^/    /'
        DIAG_ERRORS=$((DIAG_ERRORS + 1))
    else
        warn "OpenOCD: outcome unclear — see log above"
    fi
    rm -f "$OCD_LOG"
else
    warn "OpenOCD not available — skipping connection test"
fi

# =============================================================================
# STEP 7 — Full GDB run reading g_results (if all tools are available)
# =============================================================================
head "STEP 7: Full GDB read of g_results"
echo ""

if [[ -n "$GDB_FOUND" && -n "$OCD_FOUND" && -f "$ELF" ]]; then
    info "GDB: $GDB_FOUND"
    info "OCD: $OCD_FOUND"
    info "ELF: $ELF"
    echo ""

    GDB_LOG=$(mktemp /tmp/gdb_diag_XXXX.log)
    GDB_SCRIPT=$(mktemp /tmp/gdb_diag_XXXX.gdb)

    cat > "$GDB_SCRIPT" << GDBEOF
set pagination off
set confirm off
set print pretty on

# Connect via OpenOCD
target extended-remote | ${OCD_FOUND} \\
    -f interface/stlink.cfg \\
    -f target/stm32f4x.cfg \\
    -c "gdb_port pipe; log_output /dev/null"

file ${ELF}

printf "\\n=== GDB DIAGNOSTIC DUMP ===\\n"
printf "[Connection] OK - connected to STM32\\n"

printf "\\n[g_results address]\\n"
info address g_results

printf "\\n[Raw status word]\\n"
printf "  magic  = 0x%08x\\n", g_results.magic
printf "  status = 0x%08x\\n", g_results.status
printf "  phase  = 0x%02x\\n",  g_results.current_phase
printf "  error  = 0x%08x\\n", g_results.last_error

printf "\\n[Batch results]\\n"
printf "  N=1  sent=%u verify_ok=%u\\n", g_results.rounds_sent[0], g_results.rounds_verify_ok[0]
printf "  N=10 sent=%u verify_ok=%u\\n", g_results.rounds_sent[1], g_results.rounds_verify_ok[1]
printf "  N=50 sent=%u verify_ok=%u\\n", g_results.rounds_sent[2], g_results.rounds_verify_ok[2]

printf "\\n[Security]\\n"
printf "  security_ok = %u\\n", g_results.security_ok

printf "\\n[Timing]\\n"
printf "  wall_ms     = %u\\n", g_results.wall_ms
printf "  ots_cycles  = %u\\n", g_results.ots_cycles

printf "\\n=== END GDB DIAGNOSTIC ===\\n"
quit
GDBEOF

    info "Running GDB (timeout 30s)..."
    echo ""
    timeout 30 "$GDB_FOUND" -nx -batch -x "$GDB_SCRIPT" \
        2>&1 | tee "$GDB_LOG" | grep -v "^$" | grep -v "^Reading" | grep -v "^warning"

    echo ""
    if grep -q "Connection.*OK\|END GDB DIAGNOSTIC" "$GDB_LOG"; then
        ok "GDB connected and read g_results successfully"
    elif grep -qi "no such symbol\|undefined symbol" "$GDB_LOG"; then
        fail "GDB connected but g_results is missing — wrong ELF?"
        DIAG_ERRORS=$((DIAG_ERRORS + 1))
    elif grep -qi "connection refused\|failed\|error\|timeout" "$GDB_LOG"; then
        fail "GDB failed to connect:"
        grep -i "error\|failed\|refused" "$GDB_LOG" | head -5 | sed 's/^/    /'
        DIAG_ERRORS=$((DIAG_ERRORS + 1))
    else
        warn "Outcome unclear — see log above"
    fi

    rm -f "$GDB_LOG" "$GDB_SCRIPT"
else
    [[ -z "$GDB_FOUND" ]] && fail "GDB missing — skipping Step 7"
    [[ -z "$OCD_FOUND" ]] && fail "OpenOCD missing — skipping Step 7"
    [[ ! -f "$ELF" ]]     && fail "ELF missing — skipping Step 7"
fi

# =============================================================================
# STEP 8 — Summary and next actions
# =============================================================================
head "STEP 8: Summary"
sep
echo ""

if [[ $DIAG_ERRORS -eq 0 && -n "$GDB_FOUND" && -n "$OCD_FOUND" ]]; then
    echo -e "  ${BOLD}${GRN}✓ All checks passed — GDB telemetry should work.${RST}"
    echo ""
    echo -e "  ${CYN}To run only the GDB stage of the benchmark:${RST}"
    echo "    GDB_CMD=\"${GDB_FOUND}\" bash run_benchmark.sh --no-flash"
else
    echo -e "  ${BOLD}${RED}✗ ${DIAG_ERRORS} issue(s) found.${RST}"
    echo ""

    if [[ -z "$GDB_FOUND" ]]; then
        echo -e "  ${YLW}▶ GDB missing — install with:${RST}"
        echo "      sudo apt-get install -y gdb-multiarch"
        echo "      sudo ln -sf \$(which gdb-multiarch) /usr/local/bin/arm-none-eabi-gdb"
        echo ""
    fi

    if [[ -z "$OCD_FOUND" ]]; then
        echo -e "  ${YLW}▶ OpenOCD missing — install with:${RST}"
        echo "      sudo apt-get install -y openocd"
        echo ""
    fi

    if [[ ! -f "$ELF" ]]; then
        echo -e "  ${YLW}▶ ELF missing — build first:${RST}"
        echo "      cmake --build build"
        echo ""
    fi
fi

sep
echo ""
echo -e "  ${CYN}After applying fixes, re-run:${RST}"
echo "    bash diagnose_gdb.sh ${ELF}"
echo ""
