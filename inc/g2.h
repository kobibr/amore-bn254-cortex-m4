#pragma once
#include "fp2.h"

/* G2 points on y^2 = x^3 + b2  where b2 = 3/(9+u) in Fp2.
 * BN128 twisted curve (D-type twist).
 * Jacobian: (X:Y:Z) → affine (X/Z^2, Y/Z^3). */
typedef struct { Fp2 X, Y, Z; } G2Point;

void g2_inf(G2Point *r);
int  g2_is_inf(const G2Point *p);
void g2_generator(G2Point *r);
void g2_neg(G2Point *r, const G2Point *a);
void g2_dbl(G2Point *r, const G2Point *a);
void g2_add(G2Point *r, const G2Point *a, const G2Point *b);

/* r = k * P */
void g2_scalar_mul(G2Point *r, const G2Point *p, const uint32_t k[8], int nbits);

void g2_to_affine(Fp2 *rx, Fp2 *ry, const G2Point *p);

/* Serialise: 128 bytes = 4 × 32 bytes (X.c0, X.c1, Y.c0, Y.c1, big-endian raw) */
void g2_to_bytes(uint8_t out[128], const G2Point *p);
void g2_from_bytes(G2Point *r, const uint8_t in[128]);
