#!/usr/bin/env python3
"""
=============================================================================
 test_c_impl.py  —  C Implementation Paranoid Tests
=============================================================================

These tests compile the ACTUAL C code (fp.c, fq.c, fp2.c, fp12.c, g1.c,
g2.c) and call it via ctypes, then compare every result against py_ecc.

If fp_mul() has a single wrong bit in CIOS, this will catch it.
If fp12_exp() has a wrong exponentiation loop, this will catch it.
If g1_scalar_mul() has a Jacobian formula error, this will catch it.

This is the layer between "Python protocol is correct" and
"hardware actually runs AmorE correctly".

PREREQUISITE:
    bash build_c_testlib.sh    # builds tests/libamore_test.so

RUN:
    pytest tests/test_c_impl.py -v
    pytest tests/test_c_impl.py -v -k fp_mul    # one test
=============================================================================
"""

import ctypes, os, struct, random, sys
import pytest

# ── py_ecc reference ─────────────────────────────────────────────────────────
from py_ecc.bn128 import (
    field_modulus as p, curve_order as q,
    G1, G2, multiply, add, neg, pairing
)
import py_ecc.bn128 as _bn
FQ12 = _bn.FQ12
FQ   = _bn.FQ
FQ2  = _bn.FQ2

# ── Load shared library ───────────────────────────────────────────────────────
_HERE   = os.path.dirname(os.path.abspath(__file__))
_LIBPATH = os.path.join(_HERE, "libamore_test.so")

def _load_lib():
    if not os.path.exists(_LIBPATH):
        pytest.skip(
            f"libamore_test.so not found at {_LIBPATH}\n"
            "Run:  bash build_c_testlib.sh"
        )
    return ctypes.CDLL(_LIBPATH)

_lib = None

def get_lib():
    global _lib
    if _lib is None:
        _lib = _load_lib()
    return _lib


# ── C type helpers ────────────────────────────────────────────────────────────
# uint32_t[8]  ↔  Python int  (little-endian limbs)
Fp_c  = ctypes.c_uint32 * 8
Fp2_c = ctypes.c_uint32 * 16   # two Fp side by side: c0[8] + c1[8]

# G1Point: X[8], Y[8], Z[8]  = 24 uint32_t
G1Point_c = ctypes.c_uint32 * 24

# G2Point: X.c0[8], X.c1[8], Y.c0[8], Y.c1[8], Z.c0[8], Z.c1[8] = 48 uint32_t
G2Point_c = ctypes.c_uint32 * 48

# Fp12: 12 × Fp = 96 uint32_t
Fp12_c = ctypes.c_uint32 * 96

R     = pow(2, 256, p)   # Montgomery base
R_mod_p = R % p
R_mod_q = R % q

def int_to_fp(v: int, mod: int = p) -> Fp_c:
    """Python int (plain) → Montgomery Fp_c (×R mod mod)."""
    R_mod = pow(2, 256, mod)
    mont = (v * R_mod) % mod
    arr = Fp_c()
    for i in range(8):
        arr[i] = (mont >> (32 * i)) & 0xFFFFFFFF
    return arr

def fp_to_int(arr: Fp_c, mod: int = p) -> int:
    """Montgomery Fp_c → plain Python int."""
    mont = sum(int(arr[i]) << (32 * i) for i in range(8))
    r_inv = pow(2, -256, mod)
    return (mont * r_inv) % mod

def int_to_fq(v: int) -> Fp_c:
    return int_to_fp(v, q)

def fq_to_int(arr: Fp_c) -> int:
    return fp_to_int(arr, q)

def g1_from_py(pt) -> G1Point_c:
    """py_ecc G1 affine → C G1Point Jacobian (Z=1, Montgomery)."""
    arr = G1Point_c()
    xm = int_to_fp(int(pt[0]))
    ym = int_to_fp(int(pt[1]))
    zm = int_to_fp(1)          # Z=1 in Montgomery
    for i in range(8):
        arr[i]    = xm[i]
        arr[8+i]  = ym[i]
        arr[16+i] = zm[i]
    return arr

