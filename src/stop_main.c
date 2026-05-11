/*
 * stop_main.c — Stop-mode test.
 *
 * Behavior:
 *   1. Init clocks, LEDs, GPIO triggers.
 *   2. Loop 6 times:
 *      - PA0 HIGH (visible run period)
 *      - 100 ms busy wait (gives Mock PPK2 sampler time to see HIGH)
 *      - PA0 LOW
 *      - Arm RTC wakeup for ~5 seconds
 *      - Enter Stop mode (CPU off, only RTC/LSI running)
 *      - Wake → restore clock → next iteration
 *   3. LED pattern: PD12 toggles on each wake to confirm visual liveness.
 *
 * Expected energy signature on PPK2:
 *   - Spikes at PA0 HIGH (CPU at 168 MHz, ~30 mA)
 *   - Floor at PA0 LOW (Stop mode, ~3 µA quiescent)
 *   - 6 spikes over ~30 seconds
 *
 * Status: completes silently. LEDs cycle continuously — confirms it returned.
 */
#include "stm32f4xx_hal.h"
#include "triggers.h"
#include "lowpower.h"

#define STOP_TEST_CYCLES   6u
#define STOP_DURATION_TICKS 80000u   /* ~5 s at 16 kHz wakeup tick */

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

    LED_Set(0x1);   /* PD12 on = booting */

    for (uint32_t i = 0; i < STOP_TEST_CYCLES; i++) {
        /* Run phase: ~100 ms wake-and-blink */
        TRIG_COMPUTE_HI();
        LED_Set(0xF);    /* all 4 LEDs on */
        HAL_Delay(100);
        LED_Set(0x0);
        TRIG_COMPUTE_LO();

        /* Stop phase: arm RTC, enter Stop, wake */
        LowPower_ArmWakeUp(STOP_DURATION_TICKS);
        LowPower_EnterStopAndWait();
        LowPower_RestoreSysClock168();
    }

    /* Done — slow blink on PD12 to indicate completion */
    while (1) {
        LED_Set(0x1);
        HAL_Delay(500);
        LED_Set(0x0);
        HAL_Delay(500);
    }
}

void NMI_Handler(void)      { while(1); }
void HardFault_Handler(void){ while(1); }
void MemManage_Handler(void){ while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void){ while(1); }
