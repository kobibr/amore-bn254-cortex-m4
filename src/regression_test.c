/* =========================================================================
 *  regression_test.c — v5 (final)
 *
 *  Targeted regression test for the patched arm-asm-254 backend on
 *  STM32F407 (Cortex-M4 + FPU). Validates that the five _low routines
 *  patched in the accompanying patch series produce numerically correct
 *  outputs:
 *
 *    - fp_addn_low   (modular addition)
 *    - fp_subn_low   (modular subtraction)
 *    - fp_mulm_low   (Montgomery multiplication)
 *    - fp_sqrm_low   (Montgomery squaring)
 *    - fp_rdcn_low   (Montgomery reduction)
 *
 *  Three layers:
 *
 *    Layer 1 — Constant-vector tests (6 tests).
 *      Specific inputs with known outputs; catches gross errors
 *      (wrong opcode, register mix-up) immediately.
 *
 *    Layer 2 — Algebraic identity tests (8000 random tests).
 *      For random a, b, c repeated 1000 times each, verifies:
 *        (a + b) + c == a + (b + c)      (associativity of +)
 *        a + b == b + a                   (commutativity of +)
 *        a + (-a) == 0                    (additive inverse)
 *        a - b == a + (-b)                (subtraction as add+neg)
 *        (a * b) * c == a * (b * c)       (associativity of *)
 *        a * b == b * a                   (commutativity of *)
 *        a * (b + c) == (a*b) + (a*c)    (distributivity)
 *        fp_sqr(a) == fp_mul(a, a)        (squaring consistency)
 *      Catches subtle bugs (carry propagation, register clobber)
 *      across uniformly-distributed inputs.
 *
 *    Layer 3 — Boundary cases (5 tests).
 *      Inputs that exercise the limb-6 path specifically — i.e., the
 *      path through the patched MOVW/MOVT immediates:
 *        T3.1: fp_sub(0, 1)        == p - 1     (borrow chain)
 *        T3.2: 2*(p-1) + 2         == 0          (carry chain ×2)
 *        T3.3: fp_mul(p-1, p-1)    == 1          (max-value squaring via mul)
 *        T3.4: fp_sqr(p-1)         == 1          (max-value squaring)
 *        T3.5: fp_add(0, p-1)      == p - 1     (max-value add identity)
 *
 *  Output is captured in g_reg_results, readable via GDB after the
 *  target halts:
 *
 *    (gdb) p g_reg_results
 *
 *  Pass criterion: g_reg_results.status == 0xC0DEC0DE.
 *  Total tests:    8011 (6 + 8000 + 5).
 *
 *  Author: Kobi Brener <kob.tov@gmail.com>
 *  License: same as RELIC (LGPL-2.1 / Apache-2.0)
 * ========================================================================= */

#include <relic.h>
#include <string.h>
#include <stdint.h>

#include "regression_test.h"

/* Cortex-M cycle counter for performance accounting */
#define DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004u)
static inline uint32_t cyc(void) { return DWT_CYCCNT; }

/* Local pointer to the results struct, set by regression_run() entry. */
static regression_results_t *g_out = NULL;

/* ------------------------------------------------------------------ */
/* Deterministic RNG for reproducibility                               */
/* ------------------------------------------------------------------ */

static uint32_t prng_state = 0xDEADBEEFu;

static void test_rand(uint8_t *buf, size_t len, void *ctx) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) {
        prng_state ^= prng_state << 13;
        prng_state ^= prng_state >> 17;
        prng_state ^= prng_state <<  5;
        buf[i] = (uint8_t)(prng_state & 0xFFu);
    }
}

/* ------------------------------------------------------------------ */
/* Failure helpers                                                     */
/*                                                                     */
/*   EXPECT_EQ_FP   — compares two fp_t values directly.              */
/*   EXPECT_EQ_DIG  — compares an fp_t to a digit; uses fp_cmp_dig    */
/*                    which handles Montgomery conversion internally. */
/*   EXPECT_ZERO    — compares an fp_t against zero via fp_is_zero.   */
/* ------------------------------------------------------------------ */

#define EXPECT_EQ_FP(a, b, layer, idx) do {                       \
    if (fp_cmp((a), (b)) != RLC_EQ) {                             \
        if (g_out->first_fail_layer == 0) {                       \
            g_out->first_fail_layer = (layer);                    \
            g_out->first_fail_index = (idx);                      \
        }                                                         \
        g_out->total_failed++;                                    \
        return -1;                                                \
    }                                                             \
} while (0)

