/*
 * triggers.h — GPIO trigger lines for energy/timing instrumentation.
 *
 * Direct BSRR writes, ~3 cycles per toggle, no HAL overhead.
 * When AMORE_TRIGGERS_ENABLED == 0, all macros expand to nothing —
 * firmware builds remain byte-identical to the un-instrumented baseline.
 *
 * Channel assignments (per PIN_DIAGRAM.md / MOCK_PPK2_SPEC §5):
 *   PA0 -> bit 0 of gpio_byte: Setup / Blind / Verify  (compute phases)
 *   PA1 -> bit 1 of gpio_byte: ServerWait              (CPU idle / UART RTT)
 *   PA4 -> bit 2 of gpio_byte: Mode-C UART burst       (Mode C only)
 *
 * Note: PA2 (USART2 TX) and PA3 (USART2 RX) are reserved by the protocol
 *       UART and must NOT be reconfigured here.
 */
#ifndef AMORE_TRIGGERS_H
#define AMORE_TRIGGERS_H

#include "stm32f4xx_hal.h"

#define TRIG_PIN_COMPUTE   GPIO_PIN_0   /* PA0 */
#define TRIG_PIN_WAIT      GPIO_PIN_1   /* PA1 */
#define TRIG_PIN_UART      GPIO_PIN_4   /* PA4 */

#define TRIG_ALL_PINS      (TRIG_PIN_COMPUTE | TRIG_PIN_WAIT | TRIG_PIN_UART)

#ifndef AMORE_TRIGGERS_ENABLED
#  define AMORE_TRIGGERS_ENABLED 0
#endif

#if AMORE_TRIGGERS_ENABLED

/* BSRR upper half (bits 16..31) clears, lower half (bits 0..15) sets. */
#define TRIG_SET_HI(pin)   do { GPIOA->BSRR = (uint32_t)(pin); } while (0)
#define TRIG_SET_LO(pin)   do { GPIOA->BSRR = ((uint32_t)(pin)) << 16; } while (0)

#define TRIG_COMPUTE_HI()  TRIG_SET_HI(TRIG_PIN_COMPUTE)
#define TRIG_COMPUTE_LO()  TRIG_SET_LO(TRIG_PIN_COMPUTE)
#define TRIG_WAIT_HI()     TRIG_SET_HI(TRIG_PIN_WAIT)
#define TRIG_WAIT_LO()     TRIG_SET_LO(TRIG_PIN_WAIT)
#define TRIG_UART_HI()     TRIG_SET_HI(TRIG_PIN_UART)
#define TRIG_UART_LO()     TRIG_SET_LO(TRIG_PIN_UART)

/* Single-shot lower-all-triggers — useful before returns on error paths
 * so the pins don't latch high while the firmware halts. */
#define TRIG_ALL_LO()      do { GPIOA->BSRR = ((uint32_t)(TRIG_ALL_PINS)) << 16; } while (0)

static inline void Triggers_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = TRIG_ALL_PINS;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    TRIG_ALL_LO();
}

#else  /* AMORE_TRIGGERS_ENABLED == 0 */

#define TRIG_COMPUTE_HI()  ((void)0)
#define TRIG_COMPUTE_LO()  ((void)0)
#define TRIG_WAIT_HI()     ((void)0)
#define TRIG_WAIT_LO()     ((void)0)
#define TRIG_UART_HI()     ((void)0)
#define TRIG_UART_LO()     ((void)0)
#define TRIG_ALL_LO()      ((void)0)

static inline void Triggers_Init(void) { /* no-op */ }

#endif  /* AMORE_TRIGGERS_ENABLED */

#endif  /* AMORE_TRIGGERS_H */