def g1_to_py_affine(arr: G1Point_c):
    """C G1Point → py_ecc affine (or None if infinity)."""
    Z = [int(arr[16+i]) for i in range(8)]
    if all(v == 0 for v in Z):
        return None
    # Convert to affine by calling C g1_to_affine via bytes route
    buf = (ctypes.c_uint8 * 64)()
    lib = get_lib()
    lib.g1_to_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint8),
                                 ctypes.POINTER(ctypes.c_uint32)]
    lib.g1_to_bytes.restype  = None
    lib.g1_to_bytes(buf, arr)
    x = int.from_bytes(bytes(buf[:32]),  'big')
    y = int.from_bytes(bytes(buf[32:64]),'big')
    return (FQ(x), FQ(y))

def g2_from_py(pt) -> G2Point_c:
    """py_ecc G2 affine → C G2Point Jacobian (Z=(1,0), Montgomery)."""
    arr = G2Point_c()
    x0 = int_to_fp(int(pt[0].coeffs[0]))
    x1 = int_to_fp(int(pt[0].coeffs[1]))
    y0 = int_to_fp(int(pt[1].coeffs[0]))
    y1 = int_to_fp(int(pt[1].coeffs[1]))
    z0 = int_to_fp(1)
    z1 = int_to_fp(0)
    for i in range(8):
        arr[0*8+i]  = x0[i]; arr[1*8+i]  = x1[i]
        arr[2*8+i]  = y0[i]; arr[3*8+i]  = y1[i]
        arr[4*8+i]  = z0[i]; arr[5*8+i]  = z1[i]
    return arr

def g2_to_py_affine(arr: G2Point_c):
    """C G2Point → py_ecc affine (via g2_to_bytes)."""
    buf = (ctypes.c_uint8 * 128)()
    lib = get_lib()
    lib.g2_to_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint8),
                                 ctypes.POINTER(ctypes.c_uint32)]
    lib.g2_to_bytes.restype  = None
    lib.g2_to_bytes(buf, arr)
    x0 = int.from_bytes(bytes(buf[0:32]),   'big')
    x1 = int.from_bytes(bytes(buf[32:64]),  'big')
    y0 = int.from_bytes(bytes(buf[64:96]),  'big')
    y1 = int.from_bytes(bytes(buf[96:128]), 'big')
    return (FQ2([FQ(x0), FQ(x1)]), FQ2([FQ(y0), FQ(y1)]))

def flat12_py(elem) -> list:
    r = []
    def f(v):
        if hasattr(v,'coeffs'):
            for c in v.coeffs: f(c)
        else: r.append(int(v))
    f(elem); return r

def fp12_from_bytes_c(buf_bytes: bytes):
    """12×32-byte big-endian buffer → py_ecc FQ12."""
    coeffs = tuple(int.from_bytes(buf_bytes[i*32:(i+1)*32],'big') for i in range(12))
    return FQ12(coeffs)


