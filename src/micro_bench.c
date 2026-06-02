/* =========================================================================
 *  micro_bench.c  —  FAIR primitive microbench for AmorE (Level 1)
 *
 *  Measures the AmorE client's building blocks with RELIC's OWN routines,
 *  on the same STM32F407@168MHz + same DWT + same librelic_s.a that
 *  produced the pp_map_oatep_k12 baseline. Drop-in for the relic_bench
 *  harness (same init path as pairing_bench.c: core_init +
 *  ep_param_set_any_pairf + rand_seed).
 *
 *  INTEGRATION (two options):
 *   (a) Add a CMake target like relic_bench but with src/micro_main.c
 *       (a 6-line main that calls Micro_Run(&g_micro)), OR
 *   (b) In pairing_main.c, after PairingBench_Run(...), add:
 *           extern void Micro_Run(void);  Micro_Run();
 *       and add src/micro_bench.c to that target's sources.
 *
 *  READ-OUT: g_micro is GDB-readable, same as g_pb_results. After the run:
 *       (gdb) print g_micro
 *  All fields are cycles (min over ITERS), 168 MHz. Feed them to
 *  paper_predict.py per FAIR_COMPARISON_PROTOCOL.md, Level 1.
 * ========================================================================= */

#include "stm32f4xx_hal.h"
#include <relic.h>
#include <string.h>

#define CYC()   (DWT->CYCCNT)
#define ITERS   16          /* min-of-N: kills jitter, each op < 25.6 s     */
#define PHI     90u         /* short-scalar bits (AMORE_PHI)                */

typedef struct {
    uint32_t magic;         /* 0x600D0000 on success                       */
    uint32_t p_cyc;         /* pp_map_oatep_k12 (must match baseline)       */
    uint32_t m1_var;        /* ep_mul, variable base, full                  */
    uint32_t m1_fix;        /* ep_mul_gen, fixed base, full  (U=[u]P)       */
    uint32_t m1_short;      /* ep_mul, variable base, 90-bit                */
    uint32_t m2_var;        /* ep2_mul, variable base, full                 */
    uint32_t m2_fix;        /* ep2_mul_gen, fixed base, full (V,X)          */
    uint32_t m2_short;      /* ep2_mul, variable base, 90-bit (m_bar_2)     */
    uint32_t mT_full;       /* gt_exp, full 254/255-bit (OneTimeSetup)      */
    uint32_t mT_short;      /* gt_exp, 90-bit  (m_bar_T, cyclotomic!)       */
    uint32_t memT;          /* gt_is_valid (membership)                     */
    uint32_t status;        /* 0x600D0000 ok / 0xBAD..                      */
    uint32_t last_error;
} MicroResults;

MicroResults g_micro;

static void micro_rand(uint8_t *buf, size_t len, void *a) {
    (void)a; static uint32_t s = 0xCAFEBABEu;
    for (size_t i=0;i<len;i++){ s^=s<<13; s^=s>>17; s^=s<<5; buf[i]=(uint8_t)s; }
}

/* run op() ITERS times, return min cycles. op is a statement block via macro */
#define BENCH(dst, STMT) do {                              \
    STMT;                          /* warm-up */           \
    uint32_t _best = 0xFFFFFFFFu;                          \
    for (int _i=0; _i<ITERS; _i++){                        \
        uint32_t _t0 = CYC();                              \
        STMT;                                              \
        uint32_t _d = CYC() - _t0;                         \
        if (_d < _best) _best = _d;                        \
    }                                                      \
    (dst) = _best;                                         \
} while (0)

void Micro_Run(void) {
    memset(&g_micro, 0, sizeof(g_micro));
    g_micro.status = 0xBAD00000u;

    if (core_init() != RLC_OK)               { g_micro.last_error=1; return; }
    rand_seed(micro_rand, NULL);
    if (ep_param_set_any_pairf() != RLC_OK)  { g_micro.last_error=2; core_clean(); return; }

    ep_t  P, Pv, R1;
    ep2_t Q, Qv, R2;
    gt_t  gtv, Rt;
    bn_t  ord, kfull, kshort;

    ep_null(P); ep_null(Pv); ep_null(R1);
    ep2_null(Q); ep2_null(Qv); ep2_null(R2);
    gt_null(gtv); gt_null(Rt);
    bn_null(ord); bn_null(kfull); bn_null(kshort);

    RLC_TRY {
        ep_new(P); ep_new(Pv); ep_new(R1);
        ep2_new(Q); ep2_new(Qv); ep2_new(R2);
        gt_new(gtv); gt_new(Rt);
        bn_new(ord); bn_new(kfull); bn_new(kshort);

        /* generators, a valid GT element, random variable-base points */
        ep_curve_get_gen(P);
        ep2_curve_get_gen(Q);
        pp_map_oatep_k12(gtv, P, Q);          /* gtv is a real GT element */
        ep_rand(Pv);                          /* non-generator G1 point   */
        ep2_rand(Qv);                         /* non-generator G2 point   */

        ep_curve_get_ord(ord);
        bn_rand_mod(kfull, ord);              /* full-range exponent      */
        bn_rand(kshort, RLC_POS, PHI);        /* exactly 90-bit           */

        /* ---- reference pairing (must match the published baseline) ---- */
        BENCH(g_micro.p_cyc,   pp_map_oatep_k12(Rt, P, Q));

        /* ---- G1 ---- */
        BENCH(g_micro.m1_var,   ep_mul(R1, Pv, kfull));    /* variable full */
        BENCH(g_micro.m1_fix,   ep_mul_gen(R1, kfull));    /* fixed   full  */
        BENCH(g_micro.m1_short, ep_mul(R1, Pv, kshort));   /* variable 90b  */

        /* ---- G2 ---- */
        BENCH(g_micro.m2_var,   ep2_mul(R2, Qv, kfull));   /* variable full */
        BENCH(g_micro.m2_fix,   ep2_mul_gen(R2, kfull));   /* fixed   full  */
        BENCH(g_micro.m2_short, ep2_mul(R2, Qv, kshort));  /* variable 90b  */

        /* ---- GT (MUST use gt_exp = cyclotomic, never fp12_exp) ---- */
        BENCH(g_micro.mT_full,  gt_exp(Rt, gtv, kfull));   /* full exp      */
        BENCH(g_micro.mT_short, gt_exp(Rt, gtv, kshort));  /* m_bar_T (90b) */

        /* ---- membership ---- */
        BENCH(g_micro.memT,     (void)gt_is_valid(gtv));

        g_micro.status = 0x600D0000u;
    } RLC_CATCH_ANY {
        g_micro.last_error = 99;
    } RLC_FINALLY {
        ep_free(P); ep_free(Pv); ep_free(R1);
        ep2_free(Q); ep2_free(Qv); ep2_free(R2);
        gt_free(gtv); gt_free(Rt);
        bn_free(ord); bn_free(kfull); bn_free(kshort);
    }
    core_clean();
}

/* Accessor so the entry point can check success without redefining the struct. */
uint32_t Micro_Status(void) { return g_micro.status; }
