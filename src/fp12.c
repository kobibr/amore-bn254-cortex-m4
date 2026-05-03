#include "fp12.h"
#include <string.h>

/*
 * Fp12  —  degree-12 extension of Fp with modulus  x^12 - 18*x^6 + 82 = 0
 *
 * This representation is IDENTICAL to py_ecc's bn128 FQ12, so GT elements
 * serialised by the Python server are directly usable by this C code.
 *
 * Element: a[0] + a[1]*x + ... + a[11]*x^11  (12 Fp coefficients)
 *
 * Reduction rule from  x^12 = 18*x^6 - 82:
 *   For k >= 12:  coeff[k] contributes  +18*coeff[k-6]  and  -82*coeff[k-12]
 */

/* ── Fp12 element stored as 12 Fp coefficients ── */
/* Fp12 struct (from fp12.h): c[2] are Fp6, each Fp6 has c[3] Fp2, each Fp2 has c0,c1 Fp
 * LAYOUT: c[0].c[0].c0 = coeff[0], c[0].c[0].c1 = coeff[1], ... c[1].c[2].c1 = coeff[11]
 * Total: 12 Fp values — maps directly to the polynomial coefficients a[0..11].
 * We access them via a cast to a flat Fp array. */

typedef Fp Fp12Flat[12];

static inline Fp12Flat *fp12_flat(Fp12 *a) {
    return (Fp12Flat *)a;
}
static inline const Fp12Flat *fp12_flat_c(const Fp12 *a) {
    return (const Fp12Flat *)a;
}

void fp12_zero(Fp12 *r) { memset(r, 0, sizeof(Fp12)); }

void fp12_one(Fp12 *r) {
    memset(r, 0, sizeof(Fp12));
    fp_one((*fp12_flat(r))[0]);   /* coefficient of x^0 = 1 */
}

void fp12_copy(Fp12 *r, const Fp12 *a) { memcpy(r, a, sizeof(Fp12)); }

int fp12_eq(const Fp12 *a, const Fp12 *b) {
    const Fp12Flat *fa = fp12_flat_c(a);
    const Fp12Flat *fb = fp12_flat_c(b);
    for (int i = 0; i < 12; i++)
        if (!fp_eq((*fa)[i], (*fb)[i])) return 0;
    return 1;
}

/*
 * fp12_mul: schoolbook multiplication of two degree-11 polynomials modulo
 *           x^12 - 18*x^6 + 82, all coefficients in Fp.
 *
 * Step 1: compute unreduced product (degree 22), cost = 144 fp_mul + ~132 fp_add
 * Step 2: reduce coefficients 22..12 using x^12 = 18*x^6 - 82
 */
void fp12_mul(Fp12 *r, const Fp12 *a, const Fp12 *b) {
    const Fp12Flat *fa = fp12_flat_c(a);
    const Fp12Flat *fb = fp12_flat_c(b);

    /* Temporary 23-coefficient product (indices 0..22) */
    Fp tmp[23];
    for (int i = 0; i < 23; i++) fp_zero(tmp[i]);

    /* Accumulate a[i]*b[j] into tmp[i+j] */
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 12; j++) {
            Fp t;
            fp_mul(t, (*fa)[i], (*fb)[j]);
            fp_add(tmp[i+j], tmp[i+j], t);
        }
    }

    /*
     * Reduce: for k = 22 down to 12
     *   tmp[k-6]  += 18 * tmp[k]      (from x^k = x^{k-12} * 18x^6)
     *   tmp[k-12] -= 82 * tmp[k]
     *   tmp[k] = 0
     *
     * Multiply by 18: 18*t = 16*t + 2*t  (3 fp_add each)
     * Multiply by 82: 82*t = 64*t + 16*t + 2*t  (4 fp_add each)
     */
    for (int k = 22; k >= 12; k--) {
        Fp c;
        fp_copy(c, tmp[k]);
        if (fp_is_zero(c)) continue;

        /* tmp[k-6] += 18*c */
        Fp t18;
        fp_add(t18, c, c);          /* 2c */
        {
            Fp t4; fp_add(t4,t18,t18);   /* 4c */
            Fp t16; fp_add(t16,t4,t4); fp_add(t16,t16,t16); /* 16c */
            fp_add(t18, t18, t16);  /* 18c = 2c + 16c */
        }
        fp_add(tmp[k-6], tmp[k-6], t18);

        /* tmp[k-12] -= 82*c  (82 = 64+16+2) */
        Fp t2, t16, t64, t82;
        fp_add(t2,  c,   c);            /* 2 */
        fp_add(t16, t2,  t2);
        fp_add(t16, t16, t16);
        fp_add(t16, t16, t16);          /* 16 */
        fp_add(t64, t16, t16);
        fp_add(t64, t64, t64);          /* 64 */
        fp_add(t82, t64, t16);
        fp_add(t82, t82, t2);           /* 82 */
        fp_sub(tmp[k-12], tmp[k-12], t82);

        fp_zero(tmp[k]);
    }

    /* Copy result */
    Fp12Flat *fr = fp12_flat(r);
    for (int i = 0; i < 12; i++) fp_copy((*fr)[i], tmp[i]);
}

