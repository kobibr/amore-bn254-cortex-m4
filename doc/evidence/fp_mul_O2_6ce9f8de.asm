
/home/kobi/git_backups/binaries/main_O2_6ce9f8de.elf:     file format elf32-littlearm


Disassembly of section .text:

08000a10 <fp_mul>:
 *
 * Loop invariant: at the start of iteration i, T[N+1..0] holds an
 * intermediate value that is always less than 2*p, so T[N+1] = 0
 * (T[N] may be 0 or 1 only).
 */
void fp_mul(Fp r, const Fp a, const Fp b) {
 8000a10:	e92d 4ff0 	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
 8000a14:	ed2d 8b02 	vpush	{d8}
 8000a18:	b0af      	sub	sp, #188	@ 0xbc
    /* Accumulator: FP_LIMBS + 2 words.
     * Each cell holds 32 valid bits; uint64_t gives headroom for carry. */
    uint64_t T[FP_LIMBS + 2];
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;
 8000a1a:	af12      	add	r7, sp, #72	@ 0x48
void fp_mul(Fp r, const Fp a, const Fp b) {
 8000a1c:	4614      	mov	r4, r2
 8000a1e:	460e      	mov	r6, r1
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;
 8000a20:	2270      	movs	r2, #112	@ 0x70
void fp_mul(Fp r, const Fp a, const Fp b) {
 8000a22:	9005      	str	r0, [sp, #20]
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;
 8000a24:	2100      	movs	r1, #0
 8000a26:	4638      	mov	r0, r7
 8000a28:	f7ff fb4e 	bl	80000c8 <memset>

        /* By construction T[0] is now 0. Shift T right by one word. */
        for (int j = 0; j < FP_LIMBS + 1; j++) {
            T[j] = T[j + 1];
        }
        T[FP_LIMBS + 1] = 0;
 8000a2c:	ed9f 8b52 	vldr	d8, [pc, #328]	@ 8000b78 <fp_mul+0x168>
 8000a30:	f104 022c 	add.w	r2, r4, #44	@ 0x2c
 8000a34:	1f23      	subs	r3, r4, #4
 8000a36:	9201      	str	r2, [sp, #4]
 8000a38:	1f32      	subs	r2, r6, #4
 8000a3a:	9202      	str	r2, [sp, #8]
 8000a3c:	362c      	adds	r6, #44	@ 0x2c
        uint64_t carry = 0;
 8000a3e:	2400      	movs	r4, #0
 8000a40:	ad2a      	add	r5, sp, #168	@ 0xa8
 8000a42:	469b      	mov	fp, r3
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 8000a44:	f85b 8f04 	ldr.w	r8, [fp, #4]!
 8000a48:	f8dd c008 	ldr.w	ip, [sp, #8]
 8000a4c:	9704      	str	r7, [sp, #16]
 8000a4e:	4638      	mov	r0, r7
        uint64_t carry = 0;
 8000a50:	2300      	movs	r3, #0
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 8000a52:	e9d0 2100 	ldrd	r2, r1, [r0]
 8000a56:	f85c ef04 	ldr.w	lr, [ip, #4]!
 8000a5a:	fbee 2108 	umlal	r2, r1, lr, r8
 8000a5e:	18d2      	adds	r2, r2, r3
            T[j]  = x & 0xFFFFFFFFu;
 8000a60:	f840 2b08 	str.w	r2, [r0], #8
            carry = x >> 32;
 8000a64:	f141 0300 	adc.w	r3, r1, #0
        for (int j = 0; j < FP_LIMBS; j++) {
 8000a68:	4566      	cmp	r6, ip
            T[j]  = x & 0xFFFFFFFFu;
 8000a6a:	f840 4c04 	str.w	r4, [r0, #-4]
        for (int j = 0; j < FP_LIMBS; j++) {
 8000a6e:	d1f0      	bne.n	8000a52 <fp_mul+0x42>
        T[FP_LIMBS] += carry;
 8000a70:	9a2a      	ldr	r2, [sp, #168]	@ 0xa8
        uint32_t k = (uint32_t)T[0] * CURVE_N0P_FP;
 8000a72:	9912      	ldr	r1, [sp, #72]	@ 0x48
 8000a74:	4842      	ldr	r0, [pc, #264]	@ (8000b80 <fp_mul+0x170>)
 8000a76:	9003      	str	r0, [sp, #12]
        T[FP_LIMBS] += carry;
 8000a78:	189b      	adds	r3, r3, r2
 8000a7a:	9a2b      	ldr	r2, [sp, #172]	@ 0xac
 8000a7c:	f142 0a00 	adc.w	sl, r2, #0
        uint32_t k = (uint32_t)T[0] * CURVE_N0P_FP;
 8000a80:	ebc1 3281 	rsb	r2, r1, r1, lsl #14
 8000a84:	eb02 4202 	add.w	r2, r2, r2, lsl #16
 8000a88:	eb01 0e82 	add.w	lr, r1, r2, lsl #2
 8000a8c:	4681      	mov	r9, r0
 8000a8e:	46bc      	mov	ip, r7
        carry = 0;
 8000a90:	2200      	movs	r2, #0
 8000a92:	4698      	mov	r8, r3
            uint64_t x = T[j] + (uint64_t)CURVE_P[j] * k + carry;
 8000a94:	e9dc 1000 	ldrd	r1, r0, [ip]
 8000a98:	f859 3b04 	ldr.w	r3, [r9], #4
 8000a9c:	fbe3 100e 	umlal	r1, r0, r3, lr
 8000aa0:	1889      	adds	r1, r1, r2
            T[j]  = x & 0xFFFFFFFFu;
 8000aa2:	f84c 1b08 	str.w	r1, [ip], #8
            carry = x >> 32;
 8000aa6:	f140 0200 	adc.w	r2, r0, #0
        for (int j = 0; j < FP_LIMBS; j++) {
 8000aaa:	4565      	cmp	r5, ip
            T[j]  = x & 0xFFFFFFFFu;
 8000aac:	f84c 4c04 	str.w	r4, [ip, #-4]
        for (int j = 0; j < FP_LIMBS; j++) {
 8000ab0:	d1f0      	bne.n	8000a94 <fp_mul+0x84>
        T[FP_LIMBS] += carry;
 8000ab2:	4643      	mov	r3, r8
 8000ab4:	189b      	adds	r3, r3, r2
 8000ab6:	f144 0900 	adc.w	r9, r4, #0
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 8000aba:	eb19 090a 	adds.w	r9, r9, sl
 8000abe:	f144 0a00 	adc.w	sl, r4, #0
            T[j] = T[j + 1];
 8000ac2:	2268      	movs	r2, #104	@ 0x68
 8000ac4:	a914      	add	r1, sp, #80	@ 0x50
 8000ac6:	4638      	mov	r0, r7
        T[FP_LIMBS] &= 0xFFFFFFFFu;
 8000ac8:	e9cd 342a 	strd	r3, r4, [sp, #168]	@ 0xa8
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 8000acc:	f8cd 90b0 	str.w	r9, [sp, #176]	@ 0xb0
 8000ad0:	f8cd a0b4 	str.w	sl, [sp, #180]	@ 0xb4
            T[j] = T[j + 1];
 8000ad4:	f7ff fcc2 	bl	800045c <memmove>
    for (int i = 0; i < FP_LIMBS; i++) {
 8000ad8:	9b01      	ldr	r3, [sp, #4]
 8000ada:	455b      	cmp	r3, fp
        T[FP_LIMBS + 1] = 0;
 8000adc:	ed8d 8b2c 	vstr	d8, [sp, #176]	@ 0xb0
    for (int i = 0; i < FP_LIMBS; i++) {
 8000ae0:	d1b0      	bne.n	8000a44 <fp_mul+0x34>
 8000ae2:	e9dd 8b03 	ldrd	r8, fp, [sp, #12]
 8000ae6:	ac06      	add	r4, sp, #24
 8000ae8:	4620      	mov	r0, r4
 8000aea:	4623      	mov	r3, r4

    /* T now holds a value in [0, 2p). Final conditional subtract. */
    /* Pack T[0..FP_LIMBS-1] into a uint32_t array for fp_cmp / fp_raw_sub */
    uint32_t result[FP_LIMBS];
    for (int i = 0; i < FP_LIMBS; i++) {
        result[i] = (uint32_t)T[i];
 8000aec:	f85b 2b08 	ldr.w	r2, [fp], #8
 8000af0:	f843 2b04 	str.w	r2, [r3], #4
    for (int i = 0; i < FP_LIMBS; i++) {
 8000af4:	455d      	cmp	r5, fp
 8000af6:	d1f9      	bne.n	8000aec <fp_mul+0xdc>
    }

    /* If T[FP_LIMBS] != 0, definitely >= p (since p fits in FP_LIMBS words).
     * Otherwise check result vs p. */
    if (T[FP_LIMBS] != 0 || fp_cmp(result, CURVE_P) >= 0) {
 8000af8:	ea59 0a0a 	orrs.w	sl, r9, sl
 8000afc:	d10a      	bne.n	8000b14 <fp_mul+0x104>
 8000afe:	4a21      	ldr	r2, [pc, #132]	@ (8000b84 <fp_mul+0x174>)
 8000b00:	463b      	mov	r3, r7
        if (a[i] < b[i]) return -1;
 8000b02:	f853 5d04 	ldr.w	r5, [r3, #-4]!
 8000b06:	f852 1d04 	ldr.w	r1, [r2, #-4]!
 8000b0a:	428d      	cmp	r5, r1
 8000b0c:	d31d      	bcc.n	8000b4a <fp_mul+0x13a>
        if (a[i] > b[i]) return  1;
 8000b0e:	d801      	bhi.n	8000b14 <fp_mul+0x104>
    for (int i = FP_LIMBS - 1; i >= 0; i--) {
 8000b10:	429c      	cmp	r4, r3
 8000b12:	d1f6      	bne.n	8000b02 <fp_mul+0xf2>
 8000b14:	9b05      	ldr	r3, [sp, #20]
 8000b16:	1f1c      	subs	r4, r3, #4
 8000b18:	f103 052c 	add.w	r5, r3, #44	@ 0x2c
    for (int i = 0; i < FP_LIMBS; i++) {
 8000b1c:	2300      	movs	r3, #0
 8000b1e:	4619      	mov	r1, r3
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8000b20:	f858 3b04 	ldr.w	r3, [r8], #4
 8000b24:	f850 2b04 	ldr.w	r2, [r0], #4
 8000b28:	1ad2      	subs	r2, r2, r3
 8000b2a:	eb63 0303 	sbc.w	r3, r3, r3
 8000b2e:	1a52      	subs	r2, r2, r1
        r[i]   = (uint32_t)x;
 8000b30:	f844 2f04 	str.w	r2, [r4, #4]!
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8000b34:	f163 0300 	sbc.w	r3, r3, #0
    for (int i = 0; i < FP_LIMBS; i++) {
 8000b38:	42a5      	cmp	r5, r4
        borrow = (x >> 63) & 1;
 8000b3a:	ea4f 71d3 	mov.w	r1, r3, lsr #31
    for (int i = 0; i < FP_LIMBS; i++) {
 8000b3e:	d1ef      	bne.n	8000b20 <fp_mul+0x110>
        fp_raw_sub(r, result, CURVE_P);
    } else {
        memcpy(r, result, sizeof(Fp));
    }
}
 8000b40:	b02f      	add	sp, #188	@ 0xbc
 8000b42:	ecbd 8b02 	vpop	{d8}
 8000b46:	e8bd 8ff0 	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
        memcpy(r, result, sizeof(Fp));
 8000b4a:	9d05      	ldr	r5, [sp, #20]
 8000b4c:	4626      	mov	r6, r4
 8000b4e:	ce0f      	ldmia	r6!, {r0, r1, r2, r3}
 8000b50:	42be      	cmp	r6, r7
 8000b52:	f105 0510 	add.w	r5, r5, #16
 8000b56:	f104 0410 	add.w	r4, r4, #16
 8000b5a:	f845 0c10 	str.w	r0, [r5, #-16]
 8000b5e:	f845 1c0c 	str.w	r1, [r5, #-12]
 8000b62:	f845 2c08 	str.w	r2, [r5, #-8]
 8000b66:	f845 3c04 	str.w	r3, [r5, #-4]
 8000b6a:	d1ef      	bne.n	8000b4c <fp_mul+0x13c>
}
 8000b6c:	b02f      	add	sp, #188	@ 0xbc
 8000b6e:	ecbd 8b02 	vpop	{d8}
 8000b72:	e8bd 8ff0 	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
 8000b76:	bf00      	nop
	...
 8000b80:	08003ad0 	.word	0x08003ad0
 8000b84:	08003b00 	.word	0x08003b00
