/* test_g.c — Host-side test harness for g1.c and g2.c
 *
 * Tests:
 *   - g1_scalar_mul: k*G computed in C matches py_ecc reference
 *   - g2_scalar_mul: same for G2
 *   - 2*G == G + G  (doubling consistency)
 *   - q*G == infinity  (group order test, the strongest!)
 *   - G + (-G) == infinity  (negation)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "g1.h"
#include "g2.h"
#include "g_test_vectors.h"

static int compare_fp(const char *opname, const char *vec_name, const char *coord,
                      const uint32_t got[12], const uint32_t expected[12]) {
    if (memcmp(got, expected, 12 * sizeof(uint32_t)) == 0) {
        return 1;
    }
    printf("  ✗ FAIL: %-25s on %s.%s\n", opname, vec_name, coord);
    printf("    got:      ");
    for (int i = 11; i >= 0; i--) printf("%08x", got[i]);
    printf("\n");
    printf("    expected: ");
    for (int i = 11; i >= 0; i--) printf("%08x", expected[i]);
    printf("\n");
    return 0;
}

int main(void) {
    int total = 0, passed = 0;
    int failed_g1_mul = 0, failed_g2_mul = 0;
    int failed_g1_dbl = 0, failed_g2_dbl = 0;
    int failed_g1_order = 0, failed_g2_order = 0;
    int failed_g1_neg = 0, failed_g2_neg = 0;

    printf("==== g1.c / g2.c host-side tests (BLS12-381) ====\n\n");

    /* ============ G1 scalar multiplication tests ============ */
    printf("--- G1 scalar multiplication ---\n");
    for (int i = 0; i < NUM_G1_TEST_VECTORS; i++) {
        const g1_test_vec_t *v = &G1_TEST_VECTORS[i];
        G1Point G, R;
        Fp x_aff, y_aff;

        g1_generator(&G);
        g1_scalar_mul(&R, &G, v->k, v->nbits);
        g1_to_affine(x_aff, y_aff, &R);

        int ok_x = compare_fp("g1_scalar_mul (x)", v->name, "x", x_aff, v->x);
        int ok_y = compare_fp("g1_scalar_mul (y)", v->name, "y", y_aff, v->y);
        if (ok_x && ok_y) passed++;
        else failed_g1_mul++;
        total++;
    }

    /* ============ G2 scalar multiplication tests ============ */
    printf("\n--- G2 scalar multiplication ---\n");
    for (int i = 0; i < NUM_G2_TEST_VECTORS; i++) {
        const g2_test_vec_t *v = &G2_TEST_VECTORS[i];
        G2Point G, R;
        Fp2 x_aff, y_aff;

        g2_generator(&G);
        g2_scalar_mul(&R, &G, v->k, v->nbits);
        g2_to_affine(&x_aff, &y_aff, &R);

        int ok_x_c0 = compare_fp("g2_scalar_mul (x.c0)", v->name, "x.c0", x_aff.c0, v->x_c0);
        int ok_x_c1 = compare_fp("g2_scalar_mul (x.c1)", v->name, "x.c1", x_aff.c1, v->x_c1);
        int ok_y_c0 = compare_fp("g2_scalar_mul (y.c0)", v->name, "y.c0", y_aff.c0, v->y_c0);
        int ok_y_c1 = compare_fp("g2_scalar_mul (y.c1)", v->name, "y.c1", y_aff.c1, v->y_c1);
        if (ok_x_c0 && ok_x_c1 && ok_y_c0 && ok_y_c1) passed++;
        else failed_g2_mul++;
        total++;
    }

    /* ============ G1 doubling consistency: 2*G == G + G ============ */
    printf("\n--- G1 doubling consistency ---\n");
    {
        G1Point G, doubled, summed;
        g1_generator(&G);
        g1_dbl(&doubled, &G);
        g1_add(&summed, &G, &G);

        Fp dx, dy, sx, sy;
        g1_to_affine(dx, dy, &doubled);
        g1_to_affine(sx, sy, &summed);

        int ok_x = compare_fp("g1: 2G vs G+G", "doubling", "x", dx, sx);
        int ok_y = compare_fp("g1: 2G vs G+G", "doubling", "y", dy, sy);
        if (ok_x && ok_y) {
            printf("  ✓ g1_dbl(G) == g1_add(G, G)\n");
            passed++;
        } else {
            failed_g1_dbl++;
        }
        total++;
    }

    /* ============ G2 doubling consistency ============ */
    printf("\n--- G2 doubling consistency ---\n");
    {
        G2Point G, doubled, summed;
        g2_generator(&G);
        g2_dbl(&doubled, &G);
        g2_add(&summed, &G, &G);

        Fp2 dx, dy, sx, sy;
        g2_to_affine(&dx, &dy, &doubled);
        g2_to_affine(&sx, &sy, &summed);

        int ok = compare_fp("g2: 2G vs G+G", "doubling", "x.c0", dx.c0, sx.c0)
              && compare_fp("g2: 2G vs G+G", "doubling", "x.c1", dx.c1, sx.c1)
              && compare_fp("g2: 2G vs G+G", "doubling", "y.c0", dy.c0, sy.c0)
              && compare_fp("g2: 2G vs G+G", "doubling", "y.c1", dy.c1, sy.c1);
        if (ok) {
            printf("  ✓ g2_dbl(G) == g2_add(G, G)\n");
            passed++;
        } else {
            failed_g2_dbl++;
        }
        total++;
    }

    /* ============ G1 negation: G + (-G) == infinity ============ */
    printf("\n--- G1 negation ---\n");
    {
        G1Point G, negG, sum;
        g1_generator(&G);
        g1_neg(&negG, &G);
        g1_add(&sum, &G, &negG);
        if (g1_is_inf(&sum)) {
            printf("  ✓ G + (-G) = infinity\n");
            passed++;
        } else {
            printf("  ✗ G + (-G) != infinity\n");
            failed_g1_neg++;
        }
        total++;
    }

    /* ============ G2 negation ============ */
    printf("\n--- G2 negation ---\n");
    {
        G2Point G, negG, sum;
        g2_generator(&G);
        g2_neg(&negG, &G);
        g2_add(&sum, &G, &negG);
        if (g2_is_inf(&sum)) {
            printf("  ✓ G + (-G) = infinity\n");
            passed++;
        } else {
            printf("  ✗ G + (-G) != infinity\n");
            failed_g2_neg++;
        }
        total++;
    }

    /* ============ G1 group order: q * G == infinity (CRITICAL TEST) ============ */
    printf("\n--- G1 group order test ---\n");
    {
        G1Point G, R;
        g1_generator(&G);
        printf("  Computing q * G1 (255-bit scalar mul, may take a few seconds)...\n");
        fflush(stdout);
        g1_scalar_mul(&R, &G, BLS_Q_8LIMB, 255);
        if (g1_is_inf(&R)) {
            printf("  ✓ q * G1 = infinity (G1 is in the correct subgroup)\n");
            passed++;
        } else {
            printf("  ✗ q * G1 != infinity — group order MISMATCH!\n");
            failed_g1_order++;
        }
        total++;
    }

    /* ============ G2 group order: q * G == infinity ============ */
    printf("\n--- G2 group order test ---\n");
    {
        G2Point G, R;
        g2_generator(&G);
        printf("  Computing q * G2 (slower because of Fp2 ops)...\n");
        fflush(stdout);
        g2_scalar_mul(&R, &G, BLS_Q_8LIMB, 255);
        if (g2_is_inf(&R)) {
            printf("  ✓ q * G2 = infinity (G2 is in the correct subgroup)\n");
            passed++;
        } else {
            printf("  ✗ q * G2 != infinity — group order MISMATCH!\n");
            failed_g2_order++;
        }
        total++;
    }

    /* ============ Summary ============ */
    printf("\n==== Summary ====\n");
    printf("  Total tests:    %d\n", total);
    printf("  Passed:         %d\n", passed);
    printf("  Failed:         %d\n", total - passed);
    printf("\n  By operation:\n");
    printf("    g1_scalar_mul:  %s (%d failures)\n", failed_g1_mul ? "FAIL" : "OK", failed_g1_mul);
    printf("    g2_scalar_mul:  %s (%d failures)\n", failed_g2_mul ? "FAIL" : "OK", failed_g2_mul);
    printf("    g1_dbl == add:  %s (%d failures)\n", failed_g1_dbl ? "FAIL" : "OK", failed_g1_dbl);
    printf("    g2_dbl == add:  %s (%d failures)\n", failed_g2_dbl ? "FAIL" : "OK", failed_g2_dbl);
    printf("    g1_neg:         %s (%d failures)\n", failed_g1_neg ? "FAIL" : "OK", failed_g1_neg);
    printf("    g2_neg:         %s (%d failures)\n", failed_g2_neg ? "FAIL" : "OK", failed_g2_neg);
    printf("    g1 group order: %s (%d failures)\n", failed_g1_order ? "FAIL" : "OK", failed_g1_order);
    printf("    g2 group order: %s (%d failures)\n", failed_g2_order ? "FAIL" : "OK", failed_g2_order);

    if (passed == total) {
        printf("\n✓✓✓ ALL TESTS PASS — g1.c and g2.c COMPLETE for BLS12-381 ✓✓✓\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed\n");
        return 1;
    }
}
