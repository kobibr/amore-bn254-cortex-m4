# AmorE on BN254 — System Documentation

> **Environment variables used in this document.**
> Every command in this document references the following variables.
> Set them in your shell before running anything:
> ```bash
> export RPI_HOST=$(getent hosts raspberrypi.local | awk '{print $1}')   # dynamic IP via mDNS
> export RPI_USER=pi                                                       # SSH user on the RPi
> export PROJECT_DIR=<path-to-project>                                     # project path (laptop and RPi)
> ```
> If mDNS is not available, `run_benchmark.sh` can still be run without
> these variables — it falls back to an ARP scan.

---

## 1. Devices in the system

### Laptop (Linux)
- **Role:** build, flash, run the master script, read GDB telemetry.
- **STM32 connection:** USB to the on-board ST-Link of the Discovery board.
- **RPi connection:** LAN (SSH / SCP).
- **Tools:** `cmake`, `arm-none-eabi-gcc`, `st-flash`, `gdb-multiarch`, `openocd`.

### Raspberry Pi 3B
- **Role:** runs `server.py`; computes BN254 pairings and returns the results to the STM32.
- **Address:** `${RPI_HOST}` (dynamic, discovered via mDNS or ARP on each run).
- **SSH user:** `${RPI_USER}` (default: `pi`).
- **STM32 connection:** hardware UART on `/dev/ttyAMA0`, GPIO 14/15 (TX/RX).
- **Baud rate:** 921600.
- **Tools:** `python3`, `py_ecc 8.0.0`, `pyserial`.
- **`server.py` location:** `${PROJECT_DIR}/server.py`.

### STM32F407 Discovery
- **Role:** runs the AmorE protocol — sends challenges to the RPi and verifies the responses.
- **Laptop connection:** USB to ST-Link (flash + debug).
- **RPi connection:** UART on PA2 (TX) / PA3 (RX) via USART2.
- **Clock:** 168 MHz (Cortex-M4 + FPU).
- **Memory:** 1 MB flash, 192 KB SRAM.

---

## 2. Wiring diagram

```
 ┌─────────────────┐        USB (ST-Link)       ┌──────────────────────┐
 │                 │◄──────────────────────────►│  STM32F407 Discovery │
 │     Laptop      │                            │                      │
 │                 │        LAN (SSH/SCP)        │  PA2 TX ──► GPIO15   │
 │                 │◄──────────────────────────►│  PA3 RX ◄── GPIO14   │
 └─────────────────┘                             └──────────┬───────────┘
                                                            │ UART 921600
                                                 ┌──────────▼───────────┐
                                                 │  Raspberry Pi 3B     │
                                                 │  /dev/ttyAMA0        │
                                                 │  server.py           │
                                                 └──────────────────────┘
```

---

## 3. End-to-end flow — `./run_benchmark.sh`

### Step 1 — Build (on the laptop)
- Cleans the `build/` directory.
- Runs `cmake` and `make`.
- Produces `amorebn128.elf`, `.hex`, and `.bin`.

### Step 2 — Flash (laptop → STM32)
- Detaches `cdc_acm` so libusb can claim the ST-Link.
- Runs `st-flash write amorebn128.bin 0x08000000`.
- Re-attaches `cdc_acm` (restores `/dev/ttyACM0`).
- The STM32 boots and waits for `CMD_READY` (timeout: 30 seconds).

### Step 3 — Server (RPi)
- The script discovers the RPi on the network (mDNS, with ARP scan as fallback).
- The laptop connects via SSH and runs:
  ```
  python3 ${PROJECT_DIR}/server.py --port /dev/ttyAMA0 --baud 921600 --honest-rounds 61
  ```
- The output is streamed back to the laptop and saved to `logs/server_TIMESTAMP.log`.

### Step 4 — GDB telemetry (laptop → STM32)
- `gdb-multiarch` connects to the STM32 via OpenOCD + ST-Link.
- Reads the `g_results` struct directly from SRAM.
- Prints full telemetry: cycles, timing, phase log, status code.

---

## 4. UART protocol (packet format)

```
[0xAA][0x55][CMD:1][LEN_LO:1][LEN_HI:1][DATA:LEN][CRC8:1]
```

| Command     | Hex  | Direction      | Data                                |
|-------------|------|----------------|-------------------------------------|
| CMD_READY   | 0x40 | RPi → STM32    | 1 byte: 0=honest, 1=malicious       |
| CMD_SETUP   | 0x10 | STM32 → RPi    | 384 bytes: A(64)+B(128)+C(64)+D(128) |
| CMD_RESULT  | 0x20 | RPi → STM32    | 768 bytes: gamma(384)+rho(384)      |
| CMD_STATUS  | 0x30 | STM32 → RPi    | 1 byte: 1=ok, 0=fail                |

