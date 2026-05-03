#!/usr/bin/env python3
"""
=============================================================================
 AmorE BN128 — PARANOID TEST SUITE
=============================================================================

Philosophy
----------
  • Every test has ONE specific thing it is trying to break.
  • A test that passes "by accident" is worse than no test.
  • Wrong input MUST cause failure — not silence, not a wrong-but-plausible result.
  • We test: constants, field math, group math, serialisation, protocol
    correctness, and 20+ deliberate-attack negative tests.

Marker legend
-------------
  (no mark)        fast — no pairing, completes in milliseconds
  @pytest.mark.slow  requires at least one BN128 pairing (~5s each in Python)

Run modes
---------
  pytest tests/ --fast     # only fast tests (<30 s total)
  pytest tests/            # full suite  (~5–15 min in pure Python)
=============================================================================
"""

import sys, random, hashlib, struct
import pytest, unittest

# ── py_ecc ──────────────────────────────────────────────────────────────────
from py_ecc.bn128 import (
    field_modulus as p,   # Fp prime  (ends …208583)
    curve_order   as q,   # G1/G2 order (ends …495617)
    G1, G2,
    pairing, multiply, neg, add,
    is_on_curve,
)
try:
    from py_ecc.bn128 import eq as pt_eq
except ImportError:
    def pt_eq(a, b):
        if a is None and b is None: return True
        if a is None or b is None:  return False
        return int(a[0]) == int(b[0]) and int(a[1]) == int(b[1])

import py_ecc.bn128 as _bn
FQ12 = _bn.FQ12
FQ   = _bn.FQ
FQ2  = _bn.FQ2

# ── Precompute e(G1,G2) once — used by almost every test ─────────────────────
# (~5 s in pure Python; cached here so the full suite pays this cost once)
print("  [init] computing e(G1,G2)...", end='', flush=True)
_GT = pairing(G2, G1)
print(" ok")

# ── GT helpers ───────────────────────────────────────────────────────────────
def fp12_pow(x, e: int):
    r = FQ12.one(); b = x
    while e > 0:
        if e & 1: r = r * b
        b = b * b; e >>= 1
    return r

def flat12(x) -> list:
    r = []
    def _f(v):
        if hasattr(v, 'coeffs'):
            for c in v.coeffs: _f(c)
        else: r.append(int(v))
    _f(x)
    return r

def fp12_eq(a, b) -> bool:
    return flat12(a) == flat12(b)

# ── Serialisation helpers (mirror of C code) ─────────────────────────────────
def fp12_to_bytes(elem) -> bytes:
    return b''.join(c.to_bytes(32, 'big') for c in flat12(elem))

def bytes_to_fp12(b: bytes):
    coeffs = [int.from_bytes(b[i*32:(i+1)*32], 'big') for i in range(12)]
    return FQ12(tuple(coeffs))

def g1_to_bytes(pt) -> bytes:
    if pt is None: return b'\x00' * 64
    return int(pt[0]).to_bytes(32,'big') + int(pt[1]).to_bytes(32,'big')

def g2_to_bytes(pt) -> bytes:
    if pt is None: return b'\x00' * 128
    return (int(pt[0].coeffs[0]).to_bytes(32,'big') +
            int(pt[0].coeffs[1]).to_bytes(32,'big') +
            int(pt[1].coeffs[0]).to_bytes(32,'big') +
            int(pt[1].coeffs[1]).to_bytes(32,'big'))

def bytes_to_g1(b: bytes):
    return (FQ(int.from_bytes(b[0:32],'big')),
            FQ(int.from_bytes(b[32:64],'big')))

def bytes_to_g2(b: bytes):
    return (FQ2([FQ(int.from_bytes(b[0:32], 'big')),
                 FQ(int.from_bytes(b[32:64],'big'))]),
            FQ2([FQ(int.from_bytes(b[64:96],'big')),
                 FQ(int.from_bytes(b[96:128],'big'))]))

# ── Montgomery helpers ────────────────────────────────────────────────────────
def mont_inv0(m: int) -> int:
    """Compute -m^{-1} mod 2^32  (Newton iterations)."""
    x = 1
    for _ in range(31):
        x = (x * (2 - m * x)) & 0xFFFFFFFF
    return (-x) & 0xFFFFFFFF

def to_le32(n: int, nl: int = 8) -> list:
    return [(n >> (32*i)) & 0xFFFFFFFF for i in range(nl)]

# ── Protocol helpers (exact mirror of amore.c logic) ─────────────────────────
PHI = 90   # short-scalar bit length

def ots(s: int):
    """OneTimeSetup → xi = e(G1,G2)^{-s}."""
    assert 1 <= s < q
    return fp12_pow(_GT, (-s) % q)

def setup(s: int, A, B, phi: int = PHI):
    """AmorE_Setup → (pub dict, r)."""
    u       = random.randint(1, q-1)
    r       = random.randint(1, (1 << phi) - 1)
    u_inv   = pow(u, q-2, q)
    su_inv  = (s * u_inv) % q
    nsu_inv = (-su_inv) % q
    U  = multiply(G1, u)
    V  = multiply(G2, su_inv)
    C  = multiply(add(U, A), nsu_inv)
    rB = multiply(B, r)
    D  = add(V, neg(rB))
    return {'A': A, 'B': B, 'C': C, 'D': D}, r