#define EXPECT_EQ_DIG(a, d, layer, idx) do {                      \
    if (fp_cmp_dig((a), (d)) != RLC_EQ) {                         \
        if (g_out->first_fail_layer == 0) {                       \
            g_out->first_fail_layer = (layer);                    \
            g_out->first_fail_index = (idx);                      \
        }                                                         \
        g_out->total_failed++;                                    \
        return -1;                                                \
    }                                                             \
} while (0)

#define EXPECT_ZERO(a, layer, idx) do {                           \
    if (fp_is_zero(a) != 1) {                                     \
        if (g_out->first_fail_layer == 0) {                       \
            g_out->first_fail_layer = (layer);                    \
            g_out->first_fail_index = (idx);                      \
        }                                                         \
        g_out->total_failed++;                                    \
        return -1;                                                \
    }                                                             \
} while (0)

#define PASS_LAYER1(routine) do {                                 \
    g_out->total_passed++;                                        \
    g_out->layer1_passed++;                                       \
    g_out->routine ## _passed++;                                  \
} while (0)

#define PASS_LAYER2(routine) do {                                 \
    g_out->total_passed++;                                        \
    g_out->layer2_passed++;                                       \
    g_out->routine ## _passed++;                                  \
} while (0)

#define PASS_LAYER3(routine) do {                                 \
    g_out->total_passed++;                                        \
    g_out->layer3_passed++;                                       \
    g_out->routine ## _passed++;                                  \
} while (0)

/* ------------------------------------------------------------------ */
/* Layer 1 — Constant-vector tests                                     */
/* ------------------------------------------------------------------ */

