#include "curve.h"
#include "amore.h"
#include "amore_uart.h"
#include "triggers.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * DWT helpers
 * ----------------------------------------------------------------------- */
#define DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004u)
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000u)
#define DEMCR           (*(volatile uint32_t *)0xE000EDFCu)

static void dwt_init(void) {
    DEMCR     |= 0x01000000u;
    DWT_CYCCNT = 0;
    DWT_CTRL  |= 1u;
}
static inline uint32_t cyc(void) { return DWT_CYCCNT; }

/* -----------------------------------------------------------------------
 * Phase + log helpers  (g_res must be reachable; passed as pointer)
 * ----------------------------------------------------------------------- */
static AmorE_BenchResults *g_res_ptr;

static void phase_set(uint8_t ph, uint8_t batch, uint16_t round, uint32_t extra) {
    AmorE_BenchResults *r = g_res_ptr;
    r->current_phase = ph;
    /* Write ring-log entry */
    uint32_t idx = r->log_head % PHASE_LOG_LEN;
    r->log[idx].phase  = ph;
    r->log[idx].batch  = batch;
    r->log[idx].round  = round;
    r->log[idx].cycles = cyc();
    r->log[idx].extra  = extra;
    r->log_head++;
}

static void error_set(uint32_t code, uint8_t batch, uint16_t round) {
    AmorE_BenchResults *r = g_res_ptr;
    r->last_error   = code;
    r->error_batch  = batch;
    r->error_round  = round;
    r->total_uart_errors++;
    phase_set(r->current_phase, batch, round, code);
}

/* -----------------------------------------------------------------------
 * PRNG (xoshiro128**, seeded from DWT+tick)
 *
 * !! WARNING — INSECURE BENCHMARK RNG, NOT FOR PRODUCTION KEYS !!
 *
 * Bug #5 (documented, not fixed in code): rng_seed() draws ~16-20 bits of
 * effective entropy at best:
 *   - cyc() runs right after DWT init (CYCCNT≈few thousand, narrow range)
 *   - HAL_GetTick() is ~0-few ms at startup (narrow range)
 *   - rng_s[1..3] are deterministic XOR functions of rng_s[0], so the
 *     total state entropy is dominated by rng_s[0].
 *
 * For the energy-benchmark use case this is acceptable (and even useful
 * for reproducibility across replicas). For deploying AmorE in a real
 * security setting, replace this with a true hardware RNG (STM32F4 RNG
 * peripheral or ADC-noise pool). Until then, do NOT treat sk->s or
 * sec->r values produced here as cryptographically private.
 * ----------------------------------------------------------------------- */
static uint32_t rng_s[4];

static void rng_seed(void) {
    rng_s[0] = cyc() ^ HAL_GetTick() ^ 0xDEADBEEFu;
    rng_s[1] = 0x9e3779b9u ^ rng_s[0];
    rng_s[2] = 0x6c62272eu ^ rng_s[1];
    rng_s[3] = 0x08db8267u ^ rng_s[2];
}

static uint32_t rng_u32(void) {
    uint32_t result = rng_s[1] * 5u;
    result = ((result << 7) | (result >> 25)) * 9u;
    uint32_t t = rng_s[1] << 9;
    rng_s[2] ^= rng_s[0]; rng_s[3] ^= rng_s[1];
    rng_s[1] ^= rng_s[2]; rng_s[0] ^= rng_s[3];
    rng_s[2] ^= t;
    rng_s[3] = (rng_s[3] << 11) | (rng_s[3] >> 21);
    return result;
}

static void sample_scalar_fq(Fq r) {
    uint32_t raw[8];
    int ge;  /* ge=1 means raw >= q; declared outside do-block for scope */
    do {
        for (int i = 0; i < 8; i++) raw[i] = rng_u32();
        /* BLS12-381's q[7] = 0x73eda753 (31 bits). Mask to 31 bits so
         * rejection sampling actually triggers when raw >= q. */
        raw[7] &= 0x7FFFFFFFu;
        /* Full limb-by-limb comparison: reject if raw >= q.
         * Previous version only checked limbs [7] and [0], leaving
         * limbs [1-6] unconstrained — introduces a small modular bias. */
        ge = 0;
        for (int i = 7; i >= 0; i--) {
            if (raw[i] > CURVE_Q_FQ[i]) { ge = 1; break; }
            if (raw[i] < CURVE_Q_FQ[i]) { ge = 0; break; }
        }
    } while (ge);
    uint32_t nz = 0;
    for (int i = 0; i < 8; i++) nz |= raw[i];
    if (!nz) raw[0] = 1;
    fq_from_limbs(r, raw);
}

