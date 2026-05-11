/* =========================================================================
 *  pairing_main.c — entry point of the RELIC pairing benchmark
 *
 *  This is a separate main() from the AmorE entry point; they do not
 *  coexist in the same ELF. CMakeLists.txt builds two distinct executables,
 *  each linked with its own main().
 * ========================================================================= */

#include "stm32f4xx_hal.h"
#include "pairing_bench.h"
#include "triggers.h"
#include <string.h>

/* Globals readable by GDB */
PairingBenchResults g_pb_results;

/* -----------------------------------------------------------------------
 *  System Clock Config — IDENTICAL to AmorE main.c
 *  168 MHz via PLL from HSI 16 MHz
 * ----------------------------------------------------------------------- */
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

void LED_Set(uint32_t pattern) {
    GPIOD->BSRR = ((~pattern & 0xFu) << (12 + 16)) | ((pattern & 0xFu) << 12);
}

static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    DWT_Init();
    Triggers_Init();

    LED_Set(0x1);  /* PD12 (green) on = booting */

    PairingBench_Run(&g_pb_results);

    if (g_pb_results.status == 0x600D0000u) {
        LED_Set(0xF);   /* all 4 LEDs = success */
    } else {
        LED_Set(0x5);   /* alternating = error  */
    }

    uint32_t led_state = 0;
    for (;;) {
        HAL_Delay(500);
        led_state ^= 0x1u;
        LED_Set(led_state);
    }
}

/* HAL required weak callbacks */
void NMI_Handler(void)       { while(1); }
void HardFault_Handler(void) { LED_Set(0x2); while(1); }
void MemManage_Handler(void) { LED_Set(0x3); while(1); }
void BusFault_Handler(void)  { LED_Set(0x6); while(1); }
void UsageFault_Handler(void){ LED_Set(0x9); while(1); }