# ── Fp arithmetic tests ───────────────────────────────────────────────────────
class TestFpArithmetic:
    """
    Every test calls a C function and compares to Python reference.
    A single wrong bit in CIOS will break at least one test here.
    """

    def setup_method(self):
        self.lib = get_lib()
        random.seed(0xC1AAC0DE)

    def _fp_mul_c(self, a: int, b: int) -> int:
        lib = self.lib
        lib.fp_mul.argtypes = [Fp_c, Fp_c, Fp_c]
        lib.fp_mul.restype  = None
        ra, rb, rr = int_to_fp(a), int_to_fp(b), Fp_c()
        lib.fp_mul(rr, ra, rb)
        return fp_to_int(rr)

    def _fp_add_c(self, a: int, b: int) -> int:
        lib = self.lib
        lib.fp_add.argtypes = [Fp_c, Fp_c, Fp_c]
        lib.fp_add.restype  = None
        ra, rb, rr = int_to_fp(a), int_to_fp(b), Fp_c()
        lib.fp_add(rr, ra, rb)
        return fp_to_int(rr)

    def _fp_sub_c(self, a: int, b: int) -> int:
        lib = self.lib
        lib.fp_sub.argtypes = [Fp_c, Fp_c, Fp_c]
        lib.fp_sub.restype  = None
        ra, rb, rr = int_to_fp(a), int_to_fp(b), Fp_c()
        lib.fp_sub(rr, ra, rb)
        return fp_to_int(rr)

    def _fp_inv_c(self, a: int) -> int:
        lib = self.lib
        lib.fp_inv.argtypes = [Fp_c, Fp_c]
        lib.fp_inv.restype  = None
        ra, rr = int_to_fp(a), Fp_c()
        lib.fp_inv(rr, ra)
        return fp_to_int(rr)

    def test_fp_mul_small(self):
        """2 * 3 = 6 mod p"""
        assert self._fp_mul_c(2, 3) == 6

    def test_fp_mul_one(self):
        """a * 1 = a"""
        a = random.randint(1, p-1)
        assert self._fp_mul_c(a, 1) == a

    def test_fp_mul_commutative(self):
        """a*b = b*a"""
        a, b = random.randint(1, p-1), random.randint(1, p-1)
        assert self._fp_mul_c(a, b) == self._fp_mul_c(b, a)

    def test_fp_mul_associative(self):
        """(a*b)*c = a*(b*c)"""
        a, b, c = [random.randint(1, p-1) for _ in range(3)]
        lhs = self._fp_mul_c(self._fp_mul_c(a, b), c)
        rhs = self._fp_mul_c(a, self._fp_mul_c(b, c))
        assert lhs == rhs

    def test_fp_mul_random_vs_python(self):
        """10 random pairs: C fp_mul == Python (a*b)%p"""
        for _ in range(10):
            a, b = random.randint(1, p-1), random.randint(1, p-1)
            assert self._fp_mul_c(a, b) == (a * b) % p, \
                f"fp_mul({a},{b}): C={self._fp_mul_c(a,b)} py={(a*b)%p}"

    def test_fp_mul_p_minus_1_squared(self):
        """(p-1)^2 mod p = 1"""
        assert self._fp_mul_c(p-1, p-1) == 1

    def test_fp_add_random_vs_python(self):
        """10 random pairs: C fp_add == (a+b)%p"""
        for _ in range(10):
            a, b = random.randint(0, p-1), random.randint(0, p-1)
            assert self._fp_add_c(a, b) == (a + b) % p

    def test_fp_sub_random_vs_python(self):
        """10 random pairs: C fp_sub == (a-b)%p"""
        for _ in range(10):
            a, b = random.randint(0, p-1), random.randint(0, p-1)
            assert self._fp_sub_c(a, b) == (a - b) % p

    def test_fp_inv_random_vs_python(self):
        """5 random values: C fp_inv == pow(a,p-2,p)"""
        for _ in range(5):
            a = random.randint(1, p-1)
            c_result = self._fp_inv_c(a)
            py_result = pow(a, p-2, p)
            assert c_result == py_result, \
                f"fp_inv({a}): C={c_result} py={py_result}"

    def test_fp_inv_times_a_is_one(self):
        """a * a^{-1} = 1"""
        a = random.randint(1, p-1)
        inv = self._fp_inv_c(a)
        assert self._fp_mul_c(a, inv) == 1

    def test_fp_mul_distributive_over_add(self):
        """a*(b+c) = a*b + a*c"""
        a, b, c = [random.randint(1, p-1) for _ in range(3)]
        lhs = self._fp_mul_c(a, (b + c) % p)
        rhs = (self._fp_mul_c(a, b) + self._fp_mul_c(a, c)) % p
        assert lhs == rhs


# ── Fq arithmetic tests ───────────────────────────────────────────────────────
class TestFqArithmetic:
    """Scalar field mod q — used for blinding scalars s, u."""

    def setup_method(self):
        self.lib = get_lib()
        random.seed(0xF900CAFE)

    def _fq_mul_c(self, a: int, b: int) -> int:
        lib = self.lib
        lib.fq_mul.argtypes = [Fp_c, Fp_c, Fp_c]
        lib.fq_mul.restype  = None
        ra, rb, rr = int_to_fq(a), int_to_fq(b), Fp_c()
        lib.fq_mul(rr, ra, rb)
        return fq_to_int(rr)

    def _fq_inv_c(self, a: int) -> int:
        lib = self.lib
        lib.fq_inv.argtypes = [Fp_c, Fp_c]
        lib.fq_inv.restype  = None
        ra, rr = int_to_fq(a), Fp_c()
        lib.fq_inv(rr, ra)
        return fq_to_int(rr)

    def test_fq_mul_small(self):
        assert self._fq_mul_c(2, 3) == 6

    def test_fq_mul_random_vs_python(self):
        for _ in range(10):
            a, b = random.randint(1, q-1), random.randint(1, q-1)
            assert self._fq_mul_c(a, b) == (a * b) % q

    def test_fq_inv_random_vs_python(self):
        for _ in range(5):
            a = random.randint(1, q-1)
            assert self._fq_inv_c(a) == pow(a, q-2, q)

    def test_fq_inv_product_is_one(self):
        a = random.randint(1, q-1)
        inv = self._fq_inv_c(a)
        assert self._fq_mul_c(a, inv) == 1

    def test_fq_q_minus_1_squared(self):
        """(q-1)^2 mod q = 1"""
        assert self._fq_mul_c(q-1, q-1) == 1


