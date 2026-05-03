#pragma once
#include <stdint.h>

/* -----------------------------------------------------------------------
 * BN128 (alt_bn128) constants in Montgomery form for STM32F407.
 * All 256-bit values are stored as 8 × uint32_t, little-endian limbs.
 * Montgomery base R = 2^256.  All Fp elements are stored as a*R mod p.
 * ----------------------------------------------------------------------- */

/* ---- Field prime p = 21888242871839275222246405745257275088548364400416034343698204186575808495617 */
static const uint32_t BN128_P[8] = {
    0xd87cfd47,0x3c208c16,0x6871ca8d,0x97816a91,
    0x8181585d,0xb85045b6,0xe131a029,0x30644e72};
#define BN128_N0P_FP  0xe4866389u   /* -p^{-1} mod 2^32 */

static const uint32_t BN128_R_FP[8] = {        /* R = 2^256 mod p */
    0xc58f0d9d,0xd35d438d,0xf5c70b3d,0x0a78eb28,
    0x7879462c,0x666ea36f,0x9a07df2f,0x0e0a77c1};
static const uint32_t BN128_R2_FP[8] = {       /* R^2 mod p */
    0x538afa89,0xf32cfc5b,0xd44501fb,0xb5e71911,
    0x0a417ff6,0x47ab1eff,0xcab8351f,0x06d89f71};

/* ---- Group (scalar) order q = 21888242871839275222246405745257275088696311157297823662689037894645226208583 */
static const uint32_t BN128_Q[8] = {
    0xf0000001,0x43e1f593,0x79b97091,0x2833e848,
    0x8181585d,0xb85045b6,0xe131a029,0x30644e72};
#define BN128_N0P_FQ  0xefffffffu   /* -q^{-1} mod 2^32 */

static const uint32_t BN128_R_FQ[8] = {        /* R = 2^256 mod q */
    0x4ffffffb,0xac96341c,0x9f60cd29,0x36fc7695,
    0x7879462e,0x666ea36f,0x9a07df2f,0x0e0a77c1};
static const uint32_t BN128_R2_FQ[8] = {       /* R^2 mod q */
    0xae216da7,0x1bb8e645,0xe35c59e3,0x53fe3ab1,
    0x53bb8085,0x8c49833d,0x7f4e44a5,0x0216d0b1};

/* ---- G1 generator P = (1, 2) in Montgomery form */
static const uint32_t BN128_G1X[8] = {   /* 1 * R mod p */
    0xc58f0d9d,0xd35d438d,0xf5c70b3d,0x0a78eb28,
    0x7879462c,0x666ea36f,0x9a07df2f,0x0e0a77c1};
static const uint32_t BN128_G1Y[8] = {   /* 2 * R mod p */
    0x8b1e1b3a,0xa6ba871b,0xeb8e167b,0x14f1d651,
    0xf0f28c58,0xccdd46de,0x340fbe5e,0x1c14ef83};

/* ---- G2 generator Q in Montgomery form (each FP2 component stored as two Fp) */
static const uint32_t BN128_G2X0[8] = {
    0x02bc2026,0x8e83b5d1,0x497b0172,0xdceb1935,
    0x97811adf,0xfbb82647,0xaf96503b,0x19573841};
static const uint32_t BN128_G2X1[8] = {
    0xa84c6140,0xafb4737d,0x5802d8c4,0x6043dd5a,
    0x52a02f86,0x09e950fc,0x3aea7b6b,0x14fef083};
static const uint32_t BN128_G2Y0[8] = {
    0x886be9f6,0x619dfa9d,0xf59e9b78,0xfe7fd297,
    0x231b7dfe,0xff9e1a62,0xae9e4206,0x28fd7eeb};
static const uint32_t BN128_G2Y1[8] = {
    0xc71856ee,0x64095b56,0x327d3cbb,0xdc57f922,
    0x33351076,0x55f935be,0x93fd6482,0x0da4a0e6};

