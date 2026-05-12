#pragma once
#include "curve.h"
#include <stdint.h>
#include "bls12_381_const.h"

/* 384-bit field element mod p (BLS12-381 field prime, 381 bits actual).
 * Stored in Montgomery form:  stored_value = real_value * R mod p
 * where R = 2^384.  12 × uint32_t little-endian limbs. */


typedef uint32_t Fp[FP_LIMBS];

void fp_zero(Fp r);
void fp_one(Fp r);           /* = R mod p (Montgomery form of 1) */
void fp_copy(Fp r, const Fp a);
int  fp_is_zero(const Fp a);
int  fp_eq(const Fp a, const Fp b);

/* Arithmetic in Montgomery form */
void fp_add(Fp r, const Fp a, const Fp b);
void fp_sub(Fp r, const Fp a, const Fp b);
void fp_neg(Fp r, const Fp a);
void fp_mul(Fp r, const Fp a, const Fp b);   /* CIOS Montgomery mul */
void fp_sqr(Fp r, const Fp a);               /* same as mul(r,a,a) */
void fp_inv(Fp r, const Fp a);               /* Fermat a^{p-2} */

/* Conversion helpers */
void fp_from_u32(Fp r, uint32_t x);          /* r = x * R mod p (into Mont.) */
void fp_to_mont(Fp r, const Fp a_plain);     /* r = a * R mod p  */
void fp_from_mont(Fp r, const Fp a_mont);    /* r = a * R^{-1} mod p */

/* Serialise / deserialise (48 bytes big-endian, raw field value, NOT Montgomery) */
void fp_to_bytes(uint8_t out[FP_BYTES], const Fp a);
void fp_from_bytes(Fp r, const uint8_t in[FP_BYTES]);
