/*
 * lowpower.c — Stop-mode + RTC wakeup implementation.
 *
 * Strategy:
 *   - Use LSI (~32 kHz internal RC) as RTC clock.
 *   - Wakeup timer clock = LSI / 2 = ~16 kHz → 1 tick = ~62.5 µs.
 *   - HAL_PWR_EnterSTOPMode() halts CPU; RTC IRQ wakes it.
 *   - On wake, PLL is off and SysClk fell back to HSI (16 MHz).
 *     We must re-enable PLL to restore 168 MHz operation.
 *
 * No printf, no UART, no malloc. All HAL only.
 */
#include "lowpower.h"

static RTC_HandleTypeDef hrtc;

void LowPower_RTC_Init(void) {
    /* Enable power interface and backup-domain access (RTC lives there). */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* Start LSI (the F4 Discovery has no LSE crystal). */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState       = RCC_LSI_ON;
    osc.PLL.PLLState   = RCC_PLL_NONE;  /* don't touch main PLL */
    HAL_RCC_OscConfig(&osc);

    /* Route LSI to the RTC. */
    RCC_PeriphCLKInitTypeDef pclk = {0};
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&pclk);

    __HAL_RCC_RTC_ENABLE();

    /* Init the RTC peripheral itself (calendar values are irrelevant
     * — we only use the wake-up timer). LSI typical = 32 kHz, but the
     * HAL prescalers for calendar don't apply when we don't use it. */
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 249;   /* 32000 / 128 / 250 ≈ 1 Hz (calendar) */
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;
    HAL_RTC_Init(&hrtc);

    /* Enable the RTC wake-up interrupt in NVIC. */
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);
}

void LowPower_ArmWakeUp(uint32_t interval_ticks) {
    /* Use RTCCLK/2 prescaler → 16 kHz wake-up tick at LSI 32kHz. */
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, interval_ticks, RTC_WAKEUPCLOCK_RTCCLK_DIV2);
}

void LowPower_EnterStopAndWait(void) {
    /* Suspend SysTick so it doesn't immediately wake us. */
    HAL_SuspendTick();
    /* Regulator in low-power mode, wake on interrupt. */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    /* On exit: PLL is off, SysClk fell back to HSI 16 MHz.
     * Caller may restore 168 MHz via LowPower_RestoreSysClock168(). */
    HAL_ResumeTick();
}

void LowPower_RestoreSysClock168(void) {
    /* Replay the clock tree configuration from main.c.
     * HSI is already running (it was the fallback on Stop exit). */
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 8;
    osc.PLL.PLLN            = 168;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 7;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);
}

/* RTC wake-up IRQ handler — required for HAL_PWR_EnterSTOPMode to exit. */
void RTC_WKUP_IRQHandler(void) {
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_local) {
    (void)hrtc_local;
    /* nothing to do — Stop mode resumed automatically */
}
