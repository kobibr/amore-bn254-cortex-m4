#!/usr/bin/env python3
"""
AmorE BLS12-381 — Python server with full telemetry
================================================
Runs on Raspberry Pi 3B.  Communicates with STM32 via /dev/ttyAMA0 (GPIO 14/15).

Packet format (matches amore_uart.h):
  [0xAA][0x55][CMD:1][LEN_LO:1][LEN_HI:1][DATA:LEN][CRC8:1]

Commands:
  CMD_SETUP  = 0x10   client→server   DATA = A(96) B(192) C(96) D(192) = 576 B
  CMD_RESULT = 0x20   server→client   DATA = gamma(576) rho(576)        = 1152 B
  CMD_STATUS = 0x30   client→server   DATA = 1 B  (0=fail, 1=ok)
  CMD_READY  = 0x40   server→client   DATA = 1 B  (0=honest, 1=malicious)

Usage:
  python3 server.py [--port /dev/ttyACM0] [--baud 921600]
                    [--honest-rounds 61] [--log-dir .]
"""

import argparse, json, os, random, sys, time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import Optional
import serial

# ── py_ecc imports ──────────────────────────────────────────────────────────
from py_ecc.bls12_381 import (
    field_modulus as p, curve_order as q,
    G1, G2, pairing, multiply, neg, add,
    FQ12,
)


# ============================================================================
#  Terminal colours
# ============================================================================
class C:
    RST  = "\033[0m"
    BOLD = "\033[1m"
    RED  = "\033[91m"
    GRN  = "\033[92m"
    YLW  = "\033[93m"
    BLU  = "\033[94m"
    MAG  = "\033[95m"
    CYN  = "\033[96m"
    GRY  = "\033[90m"

def ok(s):   return f"{C.GRN}✓ {s}{C.RST}"
def err(s):  return f"{C.RED}✗ {s}{C.RST}"
def warn(s): return f"{C.YLW}⚠ {s}{C.RST}"
def info(s): return f"{C.CYN}  {s}{C.RST}"
def head(s): return f"{C.BOLD}{C.BLU}{s}{C.RST}"
def ts():    return f"{C.GRY}[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}]{C.RST}"

def log(msg): print(f"{ts()} {msg}", flush=True)


# ============================================================================
#  Telemetry data structures
# ============================================================================
@dataclass
class RoundTelemetry:
    round_num:      int
    mode:           str           # "honest" | "malicious"
    t_recv_setup:   float = 0.0  # unix timestamp received setup pkt
    t_sent_result:  float = 0.0  # unix timestamp sent result pkt
    t_recv_status:  float = 0.0  # unix timestamp received status pkt
    compute_ms:     float = 0.0  # server pairing computation time
    client_ok:      Optional[bool] = None
    server_mode:    str   = "honest"
    error:          Optional[str] = None
    pkt_send_ok:    bool  = True
    status_timeout: bool  = False

@dataclass
class ServerTelemetry:
    start_time:     str   = ""
    end_time:       str   = ""
    port:           str   = ""
    baud:           int   = 0
    honest_rounds:  int   = 0
    total_rounds:   int   = 0
    honest_ok:      int   = 0
    honest_fail:    int   = 0
    malicious_caught: int = 0
    malicious_missed: int = 0
    crc_errors:     int   = 0
    timeout_errors: int   = 0
    bad_cmd_errors: int   = 0
    bad_len_errors: int   = 0
    avg_compute_ms: float = 0.0
    rounds:         list  = field(default_factory=list)


# ============================================================================
#  Packet protocol
# ============================================================================
CMD_SETUP  = 0x10
CMD_RESULT = 0x20
CMD_STATUS = 0x30
CMD_READY  = 0x40

def _crc8(cmd: int, data: bytes) -> int:
    crc = cmd ^ (len(data) & 0xFF) ^ ((len(data) >> 8) & 0xFF)
    for b in data:
        crc ^= b
    return crc & 0xFF

def send_packet(port: serial.Serial, cmd: int, data: bytes) -> bool:
    ln  = len(data)
    hdr = bytes([0xAA, 0x55, cmd, ln & 0xFF, (ln >> 8) & 0xFF])
    crc = _crc8(cmd, data)
    try:
        port.write(hdr + data + bytes([crc]))
        port.flush()
        return True
    except serial.SerialException as e:
        log(err(f"UART write error: {e}"))
        return False

