/*
 * wakeup_burst_main.c — wake-up burst for Pre-PPK2 Mock Day 4.
 *
 * 100 consecutive Stop→Run cycles, ~10 ms each (RTC wake every 10 ms).
 * Total runtime ~1 second wall-clock.
 *
 * PA0 high during Run; low during Stop. Mock PPK2 / real PPK2 sees
 * 100 rising edges, 100 falling edges. The integral of current over
 * the run intervals minus the Stop floor = E_wakeup per transition.
 *
 * LED pattern: PD12 stays on the whole test (confirms execution).
 *              All 4 LEDs after burst completes = done.
 */
#include "stm32f4xx_hal.h"
#include "triggers.h"
#include "lowpower.h"

#define WAKEUP_BURST_CYCLES   100u
#define WAKEUP_INTERVAL_TICKS 160u   /* ~10 ms at 16 kHz wake-up tick */

static void SystemClock_Config(void) {
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

    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
}

static void LED_Init(void) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &g);
}

static void LED_Set(uint32_t pattern) {
    GPIOD->BSRR = ((~pattern & 0xFu) << (12 + 16)) | ((pattern & 0xFu) << 12);
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Triggers_Init();
    LowPower_RTC_Init();

    LED_Set(0x1);   /* PD12 on for the whole burst */

    /* Pre-burst marker: PA0 quick double-pulse so the sampler can find t=0 */
    TRIG_COMPUTE_HI();
    HAL_Delay(2);
    TRIG_COMPUTE_LO();
    HAL_Delay(2);
    TRIG_COMPUTE_HI();
    HAL_Delay(2);
    TRIG_COMPUTE_LO();
    HAL_Delay(50);  /* settle */

    for (uint32_t i = 0; i < WAKEUP_BURST_CYCLES; i++) {
        TRIG_COMPUTE_HI();
        /* Brief run period — just enough to be visible (~30 µs typical wake latency
         * already counts as part of Run from energy perspective). Add a token
         * delay loop to make the Run portion easily distinguishable from noise. */
        for (volatile uint32_t k = 0; k < 1000; k++) { /* ~6 µs @ 168 MHz */ }
        TRIG_COMPUTE_LO();

        LowPower_ArmWakeUp(WAKEUP_INTERVAL_TICKS);
        LowPower_EnterStopAndWait();
        LowPower_RestoreSysClock168();
    }

    /* Burst done — all 4 LEDs on */
    LED_Set(0xF);

    while (1) {
        /* slow heartbeat */
        LED_Set(0xF);
        HAL_Delay(500);
        LED_Set(0x1);
        HAL_Delay(500);
    }
}

void NMI_Handler(void)      { while(1); }
void HardFault_Handler(void){ while(1); }
void MemManage_Handler(void){ while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void){ while(1); }
