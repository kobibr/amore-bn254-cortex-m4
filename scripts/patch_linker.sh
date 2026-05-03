#!/usr/bin/env bash
# =============================================================================
#  patch_linker.sh - increases heap/stack in the existing linker script
#
#  Change: _Min_Heap_Size  0x200  -> 0x4000  (16KB)
#          _Min_Stack_Size 0x400  -> 0x4000  (16KB)
#
#  A backup is written to STM32F407VGTX_FLASH.ld.bak before the change.
# =============================================================================
set -euo pipefail

LD_FILE="STM32F407VGTX_FLASH.ld"

if [[ ! -f "${LD_FILE}" ]]; then
    echo "✗ ${LD_FILE} not found in current directory"
    exit 1
fi

# Check whether the patch has already been applied
HEAP_VAL=$(grep -oE "_Min_Heap_Size\s*=\s*0x[0-9a-fA-F]+" "${LD_FILE}" | grep -oE "0x[0-9a-fA-F]+$" || echo "")
STACK_VAL=$(grep -oE "_Min_Stack_Size\s*=\s*0x[0-9a-fA-F]+" "${LD_FILE}" | grep -oE "0x[0-9a-fA-F]+$" || echo "")

echo "Current values:"
echo "  _Min_Heap_Size  = ${HEAP_VAL}"
echo "  _Min_Stack_Size = ${STACK_VAL}"

if [[ "${HEAP_VAL}" == "0x4000" && "${STACK_VAL}" == "0x4000" ]]; then
    echo "✓ Already patched, nothing to do."
    exit 0
fi

# Backup
if [[ ! -f "${LD_FILE}.bak" ]]; then
    cp "${LD_FILE}" "${LD_FILE}.bak"
    echo "  Backup saved: ${LD_FILE}.bak"
fi

# Apply replacement
sed -i -E 's/(_Min_Heap_Size\s*=\s*)0x[0-9a-fA-F]+/\10x4000/' "${LD_FILE}"
sed -i -E 's/(_Min_Stack_Size\s*=\s*)0x[0-9a-fA-F]+/\10x4000/' "${LD_FILE}"

echo ""
echo "After patch:"
grep -E "_Min_Heap_Size|_Min_Stack_Size" "${LD_FILE}" | head -2
echo ""
echo "✓ Linker script patched"
echo "  Heap:  16KB (was ${HEAP_VAL:-unknown})"
echo "  Stack: 16KB (was ${STACK_VAL:-unknown})"
echo ""
echo "  AmorE will continue to work — it uses very little heap/stack."