/* ---- G2 curve b2 = 3/(9+u) in FP2 (in Montgomery form) */
static const uint32_t BN128_G2B2C0[8] = {
    0x77b802a8,0x3bf938e3,0x3633535d,0x020b1b27,
    0x49755260,0x26b7edf0,0x4384a86d,0x2514c632};
static const uint32_t BN128_G2B2C1[8] = {
    0xd1dcff67,0x38e7eccc,0x93ce0d3e,0x65f0b37d,
    0x22ac00aa,0xd749d0dd,0x4a688d4d,0x0141b9ce};

/* ---- gamma_T = e(G1,G2) as 12 Fp elements in Montgomery form
 *      Layout: GT = Fp12 = Fp6×Fp6, each Fp6 = Fp2×Fp2×Fp2,
 *      Indices: [c0.c0.a, c0.c0.b, c0.c1.a, c0.c1.b, c0.c2.a, c0.c2.b,
 *                c1.c0.a, c1.c0.b, c1.c1.a, c1.c1.b, c1.c2.a, c1.c2.b] */
static const uint32_t BN128_GT0[8]  = {0x3a8329ef,0x53e1d9fc,0x9ff5465f,0x9254a194,0x1fad5084,0x3d01af56,0x6b3b7c1e,0x2ae04505};
static const uint32_t BN128_GT1[8]  = {0x43aae165,0xd514f443,0xed14f311,0x6d330380,0x30d72457,0xe6490e61,0xbfce1f4d,0x0e220885};
static const uint32_t BN128_GT2[8]  = {0x111c85f6,0x7f138852,0x5e64fff5,0x6e42d4f2,0x38c5faf0,0x1e2c29f4,0x3d0a333d,0x05a22612};
static const uint32_t BN128_GT3[8]  = {0xce4d20ad,0x0221eab5,0x979120e2,0x69b3e161,0xd02e5cd4,0xea0115f3,0x7ecf1950,0x18d86b9b};
static const uint32_t BN128_GT4[8]  = {0xa182adff,0x5869976a,0xc0a87668,0xb87c7bb0,0xddced101,0xf7d1d1a0,0xa0015959,0x07918bfc};
static const uint32_t BN128_GT5[8]  = {0x6309bae7,0x0be63e0e,0xdf47f05f,0x2efbc923,0xc5d7e7f8,0xf5f5b66c,0xb214fa73,0x253dac9c};
static const uint32_t BN128_GT6[8]  = {0xcbd60549,0x2e02a64a,0xa58e4add,0xd618018e,0xa45ba647,0x14d585f1,0x87c434fc,0x18322269};
static const uint32_t BN128_GT7[8]  = {0x56d24998,0x9458abcb,0x2a9e5adb,0xb17540bd,0x2e401a9f,0x9a9983c8,0x84c16291,0x1614817a};
static const uint32_t BN128_GT8[8]  = {0x7a4d598e,0x172d1f25,0x7ffb5ac0,0xddf5bc7b,0xbbb0f602,0xae0b22c0,0x2fae9b18,0x1b158f3c};
static const uint32_t BN128_GT9[8]  = {0x28ebfe11,0x7002907c,0xd080da67,0x7b0591d3,0x181f138e,0xde7e5aa2,0xfc43d951,0x210e437d};
static const uint32_t BN128_GT10[8] = {0x81754cdb,0xf16c96d0,0x2bceeb55,0xce039431,0x1f01ff0a,0x644e4dcf,0xe0b236cc,0x0cbea85e};
static const uint32_t BN128_GT11[8] = {0x3157aa84,0xd34bab37,0xfd0d8598,0x3511ed44,0xc2ced972,0x67e42a0b,0xfd20c55b,0x2b8f1d5d};

/* AmorE protocol security parameters */
#define AMORE_PHI    90u   /* short scalar bits (efficiency parameter) */
#define AMORE_SIGMA  40u   /* statistical security bits              */