static int layer1_constant_vectors(void) {
    fp_t a, b, r;
    fp_null(a); fp_null(b); fp_null(r);
    fp_new(a);  fp_new(b);  fp_new(r);

    int idx = 0;

    /* T1.1: fp_add(0, 0) == 0 */
    fp_zero(a);
    fp_zero(b);
    fp_add(r, a, b);
    g_out->total_tests++;
    EXPECT_ZERO(r, 1, ++idx);
    PASS_LAYER1(fp_add);

    /* T1.2: fp_add(x, 0) == x  for random x */
    fp_rand(a);
    fp_zero(b);
    fp_add(r, a, b);
    g_out->total_tests++;
    EXPECT_EQ_FP(r, a, 1, ++idx);
    PASS_LAYER1(fp_add);

    /* T1.3: fp_sub(0, 0) == 0 */
    fp_zero(a);
    fp_zero(b);
    fp_sub(r, a, b);
    g_out->total_tests++;
    EXPECT_ZERO(r, 1, ++idx);
    PASS_LAYER1(fp_sub);

    /* T1.4: fp_sub(a, a) == 0 (any a) */
    fp_rand(a);
    fp_sub(r, a, a);
    g_out->total_tests++;
    EXPECT_ZERO(r, 1, ++idx);
    PASS_LAYER1(fp_sub);

    /* T1.5: fp_mul(a, 0) == 0 */
    fp_rand(a);
    fp_zero(b);
    fp_mul(r, a, b);
    g_out->total_tests++;
    EXPECT_ZERO(r, 1, ++idx);
    PASS_LAYER1(fp_mul);

    /* T1.6: fp_sqr(0) == 0 */
    fp_zero(a);
    fp_sqr(r, a);
    g_out->total_tests++;
    EXPECT_ZERO(r, 1, ++idx);
    PASS_LAYER1(fp_sqr);

    fp_free(a); fp_free(b); fp_free(r);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Layer 2 — Algebraic identity tests (random inputs)                  */
/* ------------------------------------------------------------------ */

#define ITERATIONS_PER_IDENTITY  1000

static int layer2_algebraic_identities(void) {
    fp_t a, b, c, lhs, rhs, t1, t2;
    fp_null(a); fp_null(b); fp_null(c);
    fp_null(lhs); fp_null(rhs); fp_null(t1); fp_null(t2);
    fp_new(a); fp_new(b); fp_new(c);
    fp_new(lhs); fp_new(rhs); fp_new(t1); fp_new(t2);

    uint32_t cyc_acc_add = 0, cyc_acc_sub = 0;
    uint32_t cyc_acc_mul = 0, cyc_acc_sqr = 0;
    uint32_t calls_add = 0, calls_sub = 0;
    uint32_t calls_mul = 0, calls_sqr = 0;

    int idx = 0;

    /* I2.1: Addition associativity — (a + b) + c == a + (b + c) */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b); fp_rand(c);

        uint32_t t = cyc();
        fp_add(t1, a, b);
        cyc_acc_add += cyc() - t;
        calls_add++;
        fp_add(lhs, t1, c);

        fp_add(t2, b, c);
        fp_add(rhs, a, t2);

        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_add);
    }

    /* I2.2: Addition commutativity — a + b == b + a */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b);
        fp_add(lhs, a, b);
        fp_add(rhs, b, a);
        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_add);
    }

    /* I2.3: Additive inverse — a + (-a) == 0 */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a);
        fp_neg(b, a);

        uint32_t t = cyc();
        fp_add(lhs, a, b);
        cyc_acc_add += cyc() - t;
        calls_add++;

        g_out->total_tests++;
        EXPECT_ZERO(lhs, 2, ++idx);
        PASS_LAYER2(fp_sub);  /* exercises borrow path via negation */
    }

    /* I2.4: Subtraction == addition with negation */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b);

        uint32_t t = cyc();
        fp_sub(lhs, a, b);
        cyc_acc_sub += cyc() - t;
        calls_sub++;

        fp_neg(t1, b);
        fp_add(rhs, a, t1);

        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_sub);
    }

    /* I2.5: Multiplication associativity — (a*b)*c == a*(b*c) */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b); fp_rand(c);

        uint32_t t = cyc();
        fp_mul(t1, a, b);
        cyc_acc_mul += cyc() - t;
        calls_mul++;
        fp_mul(lhs, t1, c);

        fp_mul(t2, b, c);
        fp_mul(rhs, a, t2);

        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_mul);
    }

    /* I2.6: Multiplication commutativity — a*b == b*a */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b);
        fp_mul(lhs, a, b);
        fp_mul(rhs, b, a);
        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_mul);
    }

    /* I2.7: Distributivity — a*(b+c) == a*b + a*c */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a); fp_rand(b); fp_rand(c);

        fp_add(t1, b, c);
        fp_mul(lhs, a, t1);

        fp_mul(t1, a, b);
        fp_mul(t2, a, c);
        fp_add(rhs, t1, t2);

        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_mul);
    }

    /* I2.8: Squaring consistency — fp_sqr(a) == fp_mul(a, a) */
    for (int i = 0; i < ITERATIONS_PER_IDENTITY; i++) {
        fp_rand(a);

        uint32_t t = cyc();
        fp_sqr(lhs, a);
        cyc_acc_sqr += cyc() - t;
        calls_sqr++;

        fp_mul(rhs, a, a);

        g_out->total_tests++;
        EXPECT_EQ_FP(lhs, rhs, 2, ++idx);
        PASS_LAYER2(fp_sqr);
    }

    fp_free(a); fp_free(b); fp_free(c);
    fp_free(lhs); fp_free(rhs); fp_free(t1); fp_free(t2);

    /* Record per-call cycle averages */
    if (calls_add > 0) g_out->cyc_per_add = cyc_acc_add / calls_add;
    if (calls_sub > 0) g_out->cyc_per_sub = cyc_acc_sub / calls_sub;
    if (calls_mul > 0) g_out->cyc_per_mul = cyc_acc_mul / calls_mul;
    if (calls_sqr > 0) g_out->cyc_per_sqr = cyc_acc_sqr / calls_sqr;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Layer 3 — Boundary cases (limb-6 path stress)                       */
/*                                                                     */
/* All values constructed in Montgomery form via fp_prime_conv /       */
/* fp_prime_conv_dig. Comparisons against small constants use          */
/* fp_cmp_dig, which performs the Montgomery conversion internally.    */
/* Comparisons of large values use fp_cmp on two same-form fp_t.       */
/* ------------------------------------------------------------------ */

