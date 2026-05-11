/*
 * lowpower.h — minimal Stop-mode + RTC wakeup helpers for STM32F407.
 *
 * Uses LSI (~32 kHz) since the F4 Discovery has no LSE crystal.
 * Wake-up resolution is ~31 µs, sufficient for the ~10 ms intervals
 * used in the wake-up burst test.
 */
#ifndef AMORE_LOWPOWER_H
#define AMORE_LOWPOWER_H

#include "stm32f4xx_hal.h"

/* Initialize the RTC for wake-up timer use. Enables LSI, backup domain,
 * and configures the RTC peripheral. Call once after HAL_Init. */
void LowPower_RTC_Init(void);

/* Arm a wake-up alarm. `interval_ticks` is in units of (RTC_WAKEUPCLOCK_RTCCLK_DIV2),
 * so at LSI=32kHz the tick rate is 16 kHz → 1 tick = ~62.5 µs.
 * For 10 ms wakeup: pass 160 ticks. */
void LowPower_ArmWakeUp(uint32_t interval_ticks);

/* Enter Stop mode. Returns when wake-up event fires.
 * Note: caller MUST restore SystemClock after wake (Stop disables PLL). */
void LowPower_EnterStopAndWait(void);

/* Restore 168 MHz SysClk after waking from Stop (PLL re-enable). */
void LowPower_RestoreSysClock168(void);

#endif