void fp12_sqr(Fp12 *r, const Fp12 *a) { fp12_mul(r, a, a); }

/*
 * fp12_inv: a^{-1} using Fermat: a^{p^12 - 1 - 1} = a^{-1} mod (poly, p)
 * For BN128: the GT group has order q. We use the identity
 *   a^{-1} = a^{q-2}  if a is in the subgroup of order q — but GT
 * elements from pairings live in the order-q subgroup, so
 *   a^{-1} = (conjugate) ... complex.
 * Simpler: use the norm-based formula or just fp12_exp with (p^12-2).
 * For our use case (AmorE verify) we never actually call fp12_inv on GT,
 * so we provide a placeholder that computes via iterative extended Euclidean.
 * In practice only fp12_exp is used.
 */
void fp12_inv(Fp12 *r, const Fp12 *a) {
    /* Exponentiation: a^{q-2} works for elements of order q (subgroup GT) */
    static const uint32_t qminus2[8] = {
        0xefffffffu - 1, 0x43e1f593, 0x79b97091, 0x2833e848,
        0x8181585d, 0xb85045b6, 0xe131a029, 0x30644e72};
    fp12_exp(r, a, qminus2, 254);
}

/*
 * fp12_exp: binary square-and-multiply.
 * r = base^k,  k given as 8×uint32_t LE limbs, nbits = significant bits.
 */
void fp12_exp(Fp12 *r, const Fp12 *base, const uint32_t k[8], int nbits) {
    Fp12 result, cur;
    fp12_one(&result);
    fp12_copy(&cur, base);

    for (int w = 0; w < 8; w++) {
        uint32_t limb = k[w];
        int bits_in_limb = (w == nbits / 32) ? (nbits % 32) : 32;
        if (w * 32 >= nbits) break;
        for (int b = 0; b < bits_in_limb; b++) {
            if (limb & 1u) fp12_mul(&result, &result, &cur);
            fp12_sqr(&cur, &cur);
            limb >>= 1;
        }
    }
    fp12_copy(r, &result);
}

/* Serialise: 12 × 32 bytes, each coefficient big-endian.
 * Layout: [coeff[0], coeff[1], ..., coeff[11]] — matches py_ecc flat12() exactly. */
void fp12_to_bytes(uint8_t out[384], const Fp12 *a) {
    const Fp12Flat *fa = fp12_flat_c(a);
    for (int i = 0; i < 12; i++)
        fp_to_bytes(out + i*32, (*fa)[i]);
}

void fp12_from_bytes(Fp12 *r, const uint8_t in[384]) {
    Fp12Flat *fr = fp12_flat(r);
    for (int i = 0; i < 12; i++)
        fp_from_bytes((*fr)[i], in + i*32);
}
