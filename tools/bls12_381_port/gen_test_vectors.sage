#!/usr/bin/env sage
# =============================================================================
#  gen_test_vectors.sage — generate Fp test vectors for the BLS12-381 port
#
#  Outputs: tools/bls12_381_port/fp_test_vectors.txt
#
#  Format (one test per line):
#    OP a_hex b_hex expected_hex
#  
#  OPs:
#    ADD  a b expected   →  a + b mod p
#    SUB  a b expected   →  a - b mod p
#    MUL  a b expected   →  a * b mod p (NOT Montgomery — direct multiplication)
#    SQR  a 0 expected   →  a^2 mod p
#    INV  a 0 expected   →  a^(-1) mod p (Fermat: a^(p-2))
# =============================================================================

import os
import random

# BLS12-381 prime
p = 0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaab

# Deterministic random for reproducibility
random.seed(42)

def hex_for_c(x, n_limbs=12):
    """Output as 12-limb hex compatible with our C representation."""
    return f"0x{x:096x}"

def random_fp():
    """Random element in [0, p)."""
    return random.randint(0, p-1)

# Generate test vectors
tests = []

# --- ADD tests ---
# Edge cases first, then random
edge_cases_add = [
    (0, 0),
    (0, 1),
    (1, 0),
    (p-1, 1),       # wraps to 0
    (p-1, p-1),     # wraps to p-2
    (p//2, p//2),   # half + half
    (p-1, 2),       # wraps
]
for a, b in edge_cases_add:
    tests.append(("ADD", a, b, (a + b) % p))

for _ in range(50):
    a = random_fp()
    b = random_fp()
    tests.append(("ADD", a, b, (a + b) % p))

# --- SUB tests ---
edge_cases_sub = [
    (0, 0),
    (0, 1),         # wraps to p-1
    (1, 0),
    (p-1, p-1),
    (5, 10),        # wraps to p-5
    (p-1, 0),
    (0, p-1),       # wraps to 1
]
for a, b in edge_cases_sub:
    tests.append(("SUB", a, b, (a - b) % p))

for _ in range(50):
    a = random_fp()
    b = random_fp()
    tests.append(("SUB", a, b, (a - b) % p))

# --- MUL tests ---
edge_cases_mul = [
    (0, 0),
    (0, 1),
    (1, 1),
    (1, p-1),
    (p-1, p-1),
    (2, p-1),
    (p-1, 2),
]
for a, b in edge_cases_mul:
    tests.append(("MUL", a, b, (a * b) % p))

for _ in range(50):
    a = random_fp()
    b = random_fp()
    tests.append(("MUL", a, b, (a * b) % p))

# --- SQR tests (a*a) ---
edge_cases_sqr = [0, 1, 2, p-1, p//2]
for a in edge_cases_sqr:
    tests.append(("SQR", a, 0, (a * a) % p))

for _ in range(30):
    a = random_fp()
    tests.append(("SQR", a, 0, (a * a) % p))

# --- INV tests (Fermat: a^(p-2)) ---
# Note: cannot invert 0
edge_cases_inv = [1, 2, p-1, p//2]
for a in edge_cases_inv:
    a_inv = pow(a, p-2, p)
    tests.append(("INV", a, 0, a_inv))
    # Sanity
    assert (a * a_inv) % p == 1, f"INV verification failed for {a}"

for _ in range(20):
    a = random_fp()
    if a == 0:
        continue
    a_inv = pow(a, p-2, p)
    tests.append(("INV", a, 0, a_inv))
    assert (a * a_inv) % p == 1

# Write to file
out_path = "tools/bls12_381_port/fp_test_vectors.txt"
with open(out_path, "w") as f:
    f.write(f"# Test vectors for BLS12-381 Fp arithmetic\n")
    f.write(f"# Format: OP a_hex b_hex expected_hex\n")
    f.write(f"# All values are PLAIN (not Montgomery)\n")
    f.write(f"# Total tests: {len(tests)}\n")
    f.write(f"\n")
    for op, a, b, expected in tests:
        f.write(f"{op} {hex_for_c(a)} {hex_for_c(b)} {hex_for_c(expected)}\n")

print(f"✓ Generated {len(tests)} test vectors")
print(f"✓ Wrote {out_path}")

# Stats
op_counts = {}
for op, _, _, _ in tests:
    op_counts[op] = op_counts.get(op, 0) + 1
for op, count in sorted(op_counts.items()):
    print(f"  {op}: {count} tests")