static int layer3_boundary_cases(void) {
    fp_t a, b, r, expected, one_fp;
    bn_t prime, p_minus_1;

    fp_null(a); fp_null(b); fp_null(r); fp_null(expected); fp_null(one_fp);
    bn_null(prime); bn_null(p_minus_1);
    fp_new(a); fp_new(b); fp_new(r); fp_new(expected); fp_new(one_fp);
    bn_new(prime); bn_new(p_minus_1);

    fp_prime_back(prime, fp_prime_get());
    bn_sub_dig(p_minus_1, prime, 1);   /* p - 1 */
    fp_prime_conv_dig(one_fp, 1);      /* "1" in Montgomery form */

    int idx = 0;

    /* T3.1: fp_sub(0, 1) == p-1
     *       Stresses the borrow chain across all 8 limbs. The result
     *       limb 6 == 0x40000001 is the patched constant. */
    fp_zero(a);
    fp_prime_conv(expected, p_minus_1);
    fp_sub(r, a, one_fp);
    g_out->total_tests++;
    EXPECT_EQ_FP(r, expected, 3, ++idx);
    PASS_LAYER3(fp_sub);

    /* T3.2: 2*(p-1) + 2 == 0 mod p
     *       Stresses the carry chain (twice) through limb 6. */
    fp_prime_conv(a, p_minus_1);
    fp_add(r, a, a);                 /* r = 2*(p-1) mod p */
    fp_add(r, r, one_fp);            /* r += 1 */
    fp_add(r, r, one_fp);            /* r += 1 → r should be 0 */
    g_out->total_tests++;
    EXPECT_ZERO(r, 3, ++idx);
    PASS_LAYER3(fp_add);

    /* T3.3: fp_mul(p-1, p-1) == 1 mod p
     *       (p-1)^2 ≡ 1 (mod p). Maximally stresses the multiplier
     *       reduction logic. */
    fp_prime_conv(a, p_minus_1);
    fp_mul(r, a, a);
    g_out->total_tests++;
    EXPECT_EQ_DIG(r, 1, 3, ++idx);
    PASS_LAYER3(fp_mul);

    /* T3.4: fp_sqr(p-1) == 1 mod p
     *       Same as T3.3 but exercises fp_sqr's specialized squaring
     *       path instead of the general multiplier. */
    fp_prime_conv(a, p_minus_1);
    fp_sqr(r, a);
    g_out->total_tests++;
    EXPECT_EQ_DIG(r, 1, 3, ++idx);
    PASS_LAYER3(fp_sqr);

    /* T3.5: fp_add(0, p-1) == p-1
     *       Additive identity with the maximum-value operand. Stresses
     *       limb 6 with all bits of p-1 in the second operand, and
     *       verifies that adding 0 preserves the value bit-for-bit
     *       (no spurious carry). Comparison is fp_t-to-fp_t (both
     *       Montgomery form), avoiding any zero-canonicalization. */
    fp_zero(a);
    fp_prime_conv(b, p_minus_1);
    fp_add(r, a, b);
    g_out->total_tests++;
    EXPECT_EQ_FP(r, b, 3, ++idx);
    PASS_LAYER3(fp_add);

    fp_free(a); fp_free(b); fp_free(r); fp_free(expected); fp_free(one_fp);
    bn_free(prime); bn_free(p_minus_1);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public entry                                                        */
/* ------------------------------------------------------------------ */

void regression_run(regression_results_t *out) {
    memset(out, 0, sizeof(*out));
    out->status = REGRESSION_STATUS_FAIL_LAYER1;
    g_out = out;

    if (core_init() != RLC_OK) {
        out->magic = REGRESSION_MAGIC;
        return;
    }
    rand_seed(test_rand, NULL);

    if (ep_param_set_any_pairf() != RLC_OK) {
        core_clean();
        out->magic = REGRESSION_MAGIC;
        return;
    }

    if (layer1_constant_vectors() != 0) {
        out->status = REGRESSION_STATUS_FAIL_LAYER1;
        out->magic  = REGRESSION_MAGIC;
        core_clean();
        return;
    }

    if (layer2_algebraic_identities() != 0) {
        out->status = REGRESSION_STATUS_FAIL_LAYER2;
        out->magic  = REGRESSION_MAGIC;
        core_clean();
        return;
    }

    if (layer3_boundary_cases() != 0) {
        out->status = REGRESSION_STATUS_FAIL_LAYER3;
        out->magic  = REGRESSION_MAGIC;
        core_clean();
        return;
    }

    out->status = REGRESSION_STATUS_OK;
    out->magic  = REGRESSION_MAGIC;
    core_clean();
}
