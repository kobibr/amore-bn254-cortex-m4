#pragma once
#include "fp.h"
#include "fp2.h"   /* still needed by g2.c for Fp2 ops */

/*
 * Fp12 — degree-12 extension of Fp via  x^12 - 2*x^6 + 2 = 0  (BLS12-381)
 *
 * SAME representation as py_ecc bn128.FQ12:
 *   element = a[0] + a[1]*x + ... + a[11]*x^11
 *
 * LAYOUT IN MEMORY: 12 consecutive Fp values (each = uint32_t[8]).
 * The struct below uses a 2×Fp6×Fp2 hierarchy only to keep struct size
 * and flat-cast compatible; arithmetic uses the flat poly representation.
 *
 * Serialisation: fp12_to_bytes writes [coeff[0]..coeff[11]], 48 bytes each,
 * big-endian — identical to py_ecc fp12_to_bytes(elem).
 */

typedef struct { Fp c0, c1; }       Fp2_unused;  /* Fp2 still used by g2.c */
typedef struct { Fp2 c[3]; }        Fp6;
typedef struct { Fp6 c[2]; }        Fp12;

/* Total size: 12 Fp = 12 × 48 = 576 bytes — indices 0..11 accessible
 * as ((Fp*)fp12_ptr)[0..11] */

void fp12_zero(Fp12 *r);
void fp12_one (Fp12 *r);
void fp12_copy(Fp12 *r, const Fp12 *a);
int  fp12_eq  (const Fp12 *a, const Fp12 *b);

void fp12_mul(Fp12 *r, const Fp12 *a, const Fp12 *b);
void fp12_sqr(Fp12 *r, const Fp12 *a);
void fp12_inv(Fp12 *r, const Fp12 *a);
void fp12_exp(Fp12 *r, const Fp12 *base, const uint32_t k[8], int nbits);

void fp12_to_bytes  (uint8_t out[FP12_BYTES], const Fp12 *a);
void fp12_from_bytes(Fp12 *r, const uint8_t in[FP12_BYTES]);
