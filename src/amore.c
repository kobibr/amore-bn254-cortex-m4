#include "amore.h"
#include "amore_uart.h"
#include "bn128_const.h"
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
    do {
        for (int i = 0; i < 8; i++) raw[i] = rng_u32();
        raw[7] &= 0x3FFFFFFFu;
    } while (raw[7] > BN128_Q[7] ||
             (raw[7] == BN128_Q[7] && raw[0] == 0));
    uint32_t nz = 0;
    for (int i = 0; i < 8; i++) nz |= raw[i];
    if (!nz) raw[0] = 1;
    fq_from_limbs(r, raw);
}

static int sample_short_scalar(uint32_t out[8], uint32_t phi) {
    memset(out, 0, 32);
    uint32_t full = phi / 32, rem = phi % 32;
    for (uint32_t i = 0; i < full; i++) out[i] = rng_u32();
    if (rem) out[full] = rng_u32() & ((1u << rem) - 1u);
    uint32_t nz = 0;
    for (int i = 0; i < 8; i++) nz |= out[i];
    if (!nz) out[0] = 1;
    return (int)phi;
}

/* -----------------------------------------------------------------------
 * Load gamma_T (e(G1,G2)) from hardcoded Montgomery-form constants
 * ----------------------------------------------------------------------- */
static void load_gamma_T(Fp12 *gt) {
    memcpy(gt->c[0].c[0].c0, BN128_GT0,  32);
    memcpy(gt->c[0].c[0].c1, BN128_GT1,  32);
    memcpy(gt->c[0].c[1].c0, BN128_GT2,  32);
    memcpy(gt->c[0].c[1].c1, BN128_GT3,  32);
    memcpy(gt->c[0].c[2].c0, BN128_GT4,  32);
    memcpy(gt->c[0].c[2].c1, BN128_GT5,  32);
    memcpy(gt->c[1].c[0].c0, BN128_GT6,  32);
    memcpy(gt->c[1].c[0].c1, BN128_GT7,  32);
    memcpy(gt->c[1].c[1].c0, BN128_GT8,  32);
    memcpy(gt->c[1].c[1].c1, BN128_GT9,  32);
    memcpy(gt->c[1].c[2].c0, BN128_GT10, 32);
    memcpy(gt->c[1].c[2].c1, BN128_GT11, 32);
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

    /* neg_s = q - s */
    uint32_t neg_s[8] = {0};
    uint64_t borrow = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = (uint64_t)BN128_Q[i] - s_plain[i] - borrow;
        neg_s[i] = (uint32_t)x;
        borrow   = (x >> 63) & 1;
    }
    uint32_t sz = 0;
    for (int i = 0; i < 8; i++) sz |= neg_s[i];
    if (!sz) { fp12_one(&sk->xi); return; }

    Fp12 gt;
    load_gamma_T(&gt);
    fp12_exp(&sk->xi, &gt, neg_s, 254);
}

/* -----------------------------------------------------------------------
 * AmorE_Setup (per-round blinding)
 * ----------------------------------------------------------------------- */
void AmorE_Setup(const AmorE_SK *sk,
                 const uint8_t A_bytes[64], const uint8_t B_bytes[128],
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
        uint64_t x = (uint64_t)BN128_Q[i] - su_inv_plain[i] - borrow;
        neg_su_inv_plain[i] = (uint32_t)x;
        borrow = (x >> 63) & 1;
    }

    G1Point Pgen, U;  g1_generator(&Pgen);
    g1_scalar_mul(&U, &Pgen, u_plain, 254);

    G2Point Qgen, V;  g2_generator(&Qgen);
    g2_scalar_mul(&V, &Qgen, su_inv_plain, 254);

    G1Point UA, C;
    g1_add(&UA, &U, &A);
    g1_scalar_mul(&C, &UA, neg_su_inv_plain, 254);

    sec->nbits = sample_short_scalar(sec->r, sk->phi);

    G2Point rB, D;
    g2_scalar_mul(&rB, &B, sec->r, sec->nbits);
    g2_neg(&rB, &rB);
    g2_add(&D, &V, &rB);

    memcpy(pub->A, A_bytes, 64);
    memcpy(pub->B, B_bytes, 128);
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
static const uint8_t G1GEN_BYTES[64] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2
};

static uint8_t g_g2gen_bytes[128];   /* filled once in RunBenchmark */
static uint8_t uart_buf[800];        /* receive buffer               */

