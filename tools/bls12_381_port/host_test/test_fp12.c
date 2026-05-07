/* test_fp12.c — Host-side test harness for fp12.c
 *
 * Tests Fp¹² operations against py_ecc ground truth:
 *   fp12_mul, fp12_sqr
 *   bytes round-trip (576 bytes for BLS12-381)
 *   fp12_one, fp12_eq sanity
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "fp12.h"
#include "fp12_test_vectors.h"

static int compare_coeff(const char *opname, const char *vec_name, int idx,
                         const uint32_t got[12], const uint32_t expected[12]) {
    if (memcmp(got, expected, 12 * sizeof(uint32_t)) == 0) {
        return 1;
    }
    printf("  ✗ FAIL: %-15s on %s.coeff[%d]\n", opname, vec_name, idx);
    printf("    got:      ");
    for (int i = 11; i >= 0; i--) printf("%08x", got[i]);
    printf("\n");
    printf("    expected: ");
    for (int i = 11; i >= 0; i--) printf("%08x", expected[i]);
    printf("\n");
    return 0;
}

/* Load 12 coefficients into Fp12. */
static void load_fp12(Fp12 *r, const uint32_t * const coeffs[12]) {
    /* Fp12 is laid out as 12 contiguous Fp values; cast to flat array. */
    Fp *flat = (Fp *)r;
    for (int i = 0; i < 12; i++) {
        memcpy(flat[i], coeffs[i], sizeof(Fp));
    }
}

/* Check all 12 coefficients of an Fp12 result. */
static int check_fp12(const char *opname, const char *vec_name,
                      const Fp12 *got, const uint32_t * const expected[12]) {
    const Fp *flat = (const Fp *)got;
    int all_ok = 1;
    for (int i = 0; i < 12; i++) {
        if (!compare_coeff(opname, vec_name, i, flat[i], expected[i])) {
            all_ok = 0;
        }
    }
    return all_ok;
}

int main(void) {
    int total = 0, passed = 0;
    int failed_mul = 0, failed_sqr = 0, failed_eq = 0, failed_bytes = 0;

    Fp12 a, b, r;

    printf("==== fp12.c host-side tests (BLS12-381) ====\n\n");

    for (int i = 0; i < NUM_FP12_TEST_VECTORS; i++) {
        const fp12_test_vec_t *v = &FP12_TEST_VECTORS[i];

        load_fp12(&a, v->a);
        load_fp12(&b, v->b);

        /* fp12_mul */
        fp12_mul(&r, &a, &b);
        if (check_fp12("fp12_mul", v->name, &r, v->prod)) passed++;
        else failed_mul++;
        total++;

        /* fp12_sqr */
        fp12_sqr(&r, &a);
        if (check_fp12("fp12_sqr", v->name, &r, v->sqr_a)) passed++;
        else failed_sqr++;
        total++;

        /* eq sanity: a == a */
        Fp12 a_copy;
        fp12_copy(&a_copy, &a);
        if (fp12_eq(&a, &a_copy)) {
            passed++;
        } else {
            printf("  ✗ FAIL: fp12_eq(a,a) returned 0 on %s\n", v->name);
            failed_eq++;
        }
        total++;

        /* bytes roundtrip */
        uint8_t buf[576];
        Fp12 r2;
        fp12_to_bytes(buf, &a);
        fp12_from_bytes(&r2, buf);
        if (check_fp12("bytes roundtrip", v->name, &r2, v->a)) passed++;
        else failed_bytes++;
        total++;
    }

    /* Additional sanity: fp12_one * a = a, for the first random vector */
    {
        const fp12_test_vec_t *v = &FP12_TEST_VECTORS[4];  /* first random */
        Fp12 one, prod;
        fp12_one(&one);
        load_fp12(&a, v->a);
        fp12_mul(&prod, &one, &a);
        if (check_fp12("fp12_one * a == a", v->name, &prod, v->a)) passed++;
        else failed_mul++;
        total++;
    }

    printf("\n==== Summary ====\n");
    printf("  Total tests:    %d\n", total);
    printf("  Passed:         %d\n", passed);
    printf("  Failed:         %d\n", total - passed);
    printf("\n  By operation:\n");
    printf("    fp12_mul:        %s (%d failures)\n", failed_mul ? "FAIL" : "OK", failed_mul);
    printf("    fp12_sqr:        %s (%d failures)\n", failed_sqr ? "FAIL" : "OK", failed_sqr);
    printf("    fp12_eq:         %s (%d failures)\n", failed_eq ? "FAIL" : "OK", failed_eq);
    printf("    bytes:           %s (%d failures)\n", failed_bytes ? "FAIL" : "OK", failed_bytes);

    if (passed == total) {
        printf("\n✓✓✓ ALL TESTS PASS — fp12.c COMPLETE for BLS12-381 ✓✓✓\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed\n");
        return 1;
    }
}
