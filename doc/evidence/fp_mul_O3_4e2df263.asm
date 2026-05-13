
/home/kobi/git_backups/binaries/main_O3_4e2df263.elf:     file format elf32-littlearm


Disassembly of section .text:

08001070 <fp_mul>:
 *
 * Loop invariant: at the start of iteration i, T[N+1..0] holds an
 * intermediate value that is always less than 2*p, so T[N+1] = 0
 * (T[N] may be 0 or 1 only).
 */
void fp_mul(Fp r, const Fp a, const Fp b) {
 8001070:	e92d 4ff0 	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
 8001074:	b0c9      	sub	sp, #292	@ 0x124

    for (int i = 0; i < FP_LIMBS; i++) {
        /* Step 1: T += a * b[i] */
        uint64_t carry = 0;
        for (int j = 0; j < FP_LIMBS; j++) {
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 8001076:	680c      	ldr	r4, [r1, #0]
void fp_mul(Fp r, const Fp a, const Fp b) {
 8001078:	901f      	str	r0, [sp, #124]	@ 0x7c
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 800107a:	6848      	ldr	r0, [r1, #4]
 800107c:	9011      	str	r0, [sp, #68]	@ 0x44
 800107e:	6888      	ldr	r0, [r1, #8]
 8001080:	9012      	str	r0, [sp, #72]	@ 0x48
 8001082:	68c8      	ldr	r0, [r1, #12]
 8001084:	9013      	str	r0, [sp, #76]	@ 0x4c
 8001086:	6908      	ldr	r0, [r1, #16]
 8001088:	9014      	str	r0, [sp, #80]	@ 0x50
 800108a:	6948      	ldr	r0, [r1, #20]
 800108c:	9015      	str	r0, [sp, #84]	@ 0x54
 800108e:	6988      	ldr	r0, [r1, #24]
 8001090:	9016      	str	r0, [sp, #88]	@ 0x58
 8001092:	69c8      	ldr	r0, [r1, #28]
 8001094:	9017      	str	r0, [sp, #92]	@ 0x5c
 8001096:	6a08      	ldr	r0, [r1, #32]
 8001098:	9018      	str	r0, [sp, #96]	@ 0x60
 800109a:	6a48      	ldr	r0, [r1, #36]	@ 0x24
 800109c:	9019      	str	r0, [sp, #100]	@ 0x64
 800109e:	6a88      	ldr	r0, [r1, #40]	@ 0x28
 80010a0:	6ac9      	ldr	r1, [r1, #44]	@ 0x2c
 80010a2:	911b      	str	r1, [sp, #108]	@ 0x6c
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;
 80010a4:	2300      	movs	r3, #0
 80010a6:	1f11      	subs	r1, r2, #4
 80010a8:	322c      	adds	r2, #44	@ 0x2c
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 80010aa:	901a      	str	r0, [sp, #104]	@ 0x68
 80010ac:	9101      	str	r1, [sp, #4]
    for (int i = 0; i <= FP_LIMBS + 1; i++) T[i] = 0;
 80010ae:	e9cd 3344 	strd	r3, r3, [sp, #272]	@ 0x110
 80010b2:	e9cd 3346 	strd	r3, r3, [sp, #280]	@ 0x118
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 80010b6:	9410      	str	r4, [sp, #64]	@ 0x40
 80010b8:	9302      	str	r3, [sp, #8]
 80010ba:	921c      	str	r2, [sp, #112]	@ 0x70
 80010bc:	e9cd 3303 	strd	r3, r3, [sp, #12]
 80010c0:	e9cd 3306 	strd	r3, r3, [sp, #24]
 80010c4:	e9cd 3308 	strd	r3, r3, [sp, #32]
 80010c8:	461f      	mov	r7, r3
 80010ca:	461a      	mov	r2, r3
 80010cc:	4619      	mov	r1, r3
 80010ce:	4618      	mov	r0, r3
 80010d0:	9305      	str	r3, [sp, #20]
 80010d2:	469c      	mov	ip, r3
 80010d4:	469b      	mov	fp, r3
 80010d6:	461e      	mov	r6, r3
 80010d8:	9b01      	ldr	r3, [sp, #4]
 80010da:	9d10      	ldr	r5, [sp, #64]	@ 0x40
 80010dc:	f853 af04 	ldr.w	sl, [r3, #4]!
 80010e0:	9301      	str	r3, [sp, #4]
 80010e2:	46de      	mov	lr, fp
 80010e4:	fbe5 6e0a 	umlal	r6, lr, r5, sl
            T[j]  = x & 0xFFFFFFFFu;
            carry = x >> 32;
 80010e8:	f04f 0b00 	mov.w	fp, #0
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 80010ec:	46b1      	mov	r9, r6
 80010ee:	9e11      	ldr	r6, [sp, #68]	@ 0x44
 80010f0:	465c      	mov	r4, fp
 80010f2:	fbea e406 	umlal	lr, r4, sl, r6
 80010f6:	4623      	mov	r3, r4
 80010f8:	9c08      	ldr	r4, [sp, #32]
 80010fa:	9e12      	ldr	r6, [sp, #72]	@ 0x48
 80010fc:	eb1e 0e04 	adds.w	lr, lr, r4
 8001100:	f143 0400 	adc.w	r4, r3, #0
 8001104:	46d8      	mov	r8, fp
 8001106:	fbea 4806 	umlal	r4, r8, sl, r6
 800110a:	eb1c 0404 	adds.w	r4, ip, r4
 800110e:	9e13      	ldr	r6, [sp, #76]	@ 0x4c
 8001110:	f148 0c00 	adc.w	ip, r8, #0
 8001114:	46d8      	mov	r8, fp
 8001116:	fbea c806 	umlal	ip, r8, sl, r6
 800111a:	9e05      	ldr	r6, [sp, #20]
 800111c:	eb16 0c0c 	adds.w	ip, r6, ip
 8001120:	9e14      	ldr	r6, [sp, #80]	@ 0x50
 8001122:	f148 0300 	adc.w	r3, r8, #0
 8001126:	465d      	mov	r5, fp
 8001128:	fbea 3506 	umlal	r3, r5, sl, r6
 800112c:	18c0      	adds	r0, r0, r3
 800112e:	9e15      	ldr	r6, [sp, #84]	@ 0x54
 8001130:	f145 0500 	adc.w	r5, r5, #0
 8001134:	46d8      	mov	r8, fp
 8001136:	fbea 5806 	umlal	r5, r8, sl, r6
 800113a:	1949      	adds	r1, r1, r5
 800113c:	9e16      	ldr	r6, [sp, #88]	@ 0x58
 800113e:	9105      	str	r1, [sp, #20]
 8001140:	f148 0300 	adc.w	r3, r8, #0
 8001144:	46d8      	mov	r8, fp
 8001146:	fbea 3806 	umlal	r3, r8, sl, r6
 800114a:	18d3      	adds	r3, r2, r3
 800114c:	9a17      	ldr	r2, [sp, #92]	@ 0x5c
 800114e:	9e19      	ldr	r6, [sp, #100]	@ 0x64
 8001150:	f148 0800 	adc.w	r8, r8, #0
 8001154:	4659      	mov	r1, fp
 8001156:	fbea 8102 	umlal	r8, r1, sl, r2
 800115a:	eb17 0808 	adds.w	r8, r7, r8
 800115e:	9a18      	ldr	r2, [sp, #96]	@ 0x60
 8001160:	f141 0500 	adc.w	r5, r1, #0
 8001164:	465f      	mov	r7, fp
 8001166:	fbea 5702 	umlal	r5, r7, sl, r2
 800116a:	9a09      	ldr	r2, [sp, #36]	@ 0x24
 800116c:	1955      	adds	r5, r2, r5
 800116e:	f147 0700 	adc.w	r7, r7, #0
 8001172:	465a      	mov	r2, fp
 8001174:	fbea 7206 	umlal	r7, r2, sl, r6
 8001178:	9e06      	ldr	r6, [sp, #24]
 800117a:	19f7      	adds	r7, r6, r7
 800117c:	9e1a      	ldr	r6, [sp, #104]	@ 0x68
 800117e:	f142 0200 	adc.w	r2, r2, #0
 8001182:	4659      	mov	r1, fp
 8001184:	fbea 2106 	umlal	r2, r1, sl, r6
 8001188:	460e      	mov	r6, r1
 800118a:	9907      	ldr	r1, [sp, #28]
 800118c:	188a      	adds	r2, r1, r2
 800118e:	991b      	ldr	r1, [sp, #108]	@ 0x6c
 8001190:	9206      	str	r2, [sp, #24]
 8001192:	f146 0600 	adc.w	r6, r6, #0
 8001196:	465a      	mov	r2, fp
 8001198:	fbea 6201 	umlal	r6, r2, sl, r1
 800119c:	9904      	ldr	r1, [sp, #16]
 800119e:	198e      	adds	r6, r1, r6
        }
        T[FP_LIMBS] += carry;
 80011a0:	9902      	ldr	r1, [sp, #8]
            uint64_t x = T[j] + (uint64_t)a[j] * b[i] + carry;
 80011a2:	9604      	str	r6, [sp, #16]
 80011a4:	f142 0200 	adc.w	r2, r2, #0
        T[FP_LIMBS] += carry;
 80011a8:	1851      	adds	r1, r2, r1
 80011aa:	9a03      	ldr	r2, [sp, #12]
         *   k = T[0] * (-p^{-1}) mod 2^32 = T[0] * N0_P mod 2^32
         */
        uint32_t k = (uint32_t)T[0] * CURVE_N0P_FP;
        carry = 0;
        for (int j = 0; j < FP_LIMBS; j++) {
            uint64_t x = T[j] + (uint64_t)CURVE_P[j] * k + carry;
 80011ac:	4ebd      	ldr	r6, [pc, #756]	@ (80014a4 <fp_mul+0x434>)
        T[FP_LIMBS] += carry;
 80011ae:	9102      	str	r1, [sp, #8]
 80011b0:	f142 0200 	adc.w	r2, r2, #0
 80011b4:	9203      	str	r2, [sp, #12]
        uint32_t k = (uint32_t)T[0] * CURVE_N0P_FP;
 80011b6:	ebc9 3289 	rsb	r2, r9, r9, lsl #14
 80011ba:	eb02 4202 	add.w	r2, r2, r2, lsl #16
 80011be:	eb09 0a82 	add.w	sl, r9, r2, lsl #2
            uint64_t x = T[j] + (uint64_t)CURVE_P[j] * k + carry;
 80011c2:	4ab9      	ldr	r2, [pc, #740]	@ (80014a8 <fp_mul+0x438>)
 80011c4:	4649      	mov	r1, r9
 80011c6:	46d9      	mov	r9, fp
 80011c8:	fbea 1902 	umlal	r1, r9, sl, r2
 80011cc:	465a      	mov	r2, fp
 80011ce:	fbea e206 	umlal	lr, r2, sl, r6
 80011d2:	eb19 060e 	adds.w	r6, r9, lr
 80011d6:	960a      	str	r6, [sp, #40]	@ 0x28
 80011d8:	4eb4      	ldr	r6, [pc, #720]	@ (80014ac <fp_mul+0x43c>)
 80011da:	49b5      	ldr	r1, [pc, #724]	@ (80014b0 <fp_mul+0x440>)
 80011dc:	46de      	mov	lr, fp
 80011de:	fbea 4e06 	umlal	r4, lr, sl, r6
 80011e2:	f142 0200 	adc.w	r2, r2, #0
 80011e6:	18a4      	adds	r4, r4, r2
 80011e8:	46d9      	mov	r9, fp
 80011ea:	fbea c901 	umlal	ip, r9, sl, r1
 80011ee:	f14e 0e00 	adc.w	lr, lr, #0
 80011f2:	49b0      	ldr	r1, [pc, #704]	@ (80014b4 <fp_mul+0x444>)
 80011f4:	940b      	str	r4, [sp, #44]	@ 0x2c
 80011f6:	eb1c 0c0e 	adds.w	ip, ip, lr
 80011fa:	46de      	mov	lr, fp
 80011fc:	fbea 0e01 	umlal	r0, lr, sl, r1
 8001200:	f149 0400 	adc.w	r4, r9, #0
 8001204:	1900      	adds	r0, r0, r4
 8001206:	49ac      	ldr	r1, [pc, #688]	@ (80014b8 <fp_mul+0x448>)
 8001208:	900d      	str	r0, [sp, #52]	@ 0x34
 800120a:	f8cd c030 	str.w	ip, [sp, #48]	@ 0x30
 800120e:	9c05      	ldr	r4, [sp, #20]
 8001210:	4658      	mov	r0, fp
 8001212:	fbea 4001 	umlal	r4, r0, sl, r1
 8001216:	f14e 0200 	adc.w	r2, lr, #0
 800121a:	18a1      	adds	r1, r4, r2
 800121c:	460c      	mov	r4, r1
 800121e:	911e      	str	r1, [sp, #120]	@ 0x78
 8001220:	f140 0100 	adc.w	r1, r0, #0
 8001224:	48a5      	ldr	r0, [pc, #660]	@ (80014bc <fp_mul+0x44c>)
 8001226:	46de      	mov	lr, fp
 8001228:	fbea 3e00 	umlal	r3, lr, sl, r0
 800122c:	f100 40e2 	add.w	r0, r0, #1895825408	@ 0x71000000
 8001230:	1859      	adds	r1, r3, r1
 8001232:	f5a0 205c 	sub.w	r0, r0, #901120	@ 0xdc000
 8001236:	f14e 0200 	adc.w	r2, lr, #0
 800123a:	f2a0 703b 	subw	r0, r0, #1851	@ 0x73b
 800123e:	46de      	mov	lr, fp
 8001240:	fbea 8e00 	umlal	r8, lr, sl, r0
 8001244:	eb18 0802 	adds.w	r8, r8, r2
 8001248:	489d      	ldr	r0, [pc, #628]	@ (80014c0 <fp_mul+0x450>)
 800124a:	910f      	str	r1, [sp, #60]	@ 0x3c
 800124c:	f14e 0300 	adc.w	r3, lr, #0
 8001250:	46de      	mov	lr, fp
 8001252:	fbea 5e00 	umlal	r5, lr, sl, r0
 8001256:	18eb      	adds	r3, r5, r3
 8001258:	4d9a      	ldr	r5, [pc, #616]	@ (80014c4 <fp_mul+0x454>)
 800125a:	930e      	str	r3, [sp, #56]	@ 0x38
 800125c:	f14e 0200 	adc.w	r2, lr, #0
 8001260:	46de      	mov	lr, fp
 8001262:	fbea 7e05 	umlal	r7, lr, sl, r5
 8001266:	f1a0 601c 	sub.w	r0, r0, #163577856	@ 0x9c00000
 800126a:	18bd      	adds	r5, r7, r2
 800126c:	f5a0 203c 	sub.w	r0, r0, #770048	@ 0xbc000
 8001270:	9f06      	ldr	r7, [sp, #24]
 8001272:	951d      	str	r5, [sp, #116]	@ 0x74
 8001274:	f14e 0300 	adc.w	r3, lr, #0
 8001278:	f2a0 603d 	subw	r0, r0, #1597	@ 0x63d
 800127c:	46de      	mov	lr, fp
 800127e:	fbea 7e00 	umlal	r7, lr, sl, r0
 8001282:	eb17 0903 	adds.w	r9, r7, r3
 8001286:	9f04      	ldr	r7, [sp, #16]
 8001288:	4b8f      	ldr	r3, [pc, #572]	@ (80014c8 <fp_mul+0x458>)
            T[j]  = x & 0xFFFFFFFFu;
 800128a:	9509      	str	r5, [sp, #36]	@ 0x24
            uint64_t x = T[j] + (uint64_t)CURVE_P[j] * k + carry;
 800128c:	4672      	mov	r2, lr
 800128e:	46de      	mov	lr, fp
 8001290:	fbea 7e03 	umlal	r7, lr, sl, r3
 8001294:	f142 0200 	adc.w	r2, r2, #0
 8001298:	18bb      	adds	r3, r7, r2
            carry = x >> 32;
        }
        T[FP_LIMBS] += carry;
 800129a:	9f02      	ldr	r7, [sp, #8]
    for (int i = 0; i < FP_LIMBS; i++) {
 800129c:	9d01      	ldr	r5, [sp, #4]
            uint64_t x = T[j] + (uint64_t)CURVE_P[j] * k + carry;
 800129e:	f14e 0e00 	adc.w	lr, lr, #0
        T[FP_LIMBS] += carry;
 80012a2:	eb17 0e0e 	adds.w	lr, r7, lr
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 80012a6:	9f03      	ldr	r7, [sp, #12]
        T[FP_LIMBS] += carry;
 80012a8:	eb4b 020b 	adc.w	r2, fp, fp
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 80012ac:	18bf      	adds	r7, r7, r2
            T[j]  = x & 0xFFFFFFFFu;
 80012ae:	4620      	mov	r0, r4
 80012b0:	e9dd 620a 	ldrd	r6, r2, [sp, #40]	@ 0x28
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 80012b4:	eb4b 040b 	adc.w	r4, fp, fp
        T[FP_LIMBS] &= 0xFFFFFFFFu;
 80012b8:	e9cd 4e03 	strd	r4, lr, [sp, #12]
    for (int i = 0; i < FP_LIMBS; i++) {
 80012bc:	9c1c      	ldr	r4, [sp, #112]	@ 0x70
            T[j]  = x & 0xFFFFFFFFu;
 80012be:	9208      	str	r2, [sp, #32]
    for (int i = 0; i < FP_LIMBS; i++) {
 80012c0:	42a5      	cmp	r5, r4
            T[j]  = x & 0xFFFFFFFFu;
 80012c2:	9a0d      	ldr	r2, [sp, #52]	@ 0x34
        T[FP_LIMBS + 1] += (T[FP_LIMBS] >> 32);
 80012c4:	9702      	str	r7, [sp, #8]
            T[j]  = x & 0xFFFFFFFFu;
 80012c6:	9205      	str	r2, [sp, #20]
 80012c8:	e9cd 9306 	strd	r9, r3, [sp, #24]
 80012cc:	9f0e      	ldr	r7, [sp, #56]	@ 0x38
 80012ce:	4642      	mov	r2, r8
    for (int i = 0; i < FP_LIMBS; i++) {
 80012d0:	f47f af02 	bne.w	80010d8 <fp_mul+0x68>
 80012d4:	e9dd 4a08 	ldrd	r4, sl, [sp, #32]
 80012d8:	e9cd b033 	strd	fp, r0, [sp, #204]	@ 0xcc
 80012dc:	f8cd 9004 	str.w	r9, [sp, #4]

    /* T now holds a value in [0, 2p). Final conditional subtract. */
    /* Pack T[0..FP_LIMBS-1] into a uint32_t array for fp_cmp / fp_raw_sub */
    uint32_t result[FP_LIMBS];
    for (int i = 0; i < FP_LIMBS; i++) {
        result[i] = (uint32_t)T[i];
 80012e0:	9024      	str	r0, [sp, #144]	@ 0x90
 80012e2:	4681      	mov	r9, r0
 80012e4:	9806      	ldr	r0, [sp, #24]
 80012e6:	9421      	str	r4, [sp, #132]	@ 0x84
 80012e8:	e9cd b42d 	strd	fp, r4, [sp, #180]	@ 0xb4
 80012ec:	9c05      	ldr	r4, [sp, #20]
 80012ee:	962c      	str	r6, [sp, #176]	@ 0xb0
 80012f0:	9620      	str	r6, [sp, #128]	@ 0x80
 80012f2:	e9cd bc2f 	strd	fp, ip, [sp, #188]	@ 0xbc
 80012f6:	e9cd b135 	strd	fp, r1, [sp, #212]	@ 0xd4
 80012fa:	e9cd b837 	strd	fp, r8, [sp, #220]	@ 0xdc
 80012fe:	e9cd b739 	strd	fp, r7, [sp, #228]	@ 0xe4
 8001302:	e9cd ba3b 	strd	fp, sl, [sp, #236]	@ 0xec
 8001306:	f8cd c088 	str.w	ip, [sp, #136]	@ 0x88
 800130a:	f8cd b0c4 	str.w	fp, [sp, #196]	@ 0xc4
 800130e:	9432      	str	r4, [sp, #200]	@ 0xc8
 8001310:	9423      	str	r4, [sp, #140]	@ 0x8c
 8001312:	9125      	str	r1, [sp, #148]	@ 0x94
 8001314:	f8cd 8098 	str.w	r8, [sp, #152]	@ 0x98
 8001318:	9727      	str	r7, [sp, #156]	@ 0x9c
 800131a:	f8cd b0f4 	str.w	fp, [sp, #244]	@ 0xf4
 800131e:	e9cd 0b3e 	strd	r0, fp, [sp, #248]	@ 0xf8
    }

    /* If T[FP_LIMBS] != 0, definitely >= p (since p fits in FP_LIMBS words).
     * Otherwise check result vs p. */
    if (T[FP_LIMBS] != 0 || fp_cmp(result, CURVE_P) >= 0) {
 8001322:	e9dd 2602 	ldrd	r2, r6, [sp, #8]
 8001326:	4332      	orrs	r2, r6
 8001328:	e9cd 3b40 	strd	r3, fp, [sp, #256]	@ 0x100
        result[i] = (uint32_t)T[i];
 800132c:	e9cd a028 	strd	sl, r0, [sp, #160]	@ 0xa0
 8001330:	e9cd 3e2a 	strd	r3, lr, [sp, #168]	@ 0xa8
 8001334:	463d      	mov	r5, r7
 8001336:	f8cd b10c 	str.w	fp, [sp, #268]	@ 0x10c
    if (T[FP_LIMBS] != 0 || fp_cmp(result, CURVE_P) >= 0) {
 800133a:	f8cd e108 	str.w	lr, [sp, #264]	@ 0x108
 800133e:	d155      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 8001340:	4a62      	ldr	r2, [pc, #392]	@ (80014cc <fp_mul+0x45c>)
 8001342:	4596      	cmp	lr, r2
 8001344:	f240 8101 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 8001348:	3201      	adds	r2, #1
 800134a:	4596      	cmp	lr, r2
 800134c:	d14e      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 800134e:	4a60      	ldr	r2, [pc, #384]	@ (80014d0 <fp_mul+0x460>)
 8001350:	4293      	cmp	r3, r2
 8001352:	f240 80fa 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 8001356:	3201      	adds	r2, #1
 8001358:	4293      	cmp	r3, r2
 800135a:	d147      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 800135c:	4a5d      	ldr	r2, [pc, #372]	@ (80014d4 <fp_mul+0x464>)
 800135e:	4290      	cmp	r0, r2
 8001360:	f240 80f3 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 8001364:	3201      	adds	r2, #1
 8001366:	4290      	cmp	r0, r2
 8001368:	d140      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 800136a:	f102 4278 	add.w	r2, r2, #4160749568	@ 0xf8000000
 800136e:	f502 1240 	add.w	r2, r2, #3145728	@ 0x300000
 8001372:	f502 62a4 	add.w	r2, r2, #1312	@ 0x520
 8001376:	4592      	cmp	sl, r2
 8001378:	f240 80e7 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 800137c:	3201      	adds	r2, #1
 800137e:	4592      	cmp	sl, r2
 8001380:	d134      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 8001382:	4a55      	ldr	r2, [pc, #340]	@ (80014d8 <fp_mul+0x468>)
 8001384:	4297      	cmp	r7, r2
 8001386:	f240 80e0 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 800138a:	3201      	adds	r2, #1
 800138c:	4297      	cmp	r7, r2
 800138e:	d12d      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 8001390:	4a52      	ldr	r2, [pc, #328]	@ (80014dc <fp_mul+0x46c>)
 8001392:	4590      	cmp	r8, r2
 8001394:	f240 80d9 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 8001398:	3201      	adds	r2, #1
 800139a:	4590      	cmp	r8, r2
 800139c:	d126      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 800139e:	4a46      	ldr	r2, [pc, #280]	@ (80014b8 <fp_mul+0x448>)
 80013a0:	990f      	ldr	r1, [sp, #60]	@ 0x3c
 80013a2:	4291      	cmp	r1, r2
 80013a4:	f0c0 80d1 	bcc.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 80013a8:	d120      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 80013aa:	4a4d      	ldr	r2, [pc, #308]	@ (80014e0 <fp_mul+0x470>)
 80013ac:	4591      	cmp	r9, r2
 80013ae:	f240 80cc 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 80013b2:	3201      	adds	r2, #1
 80013b4:	4591      	cmp	r9, r2
 80013b6:	d119      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 80013b8:	4a4a      	ldr	r2, [pc, #296]	@ (80014e4 <fp_mul+0x474>)
 80013ba:	990d      	ldr	r1, [sp, #52]	@ 0x34
 80013bc:	4291      	cmp	r1, r2
 80013be:	f240 80c4 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 80013c2:	3201      	adds	r2, #1
 80013c4:	4291      	cmp	r1, r2
 80013c6:	d111      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 80013c8:	4a38      	ldr	r2, [pc, #224]	@ (80014ac <fp_mul+0x43c>)
 80013ca:	4594      	cmp	ip, r2
 80013cc:	f0c0 80bd 	bcc.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 80013d0:	d10c      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 80013d2:	4a45      	ldr	r2, [pc, #276]	@ (80014e8 <fp_mul+0x478>)
 80013d4:	990b      	ldr	r1, [sp, #44]	@ 0x2c
 80013d6:	4291      	cmp	r1, r2
 80013d8:	f240 80b7 	bls.w	800154a <fp_mul+0x4da>
        if (a[i] > b[i]) return  1;
 80013dc:	3201      	adds	r2, #1
 80013de:	4291      	cmp	r1, r2
 80013e0:	d104      	bne.n	80013ec <fp_mul+0x37c>
        if (a[i] < b[i]) return -1;
 80013e2:	4a42      	ldr	r2, [pc, #264]	@ (80014ec <fp_mul+0x47c>)
 80013e4:	990a      	ldr	r1, [sp, #40]	@ 0x28
 80013e6:	4291      	cmp	r1, r2
 80013e8:	f240 80af 	bls.w	800154a <fp_mul+0x4da>
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 80013ec:	990a      	ldr	r1, [sp, #40]	@ 0x28
        r[i]   = (uint32_t)x;
 80013ee:	9c1f      	ldr	r4, [sp, #124]	@ 0x7c
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 80013f0:	f245 5255 	movw	r2, #21845	@ 0x5555
 80013f4:	188a      	adds	r2, r1, r2
        r[i]   = (uint32_t)x;
 80013f6:	6022      	str	r2, [r4, #0]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 80013f8:	990b      	ldr	r1, [sp, #44]	@ 0x2c
 80013fa:	4a3d      	ldr	r2, [pc, #244]	@ (80014f0 <fp_mul+0x480>)
 80013fc:	eb60 0000 	sbc.w	r0, r0, r0
        borrow = (x >> 63) & 1;
 8001400:	0fc0      	lsrs	r0, r0, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001402:	188a      	adds	r2, r1, r2
 8001404:	eb61 0101 	sbc.w	r1, r1, r1
 8001408:	1a12      	subs	r2, r2, r0
 800140a:	f161 0100 	sbc.w	r1, r1, #0
        r[i]   = (uint32_t)x;
 800140e:	6062      	str	r2, [r4, #4]
        borrow = (x >> 63) & 1;
 8001410:	0fc8      	lsrs	r0, r1, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001412:	4a38      	ldr	r2, [pc, #224]	@ (80014f4 <fp_mul+0x484>)
 8001414:	990c      	ldr	r1, [sp, #48]	@ 0x30
 8001416:	188a      	adds	r2, r1, r2
 8001418:	eb61 0101 	sbc.w	r1, r1, r1
 800141c:	1a12      	subs	r2, r2, r0
        r[i]   = (uint32_t)x;
 800141e:	60a2      	str	r2, [r4, #8]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001420:	980d      	ldr	r0, [sp, #52]	@ 0x34
 8001422:	4a35      	ldr	r2, [pc, #212]	@ (80014f8 <fp_mul+0x488>)
 8001424:	f161 0100 	sbc.w	r1, r1, #0
        borrow = (x >> 63) & 1;
 8001428:	0fc9      	lsrs	r1, r1, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800142a:	1882      	adds	r2, r0, r2
 800142c:	eb60 0000 	sbc.w	r0, r0, r0
 8001430:	1a52      	subs	r2, r2, r1
        r[i]   = (uint32_t)x;
 8001432:	60e2      	str	r2, [r4, #12]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001434:	4a31      	ldr	r2, [pc, #196]	@ (80014fc <fp_mul+0x48c>)
 8001436:	f160 0000 	sbc.w	r0, r0, #0
        borrow = (x >> 63) & 1;
 800143a:	0fc0      	lsrs	r0, r0, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800143c:	eb19 0202 	adds.w	r2, r9, r2
 8001440:	eb61 0101 	sbc.w	r1, r1, r1
 8001444:	1a12      	subs	r2, r2, r0
        r[i]   = (uint32_t)x;
 8001446:	6122      	str	r2, [r4, #16]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001448:	980f      	ldr	r0, [sp, #60]	@ 0x3c
 800144a:	4a2d      	ldr	r2, [pc, #180]	@ (8001500 <fp_mul+0x490>)
 800144c:	f161 0100 	sbc.w	r1, r1, #0
        borrow = (x >> 63) & 1;
 8001450:	0fc9      	lsrs	r1, r1, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001452:	1882      	adds	r2, r0, r2
 8001454:	eb60 0000 	sbc.w	r0, r0, r0
 8001458:	1a52      	subs	r2, r2, r1
        r[i]   = (uint32_t)x;
 800145a:	6162      	str	r2, [r4, #20]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800145c:	4a29      	ldr	r2, [pc, #164]	@ (8001504 <fp_mul+0x494>)
 800145e:	f160 0000 	sbc.w	r0, r0, #0
        borrow = (x >> 63) & 1;
 8001462:	0fc0      	lsrs	r0, r0, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001464:	eb18 0202 	adds.w	r2, r8, r2
 8001468:	eb61 0101 	sbc.w	r1, r1, r1
 800146c:	1a12      	subs	r2, r2, r0
        r[i]   = (uint32_t)x;
 800146e:	61a2      	str	r2, [r4, #24]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001470:	4a25      	ldr	r2, [pc, #148]	@ (8001508 <fp_mul+0x498>)
 8001472:	f161 0100 	sbc.w	r1, r1, #0
        borrow = (x >> 63) & 1;
 8001476:	0fc9      	lsrs	r1, r1, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001478:	18ad      	adds	r5, r5, r2
 800147a:	eb62 0202 	sbc.w	r2, r2, r2
 800147e:	1a6d      	subs	r5, r5, r1
        r[i]   = (uint32_t)x;
 8001480:	4620      	mov	r0, r4
 8001482:	61e5      	str	r5, [r4, #28]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001484:	4921      	ldr	r1, [pc, #132]	@ (800150c <fp_mul+0x49c>)
 8001486:	9c1d      	ldr	r4, [sp, #116]	@ 0x74
 8001488:	f162 0200 	sbc.w	r2, r2, #0
        borrow = (x >> 63) & 1;
 800148c:	0fd2      	lsrs	r2, r2, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800148e:	eb14 0801 	adds.w	r8, r4, r1
 8001492:	eb61 0101 	sbc.w	r1, r1, r1
 8001496:	ebb8 0202 	subs.w	r2, r8, r2
        r[i]   = (uint32_t)x;
 800149a:	6202      	str	r2, [r0, #32]
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800149c:	9c01      	ldr	r4, [sp, #4]
 800149e:	f161 0200 	sbc.w	r2, r1, #0
 80014a2:	e035      	b.n	8001510 <fp_mul+0x4a0>
 80014a4:	b9feffff 	.word	0xb9feffff
 80014a8:	ffffaaab 	.word	0xffffaaab
 80014ac:	b153ffff 	.word	0xb153ffff
 80014b0:	1eabfffe 	.word	0x1eabfffe
 80014b4:	f6b0f624 	.word	0xf6b0f624
 80014b8:	6730d2a0 	.word	0x6730d2a0
 80014bc:	f38512bf 	.word	0xf38512bf
 80014c0:	434bacd7 	.word	0x434bacd7
 80014c4:	4b1ba7b6 	.word	0x4b1ba7b6
 80014c8:	1a0111ea 	.word	0x1a0111ea
 80014cc:	1a0111e9 	.word	0x1a0111e9
 80014d0:	397fe699 	.word	0x397fe699
 80014d4:	4b1ba7b5 	.word	0x4b1ba7b5
 80014d8:	64774b83 	.word	0x64774b83
 80014dc:	f38512be 	.word	0xf38512be
 80014e0:	f6b0f623 	.word	0xf6b0f623
 80014e4:	1eabfffd 	.word	0x1eabfffd
 80014e8:	b9fefffe 	.word	0xb9fefffe
 80014ec:	ffffaaaa 	.word	0xffffaaaa
 80014f0:	46010001 	.word	0x46010001
 80014f4:	4eac0001 	.word	0x4eac0001
 80014f8:	e1540002 	.word	0xe1540002
 80014fc:	094f09dc 	.word	0x094f09dc
 8001500:	98cf2d60 	.word	0x98cf2d60
 8001504:	0c7aed41 	.word	0x0c7aed41
 8001508:	9b88b47c 	.word	0x9b88b47c
 800150c:	bcb45329 	.word	0xbcb45329
 8001510:	4919      	ldr	r1, [pc, #100]	@ (8001578 <fp_mul+0x508>)
        borrow = (x >> 63) & 1;
 8001512:	0fd2      	lsrs	r2, r2, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001514:	eb14 0901 	adds.w	r9, r4, r1
 8001518:	eb61 0101 	sbc.w	r1, r1, r1
 800151c:	ebb9 0202 	subs.w	r2, r9, r2
        r[i]   = (uint32_t)x;
 8001520:	6242      	str	r2, [r0, #36]	@ 0x24
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001522:	4a16      	ldr	r2, [pc, #88]	@ (800157c <fp_mul+0x50c>)
 8001524:	f161 0100 	sbc.w	r1, r1, #0
        borrow = (x >> 63) & 1;
 8001528:	0fc9      	lsrs	r1, r1, #31
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 800152a:	189b      	adds	r3, r3, r2
 800152c:	4a14      	ldr	r2, [pc, #80]	@ (8001580 <fp_mul+0x510>)
        r[i]   = (uint32_t)x;
 800152e:	4604      	mov	r4, r0
        uint64_t x = (uint64_t)a[i] - b[i] - borrow;
 8001530:	eb60 0000 	sbc.w	r0, r0, r0
 8001534:	1a5b      	subs	r3, r3, r1
 8001536:	f160 0100 	sbc.w	r1, r0, #0
 800153a:	4472      	add	r2, lr
 800153c:	eba2 72d1 	sub.w	r2, r2, r1, lsr #31
        r[i]   = (uint32_t)x;
 8001540:	e9c4 320a 	strd	r3, r2, [r4, #40]	@ 0x28
        fp_raw_sub(r, result, CURVE_P);
    } else {
        memcpy(r, result, sizeof(Fp));
    }
}
 8001544:	b049      	add	sp, #292	@ 0x124
 8001546:	e8bd 8ff0 	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
        memcpy(r, result, sizeof(Fp));
 800154a:	9c1f      	ldr	r4, [sp, #124]	@ 0x7c
 800154c:	ae20      	add	r6, sp, #128	@ 0x80
 800154e:	af2c      	add	r7, sp, #176	@ 0xb0
 8001550:	4635      	mov	r5, r6
 8001552:	cd0f      	ldmia	r5!, {r0, r1, r2, r3}
 8001554:	42bd      	cmp	r5, r7
 8001556:	f104 0410 	add.w	r4, r4, #16
 800155a:	f106 0610 	add.w	r6, r6, #16
 800155e:	f844 0c10 	str.w	r0, [r4, #-16]
 8001562:	f844 1c0c 	str.w	r1, [r4, #-12]
 8001566:	f844 2c08 	str.w	r2, [r4, #-8]
 800156a:	f844 3c04 	str.w	r3, [r4, #-4]
 800156e:	d1ef      	bne.n	8001550 <fp_mul+0x4e0>
}
 8001570:	b049      	add	sp, #292	@ 0x124
 8001572:	e8bd 8ff0 	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
 8001576:	bf00      	nop
 8001578:	b4e4584a 	.word	0xb4e4584a
 800157c:	c6801966 	.word	0xc6801966
 8001580:	e5feee16 	.word	0xe5feee16