static int sample_short_scalar(uint32_t out[8], uint32_t phi) {
    /* out[] is exactly 8 uint32_t = 256 bits.
     * phi must be <= 256 or we corrupt the stack.
     *
     * Bug #3 fix: phi=0 used to leave out=[0,...,0], then the "if (!nz)"
     * guard below would set out[0]=1, but the function returned nbits=0.
     * Callers using nbits in `g2_scalar_mul` / `fp12_exp` would then
     * iterate zero times and silently treat the scalar as 0 — even though
     * the buffer says 1. AMORE_PHI is currently never 0, so this is a
     * defensive fix rather than an active bug. Clamp phi to >= 1 so the
     * returned nbits matches the actual scalar in the buffer.
     */
    if (phi > 256) phi = 256;
    if (phi < 1)   phi = 1;
    memset(out, 0, 32);
    uint32_t full = phi / 32, rem = phi % 32;
    for (uint32_t i = 0; i < full; i++) out[i] = rng_u32();
    if (rem && full < 8) out[full] = rng_u32() & ((1u << rem) - 1u);
    uint32_t nz = 0;
    for (int i = 0; i < 8; i++) nz |= out[i];
    if (!nz) out[0] = 1;
    return (int)phi;
}

/* -----------------------------------------------------------------------
 * Load gamma_T (e(G1,G2)) from hardcoded Montgomery-form constants
 * ----------------------------------------------------------------------- */
static void load_gamma_T(Fp12 *gt) {
    memcpy(gt->c[0].c[0].c0, CURVE_GT0, FP_BYTES);
    memcpy(gt->c[0].c[0].c1, CURVE_GT1, FP_BYTES);
    memcpy(gt->c[0].c[1].c0, CURVE_GT2, FP_BYTES);
    memcpy(gt->c[0].c[1].c1, CURVE_GT3, FP_BYTES);
    memcpy(gt->c[0].c[2].c0, CURVE_GT4, FP_BYTES);
    memcpy(gt->c[0].c[2].c1, CURVE_GT5, FP_BYTES);
    memcpy(gt->c[1].c[0].c0, CURVE_GT6, FP_BYTES);
    memcpy(gt->c[1].c[0].c1, CURVE_GT7, FP_BYTES);
    memcpy(gt->c[1].c[1].c0, CURVE_GT8, FP_BYTES);
    memcpy(gt->c[1].c[1].c1, CURVE_GT9, FP_BYTES);
    memcpy(gt->c[1].c[2].c0, CURVE_GT10, FP_BYTES);
    memcpy(gt->c[1].c[2].c1, CURVE_GT11, FP_BYTES);
}

/* -----------------------------------------------------------------------
 * AmorE_OneTimeSetup
 * ----------------------------------------------------------------------- */
void AmorE_OneTimeSetup(AmorE_SK *sk, uint32_t phi, uint32_t tau_ms) {
    rng_seed();
    sk->phi    = phi;
    sk->tau_ms = tau_ms;

    sample_scalar_fq(sk->s);

    uint32_t s_plain[8];
    fq_to_limbs(s_plain, sk->s);

    /* neg_s = q - s — xi = gt^(-s) (matches working BN254 implementation) */
    uint32_t neg_s[8] = {0};
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)CURVE_Q_FQ[i] - s_plain[i] - borrow;
        neg_s[i] = (uint32_t)x;
        borrow   = (x >> 63) & 1;
    }
    uint32_t sz = 0;
    for (int i = 0; i < 8; i++) sz |= neg_s[i];
    if (!sz) { fp12_one(&sk->xi); return; }

    Fp12 gt;
    load_gamma_T(&gt);
    fp12_exp(&sk->xi, &gt, neg_s, 255);
}

/* -----------------------------------------------------------------------
 * AmorE_Setup (per-round blinding)
 * ----------------------------------------------------------------------- */
