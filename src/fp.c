#include "fp.h"
#include "bls12_381_const.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * BLS12-381 Fp arithmetic, 12 × 32-bit Montgomery form.
 * Modulus is BLS_P (defined in bls12_381_const.h).
 *
 * Phase A: fp_zero, fp_one, fp_copy, fp_eq, fp_is_zero,
 *          fp_add, fp_sub, fp_neg
 *
 * Phase B (next): fp_mul (CIOS Montgomery)
 * Phase C (after): fp_sqr, fp_inv, conversions, serialisation
 * ----------------------------------------------------------------------- */

/* Compare a[N] vs b[N]; return -1/0/+1 for a<b/a==b/a>b */
static int fp_cmp(const uint32_t a[FP_LIMBS], const uint32_t b[FP_LIMBS]) {
    for (int i = FP_LIMBS - 1; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

/* r = a - b, assuming a >= b. Returns borrow (should be 0). */
static uint32_t fp_raw_sub(uint32_t r[FP_LIMBS],
                           const uint32_t a[FP_LIMBS],
                           const uint32_t b[FP_LIMBS]) {
    uint64_t borrow = 0;
    for (int i = 0; i < FP_LIMBS; i++) {
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
        r[i]   = (uint32_t)x;
        borrow = (x >> 63) & 1;
    }
    return (uint32_t)borrow;
}

/* r = a + b mod p (no Montgomery work, just modular addition).
 * Assumes a, b < p. */
static void fp_raw_add_mod(uint32_t r[FP_LIMBS],
                           const uint32_t a[FP_LIMBS],
                           const uint32_t b[FP_LIMBS]) {
    uint64_t carry = 0;
    for (int i = 0; i < FP_LIMBS; i++) {
        uint64_t x = (uint64_t)a[i] + b[i] + carry;
        r[i]  = (uint32_t)x;
        carry = x >> 32;
    }
    /* If carry OR result >= p, subtract p */
    if (carry || fp_cmp(r, BLS_P) >= 0) {
        fp_raw_sub(r, r, BLS_P);
    }
}

/* ===== Public API ===== */

void fp_zero(Fp r) {
    memset(r, 0, sizeof(Fp));
}

void fp_one(Fp r) {
    /* Montgomery form of 1 is R mod p */
    memcpy(r, BLS_R_FP, sizeof(Fp));
}

void fp_copy(Fp r, const Fp a) {
    memcpy(r, a, sizeof(Fp));
}

int fp_is_zero(const Fp a) {
    for (int i = 0; i < FP_LIMBS; i++) {
        if (a[i] != 0) return 0;
    }
    return 1;
}

int fp_eq(const Fp a, const Fp b) {
    return fp_cmp(a, b) == 0;
}

void fp_add(Fp r, const Fp a, const Fp b) {
    fp_raw_add_mod(r, a, b);
}

void fp_sub(Fp r, const Fp a, const Fp b) {
    /* r = a - b mod p */
    if (fp_cmp(a, b) >= 0) {
        fp_raw_sub(r, a, b);
    } else {
        /* a < b: compute (a + p) - b */
        uint32_t tmp[FP_LIMBS];
        uint64_t carry = 0;
        for (int i = 0; i < FP_LIMBS; i++) {
            uint64_t x = (uint64_t)a[i] + BLS_P[i] + carry;
            tmp[i] = (uint32_t)x;
            carry  = x >> 32;
        }
        fp_raw_sub(r, tmp, b);
    }
}

void fp_neg(Fp r, const Fp a) {
    if (fp_is_zero(a)) {
        fp_zero(r);
    } else {
        fp_raw_sub(r, BLS_P, a);
    }
}

/* ===== Phase B: CIOS Montgomery multiplication =====
 *
 * Computes r = a * b * R^{-1} mod p, where R = 2^(32 * FP_LIMBS) = 2^384.
 *
 * If a, b are in Montgomery form (a*R mod p, b*R mod p), then:
 *   r = (aR)(bR)R^{-1} = abR = (ab)_mont
 * which is exactly the Montgomery form of a*b.
 *
 * Reference: Koc, Acar, Kaliski 1996 "Analyzing and Comparing Montgomery
 *            Multiplication Algorithms".
 *
 * Loop invariant: at the start of iteration i, T[N+1..0] holds an
 * intermediate value that is always less than 2*p, so T[N+1] = 0
 * (T[N] may be 0 or 1 only).
 */
void fp_mul(Fp r, const Fp a, const Fp b) {
    /* Accumulator: FP_LIMBS + 2 words.
     * Each cell holds 32 valid bits; uint64_t gives headroom for carry. */
    uint64_t T[FP_LIMBS + 2];
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;

    for (int i = 0; i < FP_LIMBS; i++) {
        /* Step 1: T += a * b[i] */
        uint64_t carry = 0;
        for (int j = 0; j < FP_LIMBS; j++) {
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[FP_LIMBS] += carry;
        /* T[FP_LIMBS+1] absorbs any rare carry from T[FP_LIMBS] += carry */
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
        T[FP_LIMBS] &= 0xFFFFFFFFu;

        /* Step 2: Montgomery reduction by one word.
         * Choose k such that adding k * p zeros out T[0]:
         *   k = T[0] * (-p^{-1}) mod 2^32 = T[0] * N0_P mod 2^32
         */
        uint32_t k = (uint32_t)T[0] * BLS_N0P_FP;
        carry = 0;
        for (int j = 0; j < FP_LIMBS; j++) {
            uint64_t x = T[j] + (uint64_t)BLS_P[j] * k + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[FP_LIMBS] += carry;
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
        T[FP_LIMBS] &= 0xFFFFFFFFu;

        /* By construction T[0] is now 0. Shift T right by one word. */
        for (int j = 0; j < FP_LIMBS + 1; j++) {
            T[j] = T[j + 1];
        }
        T[FP_LIMBS + 1] = 0;
    }

    /* T now holds a value in [0, 2p). Final conditional subtract. */
    /* Pack T[0..FP_LIMBS-1] into a uint32_t array for fp_cmp / fp_raw_sub */
    uint32_t result[FP_LIMBS];
    for (int i = 0; i < FP_LIMBS; i++) {
        result[i] = (uint32_t)T[i];
    }

    /* If T[FP_LIMBS] != 0, definitely >= p (since p fits in FP_LIMBS words).
     * Otherwise check result vs p. */
    if (T[FP_LIMBS] != 0 || fp_cmp(result, BLS_P) >= 0) {
        fp_raw_sub(r, result, BLS_P);
    } else {
        memcpy(r, result, sizeof(Fp));
    }
}

/* ===== Phase C: helpers built on fp_mul ===== */

/* r = a * a mod p (in Montgomery form).
 * For now: just call fp_mul. Specialized squaring is a future optimization. */
void fp_sqr(Fp r, const Fp a) {
    fp_mul(r, a, a);
}

/* Convert plain value to Montgomery form: r = a * R mod p.
 * Trick: fp_mul does (x * y * R^{-1}). So:
 *   fp_mul(a_plain, R^2) = a_plain * R^2 * R^{-1} = a_plain * R = (a_plain)_mont
 */
void fp_to_mont(Fp r, const Fp a_plain) {
    fp_mul(r, a_plain, BLS_R2_FP);
}

/* Convert from Montgomery form: r = a_mont * R^{-1} = a_plain.
 * Trick: fp_mul(a_mont, ONE_PLAIN) = a_mont * 1 * R^{-1} = a_plain
 * where ONE_PLAIN = {1, 0, 0, ..., 0} (the plain integer 1, not Montgomery).
 */
void fp_from_mont(Fp r, const Fp a_mont) {
    Fp one_plain;
    fp_zero(one_plain);
    one_plain[0] = 1;
    fp_mul(r, a_mont, one_plain);
}

/* r = x * R mod p (Montgomery form of small integer x). */
void fp_from_u32(Fp r, uint32_t x) {
    Fp x_plain;
    fp_zero(x_plain);
    x_plain[0] = x;
    fp_to_mont(r, x_plain);
}

/* Modular inverse via Fermat's little theorem: a^(p-2) mod p.
 * Inputs/outputs in Montgomery form.
 *
 * We need to compute a^(p-2) where a is in Montgomery form.
 * Square-and-multiply with Montgomery arithmetic works directly:
 *   if a is Mont(a_plain), then a^k computed via fp_mul / fp_sqr
 *   gives Mont(a_plain^k) (each mul/sqr keeps the Mont form).
 *
 * We scan p-2 from MSB to LSB. p has 381 bits, so p-2 also has 381 bits.
 *
 * Note: BLS_P stored as 12 little-endian limbs; p-2 = BLS_P with limb[0] -= 2.
 * Since BLS_P[0] = 0xffffaaab > 2, no borrow.
 */
void fp_inv(Fp r, const Fp a) {
    if (fp_is_zero(a)) {
        /* By convention 0^{-1} = 0 */
        fp_zero(r);
        return;
    }

    /* Compute exponent p - 2 as 12-limb little-endian array */
    uint32_t exp[FP_LIMBS];
    memcpy(exp, BLS_P, sizeof(exp));
    /* exp -= 2 (BLS_P[0] = 0xffffaaab > 2, so no borrow) */
    exp[0] -= 2;

    /* Square-and-multiply, MSB-first */
    Fp result;
    fp_one(result);  /* start with R mod p (Montgomery form of 1) */

    int started = 0;  /* skip leading zero bits */
    for (int i = FP_LIMBS - 1; i >= 0; i--) {
        for (int b = 31; b >= 0; b--) {
            int bit = (exp[i] >> b) & 1;
            if (started) {
                fp_sqr(result, result);
                if (bit) {
                    fp_mul(result, result, a);
                }
            } else if (bit) {
                /* First 1-bit: result := a (no square needed) */
                fp_copy(result, a);
                started = 1;
            }
        }
    }

    fp_copy(r, result);
}

/* Serialize Fp element to 48 bytes, big-endian (NOT Montgomery — convert first).
 * Wait — by convention here, the input is in Montgomery form, and we
 * serialize the *plain* value as 48 bytes BE (matching standard wire format).
 */
void fp_to_bytes(uint8_t out[48], const Fp a) {
    /* First convert from Montgomery to plain */
    Fp a_plain;
    fp_from_mont(a_plain, a);

    /* Now write big-endian: limb[11] first (most significant) */
    for (int i = 0; i < FP_LIMBS; i++) {
        uint32_t w = a_plain[FP_LIMBS - 1 - i];
        out[4*i + 0] = (uint8_t)(w >> 24);
        out[4*i + 1] = (uint8_t)(w >> 16);
        out[4*i + 2] = (uint8_t)(w >> 8);
        out[4*i + 3] = (uint8_t)(w);
    }
}

/* Deserialize from 48-byte big-endian into Montgomery form. */
void fp_from_bytes(Fp r, const uint8_t in[48]) {
    Fp r_plain;
    /* Read big-endian: most significant 4 bytes go into limb[11] */
    for (int i = 0; i < FP_LIMBS; i++) {
        uint32_t w = ((uint32_t)in[4*i + 0] << 24)
                   | ((uint32_t)in[4*i + 1] << 16)
                   | ((uint32_t)in[4*i + 2] << 8)
                   | ((uint32_t)in[4*i + 3]);
        r_plain[FP_LIMBS - 1 - i] = w;
    }
    /* Convert to Montgomery form */
    fp_to_mont(r, r_plain);
}
