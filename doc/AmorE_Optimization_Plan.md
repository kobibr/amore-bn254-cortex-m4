# AmorE BLS12-381 Client — Optimization Plan (Cortex-M4 / STM32F4)

**Scope.** Forward-looking optimization plan for the pure-C AmorE client (`fp.c`,
`fp2.c`, `fp12.c`, `fq.c`, `g1.c`, `g2.c`, `amore.c`) on STM32F407 @ 168 MHz. The
implementation is a clean "textbook" reference: the design goal is a small memory
footprint and a pairing-library-free, verifiable client. The aim of these optimizations
is to **narrow the speed gap to a hand-tuned pairing library (RELIC) while keeping the
memory advantage** — not to win on raw speed.

**Status of every number here.** All per-round costs and per-item gains are
**model-based estimates in units of one `fp_mul`**, except where explicitly marked
*measured (paper)*. Nothing has been measured on this device yet. Benchmark every step
before/after with `DWT->CYCCNT` and re-run the bilinearity/verify self-tests after each
change.

---

## 1. Per-round cost (M=1), model estimate

Structure verified against source: `fp12_mul` is 12×12 schoolbook; `g1/g2_scalar_mul`
are binary double-and-add (no windows); `fp_sqr` calls `fp_mul`; `fp_inv`/`fq_inv` are
binary Fermat; Verify passes the **short** scalar `nbits=φ`, not 255. These are the
*unoptimized baseline* counts (this implementation, before any item below) — not directly
comparable to the paper's Table 2, which is post-GLV/GLS/wNAF in RELIC; the G2/G1 ratio
here (~2.8) reflects raw Fp2-over-Fp overhead, which GLS later compresses.

| Component | Est. cost (`fp_mul`) | Note |
|---|---|---|
| `U = [u]P` | ~3,562 | G1 full scalar mul |
| `V = [s·u⁻¹]Q` | ~9,920 | G2 full scalar mul — most expensive item |
| `C = [..](U+A)` | ~3,562 | G1 full scalar mul |
| `D = V − [r]B` | ~3,510 | G2 short scalar mul |
| `fq_inv(u)` | ~170 | binary Fermat (q−2), fp_mul-equiv (≈382 Fq-mul × (8/12)² limb ratio) |
| Verify `ρ^r · γ` | ~19,440 | Fp12 exp on short exponent (φ bits) + one Fp12 mul |
| **Total** | **~40,200** | |

---

## 2. Correctness & security (handle before/during speed work)

| ID | Item | Status |
|---|---|---|
| **S1** | RNG not cryptographic (DWT/tick seed; `0xCAFEBABE` in bench). Fine for benchmarking; replace with TRNG-seeded CSPRNG for production. | Real gap |
| **S2** | `fp_mul` "Bug #4": final conditional subtract is correct only for canonical inputs (`T<2p`). True today. **Must preserve the check `T[FP_LIMBS]!=0 \|\| cmp≥0` when rewriting in asm (1A).** | Contract |
| **S3** | Branch-based final reduction → timing side channel. Masked conditional subtract removes it (production hardening, not speed). | Production |

---

## 3. Where hand assembly pays on Cortex-M4

Hand assembly earns its keep at the multiprecision **field atoms** — `fp_mul`, `fq_mul`,
`fp_sqr` — and the Fp2 multiply built directly on them. Everything heavier (point ops,
Fp12, protocol) is glue the compiler schedules well; the cycles live in the inner loops
below. The targets, in priority order:

**(a) `UMAAL`-based CIOS inner loops (`fp_mul`, `fq_mul`).** `UMAAL Rlo, Rhi, Rn, Rm`
computes `(Rhi∥Rlo) ← Rn·Rm + Rlo + Rhi` — a 32×32 multiply with *double* 32-bit
accumulation whose result always fits in 64 bits (max `(2³²−1)²+2(2³²−1)=2⁶⁴−1`), so it
absorbs both the running carry and the accumulator with **no separate carry instruction**.
This is exactly the CIOS multiply-and-reduce step, and it favors a **row-wise
(operand-scanning)** layout. C with `(uint64_t)a[j]*b[i]` does not emit `UMAAL`
reliably; a hand loop does. For BLS12-381 (12 limbs) the multiply and reduction passes
are the dominant cost, and this is the single highest-leverage change since all field,
point, and Fp12 arithmetic bottoms out here.

**(b) Pipeline scheduling around `UMAAL`.** On M4 the result of a `UMAAL` consumed
immediately by a dependent `adds`/`adcs` stalls one cycle. Interleave the two limb
streams (multiply and reduction) so each `UMAAL` result is not the next instruction's
input — this is most of the gap between a naive asm port and a fast one.

**(c) Dedicated squaring with a doubling routine (`fp_sqr`).** A product-scanning square
that computes each `a[i]·a[j]` once and doubles the off-diagonal terms (a tight
doubling+MAC inner step is ~21 cycles with 3×`UMAAL`) is materially cheaper than
`fp_mul(a,a)`. Squarings dominate the Verify exponentiation and point doublings, so this
compounds (see 2C).