void AmorE_Setup(const AmorE_SK *sk,
                 const uint8_t A_bytes[G1_BYTES], const uint8_t B_bytes[G2_BYTES],
                 AmorE_Pub *pub, AmorE_Sec *sec)
{
    G1Point A;  G2Point B;
    g1_from_bytes(&A, A_bytes);
    g2_from_bytes(&B, B_bytes);

    Fq u, u_inv, su_inv;
    sample_scalar_fq(u);
    fq_inv(u_inv, u);
    fq_mul(su_inv, (const uint32_t *)sk->s, u_inv);

    uint32_t u_plain[8], su_inv_plain[8], neg_su_inv_plain[8];
    fq_to_limbs(u_plain,      u);
    fq_to_limbs(su_inv_plain, su_inv);

    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)CURVE_Q_FQ[i] - su_inv_plain[i] - borrow;
        neg_su_inv_plain[i] = (uint32_t)x;
        borrow = (x >> 63) & 1;
    }

    G1Point Pgen, U;  g1_generator(&Pgen);
    g1_scalar_mul(&U, &Pgen, u_plain, 255);

    G2Point Qgen, V;  g2_generator(&Qgen);
    g2_scalar_mul(&V, &Qgen, su_inv_plain, 255);

    G1Point UA, C;
    g1_add(&UA, &U, &A);
    g1_scalar_mul(&C, &UA, neg_su_inv_plain, 255);

    sec->nbits = sample_short_scalar(sec->r, sk->phi);

    G2Point rB, D;
    g2_scalar_mul(&rB, &B, sec->r, sec->nbits);
    g2_neg(&rB, &rB);
    g2_add(&D, &V, &rB);

    memcpy(pub->A, A_bytes, G1_BYTES);
    memcpy(pub->B, B_bytes, G2_BYTES);
    g1_to_bytes(pub->C, &C);
    g2_to_bytes(pub->D, &D);
}

/* -----------------------------------------------------------------------
 * AmorE_Verify: check xi == rho^r * gamma
 * ----------------------------------------------------------------------- */
int AmorE_Verify(const AmorE_SK *sk, const AmorE_Sec *sec, const AmorE_Out *out) {
    Fp12 gamma_gt, rho_gt, rho_r, lhs;
    fp12_from_bytes(&gamma_gt, out->gamma);
    fp12_from_bytes(&rho_gt,   out->rho);
    fp12_exp(&rho_r, &rho_gt, sec->r, sec->nbits);
    fp12_mul(&lhs, &rho_r, &gamma_gt);
    return fp12_eq(&lhs, &sk->xi);
}

/* -----------------------------------------------------------------------
 * G1/G2 generator bytes (constant, used every round)
 * ----------------------------------------------------------------------- */
/* G1 generator bytes — populated at runtime from CURVE_G1X/Y in the const header */
static uint8_t G1GEN_BYTES[96];   /* filled once in RunBenchmark */
/* G2 generator bytes — populated at runtime from CURVE_G2X/Y in the const header */
static uint8_t g_g2gen_bytes[192];   /* filled once in RunBenchmark */
static uint8_t uart_buf[1280];        /* receive buffer (gamma+rho = 1152, +headroom) */
_Static_assert(sizeof(uart_buf) >= AMORE_PROTO_IN_BYTES,
               "uart_buf must be at least AMORE_PROTO_IN_BYTES (1152)");

/* -----------------------------------------------------------------------
 * Inline helpers for the benchmark loop
 * ----------------------------------------------------------------------- */
static const uint32_t BENCH_N[BENCH_N_COUNT] = {1, 10, 50};

/* Encode pub into 576-byte flat buffer (96 + 192 + 96 + 192) */
static void pub_to_bytes(uint8_t out[AMORE_PROTO_OUT_BYTES], const AmorE_Pub *pub) {
    memcpy(out, pub->A, G1_BYTES);
    memcpy(out + G1_BYTES, pub->B, G2_BYTES);
    memcpy(out + G1_BYTES + G2_BYTES, pub->C, G1_BYTES);
    memcpy(out + 2*G1_BYTES + G2_BYTES, pub->D, G2_BYTES);
}

/* -----------------------------------------------------------------------
 * AmorE_RunBenchmark — full telemetry version
 * ----------------------------------------------------------------------- */
