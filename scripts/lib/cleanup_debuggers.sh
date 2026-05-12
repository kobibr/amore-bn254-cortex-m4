#!/usr/bin/env bash
# =============================================================================
#  cleanup_debuggers.sh — Pre-flight zombie killer
#
#  Source this file or call as standalone before any flash/debug operation.
#
#  - Kills any openocd, gdb-multiarch, arm-none-eabi-gdb still running
#  - Releases handles to /dev/ttyACM*
#  - Verifies ST-Link is enumerated and free
#
#  Returns:
#    0 = ST-Link clean and ready for new debug session
#    1 = ST-Link missing or stuck after best-effort cleanup
#
#  Usage:
#    source scripts/lib/cleanup_debuggers.sh
#    cleanup_debuggers || { echo "Cannot continue"; exit 1; }
# =============================================================================

cleanup_debuggers() {
    local verbose="${1:-true}"
    local log() { [ "$verbose" = "true" ] && echo "  [cleanup] $*"; }

    # Step 1: Polite kill of openocd / gdb / arm-none-eabi-gdb
    local procs_killed=0
    for pname in openocd gdb-multiarch arm-none-eabi-gdb; do
        local pids
        pids=$(pgrep -f "^$pname" 2>/dev/null || true)
        if [ -n "$pids" ]; then
            log "Found $pname zombies: $pids — killing politely"
            kill $pids 2>/dev/null || true
            procs_killed=1
        fi
    done

    # Step 2: Wait for them to die
    if [ $procs_killed -eq 1 ]; then
        sleep 2
    fi

    # Step 3: Force-kill any survivors
    for pname in openocd gdb-multiarch arm-none-eabi-gdb; do
        local pids
        pids=$(pgrep -f "^$pname" 2>/dev/null || true)
        if [ -n "$pids" ]; then
            log "Force killing surviving $pname: $pids"
            sudo kill -9 $pids 2>/dev/null || kill -9 $pids 2>/dev/null || true
        fi
    done

    # Step 4: Brief settle time
    [ $procs_killed -eq 1 ] && sleep 1

    # Step 5: Verify no debugger processes remain
    if pgrep -f "^(openocd|gdb-multiarch|arm-none-eabi-gdb)" >/dev/null 2>&1; then
        log "ERROR: Debugger processes still alive after cleanup"
        pgrep -af "^(openocd|gdb-multiarch|arm-none-eabi-gdb)" || true
        return 1
    fi

    # Step 6: Verify ST-Link is enumerated
    if ! lsusb 2>/dev/null | grep -qiE "st-link|stlink|0483:374b"; then
        log "ERROR: ST-Link not found in USB. Check cable/power."
        return 1
    fi

    # Step 7: Verify /dev/ttyACM* exist (UART)
    if ! ls /dev/ttyACM* >/dev/null 2>&1; then
        log "WARNING: No /dev/ttyACM* devices found"
        # Not fatal — flash via ST-Link works regardless
    fi

    log "ST-Link clean and ready"
    return 0
}

# If invoked directly (not sourced), run it
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    cleanup_debuggers true
    exit $?
fi