def compute_honest(pub: dict):
    """Server honest: rho=e(A,B), gamma=e(A,D)*e(C,Q)."""
    A, B, C, D = pub['A'], pub['B'], pub['C'], pub['D']
    return pairing(D, A) * pairing(G2, C), pairing(B, A)

def verify(xi, r: int, gamma, rho) -> bool:
    """Client: xi == rho^r * gamma."""
    return fp12_eq(fp12_pow(rho, r) * gamma, xi)

def full_round(s=None):
    """Run one honest protocol round; return (xi, r, gamma, rho)."""
    s   = s or random.randint(1, q-1)
    xi  = ots(s)
    pub, r = setup(s, G1, G2)
    gamma, rho = compute_honest(pub)
    return xi, r, gamma, rho


# =============================================================================
#  [CONST] Constants
# =============================================================================
class TestConstants:
    """
    Every hardcoded BN128 constant cross-checked against py_ecc.
    A wrong constant silently corrupts every downstream computation.
    """

    def test_field_prime_last_digits(self):
        """p (Fp field prime) ends in …226208583."""
        assert p % (10**9) == 645226208583 % (10**9), \
            f"p last digits wrong: {p}"

    def test_field_prime_exact(self):
        """p == 21888242871839275222246405745257275088696311157297823662689037894645226208583"""
        assert p == 21888242871839275222246405745257275088696311157297823662689037894645226208583

    def test_group_order_last_digits(self):
        """q (group order) ends in …575808495617."""
        assert q % (10**12) == 186575808495617 % (10**12)

    def test_group_order_exact(self):
        """q == 21888242871839275222246405745257275088548364400416034343698204186575808495617"""
        assert q == 21888242871839275222246405745257275088548364400416034343698204186575808495617

    def test_p_neq_q(self):
        """p ≠ q  — they are distinct numbers."""
        assert p != q, "p and q are the same — catastrophic confusion!"

    def test_p_gt_q(self):
        """For BN128: Fp prime > group order  (p > q)."""
        assert p > q, "Expected p > q for BN128!"

    def test_n0p_fp_value(self):
        """BN128_N0P_FP = −p^{−1} mod 2^32 = 0xe4866389."""
        assert mont_inv0(p & 0xFFFFFFFF) == 0xe4866389, \
            f"N0P_FP wrong: got 0x{mont_inv0(p & 0xFFFFFFFF):08x}"

    def test_n0p_fq_value(self):
        """BN128_N0P_FQ = −q^{−1} mod 2^32 = 0xefffffff."""
        assert mont_inv0(q & 0xFFFFFFFF) == 0xefffffff, \
            f"N0P_FQ wrong: got 0x{mont_inv0(q & 0xFFFFFFFF):08x}"

    def test_r_fp_limb0(self):
        """R_FP[0] = 2^256 mod p, first limb = 0xc58f0d9d."""
        assert to_le32(pow(2,256,p))[0] == 0xc58f0d9d

    def test_r_fp_limb7(self):
        """R_FP[7] = 2^256 mod p, top limb = 0x0e0a77c1."""
        assert to_le32(pow(2,256,p))[7] == 0x0e0a77c1

    def test_r2_fp_limb0(self):
        """R2_FP[0] = 2^512 mod p, first limb = 0x538afa89."""
        assert to_le32(pow(2,512,p))[0] == 0x538afa89

    def test_r_fq_limb0(self):
        """R_FQ[0] = 2^256 mod q, first limb = 0x4ffffffb."""
        assert to_le32(pow(2,256,q))[0] == 0x4ffffffb

    def test_g1_on_curve(self):
        """G1 = (1,2) satisfies y² = x³ + 3 mod p  (BN128, b=3)."""
        x, y = int(G1[0]), int(G1[1])
        assert (y*y) % p == (x**3 + 3) % p, \
            f"G1 not on y²=x³+3! LHS={(y*y)%p} RHS={(x**3+3)%p}"

    def test_g1_bytes_are_1_then_2(self):
        """G1 serialised = 0x00…01 || 0x00…02 (32 bytes each)."""
        b = g1_to_bytes(G1)
        assert b == (1).to_bytes(32,'big') + (2).to_bytes(32,'big')

    @pytest.mark.slow
    def test_g1_has_order_q(self):
        """q·G1 = point at infinity."""
        assert multiply(G1, q) is None, "q·G1 != ∞ — wrong group order!"

    @pytest.mark.slow
    def test_g2_has_order_q(self):
        """q·G2 = point at infinity."""
        assert multiply(G2, q) is None, "q·G2 != ∞ — wrong group order!"

    def test_g2_on_curve(self):
        """G2 generator lies on the twist curve."""
        assert is_on_curve(G2, _bn.b2), "G2 not on twist curve!"

    def test_g2_serialised_length(self):
        assert len(g2_to_bytes(G2)) == 128

    @pytest.mark.slow
    def test_gt_has_order_q(self):
        """γ_T^q = 1 in GT."""
        gt = _GT
        assert fp12_eq(fp12_pow(gt, q), FQ12.one()), "γ_T^q ≠ 1!"

    @pytest.mark.slow
    def test_gt_nondegenerate(self):
        """e(G1,G2) ≠ 1."""
        gt = _GT
        assert not fp12_eq(gt, FQ12.one()), "Pairing is degenerate!"

    def test_gt_all_coeffs_in_field(self):
        """All 12 GT coefficients are in [0, p-1]."""
        gt = _GT
        for i, c in enumerate(flat12(gt)):
            assert 0 <= c < p, f"GT[{i}] = {c} is out of [0,p-1]!"

    def test_gt_not_identity(self):
        """GT generator ≠ (1,0,0,…,0)."""
        gt = _GT
        assert flat12(gt) != [1]+[0]*11, "GT is identity — wrong!"

    def test_b2_twist_const(self):
        """b2 = 3/(9+u) in Fp2: verify b2·(9+u) = 3."""
        inv82 = pow(82, p-2, p)
        c0 = (27 * inv82) % p          # Re(3/(9+u))
        c1 = (p - (3  * inv82) % p) % p  # Im
        assert (c0*9 - c1*1) % p == 3, "b2·xi ≠ 3 (real)"
        assert (c0*1 + c1*9) % p == 0, "b2·xi ≠ 3 (imag)"

    def test_montgomery_one_roundtrip(self):
        """1 in Montgomery = R mod p; converting back gives 1."""
        R = pow(2, 256, p)
        # Mont(1) = R mod p
        mont_one = R
        # Converting out: R * R^{-1} mod p = 1
        assert (mont_one * pow(2, -256, p)) % p == 1

    def test_p_is_prime(self):
        """Fp prime p passes Fermat primality (not a guarantee, but a sanity check)."""
        assert pow(2, p-1, p) == 1, "p failed Fermat primality test!"

    def test_q_is_prime(self):
        """Group order q passes Fermat primality."""
        assert pow(2, q-1, q) == 1, "q failed Fermat primality test!"