void AmorE_RunBenchmark(AmorE_BenchResults *res) {
    memset(res, 0, sizeof(*res));
    g_res_ptr = res;

    res->fw_version = AMORE_FW_VER;
    res->core_mhz   = 168;
    for (int i = 0; i < BENCH_N_COUNT; i++) res->N_vals[i] = BENCH_N[i];

    dwt_init();
    uart_init(921600);

    uint32_t wall_start = HAL_GetTick();

    /* --- Wait for server READY --- */
    phase_set(PHASE_WAIT_READY, 0, 0, 0);
    {
        uint8_t cmd; uint16_t len;
        int rc = uart_recv_packet(&cmd, uart_buf, &len, sizeof(uart_buf), 30000);
        if (rc != 0) {
            error_set(ERR_UART_TIMEOUT, 0, 0);
            TRIG_ALL_LO();
            res->status = 0xDEAD0001u;
            return;
        }
        /* byte 0: 0=honest, 1=malicious (informational only) */
    }

    /* --- OneTimeSetup --- */
    phase_set(PHASE_OTS, 0, 0, 0);
    AmorE_SK sk;
    uint32_t t0 = cyc();
    AmorE_OneTimeSetup(&sk, AMORE_PHI, 60000);
    res->ots_cycles = cyc() - t0;
    res->ots_ok     = 1;
    phase_set(PHASE_OTS_DONE, 0, 0, res->ots_cycles);

    /* Pre-compute G1 and G2 generator bytes (BLS12-381 generators are not
     * trivially expressible like BN254's (1,2), so we compute them at runtime). */
    {
        G1Point P; g1_generator(&P);
        g1_to_bytes(G1GEN_BYTES, &P);

        G2Point Q; g2_generator(&Q);
        g2_to_bytes(g_g2gen_bytes, &Q);
    }

    /* --- Benchmark batches --- */
    for (uint8_t bi = 0; bi < BENCH_N_COUNT; bi++) {
        uint32_t N = BENCH_N[bi];

        for (uint32_t rnd = 0; rnd < N; rnd++) {
            AmorE_Pub pub;
            AmorE_Sec sec;
            uint8_t pkt[AMORE_PROTO_OUT_BYTES];

            /* Setup (blind) */
            phase_set(PHASE_BENCH_SETUP, bi, (uint16_t)rnd, 0);
            TRIG_COMPUTE_HI();
            t0 = cyc();
            AmorE_Setup(&sk, G1GEN_BYTES, g_g2gen_bytes, &pub, &sec);
            uint32_t blind_cyc_this_round = cyc() - t0;
            res->blind_total_cycles[bi] += blind_cyc_this_round;
            TRIG_COMPUTE_LO();
            if (N == 50 && rnd < 50) {
                res->per_round_blind_n50[rnd] = blind_cyc_this_round;
            }

            /* Send */
            phase_set(PHASE_BENCH_SEND, bi, (uint16_t)rnd, 0);
            TRIG_WAIT_HI();
            pub_to_bytes(pkt, &pub);
            int rc = uart_send_packet(UART_CMD_SETUP, pkt, AMORE_PROTO_OUT_BYTES);
            res->rounds_sent[bi]++;
            res->total_rounds_sent++;
            if (rc != 0) {
                error_set(ERR_UART_SEND, bi, rnd);
                res->rounds_uart_err[bi]++;
                TRIG_ALL_LO();
                res->status = 0xDEAD0010u | bi;
                return;
            }

            /* Wait for result */
            phase_set(PHASE_BENCH_WAIT, bi, (uint16_t)rnd, 0);
            uint8_t cmd; uint16_t rlen;
            rc = uart_recv_packet(&cmd, uart_buf, &rlen, sizeof(uart_buf), UART_TIMEOUT_MS);
            if (rc != 0) {
                error_set(ERR_UART_TIMEOUT, bi, rnd);
                res->rounds_uart_err[bi]++;
                TRIG_ALL_LO();
                res->status = 0xDEAD0020u | bi;
                return;
            }
            if (cmd != UART_CMD_RESULT) {
                error_set(ERR_UART_CMD, bi, rnd);
                res->rounds_uart_err[bi]++;
                TRIG_ALL_LO();
                res->status = 0xDEAD0030u | bi;
                return;
            }
            if (rlen != AMORE_PROTO_IN_BYTES) {
                error_set(ERR_UART_LEN, bi, rnd);
                res->rounds_uart_err[bi]++;
                TRIG_ALL_LO();
                res->status = 0xDEAD0040u | bi;
                return;
            }
            res->rounds_recv_ok[bi]++;
            TRIG_WAIT_LO();

            /* Verify */
            phase_set(PHASE_BENCH_VERIFY, bi, (uint16_t)rnd, 0);
            AmorE_Out out_val;
            memcpy(out_val.gamma, uart_buf, FP12_BYTES);
            memcpy(out_val.rho, uart_buf + FP12_BYTES, FP12_BYTES);
            TRIG_COMPUTE_HI();
            t0 = cyc();
            /* === DBG CAPTURE: round 0 only (non-invasive snapshot) === */
            if (bi == 0 && rnd == 0) {
                memcpy(res->dbg_first_uart, uart_buf, 32);
                memcpy(res->dbg_first_gamma, out_val.gamma, 16);
                memcpy(res->dbg_first_rho, out_val.rho, 16);
                memcpy(res->dbg_first_xi, (const uint8_t*)&sk.xi, 16);
                memcpy(res->dbg_first_sec_r, (const uint8_t*)sec.r, 16);
                res->dbg_sk_s_first = ((const uint32_t*)&sk.s)[0];
                res->dbg_sec_nbits = sec.nbits;
                res->dbg_uart_rlen = rlen;
                res->dbg_round_captured = rnd;
            }
            res->dbg_verify_called++;
            int ok = AmorE_Verify(&sk, &sec, &out_val);
            res->dbg_verify_last_ok = (uint32_t)ok;
            uint32_t verify_cyc_this_round = cyc() - t0;
            res->verify_total_cycles[bi] += verify_cyc_this_round;
            TRIG_COMPUTE_LO();
            if (N == 50 && rnd < 50) {
                res->per_round_verify_n50[rnd] = verify_cyc_this_round;
            }

            if (ok) {
                res->rounds_verify_ok[bi]++;
                res->total_verify_ok++;
            } else {
                res->rounds_verify_fail[bi]++;
                error_set(ERR_VERIFY_HONEST, bi, rnd);
            }

            /* Send status */
            phase_set(PHASE_BENCH_STATUS, bi, (uint16_t)rnd, (uint32_t)ok);
            uint8_t st = ok ? 1u : 0u;
            uart_send_packet(UART_CMD_STATUS, &st, 1);

            if (!ok) {
                /* Honest round failed — abort */
                TRIG_ALL_LO();
                res->status = 0xDEAD0050u | bi;
                return;
            }
        }

        /* CRITICAL: cast to uint64_t before adding to avoid uint32 overflow.
         * In BLS12_381 N=50, total can exceed 2^32 cycles (~25.6 sec wall). */
        uint64_t total_cyc = res->blind_total_cycles[bi] +
                             res->verify_total_cycles[bi];
        if (total_cyc < res->blind_total_cycles[bi]) {
            /* defensive: this should never trigger now that fields are uint64 */
            res->overflow_detected = 1;
        }
        res->amort_cycles[bi] = (uint32_t)(total_cyc / N);
    }

    /* --- Security check: one malicious round --- */
    {
        AmorE_Pub pub;
        AmorE_Sec sec;
        uint8_t pkt[AMORE_PROTO_OUT_BYTES];

        phase_set(PHASE_SECURITY, 3, 0, 0);
        AmorE_Setup(&sk, G1GEN_BYTES, g_g2gen_bytes, &pub, &sec);

        phase_set(PHASE_BENCH_SEND, 3, 0, 0);
        pub_to_bytes(pkt, &pub);
        uart_send_packet(UART_CMD_SETUP, pkt, AMORE_PROTO_OUT_BYTES);
        res->sec_sent = 1;

        phase_set(PHASE_SECURITY_WAIT, 3, 0, 0);
        uint8_t cmd; uint16_t rlen;
        int rc = uart_recv_packet(&cmd, uart_buf, &rlen, sizeof(uart_buf), UART_TIMEOUT_MS);

        if (rc == 0 && cmd == UART_CMD_RESULT && rlen == AMORE_PROTO_IN_BYTES) {
            res->sec_recv_ok = 1;

            phase_set(PHASE_SECURITY_VFY, 3, 0, 0);
            AmorE_Out out_val;
            memcpy(out_val.gamma, uart_buf, FP12_BYTES);
            memcpy(out_val.rho, uart_buf + FP12_BYTES, FP12_BYTES);
            int ok = AmorE_Verify(&sk, &sec, &out_val);

            res->sec_verify_result = ok ? 1u : 0u;
            res->security_ok       = ok ? 0u : 1u;   /* 1 = correctly rejected */

            if (ok) {
                /* Malicious round accepted — this is a bug */
                error_set(ERR_VERIFY_MALICIOUS, 3, 0);
            }

            uint8_t st = 0;   /* always report fail to server for malicious round */
            uart_send_packet(UART_CMD_STATUS, &st, 1);
        } else {
            res->sec_recv_ok       = 0;
            res->sec_verify_result = 2;
            res->security_ok       = 2;   /* no response */
            error_set(ERR_UART_TIMEOUT, 3, 0);
        }
    }

    /* --- Finalise --- */
    res->wall_ms = HAL_GetTick() - wall_start;
    phase_set(PHASE_DONE, 0, 0, 0);

    res->magic  = AMORE_MAGIC;
    res->status = (res->security_ok == 1u && res->total_verify_ok == (1u + 10u + 50u))
                  ? 0x600D0000u
                  : 0xBAD00000u;
}
