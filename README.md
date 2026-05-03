# AmorE BN254 Reference Implementation for Cortex-M4

An embedded implementation of the **AmorE** amortized pairing-delegation
protocol for the BN254 elliptic curve, targeting the STM32F407 Cortex-M4
microcontroller.

> **Curve naming.** This implementation targets the Barreto–Naehrig curve
> with a 254-bit prime field. It is referred to as **BN254** in academic
> cryptography, **alt_bn128** in `py_ecc`, and **BN128** in the
> Ethereum/EVM ecosystem. All three names refer to the same curve.

## Status

This is an independent implementation built from the AmorE pre-print
([eprint.iacr.org/2025/542](https://eprint.iacr.org/2025/542)). It is a
personal research project and is not affiliated with the paper's authors.
The protocol has been implemented end-to-end on real hardware and validated
against an optimized native pairing library (RELIC) on the same MCU.

## What is AmorE?

AmorE — *Amortized Efficiency for Pairing Delegation* — is a recent
protocol by **Antonio Pérez Keilty, Diego F. Aranha, Elena Pagnin, and
Francisco Rodríguez-Henríquez** (eprint 2025/542) that lets a
memory-constrained device outsource pairing computations to an untrusted
helper while preserving:

- **Input privacy** — the helper never sees the plaintext inputs
  `(A, B)`; only blinded versions are transmitted.
- **Cheating detection** — a forged response from a malicious helper is
  rejected with overwhelming probability.
- **Amortization** — per-round client cost is essentially constant
  across batch sizes.

The protocol is particularly relevant for IoT and embedded settings
where a full pairing library does not fit in memory but the device must
still participate in pairing-based cryptography (BLS signatures,
Identity-Based Encryption, ZKP verification, etc.).

## What this repository contains

A complete, self-contained client implementation:

- **Field arithmetic** for `Fp`, `Fp²`, `Fp¹²` over a 254-bit prime
  (CIOS Montgomery, 8 × 32-bit limbs).
- **Group operations** on G1 (over `Fp`) and G2 (over `Fp²`) in Jacobian
  coordinates.
- **AmorE protocol logic** — `OneTimeSetup`, `Setup`, and `Verify` phases.
- **UART transport layer** with a length-prefixed framing format and a
  CRC8 over a 921 600-baud link.
- **Companion Python server** (`rpi/server.py`) that runs on a Raspberry Pi
  and computes pairings using `py_ecc`.
- **Telemetry struct** (`g_results`) read post-run via GDB from SRAM,
  giving cycle-accurate timing for every protocol phase.

The repository also includes a separate **RELIC pairing benchmark**
(`relic_bench.elf`) and a **regression test for RELIC's `arm-asm-254`
backend** (`regression_test.elf`) used in the validation analysis.

## Hardware setup

```
                        ┌───────────── ────┐
                        │     Laptop       │
                        └────┬───── ──┬────┘
              USB (ST-Link)  │        │  LAN (SSH/SCP)
                             │        │
                             ▼        ▼
      ┌──────────────────────┐        ┌──────────────────────┐
      │  STM32F407 Discovery │        │   Raspberry Pi 3B    │
      │                      │  UART  │                      │
      │     PA2 TX ──────► GPIO15 (RX)│  /dev/ttyAMA0        │
      │     PA3 RX ◄────── GPIO14 (TX)│  server.py           │
      │                      │ 921600 │                      │
      └──────────────────────┘        └──────────────────────┘
```

- **Client (STM32F407 Discovery):** runs the AmorE protocol; 168 MHz
  Cortex-M4 with FPU.
- **Server (Raspberry Pi 3B):** runs `py_ecc 8.0.0` to compute pairings
  on demand; communicates with the STM32 over hardware UART (PA2/PA3).
- **Laptop:** builds, flashes, and reads telemetry over GDB.

See [`doc/SYSTEM_DOC.md`](doc/SYSTEM_DOC.md) for the complete protocol,
packet format, status codes, and timing model.

## Validation summary

A complete benchmark report is in
[`doc/AmorE_BN128_Results.txt`](doc/AmorE_BN128_Results.txt). Headline
results from one run on STM32F407 Discovery @ 168 MHz:

### Correctness

| Metric                            | Result                  |
|-----------------------------------|-------------------------|
| Honest rounds accepted            | **61 / 61**  (100.0%)   |
| Malicious round rejected          | **1 / 1**  (correctly)  |
| Verify failures                   | 0                       |
| UART / CRC / CMD / length errors  | 0                       |
| Final status word                 | `0x600D0000`  (PASS)    |

### Memory footprint

| Section          | Size       | % of available           |
|------------------|------------|--------------------------|
| Flash (`.text`)  | 18,532 B   | 1.77 % of 1 MB           |
| SRAM (`.bss`)    | 3,156 B    | 1.65 % of 192 KB         |
| **Total ELF**    | **21,708 B** | —                      |

### Timing (DWT cycle counter at 168 MHz)

| Operation                  | N=50 amortized       |
|----------------------------|----------------------|
| Blind (Setup)              | 199.4 ms / round     |
| Verify                     | 182.4 ms / round     |
| **Per-round amortized**    | **381.8 ms / round** |

Across batch sizes N=1, N=10, N=50, the per-round cost varies by less
than 2.5 % — confirming the protocol's claimed O(1) amortized
complexity per round.

## Apples-to-apples comparison vs RELIC

To put the AmorE results in honest context, this repository also
includes a benchmark of an optimized native BN254 pairing on the same
STM32 hardware, using [RELIC v0.6.0](https://github.com/relic-toolkit/relic)
with its hand-tuned `arm-asm-254` backend.

| Operation                                                | Time     |
|----------------------------------------------------------|----------|
| Single BN254 pairing on STM32F4 (RELIC, optimized C+asm) | 252.3 ms |
| AmorE per-round amortized cost (N=50)                    | 381.8 ms |

So **AmorE costs ≈1.5× more** per round than computing a single
BN254 pairing directly on the same STM32 with optimized code. AmorE's
value on this part is therefore not raw speed; it is in:

- **Memory footprint** — the AmorE client is **33× smaller** in SRAM and
  **3× smaller** in Flash than a full RELIC pairing build.
- **Privacy** — the server never sees plaintext inputs.
- **Feasibility on smaller MCUs** — RELIC's pairing state (101 KB SRAM)
  does not fit on parts like STM32L0/G0 with 4–32 KB SRAM, where the
  AmorE client comfortably fits.

The full discussion is in Sections 7, 11, and 12 of the results report.

## Repository layout

```
amore-bn254-cortex-m4/
├── src/                 # AmorE client implementation (C, 19 files)
├── inc/                 # headers (15 files)
├── rpi/server.py        # Python server for the Raspberry Pi
├── scripts/             # build, flash, benchmark, diagnostic scripts
├── tests/               # Python tests (correctness vs reference)
├── cmake/               # arm-none-eabi toolchain file
├── doc/
│   ├── SYSTEM_DOC.md             # full system documentation
│   └── AmorE_BN128_Results.txt   # complete benchmark report
├── CMakeLists.txt
└── STM32F407VGTX_FLASH.ld
```

## Building

See [`INSTALLATION.md`](INSTALLATION.md) for the full setup instructions
(toolchain, RELIC build, RPi configuration).

In summary:

```bash
# 1. Install the ARM toolchain and dependencies
sudo apt install gcc-arm-none-eabi gdb-multiarch openocd stlink-tools

# 2. Place STM32CubeF4 v1.27.0 alongside this repo (or set CUBE_ROOT)
#    See INSTALLATION.md for details.

# 3. Build RELIC for the target (required for relic_bench.elf and regression_test.elf)
bash scripts/build-relic.sh

# 4. Configure and build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake
cmake --build build

# 5. Flash the AmorE client
cmake --build build --target flash
```

This produces three ELFs:
- `amorebn128.elf` — the AmorE client (the main contribution).
- `relic_bench.elf` — local pairing benchmark via RELIC, used for the
  apples-to-apples comparison.
- `regression_test.elf` — verifies that the RELIC `arm-asm-254` assembly
  backend produces numerically correct outputs for the patched
  toolchain (see the upstream pull request
  [relic-toolkit/relic#317](https://github.com/relic-toolkit/relic/pull/317)).

## Running a full benchmark

With the STM32 connected via ST-Link and a Raspberry Pi reachable on
the LAN:

```bash
# Discover the RPi automatically (mDNS, with ARP scan as fallback),
# build, flash, run the protocol, and dump telemetry over GDB.
bash scripts/run_benchmark.sh
```

Output includes:

- Server-side log (per-round pairing timings).
- STM32-side telemetry (cycles, phases, verification outcomes).
- A combined report under `logs/`.

## RELIC toolchain fix

The RELIC `arm-asm-254` Cortex-M4 backend used by this project's
`relic_bench.elf` and `regression_test.elf` requires a small toolchain
fix to build with modern `arm-none-eabi-gcc` (≥ 11). The fix has been
submitted upstream:

→ **[relic-toolkit/relic#317](https://github.com/relic-toolkit/relic/pull/317)**

Until merged upstream, `scripts/build-relic.sh` will use the author's fork at
[github.com/kobibr/relic](https://github.com/kobibr/relic) on branch
`arm-m4-toolchain-fix`.

## Citing

If you build on this work, please cite the original AmorE paper:

> A. Pérez Keilty, D. F. Aranha, E. Pagnin, F. Rodríguez-Henríquez.
> *AmorE: Amortized Efficiency for Pairing Delegation.*
> Cryptology ePrint Archive, Paper 2025/542, 2025.
> [https://eprint.iacr.org/2025/542](https://eprint.iacr.org/2025/542)

## License

[Apache License 2.0](LICENSE). See `LICENSE` for the full text.

The included STM32 HAL templates and RELIC are distributed under their
own licenses; this repository contains only the original AmorE client
code, build scripts, and documentation.

## Author

Kobi Brener  ·  kob.tov@gmail.com  ·  [github.com/kobibr](https://github.com/kobibr)
