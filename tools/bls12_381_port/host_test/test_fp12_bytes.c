/* test_fp12_bytes.c — verify fp12 bytes serialization round-trip
 * 
 * The server sends gamma, rho as 576-byte buffers (12 coefficients × 48 bytes each).
 * Each coefficient is a plain integer in big-endian.
 *
 * Test:
 *   gt → bytes → fp12 → bytes
 *   should equal original bytes
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fp12.h"

int main(void) {
    /* Construct a known plain-form value: gt[0] = 0x1625cbe5b8f9885da3eccb3b15ce... 
     * Per py_ecc, gt.coeffs[0] in plain form. We'll synthesize bytes representing this.
     */
    
    /* Take simple test: bytes representing constant Fp12 = (1, 2, 3, ..., 12)
     * coefficient 0 = 1, coefficient 1 = 2, ..., coefficient 11 = 12 (all plain integers)
     */
    uint8_t bytes_in[576];
    memset(bytes_in, 0, sizeof(bytes_in));
    for (int i = 0; i < 12; i++) {
        /* Each coefficient is 48 bytes, big-endian. Set the last byte = i+1 */
        bytes_in[(i+1) * 48 - 1] = (uint8_t)(i + 1);
    }
    
    /* Convert bytes → fp12 */
    Fp12 a;
    fp12_from_bytes(&a, bytes_in);
    
    /* Convert fp12 → bytes */
    uint8_t bytes_out[576];
    fp12_to_bytes(bytes_out, &a);
    
    /* Compare */
    if (memcmp(bytes_in, bytes_out, 576) == 0) {
        printf("✓ PASS: fp12 bytes round-trip preserves data\n");
        return 0;
    } else {
        printf("✗ FAIL: fp12 bytes round-trip differs\n");
        printf("  First 16 bytes input :  ");
        for (int i = 0; i < 16; i++) printf("%02x ", bytes_in[i]);
        printf("\n  First 16 bytes output:  ");
        for (int i = 0; i < 16; i++) printf("%02x ", bytes_out[i]);
        printf("\n");
        
        /* Find first differing byte */
        for (int i = 0; i < 576; i++) {
            if (bytes_in[i] != bytes_out[i]) {
                printf("  First diff at byte %d: in=0x%02x out=0x%02x\n",
                       i, bytes_in[i], bytes_out[i]);
                printf("  This is coefficient %d, byte-in-coeff %d\n",
                       i / 48, i % 48);
                break;
            }
        }
        return 1;
    }
}
