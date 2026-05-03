# Installation Guide

This document describes how to set up the full environment to build,
flash, and run the AmorE BN254 Cortex-M4 reference implementation.

## Overview

The project requires three machines:

1. **Development laptop** (Linux): builds the firmware, flashes the
   STM32 over ST-Link, and reads telemetry over GDB.
2. **Target board** (STM32F407 Discovery): runs the AmorE client.
3. **Remote helper** (Raspberry Pi 3B): runs the Python server that
   computes pairings.

The laptop and the RPi communicate over LAN. The STM32 is connected to
the laptop via USB (ST-Link) and to the RPi via UART.

---

## 1. Laptop setup (Ubuntu/Debian)

### 1.1 Install the ARM toolchain and debug tools

```bash
sudo apt update
sudo apt install -y \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    gdb-multiarch \
    openocd \
    stlink-tools \
    cmake \
    make \
    git \
    python3-pip
```

`gdb-multiarch` is the package that ships the actual ARM-capable GDB on
recent Ubuntu/Debian releases (`arm-none-eabi-gdb` is no longer packaged
separately). Some scripts invoke it under the canonical name; you can
expose it with a symlink:

```bash
sudo ln -sf "$(which gdb-multiarch)" /usr/local/bin/arm-none-eabi-gdb
```

### 1.2 Verify the toolchain

```bash
arm-none-eabi-gcc --version    # should report 11.x or later
gdb-multiarch --version
openocd --version
st-info --version
```

A diagnostic helper that walks all of the above checks at once is
provided:

```bash
bash scripts/diagnose_gdb.sh
```

### 1.3 udev rules for ST-Link (one-time)

So that you do not have to run `st-flash` as root, install the
ST-Link udev rules:

```bash
sudo curl -L -o /etc/udev/rules.d/49-stlinkv2-1.rules \
    https://raw.githubusercontent.com/stlink-org/stlink/master/config/udev/rules.d/49-stlinkv2-1.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

You may need to log out and back in, or unplug and replug the board.

### 1.4 Clone this repository

```bash
git clone https://github.com/kobibr/amore-bn254-cortex-m4.git
cd amore-bn254-cortex-m4
```

---

## 2. STM32CubeF4 (HAL) — required dependency

The build system uses the STM32CubeF4 HAL to drive USART2, the system
clock configuration, and the on-board peripherals. This is a separate
package from STMicroelectronics and is **not** redistributed in this
repository.

### 2.1 Download

Get **STM32CubeF4 v1.27.0** (or any compatible 1.27.x release) from
either:

- ST's official page: <https://www.st.com/en/embedded-software/stm32cubef4.html>
- The mirror on GitHub: <https://github.com/STMicroelectronics/STM32CubeF4>

### 2.2 Place it next to the project

The `CMakeLists.txt` looks for `STM32CubeF4/` in the project root by
default. You can either place it there directly:

```bash
# From inside amore-bn254-cortex-m4/
git clone --depth 1 https://github.com/STMicroelectronics/STM32CubeF4.git
```

…or keep it elsewhere and override the path at configure time:

```bash
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake \
    -DCUBE_ROOT=/path/to/STM32CubeF4
```

The HAL itself is large (~1 GB). It is intentionally excluded from this
repository via `.gitignore`.

---

## 3. RELIC — required for `relic_bench.elf` and `regression_test.elf`

The AmorE client itself does not depend on RELIC. However, the two
companion targets — the local pairing benchmark and the regression
test — do require RELIC built for the Cortex-M4 with the
`arm-asm-254` backend.

### 3.1 Build RELIC for the target

A helper script is provided. It clones the patched RELIC fork (see
the companion repository
[`kobibr/relic-arm-m4-fix`](https://github.com/kobibr/relic-arm-m4-fix))
and builds a static library with the hand-tuned BN254 assembly enabled:

```bash
bash scripts/build-relic.sh
```

When it finishes, you should have:

```
relic_install/lib/librelic_s.a
relic_install/include/relic/relic*.h
```

### 3.2 Re-run CMake to detect RELIC

After the RELIC install directory exists, re-run CMake configuration so
that the `relic_bench.elf` and `regression_test.elf` targets are
enabled:

```bash
rm -rf build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake
```

You should see a line in the configure output like:

```
-- RELIC found at /.../relic_install — relic_bench.elf target enabled
-- regression_test.elf target enabled
```

If RELIC is not present, those two targets are silently skipped and only
the AmorE client (`amorebn128.elf`) will be built.

---

## 4. Building

### 4.1 Configure

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-stm32f4.cmake
```

### 4.2 Build all targets

```bash
cmake --build build
```

You should obtain:

| ELF                    | Size (bin) | Description                           |
|------------------------|-----------:|---------------------------------------|
| `amorebn128.elf`       |     ~15 KB | AmorE client (the main contribution). |
| `relic_bench.elf`      |     ~62 KB | Local pairing benchmark via RELIC.    |
| `regression_test.elf`  |     ~58 KB | Regression test for `arm-asm-254`.    |

### 4.3 Flash

Connect the STM32F407 Discovery to the laptop with a USB Mini-B cable
(the ST-Link side, not the User USB).

```bash
# Flash the AmorE client
cmake --build build --target flash

# (optional) flash the RELIC benchmark
cmake --build build --target flash_relic

# (optional) flash the regression test
cmake --build build --target flash_regression
```

Each `flash` target runs `st-flash` under the hood; the firmware boots
from `0x08000000` immediately after flashing.

---

## 5. Raspberry Pi setup (server)

The Python server (`rpi/server.py`) runs on the Raspberry Pi and answers
pairing requests sent by the STM32 over UART.

### 5.1 OS and Python