def recv_packet(port: serial.Serial, timeout_s: float = 30.0) -> tuple[int, bytes]:
    """Returns (cmd, data). Raises TimeoutError or ValueError on problems."""
    deadline = time.monotonic() + timeout_s
    # Hunt for 0xAA 0x55 sync
    state = 0
    while time.monotonic() < deadline:
        b = port.read(1)
        if not b:
            continue
        if state == 0 and b[0] == 0xAA:
            state = 1
        elif state == 1 and b[0] == 0x55:
            break
        elif state == 1 and b[0] == 0xAA:
            # Stay in state=1 — this 0xAA could be the real start
            pass
        elif b[0] == 0xAA:
            # Recovery: start tracking from this 0xAA
            state = 1
        else:
            state = 0
    else:
        raise TimeoutError("Sync timeout")

    hdr = port.read(3)
    if len(hdr) < 3:
        raise TimeoutError("Header timeout")
    cmd, len_lo, len_hi = hdr[0], hdr[1], hdr[2]
    plen = len_lo | (len_hi << 8)

    data = bytes(port.read(plen)) if plen else b''
    crc_recv_b = port.read(1)
    if not crc_recv_b:
        raise TimeoutError("CRC byte timeout")
    crc_recv = crc_recv_b[0]
    crc_calc = _crc8(cmd, data)
    if crc_recv != crc_calc:
        raise ValueError(f"CRC mismatch: rx=0x{crc_recv:02x} calc=0x{crc_calc:02x}")
    return cmd, data


# ============================================================================
#  BN128 serialisation
# ============================================================================
def _to_fp(b: bytes):
    from py_ecc.bls12_381 import FQ
    return FQ(int.from_bytes(b, 'big'))

def _to_fp2(b: bytes):
    from py_ecc.bls12_381 import FQ2
    return FQ2([int.from_bytes(b[0:48],'big'), int.from_bytes(b[48:96],'big')])

def bytes_to_g1(b: bytes):
    return (_to_fp(b[0:48]), _to_fp(b[48:96]))

def bytes_to_g2(b: bytes):
    return (_to_fp2(b[0:96]), _to_fp2(b[96:192]))

def _flat(x) -> list[int]:
    r = []
    if hasattr(x, 'coeffs'):
        for c in x.coeffs: r += _flat(c)
    else:
        r = [int(x)]
    return r

def fp12_to_bytes(elem) -> bytes:
    return b''.join(c.to_bytes(48, 'big') for c in _flat(elem))

def fp12_pow(x, e: int):
    r = FQ12.one(); b = x
    while e:
        if e & 1: r = r * b
        b = b * b; e >>= 1
    return r


# ============================================================================
#  Server computation
# ============================================================================
def compute_honest(A, B, C, D):
    """rho = e(A,B),  gamma = e(A,D) * e(C,Q)"""
    rho   = pairing(B, A)
    gamma = pairing(D, A) * pairing(G2, C)
    return gamma, rho

def compute_malicious(A, B, C, D):
    """Scale both outputs by random x — breaks verify with overwhelming prob."""
    gamma, rho = compute_honest(A, B, C, D)
    x = random.randint(2, q - 2)
    return fp12_pow(gamma, x), fp12_pow(rho, x)


