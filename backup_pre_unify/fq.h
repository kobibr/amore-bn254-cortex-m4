#pragma once
#include <stdint.h>

/* Scalar field Zq where q = group order of BN128.
 * Used for secret scalars s, u, r.
 * Elements stored in Montgomery form (8 × uint32_t little-endian). */
typedef uint32_t Fq[8];

void fq_zero(Fq r);
void fq_one(Fq r);
void fq_copy(Fq r, const Fq a);
int  fq_is_zero(const Fq a);

void fq_add(Fq r, const Fq a, const Fq b);
void fq_sub(Fq r, const Fq a, const Fq b);
void fq_neg(Fq r, const Fq a);
void fq_mul(Fq r, const Fq a, const Fq b);
void fq_inv(Fq r, const Fq a);     /* Fermat a^{q-2} mod q */

/* Convert raw uint32_t[8] (little-endian, < q) → Montgomery form */
void fq_from_limbs(Fq r, const uint32_t limbs[8]);
/* Convert raw bytes (big-endian, < q) → Montgomery form */
void fq_from_bytes(Fq r, const uint8_t in[32]);

/* Get limbs out of Montgomery form (for use as scalar exponent) */
void fq_to_limbs(uint32_t out[8], const Fq a);