# ── G1 scalar multiplication tests ───────────────────────────────────────────
class TestG1ScalarMul:
    """
    The most security-critical C function.
    Wrong Jacobian formula → wrong EC points → wrong pairings → silently broken.
    """

    def setup_method(self):
        self.lib = get_lib()
        self.lib.g1_scalar_mul.argtypes = [
            G1Point_c,                    # result
            ctypes.POINTER(ctypes.c_uint32),  # input point
            ctypes.POINTER(ctypes.c_uint32),  # scalar k[8]
            ctypes.c_int                  # nbits
        ]
        self.lib.g1_scalar_mul.restype = None
        random.seed(0xAABBCC01)

    def _scalar_mul_c(self, k: int, pt=None) -> tuple:
        if pt is None:
            pt = G1
        pt_c = g1_from_py(pt)
        res  = G1Point_c()
        k_arr = (ctypes.c_uint32 * 8)()
        for i in range(8):
            k_arr[i] = (k >> (32*i)) & 0xFFFFFFFF
        nbits = k.bit_length()
        self.lib.g1_scalar_mul(res, pt_c, k_arr, nbits)
        return g1_to_py_affine(res)

    def _pts_equal(self, a, b) -> bool:
        if a is None and b is None: return True
        if a is None or b is None:  return False
        return int(a[0]) == int(b[0]) and int(a[1]) == int(b[1])

    def test_scalar_2_vs_py_ecc(self):
        """[2]G1: C == py_ecc"""
        c  = self._scalar_mul_c(2)
        py = multiply(G1, 2)
        assert self._pts_equal(c, py), f"[2]G1 mismatch: C={c} py={py}"

    def test_scalar_3_vs_py_ecc(self):
        c  = self._scalar_mul_c(3)
        py = multiply(G1, 3)
        assert self._pts_equal(c, py)

    def test_scalar_random_vs_py_ecc(self):
        """5 random scalars: [k]G1 C == py_ecc"""
        for _ in range(5):
            k  = random.randint(2, q-1)
            c  = self._scalar_mul_c(k)
            py = multiply(G1, k)
            assert self._pts_equal(c, py), f"[{k}]G1 mismatch"

    @pytest.mark.slow
    def test_scalar_q_is_infinity(self):
        """[q]G1 = point at infinity"""
        c = self._scalar_mul_c(q)
        assert c is None, f"[q]G1 should be infinity, got {c}"

    def test_scalar_additive(self):
        """[a+b]G1 = [a]G1 + [b]G1  (in py_ecc, comparing C results)"""
        a, b = random.randint(1, 1000), random.randint(1, 1000)
        ca = self._scalar_mul_c(a)
        cb = self._scalar_mul_c(b)
        cab = self._scalar_mul_c((a+b) % q)
        py_sum = add(ca, cb) if ca and cb else (ca or cb)
        assert self._pts_equal(cab, py_sum), \
            f"[{a}+{b}]G1 != [{a}]G1 + [{b}]G1"