**CRC8** is the XOR of `CMD ^ LEN_LO ^ LEN_HI ^ all DATA bytes`.

---

## 5. AmorE protocol — what happens in each round

```
STM32                                    RPi (server.py)
  │                                           │
  │──── CMD_SETUP (A, B, C, D) ─────────────►│
  │                                           │  computes:
  │                                           │  rho   = e(A, B)
  │                                           │  gamma = e(A, D) * e(C, G2)
  │◄─── CMD_RESULT (gamma, rho) ─────────────│
  │                                           │
  │  verifies:                                │
  │  xi == rho^r * gamma ?                    │
  │                                           │
  │──── CMD_STATUS (ok/fail) ───────────────►│
```

### OneTimeSetup (once at startup)
- The STM32 picks a random secret scalar `s`.
- Computes `xi = e(G1, G2)^(−s)` — this is the verification "key".

### Setup (per round)
- The STM32 generates a fresh blinding value `u`.
- Sends `C = −(s/u) * (U + A)` and `D = (s/u) * G2 − r * B`.
- Stores the short scalar `r` for later verification.

### Verify
- The STM32 checks: `rho^r * gamma == xi`.
- If equal → honest server.
- Otherwise → malicious server (forged response).

---

## 6. Batch sizes

| Batch | N  | Description       |
|-------|----|-------------------|
| 0     | 1  | sanity check      |
| 1     | 10 | small benchmark   |
| 2     | 50 | main benchmark    |
| sec   | 1  | malicious round   |

**Total rounds:** 61 honest + 1 malicious = 62.

---

## 7. Project layout

```
${PROJECT_DIR}/
├── src/
│   ├── main.c                — entry point: clock config, LED, RunBenchmark
│   ├── amore.c               — AmorE logic: Setup, Verify, RunBenchmark
│   ├── amore_uart.c          — UART layer: send/recv packets, CRC
│   ├── fp.c / fq.c           — Fp, Fq field arithmetic
│   ├── fp2.c / fp12.c        — Fp2, Fp12 field-extension arithmetic
│   ├── g1.c / g2.c           — group operations on G1, G2
│   └── startup_stm32f407xx.c — ISR vector table, reset handler
├── inc/
│   ├── amore.h               — main definitions, AmorE_BenchResults struct
│   ├── amore_uart.h          — UART_TIMEOUT_MS, CMD constants
│   └── bn128_const.h         — BN254 constants (q, GT constants)
├── rpi/server.py             — Python server running on the RPi
├── scripts/run_benchmark.sh  — master driver script
├── CMakeLists.txt            — build system
├── STM32F407VGTX_FLASH.ld    — linker script
└── logs/                     — build logs, server logs, STM32 reports
```

---

## 8. Important timeouts

| Timeout            | Value  | Location           | Description                           |
|--------------------|--------|--------------------|---------------------------------------|
| UART_TIMEOUT_MS    | 120000 | amore_uart.h       | wait for CMD_RESULT from RPi          |
| server status      | 60s    | server.py          | wait for CMD_STATUS from STM32        |
| server ready       | 30s    | amore.c            | wait for CMD_READY at startup         |
| server idle        | 120s   | server.py          | auto-exit if no packets are received  |

---

## 9. Status codes (`g_results.status`)

| Value        | Meaning                                  |
|--------------|------------------------------------------|
| 0x600D0000   | ✓ all rounds passed — PASS               |
| 0xBAD00000   | ✗ general failure                        |
| 0xDEAD0001   | timeout on CMD_READY                     |
| 0xDEAD0010x  | UART send error (batch x)                |
| 0xDEAD0020x  | timeout on CMD_RESULT                    |
| 0xDEAD0030x  | unexpected CMD received                  |
| 0xDEAD0040x  | invalid packet length                    |
| 0xDEAD0050x  | honest round failed verification         |

---

## 10. Useful commands

```bash
# Discover the RPi IP via mDNS
export RPI_HOST=$(getent hosts raspberrypi.local | awk '{print $1}')
export RPI_USER=pi

# Run the full pipeline (the script discovers the IP itself if RPI_HOST is unset)
./scripts/run_benchmark.sh

# Quick RPi sanity check
ssh ${RPI_USER}@${RPI_HOST} "python3 -c 'from py_ecc.bn128 import pairing,G1,G2; print(\"OK\")'"

# Inspect the current UART timeout
grep UART_TIMEOUT ${PROJECT_DIR}/inc/amore_uart.h

# List the most recent logs
ls -lt logs/ | head -10

# Run the server manually on the RPi for debugging
ssh ${RPI_USER}@${RPI_HOST} "python3 ${PROJECT_DIR}/rpi/server.py --port /dev/ttyAMA0 --baud 921600"

# Copy server.py from the RPi to the laptop
scp ${RPI_USER}@${RPI_HOST}:${PROJECT_DIR}/rpi/server.py ./rpi/server.py
```
