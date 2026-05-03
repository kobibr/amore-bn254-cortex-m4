/* =========================================================================
 *  regression_test.h — header for regression test of patched arm-asm-254
 *  backend. See regression_test.c for full description.
 *
 *  Author: Kobi Brener <kob.tov@gmail.com>
 * ========================================================================= */

#ifndef REGRESSION_TEST_H
#define REGRESSION_TEST_H

#include <stdint.h>

/* Status codes — set in g_reg_results.status */
#define REGRESSION_STATUS_OK            0xC0DEC0DE
#define REGRESSION_STATUS_FAIL_LAYER1   0xBAD00001
#define REGRESSION_STATUS_FAIL_LAYER2   0xBAD00002
#define REGRESSION_STATUS_FAIL_LAYER3   0xBAD00003

/* Magic — set after run completes (regardless of pass/fail) */
#define REGRESSION_MAGIC                0xC0DEAA00

typedef struct {
    uint32_t magic;             /* 0xC0DEAA00 if test reached end     */
    uint32_t status;            /* see codes above                    */

    /* per-routine pass counters */
    uint32_t fp_add_passed;
    uint32_t fp_sub_passed;
    uint32_t fp_mul_passed;
    uint32_t fp_sqr_passed;
    uint32_t fp_rdc_passed;

    /* per-layer pass counters */
    uint32_t layer1_passed;     /* constant vectors                   */
    uint32_t layer2_passed;     /* algebraic identities               */
    uint32_t layer3_passed;     /* boundary cases                     */

    /* totals */
    uint32_t total_tests;
    uint32_t total_passed;
    uint32_t total_failed;

    /* failure diagnostic — first failure recorded                    */
    uint32_t first_fail_layer;  /* 1, 2, or 3                         */
    uint32_t first_fail_index;  /* test index within layer            */

    /* performance counters (cycles per call, average)                */
    uint32_t cyc_per_add;
    uint32_t cyc_per_sub;
    uint32_t cyc_per_mul;
    uint32_t cyc_per_sqr;
} regression_results_t;

/* Entry point — runs all regression tests and fills results structure.
 * Caller is responsible for clock initialization (168 MHz expected) and
 * DWT cycle counter setup before invoking this function. */
void regression_run(regression_results_t *out);

#endif /* REGRESSION_TEST_H */
