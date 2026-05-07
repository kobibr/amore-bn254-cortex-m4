/* test_fp.c — Host-side test harness for fp.c
 *
 * Tests:
 *   Phase A: fp_add, fp_sub
 *   Phase B: fp_mul
 *   Phase C: fp_sqr, fp_to_mont, fp_from_mont, fp_inv,
 *            fp_to_bytes, fp_from_bytes
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "fp.h"
#include "fp_test_vectors.h"

static int compare_fp(const char *opname, const char *vec_name,
                      const uint32_t got[12], const uint32_t expected[12]) {
    if (memcmp(got, expected, 12 * sizeof(uint32_t)) == 0) {
        return 1;
    }
    printf("  ✗ FAIL: %-25s  on  %s\n", opname, vec_name);
    printf("    got:      ");
    for (int i = 11; i >= 0; i--) printf("%08x", got[i]);
    printf("\n");
    printf("    expected: ");
    for (int i = 11; i >= 0; i--) printf("%08x", expected[i]);
    printf("\n");
    return 0;
}

static int compare_bytes(const char *opname, const char *vec_name,
                         const uint8_t got[48], const uint8_t expected[48]) {
    if (memcmp(got, expected, 48) == 0) {
        return 1;
    }
    printf("  ✗ FAIL: %-25s  on  %s (bytes mismatch)\n", opname, vec_name);
    return 0;
}

int main(void) {
    int total = 0, passed = 0;
    int failed_add = 0, failed_sub = 0, failed_mul = 0;
    int failed_sqr = 0, failed_inv = 0;
    int failed_to_mont = 0, failed_from_mont = 0;
    int failed_bytes = 0;

    Fp r;

    printf("==== fp.c host-side tests (BLS12-381, 12 limbs) ====\n\n");

    for (int i = 0; i < NUM_TEST_VECTORS; i++) {
        const fp_test_vec_t *v = &FP_TEST_VECTORS[i];

        /* --- Phase A: add, sub --- */
        fp_add(r, (uint32_t*)v->a_mont, (uint32_t*)v->b_mont);
        if (compare_fp("fp_add", v->name, r, v->sum_mont)) passed++;
        else failed_add++;
        total++;

        fp_sub(r, (uint32_t*)v->a_mont, (uint32_t*)v->b_mont);
        if (compare_fp("fp_sub", v->name, r, v->diff_mont)) passed++;
        else failed_sub++;
        total++;

        /* --- Phase B: mul --- */
        fp_mul(r, (uint32_t*)v->a_mont, (uint32_t*)v->b_mont);
        if (compare_fp("fp_mul", v->name, r, v->prod_mont)) passed++;
        else failed_mul++;
        total++;

        /* --- Phase C: sqr --- */
        fp_sqr(r, (uint32_t*)v->a_mont);
        if (compare_fp("fp_sqr", v->name, r, v->sqr_mont)) passed++;
        else failed_sqr++;
        total++;

        /* --- Phase C: to_mont --- */
        /* a_plain → Montgomery should equal a_mont */
        fp_to_mont(r, (uint32_t*)v->a_plain);
        if (compare_fp("fp_to_mont", v->name, r, v->a_mont)) passed++;
        else failed_to_mont++;
        total++;

        /* --- Phase C: from_mont --- */
        /* a_mont → plain should equal a_plain */
        fp_from_mont(r, (uint32_t*)v->a_mont);
        if (compare_fp("fp_from_mont", v->name, r, v->a_plain)) passed++;
        else failed_from_mont++;
        total++;

        /* --- Phase C: inv (only if a != 0) --- */
        if (!v->a_is_zero) {
            fp_inv(r, (uint32_t*)v->a_mont);
            if (compare_fp("fp_inv", v->name, r, v->inv_mont)) passed++;
            else failed_inv++;
            total++;

            /* Sanity: a * a^{-1} = 1 (in Montgomery, equals BLS_R_FP) */
            Fp prod, one_mont;
            fp_one(one_mont);
            fp_mul(prod, (uint32_t*)v->a_mont, r);
            if (compare_fp("fp_inv (sanity a*inv=1)", v->name, prod, one_mont)) passed++;
            else failed_inv++;
            total++;
        }

        /* --- Phase C: to_bytes / from_bytes round-trip --- */
        uint8_t bytes[48];
        Fp r2;
        fp_to_bytes(bytes, (uint32_t*)v->a_mont);
        fp_from_bytes(r2, bytes);
        if (compare_fp("bytes roundtrip", v->name, r2, v->a_mont)) passed++;
        else failed_bytes++;
        total++;
    }

    printf("\n==== Summary ====\n");
    printf("  Total tests:          %d\n", total);
    printf("  Passed:               %d\n", passed);
    printf("  Failed:               %d\n", total - passed);
    printf("\n  By operation:\n");
    printf("    fp_add:        %s (%d failures)\n", failed_add ? "FAIL" : "OK", failed_add);
    printf("    fp_sub:        %s (%d failures)\n", failed_sub ? "FAIL" : "OK", failed_sub);
    printf("    fp_mul:        %s (%d failures)\n", failed_mul ? "FAIL" : "OK", failed_mul);
    printf("    fp_sqr:        %s (%d failures)\n", failed_sqr ? "FAIL" : "OK", failed_sqr);
    printf("    fp_to_mont:    %s (%d failures)\n", failed_to_mont ? "FAIL" : "OK", failed_to_mont);
    printf("    fp_from_mont:  %s (%d failures)\n", failed_from_mont ? "FAIL" : "OK", failed_from_mont);
    printf("    fp_inv:        %s (%d failures)\n", failed_inv ? "FAIL" : "OK", failed_inv);
    printf("    bytes:         %s (%d failures)\n", failed_bytes ? "FAIL" : "OK", failed_bytes);

    int all_ok = (passed == total);
    if (all_ok) {
        printf("\n✓✓✓ ALL TESTS PASS — fp.c COMPLETE for BLS12-381 ✓✓✓\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed\n");
        return 1;
    }
}