# ── G2 scalar multiplication tests ───────────────────────────────────────────
class TestG2ScalarMul:
    """G2 over Fp2 — same formulas as G1 but more complex."""

    def setup_method(self):
        self.lib = get_lib()
        self.lib.g2_scalar_mul.argtypes = [
            G2Point_c,
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.POINTER(ctypes.c_uint32),
            ctypes.c_int
        ]
        self.lib.g2_scalar_mul.restype = None
        random.seed(0xAABBCC02)

    def _scalar_mul_c(self, k: int) -> tuple:
        pt_c = g2_from_py(G2)
        res  = G2Point_c()
        k_arr = (ctypes.c_uint32 * 8)()
        for i in range(8): k_arr[i] = (k >> (32*i)) & 0xFFFFFFFF
        self.lib.g2_scalar_mul(res, pt_c, k_arr, k.bit_length())
        return g2_to_py_affine(res)

    def _g2_eq(self, a, b) -> bool:
        if a is None and b is None: return True
        if a is None or b is None:  return False
        return (int(a[0].coeffs[0]) == int(b[0].coeffs[0]) and
                int(a[0].coeffs[1]) == int(b[0].coeffs[1]) and
                int(a[1].coeffs[0]) == int(b[1].coeffs[0]) and
                int(a[1].coeffs[1]) == int(b[1].coeffs[1]))

    def test_scalar_2_vs_py_ecc(self):
        c  = self._scalar_mul_c(2)
        py = multiply(G2, 2)
        assert self._g2_eq(c, py), "[2]G2 mismatch!"

    def test_scalar_3_vs_py_ecc(self):
        c  = self._scalar_mul_c(3)
        py = multiply(G2, 3)
        assert self._g2_eq(c, py), "[3]G2 mismatch!"

    def test_scalar_random_vs_py_ecc(self):
        for _ in range(3):
            k  = random.randint(2, 1000)
            c  = self._scalar_mul_c(k)
            py = multiply(G2, k)
            assert self._g2_eq(c, py), f"[{k}]G2 mismatch!"


# ── Fp12 exponentiation tests ─────────────────────────────────────────────────
class TestFp12Exp:
    """
    fp12_exp is used for xi = gamma_T^{-s} and rho^r in Verify().
    If this is wrong, every verification silently fails or gives wrong results.
    """

    def setup_method(self):
        self.lib = get_lib()
        self.lib.fp12_exp.argtypes = [
            Fp12_c,                          # result
            ctypes.POINTER(ctypes.c_uint32), # base (Fp12 = 96 uint32)
            ctypes.POINTER(ctypes.c_uint32), # exponent k[8]
            ctypes.c_int                     # nbits
        ]
        self.lib.fp12_exp.restype = None

        # Build C GT (gamma_T) from constants in bn128_const.h
        # We'll use g1_to_bytes/g2_to_bytes to get the GT via serialisation
        # For now, get GT from py_ecc and convert to C format

    def _gt_to_c(self, gt) -> Fp12_c:
        """py_ecc FQ12 → C Fp12_c (Montgomery form)"""
        arr = Fp12_c()
        coeffs = []
        def f(v):
            if hasattr(v,'coeffs'):
                for c in v.coeffs: f(c)
            else: coeffs.append(int(v))
        f(gt)
        R = pow(2, 256, p)
        for i, c in enumerate(coeffs):
            mont = (c * R) % p
            for j in range(8):
                arr[i*8+j] = (mont >> (32*j)) & 0xFFFFFFFF
        return arr

    def _c_fp12_to_py(self, arr: Fp12_c):
        """C Fp12_c → py_ecc FQ12"""
        R = pow(2, 256, p)
        R_inv = pow(R, p-2, p)
        coeffs = []
        for i in range(12):
            mont = sum(int(arr[i*8+j]) << (32*j) for j in range(8))
            coeffs.append((mont * R_inv) % p)
        return FQ12(tuple(coeffs))

    def _exp_c(self, base_py, k: int):
        lib = self.lib
        base_c = self._gt_to_c(base_py)
        res    = Fp12_c()
        k_arr  = (ctypes.c_uint32 * 8)()
        for i in range(8): k_arr[i] = (k >> (32*i)) & 0xFFFFFFFF
        lib.fp12_exp(res, base_c, k_arr, max(1, k.bit_length()))
        return self._c_fp12_to_py(res)

    def _fp12_pow_py(self, x, e: int):
        r = FQ12.one(); b = x
        while e > 0:
            if e & 1: r = r * b
            b = b * b; e >>= 1
        return r

    @pytest.mark.slow
    def test_gt_exp_2_vs_py_ecc(self):
        """gamma_T^2: C == py_ecc"""
        gt = pairing(G2, G1)
        c  = self._exp_c(gt, 2)
        py = self._fp12_pow_py(gt, 2)
        assert flat12_py(c) == flat12_py(py), "gt^2 mismatch!"

    @pytest.mark.slow
    def test_gt_exp_random_vs_py_ecc(self):
        """gamma_T^k for 3 random k: C == py_ecc"""
        gt = pairing(G2, G1)
        for _ in range(3):
            k  = random.randint(2, 2**90)
            c  = self._exp_c(gt, k)
            py = self._fp12_pow_py(gt, k)
            assert flat12_py(c) == flat12_py(py), f"gt^{k} mismatch!"

    @pytest.mark.slow
    def test_gt_exp_neg_s(self):
        """gamma_T^{-s}: C == py_ecc  (this is exactly xi in OTS)"""
        gt = pairing(G2, G1)
        s  = random.randint(1, q-1)
        neg_s = (-s) % q
        c  = self._exp_c(gt, neg_s)
        py = self._fp12_pow_py(gt, neg_s)
        assert flat12_py(c) == flat12_py(py), "gt^(-s) mismatch — OTS broken!"