**(d) FPU registers as scratch.** On the M4F the 32 single-precision FP registers are
usable as fast scratch via `vmov` (one cycle/word, saving a cycle over `ldr` from stack)
to hold limbs/accumulators across the inner loop and cut memory traffic. Useful once the
loop is register-bound.

Constraint that overrides all of the above: the `fp_mul` final reduction
(`T[FP_LIMBS]!=0 || cmp≥0`, see S2) must be preserved exactly — the canonical-input
invariant is what makes the single conditional subtract correct.

---

## 4. Tier 1 — highest return

### 1A. UMAAL asm in `fp_mul` / `fq_mul` (`fp.c`, `fq.c`)
Hand-write the CIOS multiply + reduction inner loops per §3(a)–(b). **Est. ~1.8×** on
every `fp_mul`/`fq_mul`, hence ~1.8× roughly uniformly across the client — the single
largest lever. Risk: low technique, careful carry/scheduling; preserve S2.

### 1B. Tower Fp12 mul + cyclotomic square (`fp12.c`)
`fp12_mul` flattens to a 12-coeff polynomial and does 144 `fp_mul` schoolbook, ignoring
the existing tower (Fp12→Fp6→Fp2→Fp). Multiply through the tower with Karatsuba: **144 →
~54 `fp_mul`** per Fp12 mul. Add a dedicated cyclotomic `fp12_sqr` (currently calls
`fp12_mul`) using **Granger–Scott** squaring (eprint 2009/565): **~144 → ~36** — Verify
is squaring-dominated, so this is large. Granger–Scott squares directly in the cyclotomic
subgroup with no compression and no inversion (unlike Karabina compressed squaring, whose
decompression needs a field inversion — not worth it for the interleaved exponentiation
here).
*Implementation note:* this works on the GT-subgroup representation; keep the arithmetic
internal and convert to flat-12 only in `fp12_to_bytes`, so the py_ecc wire format is
untouched. Risk: medium (must match serialization).

---

## 5. Tier 2 — solid, low effort

### 2A. Use the existing mixed addition (`g1.c`, `g2.c`)
`g1_add_mixed` (Jacobian+affine) already exists but is unused; `g1_scalar_mul` calls
generic `g1_add`. Call the mixed path when the addend is affine. Each G1 add ~16 → ~11
`fp_mul`; larger saving in G2. Risk: low (function already tested).

### 2C. Dedicated `fp_sqr` + lazy reduction (`fp.c`, `fp2.c`, `fp12.c`)
`fp_sqr` currently calls `fp_mul(a,a)`; a real square halves the cross-products (~20–25%
cheaper, applied to every square). Lazy reduction in `fp2`/`fp12` defers reduction to once
per Fp2/Fp12 op. The two combine on the same hot path. Risk: low (`fp_sqr`); medium (lazy
reduction needs representation headroom).

---

## 6. Special-form (Frobenius) challenges — the paper's §7 technique *(measured)*

**The central efficiency technique of the AmorE paper (§7), and the highest-value item
here** — it attacks the two costliest per-round operations (G2 short mul, GT exp).

**Method (per paper).** G2 and GT carry an efficient p-power Frobenius ψ. Sample the
short challenge as Φ(k) sub-scalars `cᵢ` of ⌈σ/Φ(k)⌉ bits, `c = Σ cᵢ·pⁱ mod q`. For BLS,
`p ≡ z mod q`, so compute `c = Σ cᵢ·zⁱ` with **no mod-q reduction**; then
`[c]Q = Σ [cᵢ]·ψⁱ(Q)` (and the GT analogue) interleaved, cutting depth by Φ(k). For
BLS12-381, Φ(12)=4.

**Measured in the paper, not estimated** (Table 2, RELIC, BLS12-381, 10³ cycles):

| Op | standard-short (NAF) | special-form | gain |
|---|---|---|---|
| G2 short scalar mul `m̄₂` | 412 | 153 | **~63%** |
| GT short exp `m̄_T` | 427 | 252 | **~41%** |

(For reference, the *full*-range ops are larger still: `m₂`=718, `m_T`=1,074. The gains
above are standard-short → special-form, i.e. the speedup from the Frobenius sampling on
the already-short challenge.)

These are exactly the ops behind `D = V−[r]B` and the Verify exponentiation. Paper
end-to-end client gains that include this: single-delegation 22.7–45.4%, batch 38.1–83.5%
vs local pairing (Tables 3–5).

**Security — proven, with a strict scope.** The short challenge is *by design* not
full-range (φ=90 < 2λ=256, with q a 255-bit prime, for σ=40); that bounded sampling is the efficiency source, and
everlasting security is proven (Theorem 3, κ=70). Frobenius is a *representation* of that
same short challenge, not extra entropy reduction. **Scope is strict:** apply
short/Frobenius sampling **only** to the verification challenge `r`. The long-term secret
`s` and the per-round mask `u` must stay full-range — §6.2 shows that two
partially-masking secrets enable a meet-in-the-middle intersection attack (~2·|S| ops).
The current code already respects this (`s`,`u` ← `Z*q`; only `r` short).

