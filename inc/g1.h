#pragma once
#include "fp.h"

/* G1 points on y^2 = x^3 + 4  (BLS12-381 base curve, a=0, b=4).
 * Same curve equation as BN254 — only the field changed.
 * Jacobian coordinates: (X:Y:Z) -> affine (X/Z^2, Y/Z^3).
 * Infinity point = (1:1:0). */
typedef struct { Fp X, Y, Z; } G1Point;

/* Point at infinity */
void g1_inf(G1Point *r);
int  g1_is_inf(const G1Point *p);

/* Load the BLS12-381 G1 generator (RFC draft-irtf-cfrg-pairing-friendly-curves) */
void g1_generator(G1Point *r);

/* Arithmetic */
void g1_neg(G1Point *r, const G1Point *a);
void g1_dbl(G1Point *r, const G1Point *a);
void g1_add(G1Point *r, const G1Point *a, const G1Point *b);
void g1_add_mixed(G1Point *r, const G1Point *a, const Fp bx, const Fp by);

/* r = k * P  (k as 8 uint32_t LE limbs, nbits significant bits, max 255 for BLS12-381) */
void g1_scalar_mul(G1Point *r, const G1Point *p, const uint32_t k[8], int nbits);

/* Convert to affine */
void g1_to_affine(Fp rx, Fp ry, const G1Point *p);

/* Serialise to/from 96 bytes raw big-endian (plain field, NOT Montgomery) */
void g1_to_bytes(uint8_t out[96], const G1Point *p);
void g1_from_bytes(G1Point *r, const uint8_t in[96]);
