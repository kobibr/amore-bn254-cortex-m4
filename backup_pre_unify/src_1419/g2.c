#include "g2.h"
#include "bls12_381_const.h"
#include <string.h>

void g2_inf(G2Point *r) {
    fp2_one(&r->X); fp2_one(&r->Y); fp2_zero(&r->Z);
}
int g2_is_inf(const G2Point *p) { return fp2_is_zero(&p->Z); }

void g2_generator(G2Point *r) {
    memcpy(r->X.c0, BLS_G2X0, 48); memcpy(r->X.c1, BLS_G2X1, 48);
    memcpy(r->Y.c0, BLS_G2Y0, 48); memcpy(r->Y.c1, BLS_G2Y1, 48);
    fp2_one(&r->Z);
}

void g2_neg(G2Point *r, const G2Point *a) {
    fp2_copy(&r->X, &a->X);
    fp2_neg(&r->Y, &a->Y);
    fp2_copy(&r->Z, &a->Z);
}

/* Jacobian doubling for y^2 = x^3 + b (a=0), same formulas as G1 but over Fp2 */
void g2_dbl(G2Point *r, const G2Point *a) {
    if (g2_is_inf(a)) { g2_inf(r); return; }
    Fp2 A, B, C, D, E, F;
    fp2_sqr(&A, &a->X);
    fp2_sqr(&B, &a->Y);
    fp2_sqr(&C, &B);
    {
        Fp2 tmp; fp2_add(&tmp, &a->X, &B);
        fp2_sqr(&D, &tmp);
        fp2_sub(&D, &D, &A); fp2_sub(&D, &D, &C);
        fp2_add(&D, &D, &D);
    }
    fp2_add(&E, &A, &A); fp2_add(&E, &E, &A);
    fp2_sqr(&F, &E);
    {
        Fp2 twiceD; fp2_add(&twiceD, &D, &D);
        fp2_sub(&r->X, &F, &twiceD);
    }
    {
        Fp2 eightC, dm;
        fp2_add(&eightC,&C,&C); fp2_add(&eightC,&eightC,&eightC); fp2_add(&eightC,&eightC,&eightC);
        fp2_sub(&dm, &D, &r->X);
        fp2_mul(&r->Y, &E, &dm);
        fp2_sub(&r->Y, &r->Y, &eightC);
    }
    fp2_mul(&r->Z, &a->Y, &a->Z);
    fp2_add(&r->Z, &r->Z, &r->Z);
}

/* Jacobian addition (same Chudnovsky-like formula as G1 but over Fp2) */
void g2_add(G2Point *r, const G2Point *a, const G2Point *b) {
    if (g2_is_inf(a)) { *r = *b; return; }
    if (g2_is_inf(b)) { *r = *a; return; }

    Fp2 Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, Lr, V;
    fp2_sqr(&Z1Z1, &a->Z);
    fp2_sqr(&Z2Z2, &b->Z);
    fp2_mul(&U1, &a->X, &Z2Z2);
    fp2_mul(&U2, &b->X, &Z1Z1);
    fp2_mul(&S1, &a->Y, &b->Z);  fp2_mul(&S1, &S1, &Z2Z2);
    fp2_mul(&S2, &b->Y, &a->Z);  fp2_mul(&S2, &S2, &Z1Z1);

    if (fp2_eq(&U1, &U2)) {
        if (fp2_eq(&S1, &S2)) { g2_dbl(r, a); return; }
        g2_inf(r); return;
    }

    fp2_sub(&H, &U2, &U1);
    fp2_add(&I, &H, &H);  fp2_sqr(&I, &I);
    fp2_mul(&J, &H, &I);
    fp2_sub(&Lr, &S2, &S1); fp2_add(&Lr, &Lr, &Lr);
    fp2_mul(&V, &U1, &I);
    fp2_sqr(&r->X, &Lr);
    fp2_sub(&r->X, &r->X, &J);
    fp2_sub(&r->X, &r->X, &V); fp2_sub(&r->X, &r->X, &V);
    fp2_sub(&V, &V, &r->X);
    fp2_mul(&r->Y, &Lr, &V);
    fp2_mul(&S1, &S1, &J); fp2_add(&S1, &S1, &S1);
    fp2_sub(&r->Y, &r->Y, &S1);
    fp2_add(&r->Z, &a->Z, &b->Z);
    fp2_sqr(&r->Z, &r->Z);
    fp2_sub(&r->Z, &r->Z, &Z1Z1); fp2_sub(&r->Z, &r->Z, &Z2Z2);
    fp2_mul(&r->Z, &r->Z, &H);
}

void g2_scalar_mul(G2Point *r, const G2Point *p, const uint32_t k[8], int nbits) {
    G2Point acc, tmp;
    g2_inf(&acc);
    for (int i = nbits - 1; i >= 0; i--) {
        g2_dbl(&tmp, &acc);
        acc = tmp;
        if ((k[i/32] >> (i%32)) & 1u) {
            g2_add(&tmp, &acc, p);
            acc = tmp;
        }
    }
    *r = acc;
}

void g2_to_affine(Fp2 *rx, Fp2 *ry, const G2Point *p) {
    if (g2_is_inf(p)) { fp2_zero(rx); fp2_zero(ry); return; }
    Fp2 Zinv, Zinv2;
    fp2_inv(&Zinv, &p->Z);
    fp2_sqr(&Zinv2, &Zinv);
    fp2_mul(rx, &p->X, &Zinv2);
    fp2_mul(&Zinv2, &Zinv2, &Zinv);
    fp2_mul(ry, &p->Y, &Zinv2);
}

void g2_to_bytes(uint8_t out[192], const G2Point *p) {
    Fp2 rx, ry;
    g2_to_affine(&rx, &ry, p);
    fp2_to_bytes(out,    &rx);
    fp2_to_bytes(out+96, &ry);
}
void g2_from_bytes(G2Point *r, const uint8_t in[192]) {
    fp2_from_bytes(&r->X, in);
    fp2_from_bytes(&r->Y, in+96);
    fp2_one(&r->Z);
}