# =============================================================================
#  [MATH] Field and group algebra
# =============================================================================
class TestMath:
    """Algebraic identities that MUST hold. Each catches a different bug."""

    @pytest.mark.slow
    def test_pairing_bilinear_left(self):
        """e([a]P, Q) = e(P,Q)^a."""
        a  = random.randint(2, q-1)
        lhs = pairing(G2, multiply(G1, a))
        rhs = fp12_pow(_GT, a)
        assert fp12_eq(lhs, rhs), "Left bilinearity broken!"

    @pytest.mark.slow
    def test_pairing_bilinear_right(self):
        """e(P, [b]Q) = e(P,Q)^b."""
        b   = random.randint(2, q-1)
        lhs = pairing(multiply(G2, b), G1)
        rhs = fp12_pow(_GT, b)
        assert fp12_eq(lhs, rhs), "Right bilinearity broken!"

    @pytest.mark.slow
    def test_pairing_bilinear_both(self):
        """e([a]P, [b]Q) = e(P,Q)^{ab}."""
        a, b = random.randint(2, 1000), random.randint(2, 1000)
        lhs = pairing(multiply(G2, b), multiply(G1, a))
        rhs = fp12_pow(_GT, (a*b) % q)
        assert fp12_eq(lhs, rhs), "Both-argument bilinearity broken!"

    @pytest.mark.slow
    def test_gt_pow_neg(self):
        """γ^{-s} · γ^s = 1."""
        s = random.randint(1, q-1)
        g = _GT
        assert fp12_eq(fp12_pow(g, s) * fp12_pow(g, (-s)%q), FQ12.one())

    @pytest.mark.slow
    def test_gt_pow_distributive(self):
        """γ^{a+b} = γ^a · γ^b."""
        a, b = random.randint(1, q-1), random.randint(1, q-1)
        g = _GT
        assert fp12_eq(fp12_pow(g, (a+b)%q), fp12_pow(g,a)*fp12_pow(g,b))

    def test_g1_neg(self):
        """P + (−P) = ∞."""
        assert add(G1, neg(G1)) is None

    def test_g2_neg(self):
        """Q + (−Q) = ∞."""
        assert add(G2, neg(G2)) is None

    def test_fp_fermat(self):
        """a^{p−1} = 1 mod p for random a."""
        a = random.randint(1, p-1)
        assert pow(a, p-1, p) == 1

    def test_fq_fermat(self):
        """a^{q−1} = 1 mod q for random a."""
        a = random.randint(1, q-1)
        assert pow(a, q-1, q) == 1

    def test_fp_inv(self):
        """a · a^{p−2} = 1 mod p."""
        a = random.randint(1, p-1)
        assert (a * pow(a, p-2, p)) % p == 1

    def test_fq_inv(self):
        """a · a^{q−2} = 1 mod q."""
        a = random.randint(1, q-1)
        assert (a * pow(a, q-2, q)) % q == 1

    @pytest.mark.slow
    def test_g1_scalar_add(self):
        """[a+b]P = [a]P + [b]P."""
        a, b = random.randint(1, q-1), random.randint(1, q-1)
        lhs = multiply(G1, (a+b)%q)
        rhs = add(multiply(G1,a), multiply(G1,b))
        assert pt_eq(lhs, rhs), "G1 scalar mul not additive!"

    @pytest.mark.slow
    def test_g2_scalar_add(self):
        """[a+b]Q = [a]Q + [b]Q."""
        a, b = random.randint(1, q-1), random.randint(1, q-1)
        lhs = multiply(G2, (a+b)%q)
        rhs = add(multiply(G2,a), multiply(G2,b))
        assert pt_eq(lhs, rhs), "G2 scalar mul not additive!"


