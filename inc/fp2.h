#pragma once
#include "fp.h"

/* Fp2 = Fp[u] / (u^2 + 1).
 * Element a = c0 + c1*u.  Non-residue for Fp2 is -1.
 * Non-residue for Fp6-over-Fp2 is xi = 9 + u  (used in fp6.c). */
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

/* Multiply by the Fp6 non-residue xi = 9 + u:
 * (a0+a1u)(9+u) = (9a0 - a1) + (a0 + 9a1)u */
void fp2_mul_xi(Fp2 *r, const Fp2 *a);

void fp2_to_bytes(uint8_t out[64], const Fp2 *a);
void fp2_from_bytes(Fp2 *r, const uint8_t in[64]);
