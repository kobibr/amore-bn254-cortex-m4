# Day 5 Critical Discovery: NRST Floating Caused Mode B "CURVE_INIT Hang"

**Date:** 2026-05-28
**Severity:** Root cause of ALL Day 4 measurement failures.

## Symptom (chased ~12 hours)

RELIC Mode B firmware appeared to hang forever in ep_param_set_any_pairf():
- current_phase = 0x04 (CURVE_INIT), never advanced
- log_head = 2 (only INIT + CURVE_INIT logged)
- status = 0xBAD00000 (never reached success write)
- PC moved inside RELIC code (fp_smb_jmpds, fp_rdcn_low)
- Persisted 12+ minutes — looked like an infinite loop

## False leads ruled out

1. FP_SMB=JMPDS looping -> rebuilt FP_SMB=BASIC -> still hung. NOT it.
2. ep_param_set_any_pairf curve-search -> direct ep_param_set -> still hung. NOT it.
3. Heap/stack too small -> 0x4000 -> still hung. NOT it.
4. RELIC easy too slow -> waited 30 min -> still phase=4. NOT slow.
5. Build/compiler drift -> flashed EXACT May 25 binary -> still hung. NOT build.

## Root cause

STM32 was RESET-LOOPING every ~2.7 ms. Never had time to finish ep_param_set
(which legitimately takes ~3s here).

Evidence:
- CYCCNT always ~430,000 (2.6ms @ 168MHz) on every poll - counter restarting
- uwTick (HAL ms) stuck at 2 - SysTick never accumulated
- RCC_CSR showed PINRSTF (NRST pin reset) set repeatedly

### Why NRST floated

Reset helper used:
    sudo timeout 0.3 gpioset --consumer nrst -c gpiochip0 18=0
When gpioset exits, libgpiod releases the line to high-Z. STM32 internal
NRST pull-up (~40k) is weak; floating RPi GPIO 18 picked up noise that
re-triggered NRST every few ms.

## Proven fix

Hold NRST HIGH actively after the pulse via backgrounded gpioset:
    sudo timeout 0.1 gpioset -c gpiochip0 18=0
    sudo setsid bash -c 'gpioset -c gpiochip0 18=1' </dev/null >/dev/null 2>&1 &

With NRST held high, BLS12-381 Mode B completed:
    phase    = 0x000000FF (DONE)
    status   = 0x600D0000 (SUCCESS)
    pair_min = 87,933,198 cycles = 523.4 ms
EXACTLY matches Diego 2026-05-07 (523.4ms) and 2026-05-13 (87,933,033 cyc).
Mode B was never broken in firmware.

## Three-way constraint (orchestrator)

  Reset method            NRST stable?      PPK2 D-channels survive?
  openocd reset run       YES (push-pull)   NO (SWD breaks them)
  gpioset timeout (float) NO (floats)       YES (no SWD)
  gpioset hold-high       YES               YES (no SWD)

Only gpioset hold-high satisfies both. The hold process must be killed
before the next NRST pulse and before any openocd needing SWD/SRST, then
re-established.

## Implication for Day 4 data

All Day 4 cells ran with floating NRST. They reset-looped throughout:
- gpio_byte = 0 in every CSV (PA0 never driven)
- ~124 mA constant = CPU spinning in early CURVE_INIT, restarting
Day 4 CSVs are NOT valid pairing measurements. Must re-run.

## Outstanding (deferred to next session)

PPK2 D-channels stuck reading 255 though multimeter confirms hardware is
perfect (D0 tracks PA0: 3.0V HIGH / 0V LOW, VCC=3.1V, GND continuity OK).
USB unplug+replug did NOT recover this time (did earlier in day).
Suspected PPK2 firmware sampling state. May need full power-off or PPK2
firmware reflash.
