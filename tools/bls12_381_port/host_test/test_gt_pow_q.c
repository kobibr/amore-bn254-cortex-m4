/* test_gt_pow_q.c — verify that fp12_exp(gt, q) == 1
 * If this fails, fp12_exp / fp12_mul / fp12_sqr have a bug in BLS12-381.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fp12.h"

/* gt coefficients in Montgomery form */
static const uint32_t GT_COEFF_0[12] = { 0xa02f7be3,0x92a4692e,0x9011e8a5,0x6c0c09d7,0x8bf687c6,0x2aa428a8,0xf316404b,0x25483bcb,0x61ca0dfc,0x8019a515,0xa66267cd,0x0d237d53 };
static const uint32_t GT_COEFF_1[12] = { 0x84ef3426,0x963ca33e,0x238d0c91,0xec6df0b5,0x03bdb0bc,0xd1deeeb2,0xaa5e170c,0xfdb11f03,0x6231e4fc,0x3f02cb36,0x8fa3c7ff,0x0f541f5d };
static const uint32_t GT_COEFF_2[12] = { 0x1f17e8b0,0xc34044dc,0x5efc0b98,0x77491dd9,0x3023ac2b,0xb8986539,0xb1c7398d,0xd0cfa09c,0x6e0d6155,0x75ed7f06,0xa3a616e1,0x1954b3b4 };
static const uint32_t GT_COEFF_3[12] = { 0xce4777e5,0x2f20bab3,0x24041bc6,0xdc98183a,0xa2836ab8,0xd8bb3905,0x130d5207,0xc4d63bd1,0xf9360896,0x9f89a1c0,0x45ee380c,0x115a6772 };
static const uint32_t GT_COEFF_4[12] = { 0x19ade228,0xd5357d8b,0x68c2d0b7,0x1c7fcaf5,0x4bde2c13,0x2125ced2,0x17accd3d,0x184ad051,0x3b77c14b,0x59af0c92,0x12d4ff4d,0x076fb5e5 };
static const uint32_t GT_COEFF_5[12] = { 0xce99b5b0,0x6f900467,0x995350d1,0xaae46487,0x6211acd6,0x208fc835,0x8d8eb84b,0xc88fc54e,0xa15930e6,0xc9cc5c0e,0x62ab63c8,0x0e0348db };
static const uint32_t GT_COEFF_6[12] = { 0xa9815507,0x4510dbd9,0xceaf6344,0xa058bbdc,0x6164e768,0xa4093c62,0x11eedaf6,0x3711c1db,0x455819e7,0x35aa44d1,0x3f75d136,0x1917ce86 };
static const uint32_t GT_COEFF_7[12] = { 0x06f5c1a0,0x573696f2,0x9ec8f11b,0x84a411e0,0x726a9c2c,0x4ed3d771,0xd3899b2e,0xb443484d,0xf4e5be2c,0x92105566,0x57ebadfb,0x03aae555 };
static const uint32_t GT_COEFF_8[12] = { 0xb76ab492,0x71f647e6,0x664308db,0xef6914f7,0x9138900e,0x46bbcb4e,0x71af7bfa,0x276b4c76,0xb4263440,0x49a154f8,0x066abdf0,0x13911c4e };
static const uint32_t GT_COEFF_9[12] = { 0x63e27516,0xe3c004aa,0x43a69720,0x66b259c7,0x76e92ac8,0x123df761,0xe8362c99,0x8e831f19,0x1eb9fd8d,0x9a28ddb0,0x93ca1076,0x14759f7a };
static const uint32_t GT_COEFF_10[12] = { 0x59d58416,0xc7942b8d,0x9fac37d9,0x4acc036e,0x11f27c14,0x49c09eb7,0x378c498c,0xea713276,0x0a555543,0xb0c03779,0x76d1a47d,0x19ef7887 };
static const uint32_t GT_COEFF_11[12] = { 0xe8621dcf,0x4bb6d760,0x7091ee69,0x825e20b7,0x81a9a0c4,0xfd7ce9cb,0xf63a3063,0xd66f3438,0xf0c6efd0,0xf58e8597,0x2806f828,0x01af7ed4 };

static const uint32_t Q_LIMBS[8] = { 0x00000001,0xffffffff,0xfffe5bfe,0x53bda402,0x09a1d805,0x3339d808,0x299d7d48,0x73eda753 };

int main(void) {
    Fp12 gt, result, one;

    /* Load gt from coefficient constants */
    Fp *flat = (Fp *)&gt;
    memcpy(flat[0],  GT_COEFF_0,  sizeof(Fp));
    memcpy(flat[1],  GT_COEFF_1,  sizeof(Fp));
    memcpy(flat[2],  GT_COEFF_2,  sizeof(Fp));
    memcpy(flat[3],  GT_COEFF_3,  sizeof(Fp));
    memcpy(flat[4],  GT_COEFF_4,  sizeof(Fp));
    memcpy(flat[5],  GT_COEFF_5,  sizeof(Fp));
    memcpy(flat[6],  GT_COEFF_6,  sizeof(Fp));
    memcpy(flat[7],  GT_COEFF_7,  sizeof(Fp));
    memcpy(flat[8],  GT_COEFF_8,  sizeof(Fp));
    memcpy(flat[9],  GT_COEFF_9,  sizeof(Fp));
    memcpy(flat[10], GT_COEFF_10, sizeof(Fp));
    memcpy(flat[11], GT_COEFF_11, sizeof(Fp));

    /* Compute gt^q */
    printf("Computing gt^q (255 bits)...\n");
    fp12_exp(&result, &gt, Q_LIMBS, 255);

    /* Check if result == 1 */
    fp12_one(&one);
    int eq = fp12_eq(&result, &one);

    if (eq) {
        printf("✓ PASS: gt^q == 1  (fp12_exp + fp12_mul + fp12_sqr work correctly)\n");
        return 0;
    } else {
        printf("✗ FAIL: gt^q != 1\n");
        printf("Result coeff[0] (Mont):\n  ");
        Fp *r_flat = (Fp *)&result;
        for (int i = 11; i >= 0; i--) printf("%08x", r_flat[0][i]);
        printf("\n");
        printf("Expected coeff[0] (Mont, = R mod p, since 1 in Mont form):\n");
        Fp *o_flat = (Fp *)&one;
        printf("  ");
        for (int i = 11; i >= 0; i--) printf("%08x", o_flat[0][i]);
        printf("\n");
        return 1;
    }
}
