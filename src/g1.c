#include "g1.h"
#include "bn128_const.h"
#include <string.h>

/* b = 4 in Montgomery form */
static void g1_b(Fp r) { fp_from_u32(r, 4); }

void g1_inf(G1Point *r) {
    fp_one(r->X); fp_one(r->Y); fp_zero(r->Z);
}
int g1_is_inf(const G1Point *p) { return fp_is_zero(p->Z); }

void g1_generator(G1Point *r) {
    memcpy(r->X, BN128_G1X, 32);
    memcpy(r->Y, BN128_G1Y, 32);
    fp_one(r->Z);
}

void g1_neg(G1Point *r, const G1Point *a) {
    fp_copy(r->X, a->X);
    fp_neg(r->Y, a->Y);
    fp_copy(r->Z, a->Z);
}

/* Jacobian doubling (a=0, i.e. BN128):
 *   A = X^2,  B = Y^2,  C = B^2
 *   D = 2*((X+B)^2 - A - C)
 *   E = 3*A
 *   X' = E^2 - 2*D
 *   Y' = E*(D - X') - 8*C
 *   Z' = 2*Y*Z
 * Ref: hyperelliptic.org EFD/g1p/auto-shortw-jacobian-0 #doubling-dbl-2009-l */
void g1_dbl(G1Point *r, const G1Point *a) {
    /* Jacobian doubling for a=0 (BN128): dbl-2009-l
     * https://hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#doubling-dbl-2009-l */
    if (g1_is_inf(a)) { g1_inf(r); return; }
    Fp A, B, C, D, E, F;

    fp_sqr(A, a->X);                     /* A = X1^2          */
    fp_sqr(B, a->Y);                     /* B = Y1^2          */
    fp_sqr(C, B);                        /* C = B^2           */
    {
        Fp tmp; fp_add(tmp, a->X, B);
        fp_sqr(D, tmp);
        fp_sub(D, D, A); fp_sub(D, D, C);
        fp_add(D, D, D);                 /* D = 2*((X1+B)^2 - A - C) */
    }
    fp_add(E, A, A); fp_add(E, E, A);   /* E = 3*A           */
    fp_sqr(F, E);                        /* F = E^2           */
    {
        Fp twiceD; fp_add(twiceD, D, D);
        fp_sub(r->X, F, twiceD);         /* X' = F - 2*D      */
    }
    {
        Fp eightC, dm;
        fp_add(eightC,C,C); fp_add(eightC,eightC,eightC); fp_add(eightC,eightC,eightC);
        fp_sub(dm, D, r->X);
        fp_mul(r->Y, E, dm);
        fp_sub(r->Y, r->Y, eightC);     /* Y' = E*(D-X') - 8C */
    }
    fp_mul(r->Z, a->Y, a->Z);
    fp_add(r->Z, r->Z, r->Z);           /* Z' = 2*Y1*Z1      */
}

/* Jacobian + Jacobian addition
 * Ref: EFD/g1p/auto-shortw-jacobian-0 #addition-add-2007-bl */
void g1_add(G1Point *r, const G1Point *a, const G1Point *b) {
    if (g1_is_inf(a)) { *r = *b; return; }
    if (g1_is_inf(b)) { *r = *a; return; }

    Fp Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, L, V;
    fp_sqr(Z1Z1, a->Z);
    fp_sqr(Z2Z2, b->Z);
    fp_mul(U1, a->X, Z2Z2);
    fp_mul(U2, b->X, Z1Z1);
    fp_mul(S1, a->Y, b->Z);  fp_mul(S1, S1, Z2Z2);
    fp_mul(S2, b->Y, a->Z);  fp_mul(S2, S2, Z1Z1);

    /* If U1==U2: either point doubling or point at infinity */
    if (fp_eq(U1, U2)) {
        if (fp_eq(S1, S2)) { g1_dbl(r, a); return; }
        g1_inf(r); return;
    }

    fp_sub(H, U2, U1);
    fp_add(I, H, H);  fp_sqr(I, I);       /* I = (2H)^2 */
    fp_mul(J, H, I);                        /* J = H*I */
    fp_sub(L, S2, S1); fp_add(L, L, L);    /* L = 2*(S2-S1) = r_coeff */
    fp_mul(V, U1, I);
    /* X3 = L^2 - J - 2V */
    fp_sqr(r->X, L);
    fp_sub(r->X, r->X, J);
    fp_sub(r->X, r->X, V);
    fp_sub(r->X, r->X, V);
    /* Y3 = L*(V - X3) - 2*S1*J */
    fp_sub(V, V, r->X);
    fp_mul(r->Y, L, V);
    fp_mul(S1, S1, J); fp_add(S1, S1, S1);
    fp_sub(r->Y, r->Y, S1);
    /* Z3 = ((Z1+Z2)^2 - Z1Z1 - Z2Z2)*H */
    fp_add(r->Z, a->Z, b->Z);
    fp_sqr(r->Z, r->Z);
    fp_sub(r->Z, r->Z, Z1Z1);
    fp_sub(r->Z, r->Z, Z2Z2);
    fp_mul(r->Z, r->Z, H);
}

/* Mixed add: b is affine (Z=1) */
void g1_add_mixed(G1Point *r, const G1Point *a, const Fp bx, const Fp by) {
    G1Point b; fp_copy(b.X, bx); fp_copy(b.Y, by); fp_one(b.Z);
    g1_add(r, a, &b);
}

/* Binary scalar multiplication (left-to-right), alias-safe */
void g1_scalar_mul(G1Point *r, const G1Point *p, const uint32_t k[8], int nbits) {
    G1Point acc, tmp;
    g1_inf(&acc);
    for (int i = nbits - 1; i >= 0; i--) {
        g1_dbl(&tmp, &acc);          /* tmp = 2*acc  (no aliasing) */
        acc = tmp;
        if ((k[i/32] >> (i%32)) & 1u) {
            g1_add(&tmp, &acc, p);   /* tmp = acc + p (no aliasing) */
            acc = tmp;
        }
    }
    *r = acc;
}

void g1_to_affine(Fp rx, Fp ry, const G1Point *p) {
    if (g1_is_inf(p)) { fp_zero(rx); fp_zero(ry); return; }
    Fp Zinv, Zinv2;
    fp_inv(Zinv, p->Z);
    fp_sqr(Zinv2, Zinv);
    fp_mul(rx, p->X, Zinv2);
    fp_mul(Zinv2, Zinv2, Zinv);   /* Zinv^3 */
    fp_mul(ry, p->Y, Zinv2);
}

void g1_to_bytes(uint8_t out[64], const G1Point *p) {
    Fp rx, ry;
    g1_to_affine(rx, ry, p);
    fp_to_bytes(out,    rx);
    fp_to_bytes(out+32, ry);
}
void g1_from_bytes(G1Point *r, const uint8_t in[64]) {
    fp_from_bytes(r->X, in);
    fp_from_bytes(r->Y, in+32);
    fp_one(r->Z);
}