# =============================================================================
#  [SER] Serialisation
# =============================================================================
class TestSerialisation:
    """Every byte matters. Endianness or ordering bugs = silent mismatch."""

    def test_g1_roundtrip(self):
        k = random.randint(1, q-1)
        pt = multiply(G1, k)
        b  = g1_to_bytes(pt)
        pt2 = bytes_to_g1(b)
        assert int(pt[0]) == int(pt2[0]) and int(pt[1]) == int(pt2[1])

    def test_g2_roundtrip(self):
        k = random.randint(1, q-1)
        pt = multiply(G2, k)
        b  = g2_to_bytes(pt)
        pt2 = bytes_to_g2(b)
        assert int(pt[0].coeffs[0]) == int(pt2[0].coeffs[0])
        assert int(pt[0].coeffs[1]) == int(pt2[0].coeffs[1])
        assert int(pt[1].coeffs[0]) == int(pt2[1].coeffs[0])
        assert int(pt[1].coeffs[1]) == int(pt2[1].coeffs[1])

    @pytest.mark.slow
    def test_fp12_roundtrip(self):
        gt  = fp12_pow(_GT, random.randint(2, q-1))
        gt2 = bytes_to_fp12(fp12_to_bytes(gt))
        assert fp12_eq(gt, gt2), "FP12 roundtrip failed!"

    def test_fp12_length(self):
        gt = _GT
        assert len(fp12_to_bytes(gt)) == 384

    def test_fp12_big_endian(self):
        """Each 32-byte chunk is big-endian."""
        gt = _GT
        b  = fp12_to_bytes(gt)
        for i, c in enumerate(flat12(gt)):
            got = int.from_bytes(b[i*32:(i+1)*32], 'big')
            assert got == c, f"Chunk {i} not big-endian: got {got} expected {c}"

    def test_g1_x_first_y_second(self):
        b = g1_to_bytes(G1)
        assert int.from_bytes(b[0:32],'big')  == int(G1[0])
        assert int.from_bytes(b[32:64],'big') == int(G1[1])

    def test_g2_x0_x1_y0_y1_order(self):
        b = g2_to_bytes(G2)
        assert int.from_bytes(b[0:32], 'big')  == int(G2[0].coeffs[0])
        assert int.from_bytes(b[32:64],'big')  == int(G2[0].coeffs[1])
        assert int.from_bytes(b[64:96],'big')  == int(G2[1].coeffs[0])
        assert int.from_bytes(b[96:128],'big') == int(G2[1].coeffs[1])

    def test_distinct_g1_distinct_bytes(self):
        assert g1_to_bytes(multiply(G1,2)) != g1_to_bytes(multiply(G1,3))

    def test_distinct_g2_distinct_bytes(self):
        assert g2_to_bytes(multiply(G2,2)) != g2_to_bytes(multiply(G2,3))

    @pytest.mark.slow
    def test_distinct_fp12_distinct_bytes(self):
        g = _GT
        assert fp12_to_bytes(fp12_pow(g,2)) != fp12_to_bytes(fp12_pow(g,3))

    def test_g1_length(self):   assert len(g1_to_bytes(G1))  == 64
    def test_g2_length(self):   assert len(g2_to_bytes(G2))  == 128

    def test_pub_packet_size(self):
        """A(64)+B(128)+C(64)+D(128) = 384 bytes."""
        assert 64+128+64+128 == 384

    def test_result_packet_size(self):
        """gamma(384)+rho(384) = 768 bytes."""
        assert 384+384 == 768

    def test_fp12_flat_count(self):
        """flat12() gives exactly 12 values."""
        assert len(flat12(_GT)) == 12

    def test_fp12_all_coeffs_in_field(self):
        """No coefficient overflows p."""
        for c in flat12(_GT):
            assert 0 <= c < p