A standard Raspberry Pi OS image (Bullseye or later) works. Make sure
Python 3 is installed (it is by default).

### 5.2 Install the dependencies

```bash
pip3 install py_ecc==8.0.0 pyserial
```

`py_ecc` is the Python BN128 / alt_bn128 reference library used as the
pairing oracle for the AmorE protocol.

### 5.3 Enable the hardware UART (`/dev/ttyAMA0`)

By default, Raspberry Pi OS uses the primary UART for the serial
console. The server needs full control of the UART, so disable the
console first:

```bash
sudo raspi-config
#   -> 3 Interface Options
#   -> I6 Serial Port
#       Login shell over serial?      No
#       Hardware serial port enabled? Yes
sudo reboot
```

Verify the device is now available:

```bash
ls -l /dev/ttyAMA0
```

### 5.4 Wire the UART

Connect the STM32 to the Raspberry Pi as follows:

| STM32 pin | Direction | RPi pin (GPIO) | Description       |
|-----------|-----------|----------------|-------------------|
| PA2 (TX)  | →         | GPIO 15 (RX)   | STM32 → RPi data  |
| PA3 (RX)  | ←         | GPIO 14 (TX)   | RPi → STM32 data  |
| GND       | ↔         | GND            | common ground     |

The RPi GPIO pinout is the standard 40-pin header. Use the *physical*
pins 8 (TXD) and 10 (RXD), or equivalently GPIO 14 and 15.

> **Voltage levels.** Both the STM32F4 and the RPi GPIO operate at 3.3 V,
> so they can be connected directly. **Do not** use a 5 V USB-to-serial
> adaptor on the RPi side.

### 5.5 Place `server.py` on the RPi

The driver script (`scripts/run_benchmark.sh`) will SSH into the RPi
and invoke `server.py` from a known path. The default location is
`$HOME/amore-bn254-cortex-m4/rpi/server.py` on the RPi user.

You can override the path if you keep the project elsewhere on the
Pi:

```bash
RPI_SERVER_PATH=/some/other/dir bash scripts/run_benchmark.sh
```

### 5.6 SSH access

Ensure that the laptop can SSH into the RPi without a password
(public-key authentication). The standard recipe:

```bash
ssh-copy-id pi@raspberrypi.local
```

The driver script discovers the RPi automatically via mDNS
(`raspberrypi.local`) with an ARP scan as a fallback. To override
manually:

```bash
RPI_HOST=192.168.1.42 RPI_USER=pi bash scripts/run_benchmark.sh
```

---

## 6. Running an end-to-end benchmark

Once everything is in place:

```bash
cd amore-bn254-cortex-m4
bash scripts/run_benchmark.sh
```

What this does, in order:

1. Builds the firmware (re-using the existing `build/` directory).
2. Flashes `amorebn128.bin` to the STM32 via `st-flash`.
3. Discovers the RPi on the LAN (mDNS, then ARP scan).
4. Starts `server.py` on the RPi over SSH.
5. Waits for the protocol run to complete (~62 rounds).
6. Connects to the STM32 over OpenOCD + GDB and dumps the
   `g_results` telemetry struct from SRAM.
7. Saves the combined report under `logs/`.

A successful run ends with `STATUS = 0x600D0000`. See
`doc/SYSTEM_DOC.md` for the full list of status codes.

---

## 7. Troubleshooting

### `arm-none-eabi-gdb: command not found`

Most modern distributions ship the multiarch GDB instead. Install
`gdb-multiarch` and either invoke it directly or symlink it under the
canonical name:

```bash
sudo apt install gdb-multiarch
sudo ln -sf "$(which gdb-multiarch)" /usr/local/bin/arm-none-eabi-gdb
```

### `libusb_open() returned 'LIBUSB_ERROR_ACCESS'`

The udev rules for ST-Link are missing or stale. Re-run the steps in
section 1.3, then unplug and replug the board.

### CMake says `RELIC not yet built — relic_bench.elf and regression_test.elf SKIPPED`

This is informational, not an error. It means RELIC has not been built
yet. If you only want to use the AmorE client, this can be ignored.
Otherwise run `bash scripts/build-relic.sh` and re-run CMake.

### `st-flash: probe failed`

Check the USB cable (the Mini-B near the ST-Link, not the User USB on
the other side of the board). If it still fails, run `lsusb` and
confirm an entry like `STMicroelectronics ST-LINK/V2`.

### The benchmark hangs on `Waiting for CMD_READY`

Either the UART wiring is wrong (TX/RX swapped, missing GND) or the
RPi server has not started. Verify by running the server manually:

```bash
ssh pi@raspberrypi.local "python3 ~/amore-bn254-cortex-m4/rpi/server.py --port /dev/ttyAMA0 --baud 921600"
```

You should see `Server ready, waiting for STM32...` on stdout.

### Diagnostic helper

For a guided walk-through of all of the above:

```bash
bash scripts/diagnose_gdb.sh
```

It checks the toolchain, verifies the OpenOCD configuration, probes
the ST-Link, attempts a connection, and reports exactly which stage is
failing.

---

## 8. Versions tested

This combination is known to work:

| Component           | Version              |
|---------------------|----------------------|
| Ubuntu              | 24.04 LTS            |
| `gcc-arm-none-eabi` | 13.2.1               |
| `gdb-multiarch`     | 13.2                 |
| `openocd`           | 0.12.0               |
| `stlink-tools`      | 1.7.0                |
| `cmake`             | 3.28                 |
| STM32CubeF4         | 1.27.0               |
| RELIC (fork)        | `arm-m4-toolchain-fix` branch of `kobibr/relic` |
| Raspberry Pi OS     | Bookworm (12)        |
| `py_ecc`            | 8.0.0                |

Other combinations are likely to work but have not been verified.
