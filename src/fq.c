#include "fq.h"
#include "bn128_const.h"
#include <string.h>

static int fq_cmp(const uint32_t a[8], const uint32_t b[8]) {
    for (int i = 7; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

static uint32_t fq_raw_sub(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
        r[i]   = (uint32_t)x;
        borrow = (x >> 63) & 1;
    }
    return (uint32_t)borrow;
}

static void fq_raw_add_mod(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint64_t carry = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)a[i] + b[i] + carry;
        r[i]  = (uint32_t)x;
        carry = x >> 32;
    }
    if (carry || fq_cmp(r, BN128_Q) >= 0)
        fq_raw_sub(r, r, BN128_Q);
}

static void cios_fq(uint32_t r[8], const uint32_t a[8], const uint32_t b[8]) {
    uint64_t T[9] = {0};
    for (int i = 0; i < 8; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[8] += carry;

        uint32_t k = (uint32_t)T[0] * BN128_N0P_FQ;
        carry = 0;
        for (int j = 0; j < 8; j++) {
            uint64_t x = T[j] + (uint64_t)BN128_Q[j] * k + carry;
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
        }
        T[8] += carry;

        for (int j = 0; j < 8; j++) T[j] = T[j+1];
        T[8] = 0;
    }
    uint32_t tmp[8]; uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)T[i] - BN128_Q[i] - borrow;
        tmp[i]  = (uint32_t)x;
        borrow  = (x >> 63) & 1;
    }
    if (borrow) {
        for (int i = 0; i < 8; i++) r[i] = (uint32_t)T[i];
    } else {
        for (int i = 0; i < 8; i++) r[i] = tmp[i];
    }
}

void fq_zero(Fq r) { memset(r, 0, 32); }

void fq_one(Fq r)  { memcpy(r, BN128_R_FQ, 32); }

void fq_copy(Fq r, const Fq a) { memcpy(r, a, 32); }

int fq_is_zero(const Fq a) {
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) acc |= a[i];
    return acc == 0;
}

void fq_mul(Fq r, const Fq a, const Fq b) { cios_fq(r, a, b); }

void fq_add(Fq r, const Fq a, const Fq b) { fq_raw_add_mod(r, a, b); }

void fq_sub(Fq r, const Fq a, const Fq b) {
    if (fq_cmp(a, b) < 0) {
        uint32_t tmp[8];
        fq_raw_sub(tmp, b, a);
        fq_raw_sub(r, BN128_Q, tmp);
    } else {
        fq_raw_sub(r, a, b);
    }
}

void fq_neg(Fq r, const Fq a) {
    if (fq_is_zero(a)) { fq_zero(r); return; }
    fq_raw_sub(r, BN128_Q, a);
}

/* Inversion using Fermat: a^{q-2} */
void fq_inv(Fq r, const Fq a) {
    static const uint32_t exp[8] = {  /* q - 2 */
        0xefffffffu, 0x43e1f593,0x79b97091,0x2833e848,
        0x8181585d,0xb85045b6,0xe131a029,0x30644e72};
    Fq base, result;
    fq_copy(base, a);
    fq_one(result);
    for (int w = 0; w < 8; w++) {
        uint32_t limb = exp[w];
        for (int b = 0; b < 32; b++) {
            if (limb & 1u) cios_fq(result, result, base);
            cios_fq(base, base, base);
            limb >>= 1;
        }
    }
    fq_copy(r, result);
}

/* raw uint32_t[8] plain → Montgomery */
void fq_from_limbs(Fq r, const uint32_t limbs[8]) {
    cios_fq(r, limbs, BN128_R2_FQ);
}

void fq_from_bytes(Fq r, const uint8_t in[32]) {
    uint32_t tmp[8];
    for (int i = 0; i < 8; i++) {
        tmp[7-i] = ((uint32_t)in[4*i+0]<<24)|((uint32_t)in[4*i+1]<<16)
                  |((uint32_t)in[4*i+2]<< 8)|((uint32_t)in[4*i+3]     );
    }
    fq_from_limbs(r, tmp);
}

void fq_to_limbs(uint32_t out[8], const Fq a) {
    static const uint32_t one_plain[8] = {1,0,0,0,0,0,0,0};
    cios_fq(out, a, one_plain);
}
