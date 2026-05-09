#pragma once
#include "fp.h"

/* Fp2 = Fp[u] / (u^2 + 1).
 * Element a = c0 + c1*u.  Non-residue for Fp2 is -1 (so u^2 = -1).
 * Non-residue for Fp6-over-Fp2 (used in fp12.c) is xi = 1 + u for BLS12-381.
 *   (BN254 used xi = 9 + u; this is one of the structural differences.)
 */
typedef struct { Fp c0, c1; } Fp2;

void fp2_zero(Fp2 *r);
void fp2_one(Fp2 *r);
void fp2_copy(Fp2 *r, const Fp2 *a);
int  fp2_is_zero(const Fp2 *a);
int  fp2_eq(const Fp2 *a, const Fp2 *b);

void fp2_add(Fp2 *r, const Fp2 *a, const Fp2 *b);
void fp2_sub(Fp2 *r, const Fp2 *a, const Fp2 *b);
void fp2_neg(Fp2 *r, const Fp2 *a);

/* Karatsuba multiplication: (a0+a1u)(b0+b1u) = (a0b0-a1b1) + (a0b1+a1b0)u */
void fp2_mul(Fp2 *r, const Fp2 *a, const Fp2 *b);
void fp2_sqr(Fp2 *r, const Fp2 *a);
void fp2_inv(Fp2 *r, const Fp2 *a);

/* Multiply by the Fp6 non-residue xi = 1 + u (BLS12-381 specific):
 *   (a0+a1u)(1+u) = (a0 - a1) + (a0 + a1)u
 */
void fp2_mul_xi(Fp2 *r, const Fp2 *a);

/* Serialise as 96 bytes (48 per Fp coefficient, big-endian). */
void fp2_to_bytes(uint8_t out[96], const Fp2 *a);
void fp2_from_bytes(Fp2 *r, const uint8_t in[96]);