# ── Full serialisation round-trip through C ───────────────────────────────────
class TestSerialRoundTrip:
    """
    G1/G2 to_bytes → from_bytes must give back the same point.
    If byte order or endianness is wrong in C, pairing results will differ.
    """

    def setup_method(self):
        self.lib = get_lib()

    def _g1_roundtrip_c(self, k: int):
        lib = self.lib
        # [k]G1 in C
        pt_c = g1_from_py(G1)
        res  = G1Point_c()
        k_arr = (ctypes.c_uint32 * 8)()
        for i in range(8): k_arr[i] = (k >> (32*i)) & 0xFFFFFFFF
        lib.g1_scalar_mul.argtypes = [G1Point_c, ctypes.POINTER(ctypes.c_uint32),
                                       ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
        lib.g1_scalar_mul.restype = None
        lib.g1_scalar_mul(res, pt_c, k_arr, k.bit_length())

        # to_bytes
        buf = (ctypes.c_uint8 * 64)()
        lib.g1_to_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint8),
                                     ctypes.POINTER(ctypes.c_uint32)]
        lib.g1_to_bytes.restype = None
        lib.g1_to_bytes(buf, res)

        # from_bytes
        res2 = G1Point_c()
        lib.g1_from_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint32),
                                       ctypes.POINTER(ctypes.c_uint8)]
        lib.g1_from_bytes.restype = None
        lib.g1_from_bytes(res2, buf)

        # to_bytes again — must match
        buf2 = (ctypes.c_uint8 * 64)()
        lib.g1_to_bytes(buf2, res2)
        return bytes(buf) == bytes(buf2)

    def test_g1_serial_roundtrip_k2(self):
        assert self._g1_roundtrip_c(2), "[2]G1 bytes roundtrip failed!"

    def test_g1_serial_roundtrip_k5(self):
        assert self._g1_roundtrip_c(5), "[5]G1 bytes roundtrip failed!"

    def test_g1_bytes_match_py_ecc(self):
        """[7]G1 bytes from C == bytes from py_ecc"""
        lib = self.lib
        k = 7
        pt_c = g1_from_py(G1)
        res  = G1Point_c()
        k_arr = (ctypes.c_uint32 * 8)(*[k if i==0 else 0 for i in range(8)])
        lib.g1_scalar_mul.argtypes = [G1Point_c, ctypes.POINTER(ctypes.c_uint32),
                                       ctypes.POINTER(ctypes.c_uint32), ctypes.c_int]
        lib.g1_scalar_mul.restype = None
        lib.g1_scalar_mul(res, pt_c, k_arr, k.bit_length())
        buf = (ctypes.c_uint8 * 64)()
        lib.g1_to_bytes.argtypes = [ctypes.POINTER(ctypes.c_uint8),
                                     ctypes.POINTER(ctypes.c_uint32)]
        lib.g1_to_bytes.restype = None
        lib.g1_to_bytes(buf, res)
        c_bytes = bytes(buf)

        py_pt = multiply(G1, k)
        py_bytes = int(py_pt[0]).to_bytes(32,'big') + int(py_pt[1]).to_bytes(32,'big')
        assert c_bytes == py_bytes, f"[7]G1 bytes mismatch!\nC ={c_bytes.hex()}\nPy={py_bytes.hex()}"


# ── Entry point ────────────────────────────────────────────────────────────────
if __name__ == '__main__':
    result = __import__('subprocess').run(
        [sys.executable, '-m', 'pytest', __file__, '-v', '--tb=short'] + sys.argv[1:],
        cwd=str(__import__('pathlib').Path(__file__).parent.parent)
    )
    sys.exit(result.returncode)
