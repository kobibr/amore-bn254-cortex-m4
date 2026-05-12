#pragma once
#include "g1.h"
#include "curve.h"
#include "g2.h"
#include "fp12.h"
#include "fq.h"

/* -----------------------------------------------------------------------
 * AmorE Phase codes  (stored in BenchResults.current_phase)
 * Updated atomically before each step — a crash/hang → last written phase
 * tells you exactly where execution stopped.
 * ----------------------------------------------------------------------- */
#define PHASE_INIT          0x01u  /* HAL init complete                   */
#define PHASE_WAIT_READY    0x02u  /* Waiting for server CMD_READY        */
#define PHASE_OTS           0x03u  /* Executing OneTimeSetup              */
#define PHASE_OTS_DONE      0x04u  /* OneTimeSetup finished               */
#define PHASE_BENCH_SETUP   0x10u  /* Inside AmorE_Setup() for a round    */
#define PHASE_BENCH_SEND    0x11u  /* Sending CMD_SETUP packet            */
#define PHASE_BENCH_WAIT    0x12u  /* Waiting for CMD_RESULT              */
#define PHASE_BENCH_VERIFY  0x13u  /* Executing AmorE_Verify()            */
#define PHASE_BENCH_STATUS  0x14u  /* Sending CMD_STATUS                  */
#define PHASE_SECURITY      0x20u  /* Security-check round in progress    */
#define PHASE_SECURITY_WAIT 0x21u  /* Waiting for malicious result        */
#define PHASE_SECURITY_VFY  0x22u  /* Running verify on malicious output  */
#define PHASE_DONE          0xFFu  /* All phases complete, results valid   */

/* -----------------------------------------------------------------------
 * Error codes  (stored in BenchResults.last_error; 0 = no error)
 * ----------------------------------------------------------------------- */
#define ERR_NONE              0x00000000u
#define ERR_UART_SEND         0x00000001u
#define ERR_UART_RECV         0x00000002u
#define ERR_UART_CRC          0x00000003u
#define ERR_UART_CMD          0x00000004u
#define ERR_UART_LEN          0x00000005u
#define ERR_UART_TIMEOUT      0x00000006u
#define ERR_VERIFY_HONEST     0x00000010u  /* honest round rejected (bad!) */
#define ERR_VERIFY_MALICIOUS  0x00000011u  /* malicious round accepted (bug!) */
#define ERR_SCALAR_ZERO       0x00000020u

/* -----------------------------------------------------------------------
 * Core structs
 * ----------------------------------------------------------------------- */
typedef struct {
    Fq       s;
    Fp12     xi;
    uint32_t tau_ms;
    uint32_t phi;
} AmorE_SK;

typedef struct {
    uint32_t r[8];
    int      nbits;
} AmorE_Sec;

typedef struct {
    uint8_t A[G1_BYTES];   /* G1 element: 48 bytes per coord (BLS12-381) */
    uint8_t B[G2_BYTES];  /* G2 element: 96 bytes per Fp2 coord         */
    uint8_t C[G1_BYTES];
    uint8_t D[G2_BYTES];
} AmorE_Pub;

typedef struct {
    uint8_t gamma[FP12_BYTES];  /* Fp12 element: 48 bytes per coefficient × 12 */
    uint8_t rho[FP12_BYTES];
} AmorE_Out;

/* -----------------------------------------------------------------------
 * Phase ring-log — last 32 events written before crash or completion
 * ----------------------------------------------------------------------- */
#define PHASE_LOG_LEN 32u

typedef struct {
    uint8_t  phase;         /* PHASE_* code                     */
    uint8_t  batch;         /* 0=N1, 1=N10, 2=N50, 3=security  */
    uint16_t round;         /* round index within batch         */
    uint32_t cycles;        /* DWT snapshot at this moment      */
    uint32_t extra;         /* error code or auxiliary value    */
} PhaseLogEntry;

/* -----------------------------------------------------------------------
 * Master benchmark result (mapped to known symbol, readable by GDB/script)
 * ----------------------------------------------------------------------- */
