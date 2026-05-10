#pragma once

/* =========================================================================
 * curve.h — Curve selection (compile-time)
 *
 * SINGLE source of #if defined(CURVE_BN254) / #if defined(CURVE_BLS12_381)
 * in this codebase. All other files reference only the abstract macros
 * defined here.
 * ========================================================================= */

#if defined(CURVE_BN254) && defined(CURVE_BLS12_381)
#  error "Both CURVE_BN254 and CURVE_BLS12_381 are defined. Pick one."
#endif

#if !defined(CURVE_BN254) && !defined(CURVE_BLS12_381)
#  error "No curve selected. Define CURVE_BN254 or CURVE_BLS12_381."
#endif

#if defined(CURVE_BN254)
#  define CURVE_NAME    "BN254"
#  ifndef FP_LIMBS
#    define FP_LIMBS  8
#  endif
#  define FP_BITS       254
#  define FP_BYTES      32
#  define SCALAR_BITS   254
#  include "bn128_const.h"
#  define CURVE_P            BN128_P
#  define CURVE_R_FP         BN128_R_FP
#  define CURVE_R2_FP        BN128_R2_FP
#  define CURVE_N0P_FP       BN128_N0P_FP
#  define CURVE_Q_FQ         BN128_Q
#  define CURVE_R_FQ         BN128_R_FQ
#  define CURVE_R2_FQ        BN128_R2_FQ
#  define CURVE_N0P_FQ       BN128_N0P_FQ
#  define CURVE_Q_MINUS_2_FQ BN128_Q_MINUS_2_FQ
#  define CURVE_G1X          BN128_G1X
#  define CURVE_G1Y          BN128_G1Y
#  define CURVE_G2X0         BN128_G2X0
#  define CURVE_G2X1         BN128_G2X1
#  define CURVE_G2Y0         BN128_G2Y0
#  define CURVE_G2Y1         BN128_G2Y1
#  define CURVE_TWIST_B_C0   BN128_G2B2C0
#  define CURVE_TWIST_B_C1   BN128_G2B2C1
#  define CURVE_GT0   BN128_GT0
#  define CURVE_GT1   BN128_GT1
#  define CURVE_GT2   BN128_GT2
#  define CURVE_GT3   BN128_GT3
#  define CURVE_GT4   BN128_GT4
#  define CURVE_GT5   BN128_GT5
#  define CURVE_GT6   BN128_GT6
#  define CURVE_GT7   BN128_GT7
#  define CURVE_GT8   BN128_GT8
#  define CURVE_GT9   BN128_GT9
#  define CURVE_GT10  BN128_GT10
#  define CURVE_GT11  BN128_GT11
#elif defined(CURVE_BLS12_381)
#  define CURVE_NAME    "BLS12-381"
#  ifndef FP_LIMBS
#    define FP_LIMBS  12
#  endif
#  define FP_BITS       381
#  define FP_BYTES      48
#  define SCALAR_BITS   255
#  include "bls12_381_const.h"
#  define CURVE_P            BLS_P
#  define CURVE_R_FP         BLS_R_FP
#  define CURVE_R2_FP        BLS_R2_FP
#  define CURVE_N0P_FP       BLS_N0P_FP
#  define CURVE_Q_FQ         BLS_Q_FQ
#  define CURVE_R_FQ         BLS_R_FQ
#  define CURVE_R2_FQ        BLS_R2_FQ
#  define CURVE_N0P_FQ       BLS_N0P_FQ
#  define CURVE_Q_MINUS_2_FQ BLS_Q_MINUS_2_FQ
#  define CURVE_G1X          BLS_G1X
#  define CURVE_G1Y          BLS_G1Y
#  define CURVE_G2X0         BLS_G2X0
#  define CURVE_G2X1         BLS_G2X1
#  define CURVE_G2Y0         BLS_G2Y0
#  define CURVE_G2Y1         BLS_G2Y1
#  define CURVE_TWIST_B_C0   BLS_TWIST_B_C0
#  define CURVE_TWIST_B_C1   BLS_TWIST_B_C1
#  define CURVE_GT0   BLS_GT0
#  define CURVE_GT1   BLS_GT1
#  define CURVE_GT2   BLS_GT2
#  define CURVE_GT3   BLS_GT3
#  define CURVE_GT4   BLS_GT4
#  define CURVE_GT5   BLS_GT5
#  define CURVE_GT6   BLS_GT6
#  define CURVE_GT7   BLS_GT7
#  define CURVE_GT8   BLS_GT8
#  define CURVE_GT9   BLS_GT9
#  define CURVE_GT10  BLS_GT10
#  define CURVE_GT11  BLS_GT11
#endif

#define FP2_BYTES               (2  * FP_BYTES)
#define FP12_BYTES              (12 * FP_BYTES)
#define G1_BYTES                (2  * FP_BYTES)
#define G2_BYTES                (4  * FP_BYTES)
#define AMORE_PROTO_OUT_BYTES   (2 * G1_BYTES + 2 * G2_BYTES)
#define AMORE_PROTO_IN_BYTES    (2 * FP12_BYTES)

#ifndef AMORE_PHI
#  define AMORE_PHI    90u
#endif
