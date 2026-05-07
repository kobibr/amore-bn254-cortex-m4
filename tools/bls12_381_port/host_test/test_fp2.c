/* test_fp2.c — Host-side test harness for fp2.c
 *
 * Tests all Fp² operations against Python reference values:
 *   fp2_add, fp2_sub, fp2_mul, fp2_sqr, fp2_inv, fp2_mul_xi
 *   bytes round-trip (fp2_to_bytes / fp2_from_bytes, 96 bytes)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "fp2.h"
#include "fp2_test_vectors.h"

static int compare_fp(const char *opname, const char *vec_name, const char *coord,
                      const uint32_t got[12], const uint32_t expected[12]) {
    if (memcmp(got, expected, 12 * sizeof(uint32_t)) == 0) {
        return 1;
    }
    printf("  ✗ FAIL: %-20s on %s.%s\n", opname, vec_name, coord);
    printf("    got:      ");
    for (int i = 11; i >= 0; i--) printf("%08x", got[i]);
    printf("\n");
    printf("    expected: ");
    for (int i = 11; i >= 0; i--) printf("%08x", expected[i]);
    printf("\n");
    return 0;
}

/* Helper: load an Fp2 from two coord arrays (a_c0, a_c1) */
static void load_fp2(Fp2 *r, const uint32_t *c0, const uint32_t *c1) {
    memcpy(r->c0, c0, sizeof(Fp));
    memcpy(r->c1, c1, sizeof(Fp));
}

/* Helper: check both coordinates of fp2 result */
static int check_fp2(const char *opname, const char *vec_name,
                     const Fp2 *got, const uint32_t *exp_c0, const uint32_t *exp_c1) {
    int ok_c0 = compare_fp(opname, vec_name, "c0", got->c0, exp_c0);
    int ok_c1 = compare_fp(opname, vec_name, "c1", got->c1, exp_c1);
    return ok_c0 && ok_c1;
}

int main(void) {
    int total = 0, passed = 0;
    int failed_add = 0, failed_sub = 0, failed_mul = 0;
    int failed_sqr = 0, failed_inv = 0, failed_xi = 0;
    int failed_bytes = 0;

    Fp2 a, b, r;

    printf("==== fp2.c host-side tests (BLS12-381) ====\n\n");

    for (int i = 0; i < NUM_FP2_TEST_VECTORS; i++) {
        const fp2_test_vec_t *v = &FP2_TEST_VECTORS[i];

        load_fp2(&a, v->a_c0, v->a_c1);
        load_fp2(&b, v->b_c0, v->b_c1);

        /* fp2_add */
        fp2_add(&r, &a, &b);
        if (check_fp2("fp2_add", v->name, &r, v->sum_c0, v->sum_c1)) passed++;
        else failed_add++;
        total++;

        /* fp2_sub */
        fp2_sub(&r, &a, &b);
        if (check_fp2("fp2_sub", v->name, &r, v->diff_c0, v->diff_c1)) passed++;
        else failed_sub++;
        total++;

        /* fp2_mul */
        fp2_mul(&r, &a, &b);
        if (check_fp2("fp2_mul", v->name, &r, v->prod_c0, v->prod_c1)) passed++;
        else failed_mul++;
        total++;

        /* fp2_sqr */
        fp2_sqr(&r, &a);
        if (check_fp2("fp2_sqr", v->name, &r, v->sqr_c0, v->sqr_c1)) passed++;
        else failed_sqr++;
        total++;

        /* fp2_mul_xi */
        fp2_mul_xi(&r, &a);
        if (check_fp2("fp2_mul_xi", v->name, &r, v->xi_c0, v->xi_c1)) passed++;
        else failed_xi++;
        total++;

        /* fp2_inv (only if a != 0) */
        if (!v->a_is_zero) {
            fp2_inv(&r, &a);
            if (check_fp2("fp2_inv", v->name, &r, v->inv_c0, v->inv_c1)) passed++;
            else failed_inv++;
            total++;

            /* sanity: a * a^-1 = (1, 0) in Mont = (R, 0) */
            Fp2 prod, one;
            fp2_one(&one);
            fp2_mul(&prod, &a, &r);
            if (check_fp2("fp2_inv (sanity a*inv=1)", v->name, &prod,
                          one.c0, one.c1)) passed++;
            else failed_inv++;
            total++;
        }

        /* bytes roundtrip: 96 bytes for Fp² */
        uint8_t buf[96];
        Fp2 r2;
        fp2_to_bytes(buf, &a);
        fp2_from_bytes(&r2, buf);
        if (check_fp2("bytes roundtrip", v->name, &r2, v->a_c0, v->a_c1)) passed++;
        else failed_bytes++;
        total++;
    }

    printf("\n==== Summary ====\n");
    printf("  Total tests:    %d\n", total);
    printf("  Passed:         %d\n", passed);
    printf("  Failed:         %d\n", total - passed);
    printf("\n  By operation:\n");
    printf("    fp2_add:        %s (%d failures)\n", failed_add ? "FAIL" : "OK", failed_add);
    printf("    fp2_sub:        %s (%d failures)\n", failed_sub ? "FAIL" : "OK", failed_sub);
    printf("    fp2_mul:        %s (%d failures)\n", failed_mul ? "FAIL" : "OK", failed_mul);
    printf("    fp2_sqr:        %s (%d failures)\n", failed_sqr ? "FAIL" : "OK", failed_sqr);
    printf("    fp2_inv:        %s (%d failures)\n", failed_inv ? "FAIL" : "OK", failed_inv);
    printf("    fp2_mul_xi:     %s (%d failures)\n", failed_xi ? "FAIL" : "OK", failed_xi);
    printf("    bytes:          %s (%d failures)\n", failed_bytes ? "FAIL" : "OK", failed_bytes);

    if (passed == total) {
        printf("\n✓✓✓ ALL TESTS PASS — fp2.c COMPLETE for BLS12-381 ✓✓✓\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed\n");
        return 1;
    }
}