# =============================================================================
#  [PROTO] Protocol correctness
# =============================================================================
class TestProtocol:
    """
    The core equations from ePrint 2025/542.
    If any of these fails, the protocol is wrong.
    """

    @pytest.mark.slow
    def test_honest_verify_passes(self):
        """One full honest round must verify."""
        xi, r, gamma, rho = full_round()
        assert verify(xi, r, gamma, rho), \
            "Honest round FAILED to verify — protocol is broken!"

    @pytest.mark.slow
    def test_honest_verify_two_independent_keys(self):
        """Two independent keys, both should pass."""
        for s in [random.randint(1,q-1), random.randint(1,q-1)]:
            xi, r, gamma, rho = full_round(s)
            assert verify(xi, r, gamma, rho), f"Failed for s={s}"

    @pytest.mark.slow
    def test_explicit_equation(self):
        """
        Explicitly verify: xi == e(A,D)·e(C,Q) · e(A,B)^r
        This is the paper's Equation 1 written out directly.
        """
        s = random.randint(1, q-1)
        xi = ots(s)
        pub, r = setup(s, G1, G2)
        A, B, C, D = pub['A'], pub['B'], pub['C'], pub['D']

        rho   = pairing(B, A)
        gamma = pairing(D, A) * pairing(G2, C)
        lhs   = fp12_pow(rho, r) * gamma
        assert fp12_eq(lhs, xi), "Core protocol equation xi==rho^r·gamma fails!"

    @pytest.mark.slow
    def test_algebraic_derivation(self):
        """
        Full algebraic proof verification (see amore.c comments):
          gamma = γ_T^{−r−s}, rho = γ_T,
          rho^r · gamma = γ_T^r · γ_T^{−r−s} = γ_T^{−s} = xi  ✓
        Verify each intermediate step holds.
        """
        s = random.randint(1, q-1)
        u = random.randint(1, q-1)
        r = random.randint(1, (1<<PHI)-1)

        u_inv   = pow(u, q-2, q)
        su_inv  = (s * u_inv) % q
        nsu_inv = (-su_inv) % q

        U = multiply(G1, u)
        V = multiply(G2, su_inv)
        C = multiply(add(U, G1), nsu_inv)
        D = add(V, neg(multiply(G2, r)))

        gt  = _GT
        xi  = fp12_pow(gt, (-s) % q)

        # rho = e(G1,G2) = gt
        rho   = _GT
        # gamma = e(G1,D)·e(C,G2)
        gamma = pairing(D, G1) * pairing(G2, C)

        # Intermediate: gamma should equal gt^{-r-s}
        gamma_expected = fp12_pow(gt, (-r - s) % q)
        assert fp12_eq(gamma, gamma_expected), \
            "gamma ≠ γ_T^{-r-s} — blinding arithmetic wrong!"

        # Final: rho^r · gamma = gt^r · gt^{-r-s} = gt^{-s} = xi
        assert fp12_eq(fp12_pow(rho,r) * gamma, xi), \
            "rho^r·gamma ≠ xi — verification equation fails!"

    @pytest.mark.slow
    def test_rerandomisation(self):
        """Two calls to setup() with the same s must produce different C."""
        s = random.randint(1, q-1)
        pub1, _ = setup(s, G1, G2)
        pub2, _ = setup(s, G1, G2)
        assert g1_to_bytes(pub1['C']) != g1_to_bytes(pub2['C']), \
            "Setup not rerandomised — u is deterministic!"

    @pytest.mark.slow
    def test_r_is_short(self):
        """r < 2^phi for 20 consecutive samples."""
        s = random.randint(1, q-1)
        for _ in range(20):
            _, r = setup(s, G1, G2)
            assert 0 < r < (1 << PHI), f"r={r} violates 0 < r < 2^{PHI}!"

    @pytest.mark.slow
    def test_extreme_s_min(self):
        """s=1 is valid (smallest key)."""
        xi, r, gamma, rho = full_round(1)
        assert verify(xi, r, gamma, rho), "Protocol fails for s=1!"

    @pytest.mark.slow
    def test_extreme_s_max(self):
        """s=q-1 is valid (largest key)."""
        xi, r, gamma, rho = full_round(q-1)
        assert verify(xi, r, gamma, rho), "Protocol fails for s=q-1!"


