/* =========================================================================
 *  pairing_bench.c — measures pp_map_oatep_k12 on STM32F4 via RELIC
 *
 *  Flow:
 *    1. core_init()
 *    2. ep_param_set_any_pairf() — loads BN254 curve parameters
 *    3. sanity check (G1 scalar multiplication)
 *    4. generate random P, Q (in G1, G2)
 *    5. PB_N_ITER iterations of pp_map_oatep_k12, measuring DWT cycles
 *    6. bilinearity check: e([2]P, Q) == e(P, Q)^2
 * ========================================================================= */

#include "pairing_bench.h"
#include "triggers.h"
#include "stm32f4xx_hal.h"

#include <relic.h>

#include <string.h>

#define CYCLE_COUNT()  (DWT->CYCCNT)

void PB_LogPhase(PairingBenchResults *res, uint8_t phase,
                 uint8_t iteration, uint32_t extra) {
    if (!res) return;
    uint32_t idx = res->log_head & (PB_PHASE_LOG_LEN - 1u);
    res->log[idx].phase     = phase;
    res->log[idx].iteration = iteration;
    res->log[idx].cycles    = CYCLE_COUNT();
    res->log[idx].extra     = extra;
    res->log_head = (res->log_head + 1u);
    res->current_phase = phase;
}

/* RAND callback for RELIC (RAND=CALL mode).
 * Deterministic xorshift32 — sufficient for benchmarking,
 * insecure for production. */
static void rand_callback(uint8_t *buf, size_t len, void *args) {
    (void)args;
    static uint32_t state = 0xCAFEBABEu;
    for (size_t i = 0; i < len; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        buf[i] = (uint8_t)(state & 0xFFu);
    }
}

int PairingBench_Run(PairingBenchResults *res) {
    if (!res) return -1;

    memset(res, 0, sizeof(*res));
    res->magic        = 0;
    res->fw_version   = PB_FW_VER;
    res->core_mhz     = 168u;
    res->n_iterations = PB_N_ITER;
    res->status       = 0xBAD00000u;
    res->pairing_min_cycles = 0xFFFFFFFFu;

    uint32_t t0_wall = HAL_GetTick();

    /* ---- Init ---- */
    PB_LogPhase(res, PB_PHASE_RELIC_INIT, 0, 0);
    uint32_t t0 = CYCLE_COUNT();

    if (core_init() != RLC_OK) {
        res->last_error = PB_ERR_RELIC_INIT;
        return -2;
    }
    rand_seed(rand_callback, NULL);

    PB_LogPhase(res, PB_PHASE_CURVE_INIT, 0, 0);
    if (ep_param_set_any_pairf() != RLC_OK) {
        res->last_error = PB_ERR_CURVE_INIT;
        core_clean();
        return -3;
    }
    res->init_cycles = CYCLE_COUNT() - t0;
    res->init_ok     = 1;

    /* ---- Sanity check (g1 mul) ---- */
    PB_LogPhase(res, PB_PHASE_SANITY, 0, 0);
    {
        uint32_t s0 = CYCLE_COUNT();
        ep_t p, q;
        bn_t k, n;

        ep_null(p); ep_null(q);
        bn_null(k); bn_null(n);

        int ok = 0;
        RLC_TRY {
            ep_new(p); ep_new(q);
            bn_new(k); bn_new(n);

            ep_curve_get_ord(n);
            bn_rand_mod(k, n);
            ep_curve_get_gen(p);
            ep_mul(q, p, k);
            ok = !ep_is_infty(q);
        } RLC_CATCH_ANY {
            ok = 0;
        } RLC_FINALLY {
            ep_free(p); ep_free(q);
            bn_free(k); bn_free(n);
        }

        res->sanity_cycles = CYCLE_COUNT() - s0;
        res->sanity_ok = ok;
        if (!ok) {
            res->last_error = PB_ERR_SANITY;
            core_clean();
            return -4;
        }
    }

    /* ---- Generate P (G1), Q (G2) ---- */
    PB_LogPhase(res, PB_PHASE_GEN_RAND, 0, 0);

    ep_t   P;
    ep2_t  Q;
    fp12_t r;

    ep_null(P);  ep2_null(Q);  fp12_null(r);

    int crit_fail = 0;

    RLC_TRY {
        ep_new(P);
        ep2_new(Q);
        fp12_new(r);

        uint32_t g0 = CYCLE_COUNT();
        ep_rand(P);
        ep2_rand(Q);
        res->gen_rand_cycles = CYCLE_COUNT() - g0;
        res->gen_rand_ok = 1;

        /* ---- Pairing loop ---- */
        for (uint32_t i = 0; i < PB_N_ITER; i++) {
            PB_LogPhase(res, PB_PHASE_PAIRING, (uint8_t)i, 0);
            TRIG_COMPUTE_HI();
            uint32_t p0 = CYCLE_COUNT();
            pp_map_oatep_k12(r, P, Q);
            uint32_t cycles = CYCLE_COUNT() - p0;
            TRIG_COMPUTE_LO();

            res->pairing_cycles[i] = cycles;

            /* ⭐ FIX: a valid pairing result is non-zero AND not the identity (1).
             *
             *   fp12_cmp_dig(r, 1) returns RLC_EQ (==0) when r equals 1,
             *   and RLC_NE (!=0) otherwise.
             *
             *   We want pairing_ok=1 when r is neither 0 nor 1. */
            int is_zero  = fp12_is_zero(r);
            int is_one   = (fp12_cmp_dig(r, 1) == RLC_EQ);
            res->pairing_ok[i] = (!is_zero && !is_one) ? 1u : 0u;

            res->pairing_total_cycles += cycles;
            if (cycles < res->pairing_min_cycles) res->pairing_min_cycles = cycles;
            if (cycles > res->pairing_max_cycles) res->pairing_max_cycles = cycles;
        }
        res->pairing_avg_cycles = res->pairing_total_cycles / PB_N_ITER;

        /* ---- Bilinearity check ---- */
        PB_LogPhase(res, PB_PHASE_VERIFY, 0, 0);
        {
            uint32_t b0 = CYCLE_COUNT();
            ep_t   P2;
            fp12_t r1, r2;

            ep_null(P2); fp12_null(r1); fp12_null(r2);
            ep_new(P2);  fp12_new(r1); fp12_new(r2);

            ep_dbl(P2, P);
            ep_norm(P2, P2);

            pp_map_oatep_k12(r1, P,  Q);
            pp_map_oatep_k12(r2, P2, Q);
            fp12_sqr(r1, r1);

            res->bilinear_ok = (fp12_cmp(r1, r2) == RLC_EQ);
            res->bilinear_cycles = CYCLE_COUNT() - b0;

            ep_free(P2); fp12_free(r1); fp12_free(r2);
        }
    } RLC_CATCH_ANY {
        crit_fail = 1;
    } RLC_FINALLY {
        ep_free(P);
        ep2_free(Q);
        fp12_free(r);
    }

    res->wall_ms = HAL_GetTick() - t0_wall;
    core_clean();

    if (crit_fail) {
        res->last_error = PB_ERR_PAIRING_FAIL;
        return -5;
    }
    if (!res->bilinear_ok) {
        res->last_error = PB_ERR_BILINEAR_FAIL;
        return -6;
    }

    PB_LogPhase(res, PB_PHASE_DONE, 0, 0);
    res->magic  = PB_MAGIC;
    res->status = 0x600D0000u;
    return 0;
}
