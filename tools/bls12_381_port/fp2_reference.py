#!/usr/bin/env python3
"""
fp2_reference.py — Python reference for Fp² arithmetic in BLS12-381.

Fp² = Fp[u] / (u² + 1)
Element a = a0 + a1·u, where a0, a1 ∈ Fp.
"""

import os
import sys
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from fp_reference import P, to_mont, from_mont, fp_inv

# Fp² element as a tuple (c0, c1) of plain Fp ints (NOT Montgomery)

def fp2_add(a, b):
    return ((a[0] + b[0]) % P, (a[1] + b[1]) % P)

def fp2_sub(a, b):
    return ((a[0] - b[0]) % P, (a[1] - b[1]) % P)

def fp2_neg(a):
    return ((-a[0]) % P, (-a[1]) % P)

def fp2_mul(a, b):
    """Karatsuba: (a0+a1u)(b0+b1u) = (a0b0 - a1b1) + (a0b1 + a1b0)u"""
    v0 = (a[0] * b[0]) % P
    v1 = (a[1] * b[1]) % P
    cross = ((a[0] + a[1]) * (b[0] + b[1]) - v0 - v1) % P
    c0 = (v0 - v1) % P
    return (c0, cross)

def fp2_sqr(a):
    """(a0+a1u)² = (a0²-a1²) + 2·a0·a1·u"""
    return fp2_mul(a, a)  # equivalent, simpler reference

def fp2_inv(a):
    """1/(a0+a1u) = (a0-a1u)/(a0² + a1²)"""
    if a == (0, 0):
        return (0, 0)
    norm = (a[0]*a[0] + a[1]*a[1]) % P
    norm_inv = fp_inv(norm)
    return ((a[0] * norm_inv) % P, ((-a[1]) * norm_inv) % P)

def fp2_mul_xi(a):
    """Multiply by xi = 1 + u: (a0-a1) + (a0+a1)u"""
    return ((a[0] - a[1]) % P, (a[0] + a[1]) % P)


def to_mont_fp2(a):
    """Convert Fp² element to Montgomery form (each coord)."""
    return (to_mont(a[0]), to_mont(a[1]))


def from_mont_fp2(a):
    return (from_mont(a[0]), from_mont(a[1]))


def format_fp2_c(name, a_mont):
    """Format Fp² element as C struct initializer (each coord 12 limbs)."""
    from fp_reference import to_limbs
    c0_limbs = to_limbs(a_mont[0])
    c1_limbs = to_limbs(a_mont[1])
    
    # Two flat uint32_t arrays for each coord
    lines = []
    
    # c0 array
    lines.append(f"static const uint32_t {name}_c0[12] = {{")
    for i in range(0, 12, 4):
        chunk = c0_limbs[i:i+4]
        line = "    " + ", ".join(f"0x{l:08x}" for l in chunk)
        if i + 4 < 12:
            line += ","
        lines.append(line)
    lines.append("};")
    
    # c1 array
    lines.append(f"static const uint32_t {name}_c1[12] = {{")
    for i in range(0, 12, 4):
        chunk = c1_limbs[i:i+4]
        line = "    " + ", ".join(f"0x{l:08x}" for l in chunk)
        if i + 4 < 12:
            line += ","
        lines.append(line)
    lines.append("};")
    
    return "\n".join(lines)


if __name__ == "__main__":
    # Self-test
    a = (3, 5)
    b = (7, 2)
    
    s = fp2_add(a, b)
    print(f"({a}) + ({b}) = {s}")  # (10, 7)
    assert s == (10, 7)
    
    p_ = fp2_mul(a, b)
    # (3+5u)(7+2u) = 21 + 6u + 35u + 10u² = 21 + 41u - 10 = 11 + 41u
    print(f"({a}) * ({b}) = {p_}")
    assert p_ == (11, 41)
    
    sq = fp2_sqr(a)
    # (3+5u)² = 9 + 30u + 25u² = 9 + 30u - 25 = -16 + 30u  → (-16 mod P, 30)
    print(f"({a})² = ({hex(sq[0])[:30]}, {sq[1]})")
    assert sq[1] == 30
    assert sq[0] == (-16) % P
    
    inv_a = fp2_inv(a)
    prod = fp2_mul(a, inv_a)
    print(f"a * a^-1 = {prod}")
    assert prod == (1, 0)
    
    xi = fp2_mul_xi(a)
    # (3 + 5u)(1 + u) = (3-5) + (3+5)u = -2 + 8u → (P-2, 8)
    print(f"a * xi = ({hex(xi[0])[:30]}, {xi[1]})")
    assert xi[0] == (P - 2) % P
    assert xi[1] == 8
    
    print()
    print("✓ Fp² reference tests pass")