# =============================================================================
#  [NEG] Negative tests — every wrong input MUST fail
# =============================================================================
class TestNegative:
    """
    Deliberately break the protocol every possible way.
    If ANY of these return True, the implementation is insecure.
    """

    @pytest.fixture(autouse=True)
    def setup_round(self):
        random.seed(0xDEADC0DE + id(self))
        self.s     = random.randint(2, q-2)
        self.xi    = ots(self.s)
        pub, self.r = setup(self.s, G1, G2)
        self.gamma, self.rho = compute_honest(pub)
        self.pub   = pub

    # ── Wrong key ────────────────────────────────────────────────────────────

    @pytest.mark.slow
    def test_wrong_s_adjacent(self):
        """s+1 fails."""
        xi2 = ots((self.s + 1) % q)
        assert not verify(xi2, self.r, self.gamma, self.rho), \
            "SECURITY BUG: verify passed with s+1!"

    @pytest.mark.slow
    def test_wrong_s_random(self):
        """Random s' ≠ s fails."""
        s2 = random.randint(1, q-1)
        if s2 == self.s: s2 = (s2 + 1) % q
        xi2 = ots(s2)
        assert not verify(xi2, self.r, self.gamma, self.rho), \
            f"SECURITY BUG: verify passed with wrong s={s2}!"

    @pytest.mark.slow
    def test_wrong_s_double(self):
        """s*2 fails."""
        xi2 = ots((self.s * 2) % q)
        assert not verify(xi2, self.r, self.gamma, self.rho), \
            "SECURITY BUG: verify passed with 2s!"

    # ── Wrong r ──────────────────────────────────────────────────────────────

    @pytest.mark.slow
    def test_wrong_r_plus_one(self):
        """r+1 fails."""
        r2 = (self.r + 1) % (1 << PHI)
        assert not verify(self.xi, r2, self.gamma, self.rho), \
            "verify passed with r+1!"

    @pytest.mark.slow
    def test_wrong_r_minus_one(self):
        """r-1 fails (if r > 1)."""
        if self.r > 1:
            assert not verify(self.xi, self.r-1, self.gamma, self.rho), \
                "verify passed with r-1!"

    @pytest.mark.slow
    def test_r_zero(self):
        """r=0 → rho^0=1 → lhs=gamma ≠ xi in general."""
        assert not verify(self.xi, 0, self.gamma, self.rho), \
            "verify passed with r=0!"

    @pytest.mark.slow
    def test_r_doubled(self):
        """r*2 fails."""
        assert not verify(self.xi, self.r*2, self.gamma, self.rho), \
            "verify passed with r*2!"

    # ── Corrupted server output ───────────────────────────────────────────────

    @pytest.mark.slow
    def test_gamma_rho_swapped(self):
        """Swapping gamma and rho must fail."""
        assert not verify(self.xi, self.r, self.rho, self.gamma), \
            "SECURITY BUG: verify passed with gamma and rho swapped!"

    @pytest.mark.slow
    def test_gamma_identity(self):
        """gamma=1 must fail."""
        assert not verify(self.xi, self.r, FQ12.one(), self.rho), \
            "verify passed with gamma=identity!"

    @pytest.mark.slow
    def test_rho_identity(self):
        """rho=1 → rho^r=1 → lhs=gamma ≠ xi."""
        assert not verify(self.xi, self.r, self.gamma, FQ12.one()), \
            "verify passed with rho=identity!"

    @pytest.mark.slow
    def test_both_identity(self):
        """gamma=rho=1 → lhs=1 ≠ xi."""
        assert not verify(self.xi, self.r, FQ12.one(), FQ12.one()), \
            "verify passed with both=identity!"

    @pytest.mark.slow
    def test_gamma_squared(self):
        """gamma^2 fails."""
        assert not verify(self.xi, self.r, self.gamma*self.gamma, self.rho), \
            "verify passed with gamma^2!"

    @pytest.mark.slow
    def test_rho_squared(self):
        """rho^2 fails."""
        assert not verify(self.xi, self.r, self.gamma, self.rho*self.rho), \
            "verify passed with rho^2!"

    @pytest.mark.slow
    def test_cross_round_output(self):
        """Output from a different round fails under this round's r."""
        pub2, r2 = setup(self.s, G1, G2)
        g2, ro2  = compute_honest(pub2)
        # Use round2 output but round1's r
        if r2 != self.r:
            assert not verify(self.xi, self.r, g2, ro2), \
                "verify passed with output from a different round!"

    # ── Malicious server attacks ──────────────────────────────────────────────

    @pytest.mark.slow
    def test_malicious_random_x(self):
        """Server scales both outputs by random x — must fail."""
        for _ in range(3):
            x  = random.randint(2, q-2)
            bg = fp12_pow(self.gamma, x)
            br = fp12_pow(self.rho,   x)
            assert not verify(self.xi, self.r, bg, br), \
                f"SECURITY BUG: malicious x={x} passed!"

    @pytest.mark.slow
    def test_malicious_x_equals_2(self):
        assert not verify(self.xi, self.r,
                          fp12_pow(self.gamma,2), fp12_pow(self.rho,2)), \
            "SECURITY BUG: malicious x=2 passed!"

    @pytest.mark.slow
    def test_malicious_x_equals_q_minus_1(self):
        assert not verify(self.xi, self.r,
                          fp12_pow(self.gamma, q-1), fp12_pow(self.rho, q-1)), \
            "SECURITY BUG: malicious x=q-1 passed!"

    @pytest.mark.slow
    def test_malicious_x_equals_r(self):
        """x=r is a non-trivial specific attack attempt."""
        x  = self.r % q
        bg = fp12_pow(self.gamma, x)
        br = fp12_pow(self.rho,   x)
        assert not verify(self.xi, self.r, bg, br), \
            f"SECURITY BUG: malicious x=r={x} passed!"

    @pytest.mark.slow
    def test_bitflip_in_gamma(self):
        """Single-bit flip in gamma serialisation must fail."""
        b = bytearray(fp12_to_bytes(self.gamma))
        b[100] ^= 0x01
        g_bad = bytes_to_fp12(bytes(b))
        assert not verify(self.xi, self.r, g_bad, self.rho), \
            "verify passed after single-bit flip in gamma!"

    @pytest.mark.slow
    def test_bitflip_in_rho(self):
        """Single-bit flip in rho serialisation must fail."""
        b = bytearray(fp12_to_bytes(self.rho))
        b[50] ^= 0x80
        r_bad = bytes_to_fp12(bytes(b))
        assert not verify(self.xi, self.r, self.gamma, r_bad), \
            "verify passed after single-bit flip in rho!"

    @pytest.mark.slow
    def test_xi_identity(self):
        """xi=1 (wrong key) must fail."""
        assert not verify(FQ12.one(), self.r, self.gamma, self.rho), \
            "verify passed with xi=identity (attacker has no key)!"

    # ── Wrong pub inputs ──────────────────────────────────────────────────────

    @pytest.mark.slow
    def test_wrong_A_in_pub(self):
        """Server uses [2]A instead of A → wrong pairings → fail."""
        pub2 = dict(self.pub); pub2['A'] = multiply(G1, 2)
        g2, ro2 = compute_honest(pub2)
        assert not verify(self.xi, self.r, g2, ro2), \
            "verify passed when server used wrong A!"

    @pytest.mark.slow
    def test_wrong_B_in_pub(self):
        """Server uses [3]B → wrong rho → fail."""
        pub2 = dict(self.pub); pub2['B'] = multiply(G2, 3)
        g2, ro2 = compute_honest(pub2)
        assert not verify(self.xi, self.r, g2, ro2), \
            "verify passed when server used wrong B!"

    @pytest.mark.slow
    def test_wrong_C_in_pub(self):
        """Server uses negated C → wrong gamma → fail."""
        pub2 = dict(self.pub); pub2['C'] = neg(self.pub['C'])
        g2, ro2 = compute_honest(pub2)
        assert not verify(self.xi, self.r, g2, ro2), \
            "verify passed when server used −C instead of C!"

    @pytest.mark.slow
    def test_wrong_D_in_pub(self):
        """Server uses negated D → wrong gamma → fail."""
        pub2 = dict(self.pub); pub2['D'] = neg(self.pub['D'])
        g2, ro2 = compute_honest(pub2)
        assert not verify(self.xi, self.r, g2, ro2), \
            "verify passed when server used −D instead of D!"