# ============================================================================
#  Main server loop
# ============================================================================
def run_server(port_name: str, baud: int, honest_rounds: int, log_dir: str) -> ServerTelemetry:
    tel = ServerTelemetry(
        start_time  = datetime.now().isoformat(timespec='seconds'),
        port        = port_name,
        baud        = baud,
        honest_rounds = honest_rounds,
    )

    print()
    print(head("=" * 60))
    print(head("  AmorE BLS12-381 Server  —  telemetry build"))
    print(head("=" * 60))
    log(info(f"Port: {port_name}  Baud: {baud}"))
    log(info(f"Honest rounds before malicious switch: {honest_rounds}"))
    log(info(f"Log directory: {os.path.abspath(log_dir)}"))
    print()

    try:
        port = serial.Serial(port_name, baud, timeout=0.5)
    except serial.SerialException as e:
        log(err(f"Cannot open port: {e}"))
        sys.exit(1)

    time.sleep(0.3)

    # Send READY (honest mode indicator)
    log(info("Sending CMD_READY (mode=honest)"))
    send_packet(port, CMD_READY, bytes([0]))

    round_num      = 0
    compute_times  = []

    try:
        while True:
            print()
            log(head(f"── Round {round_num + 1} ── waiting for CMD_SETUP..."))

            try:
                cmd, data = recv_packet(port, timeout_s=120.0)
            except TimeoutError:
                log(warn("No more packets for 120s — assuming benchmark complete."))
                break
            except ValueError as e:
                tel.crc_errors += 1
                log(err(f"Packet error: {e}"))
                continue

            if cmd != CMD_SETUP:
                tel.bad_cmd_errors += 1
                log(warn(f"Unexpected CMD=0x{cmd:02x} (expected 0x10), skipping."))
                continue
            if len(data) != 576:
                tel.bad_len_errors += 1
                log(warn(f"Bad SETUP length {len(data)} (expected 576), skipping."))
                continue

            round_num += 1
            tel.total_rounds = round_num
            malicious = (round_num > honest_rounds)
            mode      = "malicious" if malicious else "honest"

            rt = RoundTelemetry(round_num=round_num, mode=mode)
            rt.t_recv_setup = time.time()

            # Deserialise pub = A(96) B(192) C(96) D(192)
            try:
                pt_A = bytes_to_g1(data[0:96])
                pt_B = bytes_to_g2(data[96:288])
                pt_C = bytes_to_g1(data[288:384])
                pt_D = bytes_to_g2(data[384:576])
            except Exception as e:
                rt.error = f"Deserialise failed: {e}"
                log(err(rt.error))
                tel.rounds.append(asdict(rt))
                continue

            mode_str = f"{C.MAG}MALICIOUS{C.RST}" if malicious else f"{C.GRN}honest{C.RST}"
            log(info(f"Mode: {mode_str}  |  Computing pairings..."))

            t_compute = time.monotonic()
            if malicious:
                gamma, rho = compute_malicious(pt_A, pt_B, pt_C, pt_D)
            else:
                gamma, rho = compute_honest(pt_A, pt_B, pt_C, pt_D)
            rt.compute_ms = (time.monotonic() - t_compute) * 1000.0
            compute_times.append(rt.compute_ms)
            log(info(f"Compute done in {rt.compute_ms:.1f} ms"))

            result_bytes = fp12_to_bytes(gamma) + fp12_to_bytes(rho)
            assert len(result_bytes) == 1152, "BUG: result must be 768 bytes"

            rt.pkt_send_ok = send_packet(port, CMD_RESULT, result_bytes)
            rt.t_sent_result = time.time()

            if not rt.pkt_send_ok:
                log(err("Failed to send CMD_RESULT"))
                tel.rounds.append(asdict(rt))
                continue
            log(info(f"CMD_RESULT sent ({len(result_bytes)} bytes)"))

            # Wait for client STATUS
            try:
                scmd, sdata = recv_packet(port, timeout_s=60.0)
                rt.t_recv_status = time.time()
                if scmd == CMD_STATUS and len(sdata) == 1:
                    client_ok     = (sdata[0] == 1)
                    rt.client_ok  = client_ok
                    rtt_ms = (rt.t_recv_status - rt.t_sent_result) * 1000.0

                    if malicious:
                        if not client_ok:
                            tel.malicious_caught += 1
                            log(ok(f"Malicious round {round_num}: CORRECTLY REJECTED  "
                                   f"(rtt={rtt_ms:.0f} ms)"))
                        else:
                            tel.malicious_missed += 1
                            log(err(f"Malicious round {round_num}: CLIENT ACCEPTED — SECURITY BUG!"))
                    else:
                        if client_ok:
                            tel.honest_ok += 1
                            log(ok(f"Honest round {round_num}: accepted by client  "
                                   f"(compute={rt.compute_ms:.1f} ms  rtt={rtt_ms:.0f} ms)"))
                        else:
                            tel.honest_fail += 1
                            log(err(f"Honest round {round_num}: REJECTED by client — check STM code!"))
                else:
                    log(warn(f"Unexpected status CMD=0x{scmd:02x} len={len(sdata)}"))
            except TimeoutError:
                rt.status_timeout = True
                tel.timeout_errors += 1
                log(warn(f"Round {round_num}: no STATUS within 60s"))

            tel.rounds.append(asdict(rt))

    except KeyboardInterrupt:
        log(warn("Interrupted by user."))
    finally:
        port.close()

    # Finalise telemetry
    tel.end_time    = datetime.now().isoformat(timespec='seconds')
    tel.avg_compute_ms = (sum(compute_times) / len(compute_times)) if compute_times else 0.0

    # ── Print final report ────────────────────────────────────────────────
    _print_server_report(tel)

    # ── Save JSON log ─────────────────────────────────────────────────────
    os.makedirs(log_dir, exist_ok=True)
    stamp    = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = os.path.join(log_dir, f"amore_server_{stamp}.json")
    with open(log_path, "w") as f:
        json.dump(asdict(tel), f, indent=2)
    log(info(f"JSON log saved → {log_path}"))

    return tel


