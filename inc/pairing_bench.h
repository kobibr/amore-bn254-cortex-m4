#pragma once
#include <stdint.h>

/* =========================================================================
 *  pairing_bench.h — results of the local pairing benchmark (RELIC on STM32)
 *
 *  The layout mirrors AmorE_BenchResults from the main project so that the
 *  same GDB-based inspection workflow applies. The global symbol is
 *  g_pb_results.
 *
 *  Status code 0x600D0000 indicates a successful run.
 * ========================================================================= */

/* Phase codes */
#define PB_PHASE_INIT          0x01u
#define PB_PHASE_DWT           0x02u
#define PB_PHASE_RELIC_INIT    0x03u
#define PB_PHASE_CURVE_INIT    0x04u
#define PB_PHASE_SANITY        0x05u
#define PB_PHASE_GEN_RAND      0x10u
#define PB_PHASE_PAIRING       0x11u
#define PB_PHASE_VERIFY        0x12u
#define PB_PHASE_DONE          0xFFu

/* Error codes */
#define PB_ERR_NONE            0x00000000u
#define PB_ERR_RELIC_INIT      0x00000001u
#define PB_ERR_CURVE_INIT      0x00000002u
#define PB_ERR_SANITY          0x00000003u
#define PB_ERR_PAIRING_FAIL    0x00000010u
#define PB_ERR_PAIRING_DEGEN   0x00000011u
#define PB_ERR_BILINEAR_FAIL   0x00000012u

#define PB_PHASE_LOG_LEN 32u

typedef struct {
    uint8_t  phase;
    uint8_t  iteration;
    uint16_t reserved;
    uint32_t cycles;
    uint32_t extra;
} PB_PhaseLogEntry;

#define PB_MAGIC       0xB0CCAA00u
#define PB_FW_VER      0x00010000u
#ifndef PB_N_ITER
#  define PB_N_ITER 10u
#endif

typedef struct {
    /* Identity */
    uint32_t magic;
    uint32_t fw_version;
    uint32_t core_mhz;

    /* Live state */
    uint32_t current_phase;
    uint32_t last_error;
    uint32_t error_iteration;

    /* Init */
    uint32_t init_cycles;
    uint32_t init_ok;

    /* Sanity (g1 mul) */
    uint32_t sanity_cycles;
    uint32_t sanity_ok;

    /* Random points generation */
    uint32_t gen_rand_cycles;
    uint32_t gen_rand_ok;

    /* Pairing iterations */
    uint32_t n_iterations;
    uint32_t pairing_cycles[PB_N_ITER];
    uint32_t pairing_ok[PB_N_ITER];

    /* Aggregate stats */
    uint32_t pairing_min_cycles;
    uint32_t pairing_max_cycles;
    uint32_t pairing_avg_cycles;
    uint32_t pairing_total_cycles;

    /* Bilinearity check  e(2P,Q) == e(P,Q)^2 */
    uint32_t bilinear_cycles;
    uint32_t bilinear_ok;

    /* Ring log */
    uint32_t         log_head;
    PB_PhaseLogEntry log[PB_PHASE_LOG_LEN];

    /* Final */
    uint32_t wall_ms;
    uint32_t status;
} PairingBenchResults;

/* API */
int  PairingBench_Run(PairingBenchResults *res);
void PB_LogPhase(PairingBenchResults *res, uint8_t phase,
                 uint8_t iteration, uint32_t extra);