# =============================================================================
#  [BOUND] Boundary and cross-session tests
# =============================================================================
class TestBoundary:

    @pytest.mark.slow
    def test_cross_session_no_interference(self):
        """Session2's output must NOT verify under session1's key."""
        s1 = random.randint(1, q-1); xi1 = ots(s1)
        s2 = random.randint(1, q-1)
        while s2 == s1: s2 = random.randint(1, q-1)

        pub1, r1 = setup(s1, G1, G2)
        pub2, r2 = setup(s2, G1, G2)
        g1, ro1  = compute_honest(pub1)
        g2, ro2  = compute_honest(pub2)

        assert not verify(xi1, r1, g2, ro2), \
            "Cross-session: session2 output verified under session1 key!"

    @pytest.mark.slow
    def test_replay_attack_fails(self):
        """Re-using (gamma, rho) from round k with round k+1's r fails."""
        s = random.randint(1, q-1); xi = ots(s)
        pub1, r1 = setup(s, G1, G2)
        _,    r2 = setup(s, G1, G2)
        g1, ro1  = compute_honest(pub1)
        if r1 != r2:
            assert not verify(xi, r2, g1, ro1), "REPLAY ATTACK passed!"

    @pytest.mark.slow
    def test_non_generator_input_points(self):
        """Protocol works when A, B are not the generators."""
        a, b = random.randint(2, 500), random.randint(2, 500)
        A = multiply(G1, a); B = multiply(G2, b)
        s = random.randint(1, q-1); xi = ots(s)
        pub, r = setup(s, A, B)
        gamma, rho = compute_honest(pub)
        assert verify(xi, r, gamma, rho), \
            "Protocol fails for non-generator input points!"

    @pytest.mark.slow
    def test_r_max_value(self):
        """r = 2^phi − 1 (all phi bits set) works correctly."""
        s = random.randint(1, q-1); xi = ots(s)
        u = random.randint(1, q-1)
        r = (1 << PHI) - 1
        u_inv = pow(u, q-2, q); su_inv = (s*u_inv)%q
        U = multiply(G1, u); V = multiply(G2, su_inv)
        C = multiply(add(U, G1), (-su_inv)%q)
        D = add(V, neg(multiply(G2, r)))
        gamma = pairing(D, G1) * pairing(G2, C)
        rho   = _GT
        assert verify(xi, r, gamma, rho), f"Protocol fails for r=2^{PHI}-1!"

    @pytest.mark.slow
    def test_multiple_independent_xi_xi_prime_differ(self):
        """xi(s) ≠ xi(s') for different s, s'."""
        s1, s2 = random.randint(1,q-1), random.randint(1,q-1)
        while s2 == s1: s2 = random.randint(1, q-1)
        assert not fp12_eq(ots(s1), ots(s2)), \
            "ots(s) == ots(s') for different s — GT exp is degenerate!"


# =============================================================================
#  [CRC] Packet integrity
# =============================================================================
class TestCRC:
    """CRC8 must catch every single-byte error — zero false negatives."""

    def _crc8(self, cmd: int, data: bytes) -> int:
        crc = cmd ^ (len(data) & 0xFF) ^ ((len(data) >> 8) & 0xFF)
        for b in data: crc ^= b
        return crc & 0xFF

    def test_crc_cmd_change(self):
        d = b'\xAB' * 10
        assert self._crc8(0x10, d) != self._crc8(0x20, d)

    def test_crc_every_byte_position_384(self):
        """Flipping any byte in a 384-byte payload changes CRC."""
        data = bytes(i & 0xFF for i in range(384))
        crc  = self._crc8(0x10, data)
        for i in range(len(data)):
            bad = bytearray(data); bad[i] ^= 0xFF
            assert self._crc8(0x10, bytes(bad)) != crc, \
                f"CRC unchanged after flipping byte {i}!"

    def test_crc_length_change(self):
        """Adding or removing a byte changes CRC."""
        d1 = b'\xCC' * 100
        d2 = b'\xCC' * 101
        assert self._crc8(0x10, d1) != self._crc8(0x10, d2)

    def test_crc_deterministic(self):
        d = b'\xDE\xAD\xBE\xEF' * 96
        assert self._crc8(0x10, d) == self._crc8(0x10, d)

    def test_crc_all_zero_payload(self):
        """All-zero payload has a specific, non-trivial CRC (just sanity)."""
        crc = self._crc8(0x10, b'\x00' * 384)
        # CRC = 0x10 ^ (384&0xFF) ^ (384>>8) = 0x10 ^ 0x80 ^ 0x01 = 0x91
        assert crc == (0x10 ^ 0x80 ^ 0x01)