#define AMORE_MAGIC  0xA0AAAA00u   /* set only when PHASE_DONE reached    */
#define AMORE_FW_VER 0x00020000u   /* v2.0                                */
#define BENCH_N_COUNT 3u

typedef struct {
    /* Identity */
    uint32_t magic;
    uint32_t fw_version;
    uint32_t core_mhz;

    /* Live phase tracking */
    uint32_t current_phase;
    uint32_t last_error;
    uint32_t error_round;
    uint32_t error_batch;

    /* OneTimeSetup */
    uint32_t ots_cycles;
    uint32_t ots_ok;            /* 1 = completed without error      */

    /* Per-batch telemetry (index 0=N1, 1=N10, 2=N50) */
    uint32_t N_vals[BENCH_N_COUNT];
    uint64_t blind_total_cycles[BENCH_N_COUNT];   /* uint64 to prevent overflow at large N */
    uint64_t verify_total_cycles[BENCH_N_COUNT]; /* uint64 to prevent overflow at large N */
    uint32_t amort_cycles[BENCH_N_COUNT];
    /* Extended telemetry for overflow debugging */
    uint64_t per_round_blind_n50[50];   /* per-round cycles ב-N=50, blind */
    uint64_t per_round_verify_n50[50];  /* per-round cycles ב-N=50, verify */
    uint32_t overflow_detected;          /* 1 if any aggregation hit uint32 overflow */
    uint32_t rounds_sent[BENCH_N_COUNT];
    uint32_t rounds_recv_ok[BENCH_N_COUNT];
    uint32_t rounds_verify_ok[BENCH_N_COUNT];
    uint32_t rounds_verify_fail[BENCH_N_COUNT];
    uint32_t rounds_uart_err[BENCH_N_COUNT];

    /* Security round */
    uint32_t sec_sent;
    uint32_t sec_recv_ok;
    uint32_t sec_verify_result;  /* 0=rejected(OK), 1=accepted(BUG), 2=no resp */
    uint32_t security_ok;        /* 1 = malicious server was caught  */

    /* Totals */
    uint32_t total_rounds_sent;
    uint32_t total_verify_ok;
    uint32_t total_uart_errors;
    uint32_t wall_ms;            /* elapsed ms across entire benchmark */

    /* Phase ring-log */
    uint32_t      log_head;
    PhaseLogEntry log[PHASE_LOG_LEN];


    /* === DEBUG TELEMETRY (2026-05-12, BN254 verify investigation) === */
    uint8_t  dbg_first_uart[32];    /* first 32 bytes of uart_buf after recv */
    uint8_t  dbg_first_gamma[16];   /* first 16 bytes of out_val.gamma */
    uint8_t  dbg_first_rho[16];     /* first 16 bytes of out_val.rho */
    uint8_t  dbg_first_xi[16];      /* first 16 bytes of sk.xi (from OTS) */
    uint8_t  dbg_first_sec_r[16];   /* first 16 bytes of sec.r (the secret) */
    uint32_t dbg_sk_s_first;        /* first uint32 of sk.s */
    uint32_t dbg_sec_nbits;         /* sec.nbits */
    uint32_t dbg_verify_called;     /* incremented each Verify call */
    uint32_t dbg_verify_last_ok;    /* last Verify return value */
    uint32_t dbg_round_captured;    /* round number where snapshot taken */
    uint32_t dbg_uart_rlen;         /* rlen received from server */

    /* Final status */
    uint32_t status;             /* 0x600D0000 = everything passed   */
} AmorE_BenchResults;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */
void AmorE_OneTimeSetup(AmorE_SK *sk, uint32_t phi, uint32_t tau_ms);

void AmorE_Setup(const AmorE_SK *sk,
                 const uint8_t A[G1_BYTES], const uint8_t B[G2_BYTES],
                 AmorE_Pub *pub, AmorE_Sec *sec);

int  AmorE_Verify(const AmorE_SK *sk, const AmorE_Sec *sec, const AmorE_Out *out);

void AmorE_RunBenchmark(AmorE_BenchResults *res);
