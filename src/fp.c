#include "fp.h"
#include "bn128_const.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * CIOS Montgomery multiplication: r = a * b * R^{-1} mod p
 * Reference: Koç, Acar, Kaliski 1996 "Analyzing and Comparing Montgomery
 * Multiplication Algorithms".
 * ----------------------------------------------------------------------- */

/* Compare a[8] vs b[8]; return -1/0/+1 for a<b/a==b/a>b */
static int fp_cmp(const uint32_t a[8], const uint32_t b[8]) {
    for (int i = 7; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

/* r = a - b, assuming a >= b.  Returns borrow (should be 0). */
static uint32_t fp_raw_sub(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
        r[i]   = (uint32_t)x;
        borrow = (x >> 63) & 1;
    }
    return (uint32_t)borrow;
}

/* r = a + b mod p  (no Montgomery) */
static void fp_raw_add_mod(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)a[i] + b[i] + carry;
        r[i]  = (uint32_t)x;
        carry = x >> 32;
    }
    /* carry may be 1 OR result may be >= p even without carry */
    if (carry || fp_cmp(r, BN128_P) >= 0)
        fp_raw_sub(r, r, BN128_P);
}

/* Core CIOS step – accumulates a[8]*scalar into T[9], then one Montgomery step */
static void cios_mul(uint32_t r[8], const uint32_t a[8], const uint32_t b[8],
                     const uint32_t m[8], uint32_t mp)
{
    uint64_t T[9] = {0};

    for (int i = 0; i < 8; i++) {
        /* Step 1: T += a * b[i] */
        uint64_t carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[8] += carry;

        /* Step 2: Montgomery reduction by one word */
        uint32_t k = (uint32_t)T[0] * mp;
        carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t x = T[j] + (uint64_t)m[j] * k + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[8] += carry;

        /* Shift T right by one word */
        for (int j = 0; j < 8; j++) T[j] = T[j + 1];
        T[8] = 0;
    }

    /* Conditional final subtraction */
    uint32_t tmp[8];
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)T[i] - m[i] - borrow;
        tmp[i]  = (uint32_t)x;
        borrow  = (x >> 63) & 1;
    }
    /* Use tmp if T >= m (no borrow) */
    /* IMPORTANT: T is uint64_t[9]. After CIOS each T[i] < 2^32.
     * Cast to uint32_t* would interleave lo/hi halves — WRONG.
     * Must copy T[i] individually as uint32_t. */
    if (borrow) {
        for (int i = 0; i < 8; i++) r[i] = (uint32_t)T[i];
    } else {
        for (int i = 0; i < 8; i++) r[i] = tmp[i];
    }
}

/* ---- Public API -------------------------------------------------------- */

void fp_zero(Fp r) { memset(r, 0, 32); }

void fp_one(Fp r)  { memcpy(r, BN128_R_FP, 32); }   /* 1 in Mont = R mod p */

void fp_copy(Fp r, const Fp a)  { memcpy(r, a, 32); }

int fp_is_zero(const Fp a) {
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) acc |= a[i];
    return acc == 0;
}

int fp_eq(const Fp a, const Fp b) {
    return memcmp(a, b, 32) == 0;
}

void fp_mul(Fp r, const Fp a, const Fp b) {
    cios_mul(r, a, b, BN128_P, BN128_N0P_FP);
}

void fp_sqr(Fp r, const Fp a) {
    cios_mul(r, a, a, BN128_P, BN128_N0P_FP);
}

void fp_add(Fp r, const Fp a, const Fp b) {
    fp_raw_add_mod(r, a, b);
}

void fp_sub(Fp r, const Fp a, const Fp b) {
    if (fp_cmp(a, b) < 0) {
        /* a < b → r = p - (b - a) = p + a - b */
        uint32_t tmp[8];
        fp_raw_sub(tmp, b, a);
        fp_raw_sub(r, BN128_P, tmp);
    } else {
        fp_raw_sub(r, a, b);
    }
}

void fp_neg(Fp r, const Fp a) {
    if (fp_is_zero(a)) { fp_zero(r); return; }
    fp_raw_sub(r, BN128_P, a);
}

/* r = a * R^{-1} mod p  (convert out of Montgomery) */
void fp_from_mont(Fp r, const Fp a_mont) {
    /* Multiply by 1 in standard form (not Mont.) = [1, 0, 0, ...] */
    static const uint32_t one_plain[8] = {1,0,0,0,0,0,0,0};
    cios_mul(r, a_mont, one_plain, BN128_P, BN128_N0P_FP);
}

/* r = a * R mod p  (convert into Montgomery) */
void fp_to_mont(Fp r, const Fp a_plain) {
    cios_mul(r, a_plain, BN128_R2_FP, BN128_P, BN128_N0P_FP);
}

/* r = x * R mod p  (small integer x into Montgomery form) */
void fp_from_u32(Fp r, uint32_t x) {
    Fp tmp = {x, 0,0,0,0,0,0,0};
    cios_mul(r, tmp, BN128_R2_FP, BN128_P, BN128_N0P_FP);
}

/* Inversion: r = a^{p-2} mod p using Fermat's little theorem.
 * Uses binary square-and-multiply on the exponent p-2.
 * p-2 has 254 significant bits. */
void fp_inv(Fp r, const Fp a) {
    /* exponent = p - 2 */
    static const uint32_t exp[8] = {
        0xd87cfd45,0x3c208c16,0x6871ca8d,0x97816a91,
        0x8181585d,0xb85045b6,0xe131a029,0x30644e72};

    Fp base, result;
    fp_copy(base, a);
    fp_one(result);

    for (int w = 0; w < 8; w++) {
        uint32_t limb = exp[w];
        for (int b = 0; b < 32; b++) {
            if (limb & 1u) fp_mul(result, result, base);
            fp_sqr(base, base);
            limb >>= 1;
        }
    }
    fp_copy(r, result);
}

/* Serialise to 32-byte big-endian (raw field value, not Montgomery) */
void fp_to_bytes(uint8_t out[32], const Fp a) {
    Fp raw;
    fp_from_mont(raw, a);
    for (int i = 0; i < 8; i++) {
        uint32_t limb = raw[7 - i];   /* big-endian limb order */
        out[4*i+0] = (uint8_t)(limb >> 24);
        out[4*i+1] = (uint8_t)(limb >> 16);
        out[4*i+2] = (uint8_t)(limb >>  8);
        out[4*i+3] = (uint8_t)(limb      );
    }
}

/* Deserialise from 32-byte big-endian (raw value → Montgomery form) */
void fp_from_bytes(Fp r, const uint8_t in[32]) {
    uint32_t tmp[8];
    for (int i = 0; i < 8; i++) {
        tmp[7-i] = ((uint32_t)in[4*i+0] << 24)
                 | ((uint32_t)in[4*i+1] << 16)
                 | ((uint32_t)in[4*i+2] <<  8)
                 | ((uint32_t)in[4*i+3]      );
    }
    fp_to_mont(r, tmp);
}