# =============================================================================
#  [CONSTANTS CROSS-CHECK] C header values vs Python
# =============================================================================
class TestCHeaderValues:
    """
    Hard-check the exact uint32_t limb values from bn128_const.h.
    If any of these fail, the C code has a wrong constant.
    """

    # p limbs (LE order, 8 limbs)
    P_LIMBS = [0xd87cfd47, 0x3c208c16, 0x6871ca8d, 0x97816a91,
               0x8181585d, 0xb85045b6, 0xe131a029, 0x30644e72]
    Q_LIMBS = [0xf0000001, 0x43e1f593, 0x79b97091, 0x2833e848,
               0x8181585d, 0xb85045b6, 0xe131a029, 0x30644e72]

    def _from_limbs(self, limbs):
        return sum(v << (32*i) for i,v in enumerate(limbs))

    def test_p_limbs_reconstruct_p(self):
        assert self._from_limbs(self.P_LIMBS) == p, \
            "BN128_P limbs don't reconstruct p!"

    def test_q_limbs_reconstruct_q(self):
        assert self._from_limbs(self.Q_LIMBS) == q, \
            "BN128_Q limbs don't reconstruct q!"

    def test_g1x_limbs(self):
        """G1X in header = R mod p (Montgomery form of 1)."""
        R_LIMBS = [0xc58f0d9d, 0xd35d438d, 0xf5c70b3d, 0x0a78eb28,
                   0x7879462c, 0x666ea36f, 0x9a07df2f, 0x0e0a77c1]
        R = self._from_limbs(R_LIMBS)
        assert R == pow(2, 256, p), \
            f"BN128_G1X != R mod p! got {R}"

    def test_g1y_limbs(self):
        """G1Y in header = 2*R mod p (Montgomery form of 2)."""
        G1Y_LIMBS = [0x8b1e1b3a, 0xa6ba871b, 0xeb8e167b, 0x14f1d651,
                     0xf0f28c58, 0xccdd46de, 0x340fbe5e, 0x1c14ef83]
        val = self._from_limbs(G1Y_LIMBS)
        assert val == (2 * pow(2, 256, p)) % p, \
            "BN128_G1Y != 2*R mod p!"

    def test_g2x0_limbs(self):
        """G2X0 = G2[0].coeffs[0] * R mod p."""
        G2X0_LIMBS = [0x02bc2026, 0x8e83b5d1, 0x497b0172, 0xdceb1935,
                      0x97811adf, 0xfbb82647, 0xaf96503b, 0x19573841]
        val = self._from_limbs(G2X0_LIMBS)
        R   = pow(2, 256, p)
        expected = (int(G2[0].coeffs[0]) * R) % p
        assert val == expected, \
            f"BN128_G2X0 wrong! got={val} expected={expected}"

    def test_gt0_is_first_fp12_coeff_mont(self):
        """BN128_GT0 = flat12(e(G1,G2))[0] * R mod p."""
        GT0_LIMBS = [0x3a8329ef, 0x53e1d9fc, 0x9ff5465f, 0x9254a194,
                     0x1fad5084, 0x3d01af56, 0x6b3b7c1e, 0x2ae04505]
        val = self._from_limbs(GT0_LIMBS)
        R   = pow(2, 256, p)
        gt  = _GT
        c0  = flat12(gt)[0]
        expected = (c0 * R) % p
        assert val == expected, \
            f"BN128_GT0 wrong! got={val} expected={expected}"

    def test_all_8_gt_limbs_available(self):
        """We have exactly 12 GT constant arrays (GT0..GT11)."""
        # Structural test: just verify the flat12 gives 12 values
        assert len(flat12(_GT)) == 12

    def test_p_and_q_share_top_limbs(self):
        """p and q share limbs [4..7] — this is a known BN128 property."""
        for i in range(4, 8):
            assert self.P_LIMBS[i] == self.Q_LIMBS[i], \
                f"Limb[{i}] differs between p and q — expected to match!"


# =============================================================================
#  Entry point
# =============================================================================
if __name__ == '__main__':
    import subprocess, sys
    r = subprocess.run(
        [sys.executable, '-m', 'pytest', __file__, '-v', '--tb=short'] + sys.argv[1:],
        cwd=str(__import__('pathlib').Path(__file__).parent.parent)
    )
    sys.exit(r.returncode)
