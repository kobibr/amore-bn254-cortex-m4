#pragma once
#include "fp2.h"

/* G2 points on y^2 = x^3 + 4(1+u)  (BLS12-381 twisted curve, M-type twist).
 * Note: BN254 used 3/(9+u) (D-type twist). This is one of the structural
 * differences between BN254 and BLS12-381.
 * Jacobian: (X:Y:Z) -> affine (X/Z^2, Y/Z^3). */
typedef struct { Fp2 X, Y, Z; } G2Point;

void g2_inf(G2Point *r);
int  g2_is_inf(const G2Point *p);
void g2_generator(G2Point *r);
void g2_neg(G2Point *r, const G2Point *a);
void g2_dbl(G2Point *r, const G2Point *a);
void g2_add(G2Point *r, const G2Point *a, const G2Point *b);

/* r = k * P  (max 255 bits for BLS12-381 scalar field) */
void g2_scalar_mul(G2Point *r, const G2Point *p, const uint32_t k[8], int nbits);

void g2_to_affine(Fp2 *rx, Fp2 *ry, const G2Point *p);

/* Serialise: 192 bytes = 4 × 48 bytes (X.c0, X.c1, Y.c0, Y.c1, big-endian raw) */
void g2_to_bytes(uint8_t out[G2_BYTES], const G2Point *p);
void g2_from_bytes(G2Point *r, const uint8_t in[G2_BYTES]);
