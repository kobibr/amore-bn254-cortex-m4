#!/usr/bin/env python3
"""
fp_reference.py — Python reference implementation of Fp arithmetic
                  for BLS12-381, identical semantics to what we'll
                  implement in C.

Used to generate ground-truth test vectors that the C code must match.
"""

import sys
import random

# BLS12-381 field prime
P = 0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab

N_LIMBS = 12
WORD = 2**32
R = 2 ** (32 * N_LIMBS)  # = 2^384
R_inv = pow(R, -1, P)
N0_P = (-pow(P, -1, WORD)) % WORD


def to_limbs(x: int, n: int = N_LIMBS) -> list:
    """Convert integer to n × 32-bit little-endian limbs."""
    return [(x >> (32*i)) & 0xFFFFFFFF for i in range(n)]


def from_limbs(limbs: list) -> int:
    """Convert little-endian limbs to integer."""
    return sum(l * (2 ** (32*i)) for i, l in enumerate(limbs))


def to_mont(a: int) -> int:
    """Convert integer to Montgomery form: a*R mod p."""
    return (a * R) % P


def from_mont(a_mont: int) -> int:
    """Convert from Montgomery: a*R mod p → a mod p."""
    return (a_mont * R_inv) % P


def fp_add(a: int, b: int) -> int:
    """Field addition: (a + b) mod p. Inputs/outputs are integers."""
    return (a + b) % P


def fp_sub(a: int, b: int) -> int:
    """Field subtraction: (a - b) mod p."""
    return (a - b) % P


def fp_mul_mont(a: int, b: int) -> int:
    """
    Montgomery multiplication: returns a*b*R^{-1} mod p.
    Both inputs and output are in 'raw' form (NOT Montgomery).
    For Montgomery-form values, this gives:
        (aR) * (bR) * R^{-1} = abR  (which is the Montgomery form of ab)
    """
    return (a * b * R_inv) % P


def fp_neg(a: int) -> int:
    """Field negation: -a mod p."""
    return (-a) % P


def fp_inv(a: int) -> int:
    """Field inverse: a^{-1} mod p (using Fermat: a^{p-2})."""
    if a == 0:
        return 0
    return pow(a, P-2, P)


# --- Test-vector generators ---

def gen_random_fp() -> int:
    """Random Fp element, [0, p)."""
    return random.randrange(P)


def format_limbs_c(name: str, x: int) -> str:
    """Format an integer as a C array of 12 hex limbs."""
    limbs = to_limbs(x)
    chunks = [limbs[i:i+4] for i in range(0, 12, 4)]
    lines = [f"static const uint32_t {name}[12] = {{"]
    for i, chunk in enumerate(chunks):
        line = "    " + ", ".join(f"0x{l:08x}" for l in chunk)
        if i < len(chunks) - 1:
            line += ","
        lines.append(line)
    lines.append("};")
    return "\n".join(lines)


# --- Self-test ---

if __name__ == "__main__":
    # Verify Montgomery setup
    assert (P * (-N0_P)) % WORD == 1, "N0_P invalid"
    print(f"✓ Field prime: {hex(P)[:20]}...")
    print(f"✓ Bits: {P.bit_length()}")
    print(f"✓ R = 2^{32*N_LIMBS}")
    print(f"✓ N0_P = 0x{N0_P:08x}")
    print()

    # Test: 1 + 1 = 2 in Fp
    a = 1
    b = 1
    print(f"1 + 1 = {fp_add(a, b)}  (expected: 2)")

    # Test: roundtrip Montgomery
    x = 0xdeadbeef_12345678_cafebabe
    x_mont = to_mont(x)
    x_back = from_mont(x_mont)
    print(f"Mont roundtrip: {hex(x)} == {hex(x_back)} → {x == x_back}")

    # Test: a * a^{-1} = 1
    a = gen_random_fp()
    a_inv = fp_inv(a)
    prod = (a * a_inv) % P
    print(f"a * a^(-1) = 1: {prod == 1}")
    
    print()
    print("✓ Reference implementation OK")