/* -----------------------------------------------------------------------
 * Inline helpers for the benchmark loop
 * ----------------------------------------------------------------------- */
static const uint32_t BENCH_N[BENCH_N_COUNT] = {1, 10, 50};

/* Encode pub into 384-byte flat buffer */
static void pub_to_bytes(uint8_t out[384], const AmorE_Pub *pub) {
    memcpy(out,       pub->A,  64);
    memcpy(out + 64,  pub->B, 128);
    memcpy(out + 192, pub->C,  64);
    memcpy(out + 256, pub->D, 128);
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

    /* Pre-compute G2 generator bytes */
    {
        G2Point Q; g2_generator(&Q);
        g2_to_bytes(g_g2gen_bytes, &Q);
    }

    /* --- Benchmark batches --- */
    for (uint8_t bi = 0; bi < BENCH_N_COUNT; bi++) {
        uint32_t N = BENCH_N[bi];

        for (uint32_t rnd = 0; rnd < N; rnd++) {
            AmorE_Pub pub;
            AmorE_Sec sec;
            uint8_t   pkt[384];

            /* Setup (blind) */
            phase_set(PHASE_BENCH_SETUP, bi, (uint16_t)rnd, 0);
            t0 = cyc();
            AmorE_Setup(&sk, G1GEN_BYTES, g_g2gen_bytes, &pub, &sec);
            res->blind_total_cycles[bi] += cyc() - t0;

            /* Send */
            phase_set(PHASE_BENCH_SEND, bi, (uint16_t)rnd, 0);
            pub_to_bytes(pkt, &pub);
            int rc = uart_send_packet(UART_CMD_SETUP, pkt, 384);
            res->rounds_sent[bi]++;
            res->total_rounds_sent++;
            if (rc != 0) {
                error_set(ERR_UART_SEND, bi, rnd);
                res->rounds_uart_err[bi]++;
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
                res->status = 0xDEAD0020u | bi;
                return;
            }
            if (cmd != UART_CMD_RESULT) {
                error_set(ERR_UART_CMD, bi, rnd);
                res->rounds_uart_err[bi]++;
                res->status = 0xDEAD0030u | bi;
                return;
            }
            if (rlen != 768) {
                error_set(ERR_UART_LEN, bi, rnd);
                res->rounds_uart_err[bi]++;
                res->status = 0xDEAD0040u | bi;
                return;
            }
            res->rounds_recv_ok[bi]++;

            /* Verify */
            phase_set(PHASE_BENCH_VERIFY, bi, (uint16_t)rnd, 0);
            AmorE_Out out_val;
            memcpy(out_val.gamma, uart_buf,       384);
            memcpy(out_val.rho,   uart_buf + 384, 384);
            t0 = cyc();
            int ok = AmorE_Verify(&sk, &sec, &out_val);
            res->verify_total_cycles[bi] += cyc() - t0;

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
                res->status = 0xDEAD0050u | bi;
                return;
            }
        }

        res->amort_cycles[bi] =
            (res->blind_total_cycles[bi] + res->verify_total_cycles[bi]) / N;
    }

    /* --- Security check: one malicious round --- */
    {
        AmorE_Pub pub;
        AmorE_Sec sec;
        uint8_t   pkt[384];

        phase_set(PHASE_SECURITY, 3, 0, 0);
        AmorE_Setup(&sk, G1GEN_BYTES, g_g2gen_bytes, &pub, &sec);

        phase_set(PHASE_BENCH_SEND, 3, 0, 0);
        pub_to_bytes(pkt, &pub);
        uart_send_packet(UART_CMD_SETUP, pkt, 384);
        res->sec_sent = 1;

        phase_set(PHASE_SECURITY_WAIT, 3, 0, 0);
        uint8_t cmd; uint16_t rlen;
        int rc = uart_recv_packet(&cmd, uart_buf, &rlen, sizeof(uart_buf), UART_TIMEOUT_MS);

        if (rc == 0 && cmd == UART_CMD_RESULT && rlen == 768) {
            res->sec_recv_ok = 1;

            phase_set(PHASE_SECURITY_VFY, 3, 0, 0);
            AmorE_Out out_val;
            memcpy(out_val.gamma, uart_buf,       384);
            memcpy(out_val.rho,   uart_buf + 384, 384);
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