**Implementation here.** Add ψ on G2/Fp12 + sub-scalar sampling; compute `[r]B` and `ρ^r`
interleaved. Same algorithm RELIC uses; the table gains are the realistic target. Risk:
medium (ψ implementation, sub-scalar signs).

---

## 7. Tier 3 — larger gains, higher effort (quoted gains are upper bounds)

### 1C. wNAF + GLV/GLS scalar mul (`g1.c`, `g2.c`)
Binary double-and-add today (~255 dbl + ~127 add, no windows/endomorphism). wNAF-5 cuts
additions ~127→~43; GLV (G1) / GLS (G2 via ψ) cuts doublings by the decomposition
dimension. **Up to ~2× (G1), up to ~3× (G2)** in the best case, less in practice. Scalars
stay full-entropy (representation change, no security impact). Risk: medium-high.

### 1E. safegcd (Bernstein–Yang) inversion (`fp.c`, `fq.c`)
`fp_inv`/`fq_inv` are binary Fermat (`fq_inv` ≈ 382 Fq-mul ≈ 170 `fp_mul`-equiv at the
8/12 limb ratio; `fp_inv` ≈ 570 `fp_mul`, being Fermat over the 381-bit p−2). safegcd is constant-time and
typically **~2–4× cheaper** in practice (some refs report more); also a side-channel win.
Helps `fq_inv(u)` and the affine conversions. Risk: medium (public reference impls exist).

### 2B. Fixed-base comb tables for `U=[u]P`, `V=[s·u⁻¹]Q` (`g1.c`, `g2.c`)
P, Q are constant generators but U, V are recomputed each round with a variable-base
routine. Precompute comb tables once → ~2× on U and V. Cost: table RAM (scarce here) —
size to available memory. Risk: low.

### 2D. Batch inversion of `u` (`fq.c`, `amore.c`)
If `u` values are produced in batches, invert N with one inversion + ~3N muls (Montgomery
trick), amortizing the per-round inversion. Risk: low; needs batching of `u`.

---

## 8. Expected overall improvement (conservative, model-based)

Stacking only the field/Fp12 items (1A, 1B, 2A, 2C), conservative end of each:

| Stage | Per-round (`fp_mul`) | Reduction |
|---|---|---|
| Current | ~40,200 | — |
| + UMAAL (1A), ~1.8× uniform | ~22,300 | ~44% |
| + tower-mul/cyclo-sqr (1B, on Fp12/Verify) + mixed-add/fp_sqr (2A/2C, on points) | ~15,700 | ~61% |

(The two parts of the third row hit *different* components: 1B reduces the Fp12 count
that dominates Verify, while 2A/2C reduce the point arithmetic. ~15,700 is a conservative
blend — a fuller accounting of cyclotomic squaring on the Verify exponentiation lands
somewhat lower, ~11–12k. We quote the conservative figure pending on-device numbers.)

**Floor: ~2.3–3.0× overall (≈57–67% less work), model-based, pending DWT verification.**
**§6 (Frobenius) is additional and measured in the paper** (~63% G2 short mul, ~41% GT
exp) — it targets the costliest items and pushes the Verify/Setup path further. This is
well below the ~6× an earlier draft implied (which assumed optimistic full composition).
Honest framing: ~2.5× from field/Fp12 work, plus the paper's measured Frobenius gains on
top — aimed at closing the gap to RELIC, not beating it. The items don't multiply
cleanly (UMAAL speeds the atomic `fp_mul`; tower-mul *reduces the count*), so any
"exactly N×" is false precision — the per-step on-device range is the honest form.

**Verification discipline:** benchmark each step on the device with DWT before/after
(all gains here are model estimates except §6, measured on Intel/RELIC — still to confirm
on this M4); re-run the bilinearity/verify self-tests after every change; preserve S2 when
rewriting `fp_mul` in asm.

---

## 9. Implementation order (return-on-effort)

1. **UMAAL** in `fp_mul`/`fq_mul` (1A) — touches everything, ~1.8×.
2. **Tower Fp12 mul + cyclotomic sqr** (1B) — ~62% on all GT ops.
3. **Existing mixed addition** (2A) — already written, just call it.
4. **Dedicated `fp_sqr` + lazy reduction** (2C) — small change, broad effect.
5. **Special-form Frobenius challenges** (§6) — paper's §7; ~63% G2 mul, ~41% GT exp (measured).
6. **wNAF then GLV/GLS** (1C) — largest generic point-arithmetic gain, hardest.
7. **safegcd inversion** (1E) — sizeable here, constant-time bonus.
8. **Comb tables for U,V** (2B) — if RAM allows.
9. **Batch inversion of u** (2D) — if `u` values can be batched.
