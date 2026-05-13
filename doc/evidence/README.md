# Disassembly evidence for -O2 vs -O3 fp_mul

## Background

AmorE's BLS12-381 client-side Fp arithmetic uses a CIOS Montgomery
multiplication implemented in `src/fp.c::fp_mul`. The function operates
on 12-limb 32-bit fields (BLS12-381 prime fits in 12x32 bits).

The 2.14x speedup observed when rebuilding AmorE at GCC -O3 instead of
-O2 (see Section 8 of `doc/AmorE_BLS12_381_Results.txt`) is attributable
primarily to GCC's loop unrolling at -O3.

## Files

- `fp_mul_O2_6ce9f8de.asm`: disassembly of fp_mul from
  `main_O2_6ce9f8de.elf` (binary SHA prefix 6ce9f8de...)
  Size: 376 bytes (0x178)
  Compiled with: `-O2 -g3 -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`

- `fp_mul_O3_4e2df263.asm`: disassembly of fp_mul from
  `main_O3_4e2df263.elf` (binary SHA prefix 4e2df263...)
  Size: 1,300 bytes (0x514)
  Compiled with: `-O3 -DNDEBUG -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard`
  (CMAKE_BUILD_TYPE=Release)

## How to reproduce

```bash
# Build -O2
cmake -B build/O2 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake -DCURVE=BLS12_381

# Build -O3
cmake -B build/O3 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake -DCURVE=BLS12_381 -DCMAKE_BUILD_TYPE=Release

# Disassemble
arm-none-eabi-objdump -d -S build/O2/amore_bls12_381.elf > fp_mul_O2.txt
arm-none-eabi-objdump -d -S build/O3/amore_bls12_381.elf > fp_mul_O3.txt
```

## Key observations

- **-O2 version (214 lines):** shows clear loop structure with C source
  comments `for (int i = 0; i <= FP_LIMBS + 1; i++)` and ARM
  instructions with loop counter increment.

- **-O3 version (617 lines):** fully unrolled. The 12-iteration inner
  loop `for (int j = 0; j < FP_LIMBS; j++)` is expanded into explicit
  `ldr [r1, #0]`, `ldr [r1, #4]`, `ldr [r1, #8]`, ... patterns
  showing all 12 iterations as separate instructions.

- The 3.46x code size ratio (1,300 / 376 bytes) and 2.88x line count
  ratio (617 / 214) are consistent with full unrolling of a 12-iteration
  CIOS Montgomery loop.

## Validation

Both binaries passed 61/61 + 1/1 validation:
- 50 honest rounds + 1 malicious round detected
- Status code 0x600D0000 in both cases

Measurement logs:
- -O2: `logs/regression_20260512_070929/bls12_381_baseline.txt`
  N50_amort_ms=1919.3
- -O3: `logs/combined_report_20260512_090923.txt`
  N=50 amort_cyc=150,860,571 = 898.0 ms

Speedup: 1919.3 / 898.0 = 2.137x ("2.14x")

## librelic_s.a unchanged

The RELIC pairing library used as the comparison baseline was NOT
rebuilt when AmorE moved from -O2 to -O3. Verified by SHA256:

  librelic_s.a SHA: 58431811b8974d3773436ed3ff3cfb8e226f92fd5b1d82d5e92c11afbf7aeab7
  Timestamp:        2026-05-05 17:55:48 (unchanged)

This isolates the variable: the AmorE -O2 to -O3 comparison is purely
about AmorE's compilation flags.
