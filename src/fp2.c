#include "curve.h"
#include "fp2.h"
#include <string.h>

void fp2_zero(Fp2 *r) { fp_zero(r->c0); fp_zero(r->c1); }
void fp2_one (Fp2 *r) { fp_one (r->c0); fp_zero(r->c1); }
void fp2_copy(Fp2 *r, const Fp2 *a) { fp_copy(r->c0, a->c0); fp_copy(r->c1, a->c1); }

int fp2_is_zero(const Fp2 *a) { return fp_is_zero(a->c0) && fp_is_zero(a->c1); }
int fp2_eq(const Fp2 *a, const Fp2 *b) { return fp_eq(a->c0, b->c0) && fp_eq(a->c1, b->c1); }

void fp2_add(Fp2 *r, const Fp2 *a, const Fp2 *b) {
    fp_add(r->c0, a->c0, b->c0);
    fp_add(r->c1, a->c1, b->c1);
}
void fp2_sub(Fp2 *r, const Fp2 *a, const Fp2 *b) {
    fp_sub(r->c0, a->c0, b->c0);
    fp_sub(r->c1, a->c1, b->c1);
}
void fp2_neg(Fp2 *r, const Fp2 *a) {
    fp_neg(r->c0, a->c0);
    fp_neg(r->c1, a->c1);
}

/* Karatsuba multiplication:
 *   v0 = a0*b0,  v1 = a1*b1
 *   r0 = v0 - v1            (since u^2 = -1, so a1*b1*u^2 = -a1*b1)
 *   r1 = (a0+a1)*(b0+b1) - v0 - v1
 */
void fp2_mul(Fp2 *r, const Fp2 *a, const Fp2 *b) {
    Fp v0, v1, t0, t1, t2;
    fp_mul(v0, a->c0, b->c0);
    fp_mul(v1, a->c1, b->c1);
    fp_add(t0, a->c0, a->c1);
    fp_add(t1, b->c0, b->c1);
    fp_mul(t2, t0, t1);
    fp_sub(t2, t2, v0); fp_sub(t2, t2, v1);  /* r1 = t2 - v0 - v1 */
    fp_sub(v0, v0, v1);                        /* r0 = v0 - v1     */
    fp_copy(r->c0, v0);
    fp_copy(r->c1, t2);
}

/* Complex squaring:
 *   r0 = (a0+a1)(a0-a1) = a0^2 - a1^2
 *   r1 = 2*a0*a1
 */
void fp2_sqr(Fp2 *r, const Fp2 *a) {
    Fp t0, t1;
    fp_add(t0, a->c0, a->c1);    /* t0 = a0+a1 */
    fp_sub(t1, a->c0, a->c1);    /* t1 = a0-a1 */
    fp_mul(r->c1, a->c0, a->c1);
    fp_add(r->c1, r->c1, r->c1); /* r1 = 2*a0*a1 */
    fp_mul(r->c0, t0, t1);        /* r0 = (a0+a1)(a0-a1) = a0^2 - a1^2 */
}

/* Inversion: 1/(a0+a1*u) = (a0-a1*u)/(a0^2+a1^2) */
void fp2_inv(Fp2 *r, const Fp2 *a) {
    Fp n0, n1, d;
    fp_sqr(n0, a->c0);
    fp_sqr(n1, a->c1);
    fp_add(d, n0, n1);              /* norm = a0^2 + a1^2 */
    fp_inv(d, d);                    /* d = 1/norm */
    fp_mul(r->c0, a->c0, d);
    fp_neg(n1, a->c1);
    fp_mul(r->c1, n1, d);
}

/* Multiply by xi = 1 + u (BLS12-381 Fp6 non-residue):
 *   (a0 + a1*u) * (1 + u) = (a0 + a1*u) + (a0 + a1*u)*u
 *                         = a0 + a1*u + a0*u + a1*u^2
 *                         = (a0 - a1) + (a0 + a1)*u    [using u^2 = -1]
 *
 * Note: this is much simpler than BN254's xi = 9 + u.
 */
void fp2_mul_xi(Fp2 *r, const Fp2 *a) {
    Fp tmp_c0, tmp_c1;
    fp_sub(tmp_c0, a->c0, a->c1);    /* a0 - a1 */
    fp_add(tmp_c1, a->c0, a->c1);    /* a0 + a1 */
    fp_copy(r->c0, tmp_c0);
    fp_copy(r->c1, tmp_c1);
}

/* Serialise: 96 bytes total = 48 (c0) + 48 (c1), big-endian. */
void fp2_to_bytes(uint8_t out[FP2_BYTES], const Fp2 *a) {
    fp_to_bytes(out,      a->c0);
    fp_to_bytes(out + FP_BYTES, a->c1);
}
void fp2_from_bytes(Fp2 *r, const uint8_t in[FP2_BYTES]) {
    fp_from_bytes(r->c0, in);
    fp_from_bytes(r->c1, in + FP_BYTES);
}