def _print_server_report(tel: ServerTelemetry):
    print()
    print(head("=" * 60))
    print(head("  SERVER REPORT"))
    print(head("=" * 60))
    print(f"  Start : {tel.start_time}")
    print(f"  End   : {tel.end_time}")
    print(f"  Port  : {tel.port} @ {tel.baud} baud")
    print()
    print(head("  Rounds"))
    print(f"  Total sent by client : {tel.total_rounds}")

    honest_total  = tel.honest_ok + tel.honest_fail
    mal_total     = tel.malicious_caught + tel.malicious_missed

    stat = ok if tel.honest_fail == 0 else err
    print(f"  Honest : {stat(f'{tel.honest_ok}/{honest_total} accepted')}")

    if mal_total > 0:
        stat = ok if tel.malicious_missed == 0 else err
        print(f"  Malicious : {stat(f'{tel.malicious_caught}/{mal_total} caught')}")
    else:
        print(f"  Malicious : {warn('no malicious round received')}")

    print()
    print(head("  Errors"))
    print(f"  CRC errors    : {tel.crc_errors}")
    print(f"  Timeout errors: {tel.timeout_errors}")
    print(f"  Bad CMD       : {tel.bad_cmd_errors}")
    print(f"  Bad length    : {tel.bad_len_errors}")

    print()
    print(head("  Performance"))
    print(f"  Avg server pairing compute : {tel.avg_compute_ms:.1f} ms / round")

    # Per-round summary table
    print()
    print(head("  Per-round summary"))
    print(f"  {'#':>3}  {'Mode':12}  {'Compute':>10}  {'Client':>8}  {'Status':12}")
    print(f"  {'─'*3}  {'─'*12}  {'─'*10}  {'─'*8}  {'─'*12}")
    for r in tel.rounds:
        mode   = r['mode']
        cok    = r.get('client_ok')
        cstr   = ("OK" if cok else "FAIL") if cok is not None else "no-status"
        colour = C.GRN if cok else (C.RED if cok is False else C.YLW)
        err_   = r.get('error') or ""
        print(f"  {r['round_num']:>3}  {mode:12}  {r['compute_ms']:>9.1f}ms"
              f"  {colour}{cstr:>8}{C.RST}  {err_[:12]}")

    print()
    overall_ok = (tel.honest_fail == 0 and
                  tel.malicious_caught > 0 and
                  tel.malicious_missed == 0 and
                  tel.crc_errors == 0 and
                  tel.timeout_errors == 0)
    if overall_ok:
        print(head(ok("ALL CHECKS PASSED")))
    else:
        print(head(err("ISSUES DETECTED — see details above")))
    print(head("=" * 60))
    print()


# ============================================================================
#  Entry point
# ============================================================================
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='AmorE BLS12-381 server with telemetry')
    parser.add_argument('--port',          default='/dev/ttyACM0')
    parser.add_argument('--baud',          type=int, default=921600)
    parser.add_argument('--honest-rounds', type=int, default=61,
        help='Switch to malicious after this many honest rounds (default 61 = 1+10+50)')
    parser.add_argument('--log-dir',       default='.',
        help='Directory for JSON log files')
    args = parser.parse_args()
    run_server(args.port, args.baud, args.honest_rounds, args.log_dir)
